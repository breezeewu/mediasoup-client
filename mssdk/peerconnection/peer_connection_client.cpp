#include "peer_connection_client.h"
#include "lazyutil/lazyexception.hpp"
#include "json/json.hpp"
#include "api/task_queue/default_task_queue_factory.h"
#include <api/audio_codecs/builtin_audio_decoder_factory.h>
#include <api/audio_codecs/builtin_audio_encoder_factory.h>
#include <api/create_peerconnection_factory.h>
#include <api/video_codecs/builtin_video_decoder_factory.h>
#include <api/video_codecs/builtin_video_encoder_factory.h>
#include "system_wrappers/include/field_trial.h"
#include "video_capture.h"
#include <json/json.hpp>
#include <json/sdptransform.hpp>
#include "sdputil/jsonvalidate.h"
#include <map>

/*static std::map<SdpType, const std::string> sdpType2String;
static std::map<webrtc::PeerConnectionInterface::IceConnectionState, const std::string>
iceConnectionState2String;
static std::map<webrtc::PeerConnectionInterface::IceGatheringState, const std::string>
iceGatheringState2String;
static std::map<webrtc::PeerConnectionInterface::SignalingState, const std::string> signalingState2String;*/

constexpr uint16_t SctpNumStreamsOs{ 1024u };
constexpr uint16_t SctpNumStreamsMis{ 1024u };

const char kAudioLabel[] = "audio_label";
const char kVideoLabel[] = "video_label";
const char kStreamId[] = "stream_id";

json SctpNumStreams = { { "OS", SctpNumStreamsOs }, { "MIS", SctpNumStreamsMis } };

// clang-format off
std::map<SdpType, const std::string> sdpType2String =
{
	{ SdpType::OFFER,    "offer"    },
	{ SdpType::PRANSWER, "pranswer" },
	{ SdpType::ANSWER,   "answer"   }
};

std::map<webrtc::PeerConnectionInterface::IceConnectionState, const std::string>
iceConnectionState2String =
{
	{ webrtc::PeerConnectionInterface::IceConnectionState::kIceConnectionNew,          "new"          },
	{ webrtc::PeerConnectionInterface::IceConnectionState::kIceConnectionChecking,     "checking"     },
	{ webrtc::PeerConnectionInterface::IceConnectionState::kIceConnectionConnected,    "connected"    },
	{ webrtc::PeerConnectionInterface::IceConnectionState::kIceConnectionCompleted,    "completed"    },
	{ webrtc::PeerConnectionInterface::IceConnectionState::kIceConnectionFailed,       "failed"       },
	{ webrtc::PeerConnectionInterface::IceConnectionState::kIceConnectionDisconnected, "disconnected" },
	{ webrtc::PeerConnectionInterface::IceConnectionState::kIceConnectionClosed,       "closed"       }
};

std::map<webrtc::PeerConnectionInterface::IceGatheringState, const std::string>
iceGatheringState2String =
{
	{ webrtc::PeerConnectionInterface::IceGatheringState::kIceGatheringNew,       "new"       },
	{ webrtc::PeerConnectionInterface::IceGatheringState::kIceGatheringGathering, "gathering" },
	{ webrtc::PeerConnectionInterface::IceGatheringState::kIceGatheringComplete,  "complete"  }
};

std::map<webrtc::PeerConnectionInterface::SignalingState, const std::string>
signalingState2String =
{
	{ webrtc::PeerConnectionInterface::SignalingState::kStable,             "stable"               },
	{ webrtc::PeerConnectionInterface::SignalingState::kHaveLocalOffer,     "have-local-offer"     },
	{ webrtc::PeerConnectionInterface::SignalingState::kHaveLocalPrAnswer,  "have-local-pranswer"  },
	{ webrtc::PeerConnectionInterface::SignalingState::kHaveRemoteOffer,    "have-remote-offer"    },
	{ webrtc::PeerConnectionInterface::SignalingState::kHaveRemotePrAnswer, "have-remote-pranswer" },
	{ webrtc::PeerConnectionInterface::SignalingState::kClosed,             "closed"               }
};

class SetSessionDescriptionObserver : public webrtc::SetSessionDescriptionObserver
{
public:
	SetSessionDescriptionObserver() = default;
	~SetSessionDescriptionObserver() override = default;

	std::future<void> GetFuture()
	{
		return promise.get_future();
	}
	void Reject(const std::string& error)
	{
		promise.set_exception(std::make_exception_ptr(lazyexecption(error.c_str())));
	}

	/* Virtual methods inherited from webrtc::SetSessionDescriptionObserver. */
public:
	void OnSuccess() override 
	{
		promise.set_value();
	}
	void OnFailure(webrtc::RTCError error) override
	{
		lberror("SetSessionDescriptionObserver failure [%s:%s]", webrtc::ToString(error.type()), error.message());
		Reject(std::string(error.message()));
	}

private:
	std::promise<void> promise;
};

class CreateSessionDescriptionObserver : public webrtc::CreateSessionDescriptionObserver
{
public:
	CreateSessionDescriptionObserver() = default;
	~CreateSessionDescriptionObserver() override = default;

	std::future<std::string> GetFuture()
	{
		return promise.get_future();
	}
	void Reject(const std::string& error)
	{
		return promise.set_exception(std::make_exception_ptr(lazyexecption(error.c_str())));
	}

	/* Virtual methods inherited from webrtc::CreateSessionDescriptionObserver. */
public:
	void OnSuccess(webrtc::SessionDescriptionInterface* desc) override
	{
		std::string sdp;
		std::unique_ptr<webrtc::SessionDescriptionInterface> ownedDesc(desc);
		ownedDesc->ToString(&sdp);
		promise.set_value(sdp);
	}
	void OnFailure(webrtc::RTCError error) override
	{
		auto message = std::string(error.message());
		Reject(message);
	}

private:
	std::promise<std::string> promise;
};

class RTCStatsCollectorCallback : public webrtc::RTCStatsCollectorCallback
{
public:
	RTCStatsCollectorCallback() = default;
	~RTCStatsCollectorCallback() override = default;

	std::future<nlohmann::json> GetFuture()
	{
		return promise.get_future();
	}

	/* Virtual methods inherited from webrtc::RTCStatsCollectorCallback. */
public:
	void OnStatsDelivered(const rtc::scoped_refptr<const webrtc::RTCStatsReport>& report) override
	{
		//MSC_TRACE();

		std::string s = report->ToJson();

		// RtpReceiver stats JSON string is sometimes empty.
		if (s.empty())
			this->promise.set_value(json::array());
		else
			this->promise.set_value(json::parse(s));
	};

private:
	std::promise<nlohmann::json> promise;
};

PeerConnectionClient::PeerConnectionClient(std::shared_ptr<PrivateListener> privateListener, webrtc::PeerConnectionInterface::RTCConfiguration* pconfig):pri_listener_(privateListener)
{
	webrtc::PeerConnectionInterface::RTCConfiguration config;
	webrtc::BitrateSettings bitset;
	bitset.start_bitrate_bps = 500000;
	init();
	if (pconfig)
	{
		config = *pconfig;
	}
	config.sdp_semantics = webrtc::SdpSemantics::kUnifiedPlan;
	pc_ = pc_factory_->CreatePeerConnection(config, nullptr, nullptr, privateListener.get());
	//pc_ = pc_factory_->CreatePeerConnection(config, nullptr, nullptr, privateListener);
}

PeerConnectionClient::~PeerConnectionClient()
{
	deinit();
}

int PeerConnectionClient::init()
{
	if (!pc_factory_)
	{
		const std::string forced_field_trials =
			"WebRTC-FlexFEC-03-Advertised/Enabled/WebRTC-FlexFEC-03/Enabled/";
		webrtc::field_trial::InitFieldTrialsFromString(forced_field_trials.c_str());
		network_thread_ = rtc::Thread::CreateWithSocketServer().release();
		signal_thread_ = rtc::Thread::Create().release();
		worker_thread_ = rtc::Thread::Create().release();

		network_thread_->SetName("network_thread", nullptr);
		signal_thread_->SetName("signal_thread", nullptr);
		worker_thread_->SetName("work_thread", nullptr);

		if (!network_thread_->Start() || !signal_thread_->Start() || !worker_thread_->Start())
		{
			lberror("thread start error\n");
			return -1;
		}
		adm_thread_ = worker_thread_->Invoke<rtc::scoped_refptr<webrtc::AudioDeviceModule>>(RTC_FROM_HERE, [this]() {
			task_queue_factory_ = webrtc::CreateDefaultTaskQueueFactory();
			return webrtc::AudioDeviceModule::Create(webrtc::AudioDeviceModule::kPlatformDefaultAudio, task_queue_factory_.get());
			});

		video_decoder_factory_ = webrtc::CreateBuiltinVideoDecoderFactory();

		pc_factory_ = webrtc::CreatePeerConnectionFactory(network_thread_,
			worker_thread_,
			signal_thread_,
			adm_thread_,
			webrtc::CreateBuiltinAudioEncoderFactory(),
			webrtc::CreateBuiltinAudioDecoderFactory(),
			webrtc::CreateBuiltinVideoEncoderFactory(),
			webrtc::CreateBuiltinVideoDecoderFactory(),
			nullptr /*audio_mixer*/,
			nullptr /*audio_processing*/);
	}
	return 0;
}

void PeerConnectionClient::close()
{
	if (pc_)
	{
		pc_->Close();
	}
}

void PeerConnectionClient::deinit()
{
	close();

	if (pc_factory_) {
		pc_factory_ = nullptr;
	}

	worker_thread_->Invoke<void>(RTC_FROM_HERE, [this]() {
		adm_thread_ = nullptr;
		});
}

webrtc::PeerConnectionInterface::RTCConfiguration PeerConnectionClient::GetConfiguration()
{
	return pc_->GetConfiguration();
}

bool PeerConnectionClient::SetConfiguration(const webrtc::PeerConnectionInterface::RTCConfiguration& config)
{
	//MSC_TRACE();

	webrtc::RTCError error = pc_->SetConfiguration(config);

	if (error.ok())
	{
		return true;
	}

	lberror(
		"webrtc::PeerConnection::SetConfiguration failed [%s:%s]",
		webrtc::ToString(error.type()),
		error.message());

	return false;
}

std::string PeerConnectionClient::CreateOffer(
	const webrtc::PeerConnectionInterface::RTCOfferAnswerOptions& options)
{
	//MSC_TRACE();

	CreateSessionDescriptionObserver* sessionDescriptionObserver =
		new rtc::RefCountedObject<CreateSessionDescriptionObserver>();

	auto future = sessionDescriptionObserver->GetFuture();

	pc_->CreateOffer(sessionDescriptionObserver, options);

	return future.get();
}

std::string PeerConnectionClient::CreateAnswer(
	const webrtc::PeerConnectionInterface::RTCOfferAnswerOptions& options)
{
	MSC_TRACE();

	CreateSessionDescriptionObserver* sessionDescriptionObserver =
		new rtc::RefCountedObject<CreateSessionDescriptionObserver>();

	auto future = sessionDescriptionObserver->GetFuture();

	pc_->CreateAnswer(sessionDescriptionObserver, options);

	return future.get();
}

void PeerConnectionClient::SetLocalDescription(SdpType type, const std::string& sdp)
{
	MSC_TRACE();

	webrtc::SdpParseError error;
	webrtc::SessionDescriptionInterface* sessionDescription;
	rtc::scoped_refptr<SetSessionDescriptionObserver> observer(
		new rtc::RefCountedObject<SetSessionDescriptionObserver>());

	const auto& typeStr = sdpType2String[type];
	auto future = observer->GetFuture();

	sessionDescription = webrtc::CreateSessionDescription(typeStr, sdp, &error);
	if (sessionDescription == nullptr)
	{
		lberror(
			"webrtc::CreateSessionDescription failed [%s]: %s",
			error.line.c_str(),
			error.description.c_str());

		observer->Reject(error.description);

		return future.get();
	}

	pc_->SetLocalDescription(observer, sessionDescription);

	return future.get();
}

void PeerConnectionClient::SetRemoteDescription(SdpType type, const std::string& sdp)
{
	MSC_TRACE();
	webrtc::BitrateSettings bitset;
	bitset.min_bitrate_bps = 0;
	bitset.max_bitrate_bps = 2 * 1024 * 1024;
	bitset.start_bitrate_bps = 500000;
	pc_->SetBitrate(bitset);
	webrtc::SdpParseError error;
	webrtc::SessionDescriptionInterface* sessionDescription;
	rtc::scoped_refptr<SetSessionDescriptionObserver> observer(
		new rtc::RefCountedObject<SetSessionDescriptionObserver>());

	const auto& typeStr = sdpType2String[type];
	auto future = observer->GetFuture();

	sessionDescription = webrtc::CreateSessionDescription(typeStr, sdp, &error);
	if (sessionDescription == nullptr)
	{
		lberror(
			"webrtc::CreateSessionDescription failed [%s]: %s",
			error.line.c_str(),
			error.description.c_str());

		observer->Reject(error.description);

		return future.get();
	}

	pc_->SetRemoteDescription(observer, sessionDescription);

	return future.get();
}

const std::string PeerConnectionClient::GetLocalDescription()
{
	MSC_TRACE();

	auto desc = pc_->local_description();
	std::string sdp;

	desc->ToString(&sdp);

	return sdp;
}

const std::string PeerConnectionClient::GetRemoteDescription()
{
	MSC_TRACE();

	auto desc = pc_->remote_description();
	std::string sdp;

	desc->ToString(&sdp);

	return sdp;
}

std::vector<rtc::scoped_refptr<webrtc::RtpTransceiverInterface>> PeerConnectionClient::GetTransceivers() const
{
	MSC_TRACE();

	return pc_->GetTransceivers();
}

rtc::scoped_refptr<webrtc::RtpTransceiverInterface> PeerConnectionClient::AddTransceiver(
	cricket::MediaType mediaType)
{
	MSC_TRACE();

	auto result = pc_->AddTransceiver(mediaType);

	if (!result.ok())
	{
		rtc::scoped_refptr<webrtc::RtpTransceiverInterface> transceiver = nullptr;

		return transceiver;
	}

	return result.value();
}

rtc::scoped_refptr<webrtc::RtpTransceiverInterface> PeerConnectionClient::AddTransceiver(
	rtc::scoped_refptr<webrtc::MediaStreamTrackInterface> track,
	webrtc::RtpTransceiverInit rtpTransceiverInit)
{
	//MSC_TRACE();

	/*
	 * Define a stream id so the generated local description is correct.
	 * - with a stream id:    "a=ssrc:<ssrc-id> mslabel:<value>"
	 * - without a stream id: "a=ssrc:<ssrc-id> mslabel:"
	 *
	 * The second is incorrect (https://tools.ietf.org/html/rfc5576#section-4.1)
	 */
	rtpTransceiverInit.stream_ids.emplace_back("0");

	auto result = pc_->AddTransceiver(
		track, rtpTransceiverInit); // NOLINT(performance-unnecessary-value-param)

	if (!result.ok())
	{
		rtc::scoped_refptr<webrtc::RtpTransceiverInterface> transceiver = nullptr;

		return transceiver;
	}

	return result.value();
}

std::vector<rtc::scoped_refptr<webrtc::RtpSenderInterface>> PeerConnectionClient::GetSenders()
{
	MSC_TRACE();

	return pc_->GetSenders();
}

bool PeerConnectionClient::RemoveTrack(webrtc::RtpSenderInterface* sender)
{
	MSC_TRACE();

	return pc_->RemoveTrack(sender);
}

nlohmann::json PeerConnectionClient::GetStats()
{
	MSC_TRACE();

	rtc::scoped_refptr<RTCStatsCollectorCallback> callback(
		new rtc::RefCountedObject<RTCStatsCollectorCallback>());

	auto future = callback->GetFuture();

	pc_->GetStats(callback.get());

	return future.get();
}

nlohmann::json PeerConnectionClient::GetStats(rtc::scoped_refptr<webrtc::RtpSenderInterface> selector)
{
	MSC_TRACE();

	rtc::scoped_refptr<RTCStatsCollectorCallback> callback(
		new rtc::RefCountedObject<RTCStatsCollectorCallback>());

	auto future = callback->GetFuture();

	pc_->GetStats(std::move(selector), callback);

	return future.get();
}

nlohmann::json PeerConnectionClient::GetStats(rtc::scoped_refptr<webrtc::RtpReceiverInterface> selector)
{
	MSC_TRACE();

	rtc::scoped_refptr<RTCStatsCollectorCallback> callback(
		new rtc::RefCountedObject<RTCStatsCollectorCallback>());

	auto future = callback->GetFuture();

	pc_->GetStats(std::move(selector), callback);

	return future.get();
}

rtc::scoped_refptr<webrtc::DataChannelInterface> PeerConnectionClient::CreateDataChannel(
	const std::string& label, const webrtc::DataChannelInit* config)
{
	MSC_TRACE();

	rtc::scoped_refptr<webrtc::DataChannelInterface> webrtcDataChannel =
		pc_->CreateDataChannel(label, config);

	if (webrtcDataChannel.get())
	{
		lbtrace("Success creating data channel");
	}
	else
	{
		LAZY_THROW_EXCEPTION("Failed creating data channel");
	}

	return webrtcDataChannel;
}

rtc::scoped_refptr<webrtc::VideoTrackInterface> PeerConnectionClient::CreateVideoTrack(const std::string& videoUrl,
	const std::map<std::string, std::string>& opts,
	const std::regex& publishFilter)
{

}

std::unique_ptr<PeerConnectionClient> PeerConnectionClient::Create()
{
	std::shared_ptr<PrivateListener> privateListener(new PrivateListener());
	std::unique_ptr<PeerConnectionClient> pc(new PeerConnectionClient(privateListener, NULL));
	return pc;
}

nlohmann::json PeerConnectionClient::GetNativeRtpCapabilities()
{
	std::unique_ptr<PeerConnectionClient> pcc = PeerConnectionClient::Create();
	pcc->AddTransceiver(cricket::MediaType::MEDIA_TYPE_AUDIO);
	pcc->AddTransceiver(cricket::MediaType::MEDIA_TYPE_VIDEO);
	webrtc::PeerConnectionInterface::RTCOfferAnswerOptions options;

	// May throw.
	auto offer = pcc->CreateOffer(options);
	RTC_LOG(LS_INFO) << offer;
	auto sdpObject = sdptransform::parse(offer);
	auto nativeRtpCapabilities = sdptransform::extractRtpCapabilities(sdpObject);
	/*auto rtp_cap_str = nativeRtpCapabilities.dump();
	validateRtpCapabilities(nativeRtpCapabilities);
	auto native_rtp_capabilities = nlohmann::json::parse(rtp_cap_str);
	validateRtpCapabilities(native_rtp_capabilities);*/
	//rtpcap_str = nativeRtpCapabilities.dump();
	return nativeRtpCapabilities;// nativeRtpCapabilities.dump();
}

json PeerConnectionClient::GetNativeSctpCapabilities()
{
	json caps = { { "numStreams", SctpNumStreams } };
	return caps;
}

rtc::scoped_refptr<webrtc::VideoTrackInterface> PeerConnectionClient::CreateVideoTrack()
{
	//rtc::scoped_refptr<CustomCaptureTrackSource> video_device = CustomCaptureTrackSource::create(NULL, 1920, 1080, 30, 0);
	rtc::scoped_refptr<webrtc::VideoTrackInterface> video_track(
	pc_factory_->CreateVideoTrack(kVideoLabel, CustomCaptureTrackSource::create(NULL, 1280, 720, 30, 0)));
	return video_track;
}

rtc::scoped_refptr<webrtc::AudioTrackInterface> PeerConnectionClient::CreateAudioTrack()
{
	rtc::scoped_refptr<webrtc::AudioTrackInterface> audio_track = pc_factory_->CreateAudioTrack(kAudioLabel, pc_factory_->CreateAudioSource(cricket::AudioOptions()));
	return audio_track;
}