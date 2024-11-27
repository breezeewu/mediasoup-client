#include "api/peer_connection_interface.h"
#include <future>
#include <memory>
#include <regex>
#include <json/json.hpp>
enum class SdpType : uint8_t
{
	OFFER = 0,
	PRANSWER,
	ANSWER
};
class PrivateListener : public webrtc::PeerConnectionObserver
{
	/* Virtual methods inherited from PeerConnectionObserver. */
public:
	void OnSignalingChange(webrtc::PeerConnectionInterface::SignalingState newState) override {}
	void OnAddStream(rtc::scoped_refptr<webrtc::MediaStreamInterface> stream) override {}
	void OnRemoveStream(rtc::scoped_refptr<webrtc::MediaStreamInterface> stream) override {}
	void OnDataChannel(rtc::scoped_refptr<webrtc::DataChannelInterface> dataChannel) override {}
	void OnRenegotiationNeeded() override {}
	void OnIceConnectionChange(webrtc::PeerConnectionInterface::IceConnectionState newState) override {}
	void OnIceGatheringChange(webrtc::PeerConnectionInterface::IceGatheringState newState) override {}
	void OnIceCandidate(const webrtc::IceCandidateInterface* candidate) override {}
	void OnIceCandidatesRemoved(const std::vector<cricket::Candidate>& candidates) override {}
	void OnIceConnectionReceivingChange(bool receiving) override {}
	void OnAddTrack(
		rtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver,
		const std::vector<rtc::scoped_refptr<webrtc::MediaStreamInterface>>& streams) override {}
	void OnTrack(rtc::scoped_refptr<webrtc::RtpTransceiverInterface> transceiver) override {}
	void OnRemoveTrack(rtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver) override {}
	void OnInterestingUsage(int usagePattern) override {}
};

class PeerConnectionClient
{
public:
	PeerConnectionClient(std::shared_ptr<PrivateListener> privateListener, webrtc::PeerConnectionInterface::RTCConfiguration* config = NULL);
	~PeerConnectionClient();
	int init();

	void close();

	void deinit();

	webrtc::PeerConnectionInterface::RTCConfiguration GetConfiguration();

	bool SetConfiguration(const webrtc::PeerConnectionInterface::RTCConfiguration& config);

	std::string CreateOffer(const webrtc::PeerConnectionInterface::RTCOfferAnswerOptions& options);

	std::string CreateAnswer(const webrtc::PeerConnectionInterface::RTCOfferAnswerOptions& options);

	void SetLocalDescription(SdpType type, const std::string& sdp);

	void SetRemoteDescription(SdpType type, const std::string& sdp);

	const std::string GetLocalDescription();

	const std::string GetRemoteDescription();

	std::vector<rtc::scoped_refptr<webrtc::RtpTransceiverInterface>> GetTransceivers() const;

	rtc::scoped_refptr<webrtc::RtpTransceiverInterface> AddTransceiver(cricket::MediaType mediaType);

	rtc::scoped_refptr<webrtc::RtpTransceiverInterface> AddTransceiver(
		rtc::scoped_refptr<webrtc::MediaStreamTrackInterface> track,
		webrtc::RtpTransceiverInit rtpTransceiverInit);

	std::vector<rtc::scoped_refptr<webrtc::RtpSenderInterface>> GetSenders();

	bool RemoveTrack(webrtc::RtpSenderInterface* sender);

	nlohmann::json GetStats();

	nlohmann::json GetStats(rtc::scoped_refptr<webrtc::RtpSenderInterface> selector);

	nlohmann::json GetStats(rtc::scoped_refptr<webrtc::RtpReceiverInterface> selector);

	rtc::scoped_refptr<webrtc::DataChannelInterface> CreateDataChannel(
		const std::string& label, const webrtc::DataChannelInit* config);

	rtc::scoped_refptr<webrtc::VideoTrackInterface> CreateVideoTrack(const std::string& videoUrl,
		const std::map<std::string, std::string>& opts,
		const std::regex& publishFilter);

	rtc::Thread*  get_network_thread() { return network_thread_; }
	rtc::Thread*  get_worker_thread() { return worker_thread_; }
	rtc::Thread* get_signal_thread() { return signal_thread_; }
	rtc::scoped_refptr<webrtc::AudioDeviceModule> get_adm_thread() { return adm_thread_; }
	std::unique_ptr<webrtc::VideoDecoderFactory>& video_decoder_factory(){ return video_decoder_factory_; }

	static nlohmann::json GetNativeRtpCapabilities();
	static std::unique_ptr<PeerConnectionClient> Create();
	static nlohmann::json GetNativeSctpCapabilities();

	rtc::scoped_refptr<webrtc::VideoTrackInterface> CreateVideoTrack();

	rtc::scoped_refptr<webrtc::AudioTrackInterface> CreateAudioTrack();

protected:
	// rtc thead
	rtc::Thread*									network_thread_;
	rtc::Thread*									worker_thread_;
	rtc::Thread*									signal_thread_;
	rtc::scoped_refptr<webrtc::AudioDeviceModule>	adm_thread_;

	// factory
	std::unique_ptr<webrtc::TaskQueueFactory>		task_queue_factory_;
	std::unique_ptr<webrtc::VideoDecoderFactory>	video_decoder_factory_;
	rtc::scoped_refptr<webrtc::PeerConnectionFactoryInterface> pc_factory_;

	// listener
	std::shared_ptr<PrivateListener>				pri_listener_;

	rtc::scoped_refptr<webrtc::PeerConnectionInterface>		pc_;
};