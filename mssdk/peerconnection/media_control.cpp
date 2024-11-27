#include "media_control.h"
#include "sdptransform/sdptransform.hpp"
#include "sdputil/RemoteSdp.hpp"
#include "sdputil/jsonvalidate.h"
#include "sdputil/sdputil.h"
#include "lazyutil/lazyexception.hpp"



static void fillJsonRtpEncodingParameters(json& jsonEncoding, const webrtc::RtpEncodingParameters& encoding)
{
	//MSC_TRACE();

	jsonEncoding["active"] = encoding.active;

	if (!encoding.rid.empty())
		jsonEncoding["rid"] = encoding.rid;

	if (encoding.max_bitrate_bps)
		jsonEncoding["maxBitrate"] = *encoding.max_bitrate_bps;

	if (encoding.max_framerate)
		jsonEncoding["maxFramerate"] = *encoding.max_framerate;

	if (encoding.scale_resolution_down_by)
		jsonEncoding["scaleResolutionDownBy"] = *encoding.scale_resolution_down_by;

	if (encoding.scalability_mode.has_value())
		jsonEncoding["scalabilityMode"] = *encoding.scalability_mode;

	jsonEncoding["networkPriority"] = encoding.network_priority;
}

MediaControl::MediaControl()
{
	task_queue_ = rtc::Thread::Create();
	task_queue_->Start();
	configVideoEncodings();
	//room_observer_ = room_observer;
}

MediaControl::~MediaControl()
{
	task_queue_.reset();
}

void MediaControl::setObserver(std::shared_ptr<vi::IRoomObserver> room_observer)
{
	room_observer_ = room_observer;
}

int MediaControl::join(const std::string& url, vi::peer_info* peer_info)
{
	if (!ms_api_)
	{
		ms_api_.reset(new MediaSoupAPI());
		ms_api_->register_method_handle(MS_NEW_CONSUMER, std::bind(&MediaControl::on_new_consumer, this, std::placeholders::_1));
		ms_api_->register_method_handle(MS_NEW_DATA_CONSUMER, std::bind(&MediaControl::on_new_data_consumer, this, std::placeholders::_1));
	}
	if (peer_info)
	{
		user_info_.reset(new vi::peer_info());
		user_info_->room_id = peer_info->room_id;
		user_info_->peer_id = peer_info->peer_id;
		user_info_->device_flag = peer_info->device_flag;
		user_info_->device_name = peer_info->device_name;
		user_info_->display_name = peer_info->display_name;
		user_info_->version = peer_info->version;
	}

	/*if (!pcc_)
	{
		pcc_ = PeerConnectionClient::Create();
	}*/
	int ret = ms_api_->init(url, NULL, user_info_.get());
	lbcheck_result(ret, " ms_api_->init(url:%s, NULL) failed", url.c_str());
	Sleep(1000);
	ret = ms_api_->get_router_rtp_capabilities();
	lbcheck_result(ret, " ms_api_->get_router_rtp_capabilities() failed");
	ret = ms_api_->create_send_transport();
	lbcheck_result(ret, " ms_api_->create_send_transport() failed");
	ret = ms_api_->create_recv_transport();
	lbcheck_result(ret, " ms_api_->create_recv_transport() failed");
	ret = ms_api_->join();
	lbcheck_result(ret, " ms_api_->join() failed");
	
	return ret;
}

void MediaControl::leave()
{

}

int MediaControl::publish(vi::PublishStreamType stream_type, bool benable_audio)
{
	auto pc = PeerConnectionClient::Create();
	enable_audio(pc, benable_audio);
	enable_video(pc, stream_type, true);
	lock_guard<recursive_mutex> lock(mutex_);
	publish_pcs_[stream_type] = std::move(pc);
	return 0;
}
int MediaControl::unpublish(vi::PublishStreamType stream_type)
{
	lock_guard<recursive_mutex> lock(mutex_);
	auto it = publish_pcs_.find(stream_type);
	if (it == publish_pcs_.end())
	{
		RTC_LOG(LS_ERROR) << "unpublish failed";
		return -1;
	}
	it->second->close();
	publish_pcs_.erase(it);
	return 0;
}

int MediaControl::subscribe(std::string uid)
{
	lbcheck_ptr(ms_api_, -1, "%s failed, ms_api_ not init", __FUNCTION__);
	if (uid.empty())
	{
		auto pi_list = ms_api_->get_peer_info_list();
		if (pi_list.size() <= 0)
		{
			RTC_LOG(LS_INFO) << "no peer info for subscribe";
			return -1;
		}
		auto it = pi_list.begin();
		uid = it->first;
	}
	auto cit = consumer_list_.find(uid);
	if (cit == consumer_list_.end() || consumer_list_[uid].size() <= 0)
	{
		RTC_LOG(LS_INFO) << "subscribe uid:" << uid << " failed";
		return -1;
	}
	shared_ptr<JsonTransportParam> recv_tp = ms_api_->get_recv_transport_param();
	lbcheck_ptr(recv_tp, -1, "get_recv_transport_param failed");
	shared_ptr<mediasoupclient::Sdp::RemoteSdp> rsdp(new mediasoupclient::Sdp::RemoteSdp(recv_tp->ice_params, recv_tp->ice_candidates, recv_tp->dtls_params, recv_tp->sctp_params));
	auto pc = PeerConnectionClient::Create();
	pc->init();
	/*webrtc::PeerConnectionInterface::RTCOfferAnswerOptions options;
	auto answer = pc->CreateAnswer(options);
	auto localSdpObject = sdptransform::parse(answer);
	auto dtlsParameters = extractDtlsParameters(localSdpObject);*/
	
	map<string, shared_ptr<signaling::NewConsumerRequest>>& ncr_map = cit->second;
	rsdp->UpdateDtlsRole("server");
	int mid_def_val = 0;
	bool bconnect = false;
	for (auto it : ncr_map)
	{
		string mid = it.second->data->rtpParameters->mid.value_or(std::to_string(mid_def_val));
		string kind = it.second->data->kind.value_or("");
		auto rtp_param = it.second->data->rtpParameters->toJson();
		/*auto mit = std::find_if(localSdpObject["media"].begin(), localSdpObject["media"].end(), [&mid](const json& m) {return m["mid"].get<std::string>() == mid; });
		auto& answer_media_obj = *mit;
		applyCodecParameters(rtp_param, answer_media_obj);*/
		rsdp->Receive(mid, kind, rtp_param, rtp_param["rtcp"]["cname"], recv_tp->id);
		auto offer = rsdp->GetSdp();
		pc->SetRemoteDescription(SdpType::OFFER, offer);
		webrtc::PeerConnectionInterface::RTCOfferAnswerOptions options;
		auto answer = pc->CreateAnswer(options);
		auto localSdpObject = sdptransform::parse(answer);
		auto dtlsParameters = extractDtlsParameters(localSdpObject);
		auto mit = std::find_if(localSdpObject["media"].begin(), localSdpObject["media"].end(), [&mid](const json& m) {return m["mid"].get<std::string>() == mid; });
		auto& answer_media_obj = *mit;
		applyCodecParameters(rtp_param, answer_media_obj);
		answer = sdptransform::write(localSdpObject);
		if (!bconnect)
		{
			int ret = ms_api_->connect_webrtc_transport(recv_tp->id, dtlsParameters);
			lbcheck_result(ret, "ms_api_->connect_webrtc_transport(id:%s, dtlsParameters)", recv_tp->id.c_str());
			bconnect = true;
		}
		pc->SetLocalDescription(SdpType::ANSWER, answer);
	}
	
	auto transcievers = pc->GetTransceivers();
	auto transceiverIt = std::find_if(
		transcievers.begin(), transcievers.end(), [](webrtc::RtpTransceiverInterface* t) {
			return t->media_type() == cricket::MediaType::MEDIA_TYPE_VIDEO;
		});

	if (transceiverIt == transcievers.end())
		LAZY_THROW_EXCEPTION("video transceiver not found");
	auto transceiver = *transceiverIt;
	if (room_observer_)
	{
		room_observer_->onCreateRemoteVideoTrack(transceiver->mid().value(), transceiver->receiver()->track());
	}
	lock_guard<recursive_mutex> lock(mutex_);
	sub_pcs_[uid] = std::move(pc);
	return 0;
}

int MediaControl::unsubscribe(std::string uid)
{
	lock_guard<recursive_mutex> lock(mutex_);
	auto it = sub_pcs_.find(uid);
	if (it != sub_pcs_.end())
	{
		it->second->close();
		sub_pcs_.erase(it);
	}
	return 0;
}

int MediaControl::init()
{
	

	return 0;
}

void MediaControl::deinit()
{
	
}

int MediaControl::enable_video(std::unique_ptr<PeerConnectionClient>& pc, vi::PublishStreamType stream_type, bool enable)
{
	lbcheck_ptr(ms_api_, -1, "push failed, ms_api_ not init");
	if (enable)
	{
		bool hackVp9Svc = false;
		auto vtrack = pc->CreateVideoTrack();
		lbcheck_ptr(vtrack, -1, "pc->CreateVideoTrack() failed");
		vtrack->set_enabled(true);
		if (encodings_.size() > 1)
		{
			uint8_t idx = 0;
			for (webrtc::RtpEncodingParameters& encoding : encodings_)
			{
				encoding.rid = std::string("r").append(std::to_string(idx++));
			}
		}
		webrtc::RtpTransceiverInit transceiverInit;
		transceiverInit.send_encodings = encodings_;
		transceiverInit.direction = webrtc::RtpTransceiverDirection::kSendOnly;
		rtc::scoped_refptr<webrtc::RtpTransceiverInterface> transceiver = pc->AddTransceiver(vtrack, transceiverInit);
		RTC_LOG(LS_INFO) << "pc->AddTransceiver(track, transceiverInit)";
		lbcheck_ptr(transceiver, -1, "pc->AddTransceiver(vtrack:%p, transceiverInit) failed", vtrack);
		shared_ptr<JsonTransportParam> tp = ms_api_->get_send_transport_param();
		lbcheck_ptr(tp, -1, "ms_api_->get_send_transport_param() failed");
		webrtc::PeerConnectionInterface::RTCOfferAnswerOptions options;
		string offer = pc->CreateOffer(options);
		//RTC_LOG(LS_INFO) << "video offer:" << offer;
		auto localsdpobj = sdptransform::parse(offer);
		nlohmann::json codecOptions = nlohmann::json::object();
		codecOptions["videoGoogleStartBitrate"] = 1000;
		if (!remote_sdp_)
		{
			remote_sdp_.reset(new mediasoupclient::Sdp::RemoteSdp(tp->ice_params, tp->ice_candidates, tp->dtls_params, tp->sctp_params));
		}
		auto msidx = remote_sdp_->GetNextMediaSectionIdx();
		//json& offerMediaObject = localsdpobj["media"][msidx.idx];
		string localDtlsRole = "server";
		if (tp->dtls_params.find("role") != tp->dtls_params.end() && tp->dtls_params["role"].get<std::string>() != "auto")
		{
			localDtlsRole =
				tp->dtls_params["role"].get<std::string>() == "server" ? "client" : "server";
		}
		auto dtlsParameters = extractDtlsParameters(localsdpobj);
		dtlsParameters["role"] = localDtlsRole;
		std::string remoteDtlsRole = localDtlsRole == "client" ? "server" : "client";
		printf("tp->id:%s\n", tp->id.c_str());
		//ms_api_->produce(tp->id, vtrack->kind());
		json send_rtp_param = getSendingRtpParameters(vtrack->kind(), tp->extend_rtp_capabilities);
		/*string localDtlsRole = "server";
		if (tp->dtls_params.find("role") != tp->dtls_params.end() && tp->dtls_params["role"].get<std::string>() != "auto")
		{
			localDtlsRole =
				tp->dtls_params["role"].get<std::string>() == "server" ? "client" : "server";
		}
		string remoteDtlsRole = getRemoteDtlsRoleFromSDP(localDtlsRole, localsdpobj);*/
		remote_sdp_->UpdateDtlsRole(remoteDtlsRole);
		send_rtp_param["codecs"] = reduceCodecs(send_rtp_param["codecs"], NULL);
		auto send_remote_rtp_param = getSendingRemoteRtpParameters(vtrack->kind(), tp->extend_rtp_capabilities);
		send_remote_rtp_param["codecs"] = reduceCodecs(send_remote_rtp_param["codecs"], NULL);
		pc->SetLocalDescription(SdpType::OFFER, offer);
		string localId = transceiver->mid().value();
		send_rtp_param["mid"] = localId;
		auto localSdp = pc->GetLocalDescription();
		auto localSdpObject = sdptransform::parse(localSdp);
		std::string scalability_mode =
			encodings_.size()
			? (encodings_[0].scalability_mode.has_value() ? encodings_[0].scalability_mode.value()
				: "")
			: "";

		const json& layers = parseScalabilityMode(scalability_mode);

		auto spatialLayers = layers["spatialLayers"].get<int>();

		auto mimeType = send_rtp_param["codecs"][0]["mimeType"].get<std::string>();

		std::transform(mimeType.begin(), mimeType.end(), mimeType.begin(), ::tolower);

		if (encodings_.size() == 1 && spatialLayers > 1 && mimeType == "video/vp9")
		{
			RTC_LOG(LS_INFO) << "send() | enabling legacy simulcast for VP9 SVC";

			hackVp9Svc = true;
			localSdpObject = sdptransform::parse(offer);
			json& offerMediaObject = localSdpObject["media"][msidx.idx];

			addLegacySimulcast(offerMediaObject, spatialLayers);

			offer = sdptransform::write(localSdpObject);
		}

		json& offerMediaObject = localSdpObject["media"][msidx.idx];
		//pcc_->SetBitrate(bitset);
		/*rtc::scoped_refptr<webrtc::RtpSenderInterface> sender = transceiver->sender();
		if (sender)
		{
			auto param = sender->GetParameters();
			param.encodings[0].max_bitrate_bps = 1024 * 1024 * 2;
			sender->SetParameters(param);
		}*/
		// Set RTCP CNAME.
		send_rtp_param["rtcp"]["cname"] = getCname(offerMediaObject);
		//send_rtp_param["encodings"] = getRtpEncodings(offerMediaObject);
		if (encodings_.size() == 1)
		{
			auto newEncodings = getRtpEncodings(offerMediaObject);

			fillJsonRtpEncodingParameters(newEncodings.front(), encodings_.front());

			// Hack for VP9 SVC.
			if (hackVp9Svc)
				newEncodings = json::array({ newEncodings[0] });

			send_rtp_param["encodings"] = newEncodings;
		}
		// Otherwise if more than 1 encoding are given use them verbatim.
		else
		{
			send_rtp_param["encodings"] = json::array();

			for (const auto& encoding : encodings_)
			{
				json jsonEncoding = {};

				fillJsonRtpEncodingParameters(jsonEncoding, encoding);
				send_rtp_param["encodings"].push_back(jsonEncoding);
			}
		}
		if (room_observer_)
		{
			room_observer_->onCreateLocalVideoTrack(localId, vtrack);
		}
		remote_sdp_->Send(offerMediaObject, msidx.reuseMid, send_rtp_param, send_remote_rtp_param, &codecOptions);
		validateRtpParameters(send_rtp_param);
		ms_api_->produce(tp->id, vtrack->kind(), send_rtp_param);
		auto answer = remote_sdp_->GetSdp();
		//RTC_LOG(LS_INFO) << "video answer:" << answer;
		pc->SetRemoteDescription(SdpType::ANSWER, answer);
		

		return 0;
	}
	else
	{
		return -1;
	}
}

int MediaControl::enable_audio(std::unique_ptr<PeerConnectionClient>& pc, bool enable)
{
	lbcheck_ptr(ms_api_, -1, "push failed, ms_api_ not init");
	if (enable)
	{
		auto atrack = pc->CreateAudioTrack();
		lbcheck_ptr(atrack, -1, "pc->CreateVideoTrack() failed");
		atrack->set_enabled(true);
		webrtc::RtpTransceiverInit transceiverInit;
		transceiverInit.direction = webrtc::RtpTransceiverDirection::kSendOnly;
		rtc::scoped_refptr<webrtc::RtpTransceiverInterface> transceiver = pc->AddTransceiver(atrack, transceiverInit);
		RTC_LOG(LS_INFO) << "pc->AddTransceiver(atrack, transceiverInit)";
		lbcheck_ptr(transceiver, -1, "pc->AddTransceiver(atrack:%p, transceiverInit) failed", atrack);
		shared_ptr<JsonTransportParam> tp = ms_api_->get_send_transport_param();
		lbcheck_ptr(tp, -1, "ms_api_->get_send_transport_param() failed");
		webrtc::PeerConnectionInterface::RTCOfferAnswerOptions options;
		string offer = pc->CreateOffer(options);
		//RTC_LOG(LS_INFO) << "audio offer:" << offer;
		auto localsdpobj = sdptransform::parse(offer);
		auto dtlsParameters = extractDtlsParameters(localsdpobj);
		string dtlsstr = dtlsParameters.dump();
		
		nlohmann::json codecOptions = nlohmann::json::object();
		codecOptions["opusStereo"] = true;
		codecOptions["opusDtx"] = true;
		if (!remote_sdp_)
		{
			remote_sdp_.reset(new mediasoupclient::Sdp::RemoteSdp(tp->ice_params, tp->ice_candidates, tp->dtls_params, tp->sctp_params));
		}
		auto msidx = remote_sdp_->GetNextMediaSectionIdx();
		//json& offerMediaObject = localsdpobj["media"][msidx.idx];
		//json send_rtp_param = getSendingRtpParameters(track->kind(), tp->extend_rtp_capabilities);
		string localDtlsRole = "server";
		if (tp->dtls_params.find("role") != tp->dtls_params.end() && tp->dtls_params["role"].get<std::string>() != "auto")
		{
			localDtlsRole =
				tp->dtls_params["role"].get<std::string>() == "server" ? "client" : "server";
		}
		//auto dtlsParameters = extractDtlsParameters(localsdpobj);
		dtlsParameters["role"] = localDtlsRole;
		//shared_ptr<JsonTransportParam> recv_tp = ms_api_->get_recv_transport_param();
		int ret = ms_api_->connect_webrtc_transport(tp->id, dtlsParameters);
		//int ret = ms_api_->connect_webrtc_transport(recv_tp->id, dtlsParameters);
		lbcheck_result(ret, " ms_api_->connect_webrtc_transport() failed");
		
		std::string remoteDtlsRole = localDtlsRole == "client" ? "server" : "client";
		//string remoteDtlsRole = getRemoteDtlsRoleFromSDP(localDtlsRole, localsdpobj);
		remote_sdp_->UpdateDtlsRole(remoteDtlsRole);
		json send_rtp_param = getSendingRtpParameters(atrack->kind(), tp->extend_rtp_capabilities);
		send_rtp_param["codecs"] = reduceCodecs(send_rtp_param["codecs"], NULL);
		auto send_remote_rtp_param = getSendingRemoteRtpParameters(atrack->kind(), tp->extend_rtp_capabilities);
		send_remote_rtp_param["codecs"] = reduceCodecs(send_remote_rtp_param["codecs"], NULL);
		pc->SetLocalDescription(SdpType::OFFER, offer);
		string localId = transceiver->mid().value();
		send_rtp_param["mid"] = localId;
		auto localSdp = pc->GetLocalDescription();
		auto localSdpObject = sdptransform::parse(localSdp);
		json& offerMediaObject = localSdpObject["media"][msidx.idx];

		// Set RTCP CNAME.
		send_rtp_param["rtcp"]["cname"] = getCname(offerMediaObject);
		send_rtp_param["encodings"] = getRtpEncodings(offerMediaObject);
		remote_sdp_->Send(offerMediaObject, msidx.reuseMid, send_rtp_param, send_remote_rtp_param, &codecOptions);
		string rtp_param = send_rtp_param.dump();
		RTC_LOG(LS_INFO) << "audio rtp_param:" << rtp_param;
		validateRtpParameters(send_rtp_param);
		ms_api_->produce(tp->id, atrack->kind(), send_rtp_param);
		auto answer = remote_sdp_->GetSdp();
		//RTC_LOG(LS_INFO) << "audio answer:"  << answer;
		pc->SetRemoteDescription(SdpType::ANSWER, answer);
		
		return 0;
	}
	return -1;
}

void MediaControl::OnSignalingChange(webrtc::PeerConnectionInterface::SignalingState newState)
{

}

void MediaControl::OnAddStream(rtc::scoped_refptr<webrtc::MediaStreamInterface> stream)
{

}

void MediaControl::OnRemoveStream(rtc::scoped_refptr<webrtc::MediaStreamInterface> stream)
{

}

void MediaControl::OnDataChannel(rtc::scoped_refptr<webrtc::DataChannelInterface> dataChannel)
{

}

void MediaControl::OnRenegotiationNeeded()
{

}

void MediaControl::OnIceConnectionChange(webrtc::PeerConnectionInterface::IceConnectionState newState)
{

}

void MediaControl::OnIceGatheringChange(webrtc::PeerConnectionInterface::IceGatheringState newState)
{

}

void MediaControl::OnIceCandidate(const webrtc::IceCandidateInterface* candidate)
{

}

void MediaControl::OnIceCandidatesRemoved(const std::vector<cricket::Candidate>& candidates)
{

}

void MediaControl::OnIceConnectionReceivingChange(bool receiving)
{

}

void MediaControl::OnAddTrack(
	rtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver,
	const std::vector<rtc::scoped_refptr<webrtc::MediaStreamInterface>>& streams)
{

}
void MediaControl::OnTrack(rtc::scoped_refptr<webrtc::RtpTransceiverInterface> transceiver)
{

}
void MediaControl::OnRemoveTrack(rtc::scoped_refptr<webrtc::RtpReceiverInterface> receiver)
{

}

void MediaControl::OnInterestingUsage(int usagePattern)
{

}

void MediaControl::configVideoEncodings()
{
	encodings_.clear();

	webrtc::RtpEncodingParameters ph;
	ph.rid = "h";
	ph.active = true;
	ph.max_bitrate_bps = 1300 * 1000;
	ph.scale_resolution_down_by = 1;
	ph.scalability_mode = "L1T3";

	webrtc::RtpEncodingParameters pm;
	pm.rid = "m";
	pm.active = true;
	pm.max_bitrate_bps = 500 * 1000;
	pm.scale_resolution_down_by = 2;
	pm.scalability_mode = "L1T3";

	webrtc::RtpEncodingParameters pl;
	pl.rid = "l";
	pl.active = true;
	pl.max_bitrate_bps = 150 * 1000;
	pl.scale_resolution_down_by = 4;
	pl.scalability_mode = "L1T3";

	encodings_.emplace_back(ph);
	encodings_.emplace_back(pm);
	encodings_.emplace_back(pl);
}

void MediaControl::on_new_consumer(string req)
{
	lbcheck_ptr(ms_api_, void(), "on_new_consumer failed, ms_api_ not init");
	MSC_FUNC_TRACE("on_new_consumer(req:%s)", req.c_str());

	lock_guard<recursive_mutex> lock(mutex_);
	auto new_consumer_req = from_json_string<signaling::NewConsumerRequest>(req);
#ifndef ENABLE_AUTO_SUBSCRIBE
	auto peerid = new_consumer_req->data->peerId.value_or("");
	auto kind = new_consumer_req->data->kind.value_or("");
	consumer_list_[peerid][kind] = new_consumer_req;
#else
	auto recv_id = new_consumer_req->data->id.value_or("");
	auto rtp_param = new_consumer_req->data->rtpParameters->toJson();
	auto midit = rtp_param.find("mid");
	std::string local_id;
	if (midit != rtp_param.end())
	{
		local_id = midit->get<std::string>();
	}
	else
	{
		local_id = recv_stream_list_.size();
	}
	if (!pcc_)
	{
		pcc_.reset(new PeerConnectionClient(shared_from_this(), NULL));
		pcc_->init();
	}
	shared_ptr<JsonTransportParam> recv_tp = ms_api_->get_recv_transport_param();
	//int ret = ms_api_->connect_webrtc_transport(recv_tp->id, dtlsParameters);
	//shared_ptr<JsonTransportParam> recv_tp = ms_api_->get_transport_param_by_id(recv_id);
	lbcheck_ptr(recv_tp, void(), "on_new_consumer failed, recv_tp not init");
	recv_id = recv_tp->id;
	if (!recv_tp->sdp)
	{
		recv_tp->sdp.reset(new mediasoupclient::Sdp::RemoteSdp(recv_tp->ice_params, recv_tp->ice_candidates, recv_tp->dtls_params, recv_tp->sctp_params));
		
	}

	std::shared_ptr<stream_info> si(new stream_info());
	si->id = local_id;
	si->remote_sdp = recv_tp->sdp;
	//si->transciever
	auto kind = new_consumer_req->data->kind.value_or("");
	const auto& cname = rtp_param["rtcp"]["cname"];
	recv_tp->sdp->Receive(local_id, kind, rtp_param, cname, recv_id);
	auto offer = recv_tp->sdp->GetSdp();
	pcc_->SetRemoteDescription(SdpType::OFFER, offer);
	webrtc::PeerConnectionInterface::RTCOfferAnswerOptions options;
	auto answer = pcc_->CreateAnswer(options);
	auto localSdpObject = sdptransform::parse(answer);
	auto dtlsParameters = extractDtlsParameters(localSdpObject);
	string remoteDtlsRole = "server";
	recv_tp->sdp->UpdateDtlsRole(remoteDtlsRole);
	//dtlsParameters["role"] = localDtlsRole;
	/*auto dtlsParameters = extractDtlsParameters(localSdpObject);
	string localDtlsRole = "client";
	if (recv_tp->dtls_params.find("role") != recv_tp->dtls_params.end() && recv_tp->dtls_params["role"].get<std::string>() != "auto")
	{
		localDtlsRole =
			recv_tp->dtls_params["role"].get<std::string>() == "server" ? "client" : "server";
	}
	//auto dtlsParameters = extractDtlsParameters(localsdpobj);
	dtlsParameters["role"] = localDtlsRole;*/
#if 1
	task_queue_->PostTask(RTC_FROM_HERE, [recv_tp, dtlsParameters, ms_api = ms_api_]() {
		ms_api->connect_webrtc_transport(recv_tp->id, dtlsParameters);
		});
#else
	//ms_api_->connect_webrtc_transport(recv_tp->id, dtlsParameters);
#endif
	auto mediaIt = find_if(
		localSdpObject["media"].begin(), localSdpObject["media"].end(), [&local_id](const json& m) {
			return m["mid"].get<std::string>() == local_id;
		});

	auto& answerMediaObject = *mediaIt;

	// parameters in the offer. ����rtpParameters����answerMediaObject����answer��media����
	applyCodecParameters(new_consumer_req->data->rtpParameters->toJson(), answerMediaObject);

	answer = sdptransform::write(localSdpObject);
	pcc_->SetLocalDescription(SdpType::ANSWER, answer);
	auto transcievers = pcc_->GetTransceivers();
	auto transceiverIt = std::find_if(
		transcievers.begin(), transcievers.end(), [&local_id](webrtc::RtpTransceiverInterface* t) {
			return t->mid() == local_id;
		});

	if (transceiverIt == transcievers.end())
		LAZY_THROW_EXCEPTION("new RTCRtpTransceiver not found");
	
	si->transciever = *transceiverIt;
	if (kind == "video" && room_observer_)
	{
		room_observer_->onCreateRemoteVideoTrack(local_id, si->transciever->receiver()->track());
	}
	recv_stream_list_[local_id] = si;
#endif
	ms_api_->send_base_response(new_consumer_req->id.value_or(-1));
	return ;
}

void MediaControl::on_new_data_consumer(string req)
{
	MSC_FUNC_TRACE("on_new_data_consumer(req:%s)", req.c_str());
	auto new_data_consumer_req = from_json_string<signaling::NewDataConsumerRequest>(req);
	auto peerid = new_data_consumer_req->data->peerId.value_or("");
	data_consumer_list_[peerid] = new_data_consumer_req;
	webrtc::DataChannelInit dataChannelInit;
	dataChannelInit.protocol = new_data_consumer_req->data->protocol.value();
	dataChannelInit.id = new_data_consumer_req->data->sctpStreamParameters->streamId.value();
	dataChannelInit.negotiated = true;
	nlohmann::json sctpStreamParameters =
	{
		{ "streamId", dataChannelInit.id },
		{ "ordered",  dataChannelInit.ordered }
	};
	validateSctpStreamParameters(sctpStreamParameters);
	//rtc::scoped_refptr<webrtc::DataChannelInterface> webrtcDataChannel = pcc_->CreateDataChannel(new_data_consumer_req->data->label.value(), &dataChannelInit);
	ms_api_->send_base_response(new_data_consumer_req->id.value_or(-1));
	return;
	string local_id = new_data_consumer_req->data->id.value_or("");
	auto it = recv_stream_list_.find(local_id);
	if (it == recv_stream_list_.end())
	{
		RTC_LOG(LS_ERROR) << "find recv stream list failed, local_id:" << local_id;
		return;
	}
	std::shared_ptr<stream_info>& si = it->second;
	si->remote_sdp->RecvSctpAssociation();
	auto sdpoffer = si->remote_sdp->GetSdp();
	pcc_->SetRemoteDescription(SdpType::OFFER, sdpoffer);
	webrtc::PeerConnectionInterface::RTCOfferAnswerOptions options;
	auto sdpAnswer = pcc_->CreateAnswer(options);
	pcc_->SetLocalDescription(SdpType::ANSWER, sdpAnswer);
	return;
}

int MediaControl::subscribe_stream(shared_ptr<signaling::NewConsumerRequest> con_req)
{

}

std::shared_ptr<vi::IMediaControl> create_media_control(std::shared_ptr<vi::IRoomObserver> room_observer)
{
	std::shared_ptr<MediaControl>	medis_ctrl(new MediaControl());
	medis_ctrl->setObserver(room_observer);
	return medis_ctrl;
}