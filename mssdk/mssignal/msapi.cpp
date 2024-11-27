#include <future>
#include <string>
#include "msapi.h"
#include "mssignal/signaling_models.h"
#include "sdputil/jsonvalidate.h"
#include "peerconnection/peer_connection_client.h"
#include "json/json.hpp"
MediaSoupAPI::MediaSoupAPI()
{
	loaded_ = false;
	can_produce_["audio"] = false;
	can_produce_["video"] = false;
}

MediaSoupAPI::~MediaSoupAPI()
{

}

int MediaSoupAPI::init(const string url, MSOptions* popt, vi::peer_info* pusr_info)
{
	MSC_FUNC_TRACE("init(url:%s, popt:%p, pusr_info:%p)", url.c_str(), popt, pusr_info);
	url_ = url;
	//shared_ptr<MediaSoupAPI> this_ =  shared_from_this();
	ms_sinal_.reset(new MSSignal(shared_from_this()));
	promise_.reset(new std::promise<string>());
	std::future<string> future = promise_->get_future();
	int ret = ms_sinal_->connect(url);
	user_info_.reset();
	if (pusr_info)
	{
		user_info_.reset(new vi::peer_info());
		*user_info_ = *pusr_info;
	}
	if (popt)
	{
		if (!ms_options_)
		{
			ms_options_.reset(new MSOptions());
		}
		*ms_options_ = *popt;
	}
	std::chrono::milliseconds time(3000);
	std::future_status status = future.wait_for(time);
	if (std::future_status::ready == status)
	{
		future.get();
		return 0;
	}
	else
	{
		RTC_LOG(LS_ERROR) << "websocket connect failed!";
		MessageBox(NULL, L"websocket connect failed", L"error", MB_OK);
		return -1;
	}
	return ret;
}

int MediaSoupAPI::get_router_rtp_capabilities()
{
	MSC_FUNC_TRACE("get_router_rtp_capabilities()");
	GetRouterRtpCapabilitiesRequest rtpcap;
	string msg = rtpcap.toJsonStr();
	string response = ms_sinal_->send_request_and_recv_response(msg);
	//lbtrace("get_router_rtp_capabilities response:%s\n", response.c_str());
	int ret = on_router_rtp_capabilities_response(response);
	lbtrace("get_router_rtp_capabilities end, ret:%d", ret);
	return ret;//ms_sinal_->send(msg, std::bind(&MediaSoupAPI::on_router_rtp_capabilities_response, this, placeholders::_1));
}

int MediaSoupAPI::on_router_rtp_capabilities_response(const string& resp)
{
	//MSC_FUNC_TRACE("on_router_rtp_capabilities_response(resp:%s)", resp.c_str());
	std::string err;
	auto response = fromJsonString<GetRouterRtpCapabilitiesResponse>(resp, err);
	if (!err.empty()) {
		lberror("parse response failed: {}", err);
		return -1;
	}
	lbtrace("on_router_rtp_capabilities_response:%s", resp.c_str()); 
	auto router_rtp_capabilities = nlohmann::json::parse(response->data->toJsonStr());
	load_capabilities(router_rtp_capabilities);
	
	/*if (loaded_)
	{
		if (ms_options_->produce)
		{
			create_webrtc_transport(ms_options_->produce, false);
		}
		if (ms_options_->consume)
		{
			create_webrtc_transport(false, ms_options_->consume);
		}
		
	}*/
	return 0;
}

int MediaSoupAPI::create_send_transport()
{
	MSC_FUNC_TRACE("create_send_transport()");
	return create_webrtc_transport(true, false);
}

int MediaSoupAPI::create_recv_transport()
{
	MSC_FUNC_TRACE("create_recv_transport()");
	return create_webrtc_transport(false, true);
}

int MediaSoupAPI::create_webrtc_transport(bool bproduce, bool bconsuming)
{
	MSC_FUNC_TRACE("create_webrtc_transport(bproduce:%d, bconsuming:%d)", bproduce, bconsuming);
	std::shared_ptr<CreateWebRtcTransportRequest> req(new CreateWebRtcTransportRequest());
	req->data = CreateWebRtcTransportRequest::Data();
	if (loaded_)//ms_options_ && ms_options_->datachannel &&
	{
		std::string json(sctp_capabilities_.dump().c_str());
		RTC_LOG(LS_ERROR) << "rtpCapabilities:" << json;
		if (json.empty()) {
			return -1;
		}
		std::string err;
		auto sctpCapabilities = fromJsonString<signaling::CreateWebRtcTransportRequest::SCTPCapabilities>(json, err);
		if (!err.empty()) {
			RTC_LOG(LS_ERROR) << "parse response failed:" << err;
			return -1;
		}
		req->data->sctpCapabilities = *sctpCapabilities;
	}

	req->data->consuming = bconsuming;
	req->data->producing = bproduce;
	req->data->forceTcp = false;
	std::string reqstr = req->toJsonStr();
	/*std::shared_ptr<std::promise<std::string>> promise(new std::promise<std::string>());
	//std::promise<void> promise;
	int ret = ms_sinal_->send(reqstr, promise);*/
	int ret = 0;
	if (bproduce)
	{
		std::string resp = ms_sinal_->send_request_and_recv_response(reqstr);//promise->get_future().get();
		ret = on_create_send_transport_response(resp);
		lbcheck_result(ret, "ret:%d = on_create_send_transport_response(resp)", ret);
	}
	if (bconsuming)
	{
		std::string resp = ms_sinal_->send_request_and_recv_response(reqstr);//promise->get_future().get();
		ret = on_create_recv_transport_response(resp);
		lbcheck_result(ret, "ret:%d = on_create_recv_transport_response(resp)", ret);
	}
	//req.
	return ret;
}

int MediaSoupAPI::on_create_webrtc_transport_response(const string& resp)
{
	std::string err;
	MSC_FUNC_TRACE("on_create_webrtc_transport_response(resp:%s)", resp.c_str());
	auto response = fromJsonString<CreateWebRtcTransportResponse>(resp, err);
	if (!err.empty()) {
		lberror("parse response failed: {}", err);
		return -1;
	}

	if (!response || !response->ok)
	{
		lberror("recv response:%p failed, response->ok:%d", response.get(), response ? response->ok : false);
		return -1;
	}
	lbtrace(resp.c_str());


	return 0;
}

int MediaSoupAPI::on_create_send_transport_response(const string& resp)
{
	std::string err;
	MSC_FUNC_TRACE("on_create_send_transport_response(resp)");
	auto response = fromJsonString<CreateWebRtcTransportResponse>(resp, err);
	if (!err.empty()) {
		lberror("parse response failed: {}", err);
		return -1;
	}

	if (!response || !response->ok)
	{
		lberror("recv response:%p failed, response->ok:%d", response.get(), response ? response->ok : false);
		return -1;
	}
	lbtrace(resp.c_str());
	shared_ptr<JsonTransportParam>	send_transport_param(new JsonTransportParam());
	//char buf[128];
	//sprintf(buf, "%ld", response->data->id.value_or(0));
	string id = response->data->id.value_or("");
	send_transport_param->id = id;//response->data->id.value();//buf;
	printf("send_transport_param->id:%s\n", send_transport_param->id.c_str());
	//send_transport_param.reset(new JsonTransportParam());
	send_transport_param->ice_params = nlohmann::json::parse(response->data->iceParameters->toJsonStr());
	send_transport_param->ice_candidates = nlohmann::json::array();
	for (auto& candidate : response->data->iceCandidates.value())
	{
		auto canstr = candidate.toJsonStr();
		RTC_LOG(LS_INFO) << "candidate:" << canstr;
		send_transport_param->ice_candidates.emplace_back(nlohmann::json::parse(canstr));
	}
	response->data->dtlsParameters->role = "auto";
	send_transport_param->dtls_params = nlohmann::json::parse(response->data->dtlsParameters->toJsonStr());
	send_transport_param->sctp_params = nlohmann::json::parse(response->data->sctpParameters->toJsonStr());
	send_transport_param->extend_rtp_capabilities = extend_rtp_capabilities_;
	send_transport_param->bsend_transport = true;
	transport_list_[send_transport_param->id] = send_transport_param;
	return 0;
}

int MediaSoupAPI::on_create_recv_transport_response(const string& resp)
{
	std::string err;
	MSC_FUNC_TRACE("on_create_recv_transport_response(resp)");
	auto response = fromJsonString<CreateWebRtcTransportResponse>(resp, err);
	if (!err.empty()) {
		lberror("parse response failed: {}", err);
		return -1;
	}

	if (!response || !response->ok)
	{
		lberror("recv response:%p failed, response->ok:%d", response.get(), response ? response->ok : false);
		return -1;
	}
	lbtrace(resp.c_str());
	shared_ptr<JsonTransportParam> recv_transport_param(new JsonTransportParam());
	recv_transport_param->id = response->data->id.value_or("");
	recv_transport_param->ice_params = nlohmann::json::parse(response->data->iceParameters->toJsonStr());
	recv_transport_param->ice_candidates = nlohmann::json::array();
	for (auto& candidate : response->data->iceCandidates.value())
	{
		auto canstr = candidate.toJsonStr();
		RTC_LOG(LS_INFO) << "candidate:" << canstr;
		recv_transport_param->ice_candidates.emplace_back(nlohmann::json::parse(canstr));
	}
	response->data->dtlsParameters->role = "auto";
	recv_transport_param->dtls_params = nlohmann::json::parse(response->data->dtlsParameters->toJsonStr());
	recv_transport_param->sctp_params = nlohmann::json::parse(response->data->sctpParameters->toJsonStr());
	recv_transport_param->bsend_transport = false;
	transport_list_[recv_transport_param->id] = recv_transport_param;
	return 0;
}

int MediaSoupAPI::join()
{
	MSC_FUNC_TRACE("join()");
	auto req = std::make_shared<signaling::JoinRequest>();
	req->data = signaling::JoinRequest::Data();
	if (user_info_)
	{
		req->data->displayName = user_info_->display_name;
		signaling::JoinRequest::Device device;
		device.flag = user_info_->device_flag;
		device.name = user_info_->device_name;
		device.version = user_info_->version;
		req->data->device = device;
	}
	validateRtpCapabilities(recv_rtp_capabilities_);
	std::string err;
	auto rtpcap = fromJsonString<signaling::JoinRequest::RTPCapabilities>(recv_rtp_capabilities_.dump(), err);
	req->data->rtpCapabilities = *rtpcap;
	auto sctpcap = fromJsonString<signaling::JoinRequest::SCTPCapabilities>(sctp_capabilities_.dump(), err);
	req->data->sctpCapabilities = *sctpcap;
	
	string response = ms_sinal_->send_request_and_recv_response(req->toJsonStr());
	return on_join_response(response);
}

int MediaSoupAPI::on_join_response(const string& resp)
{
	std::string err;
	MSC_FUNC_TRACE("on_join_response(resp)");
	auto join_resp = fromJsonString<signaling::JoinResponse>(resp, err);
	for (const auto& peer : join_resp->data->peers.value_or(std::vector<signaling::JoinResponse::Peer>()))
	{
		vi::peer_info ui;
		if (user_info_)
		{
			ui.room_id = user_info_->room_id;
		}
		ui.peer_id = peer.id.value_or("");
		ui.display_name = peer.displayName.value_or("");
		ui.device_flag = peer.device->flag.value_or("");
		ui.device_name = peer.device->name.value_or("");
		ui.version = peer.device->version.value_or("");
		//ui.trace();
		peer_list_[ui.peer_id] = ui;
	}
	return 0;
}

int MediaSoupAPI::on_new_consumer_request(const string& req)
{
	std::string err;
	MSC_FUNC_TRACE("on_new_consumer_request(resp)");
	auto ncq = fromJsonString<signaling::NewConsumerRequest>(req, err);
	MSC_CHECK_ERROR(err, "parser NewConsumerRequest failed");
	auto rtp_param = ncq->data->rtpParameters->toJson();
	auto app_data = ncq->data->appData->toJson();
	return 0;
}

int MediaSoupAPI::new_consumer_response()
{
	return 0;
}

int MediaSoupAPI::on_new_data_consumer_request(const string& req)
{
	MSC_FUNC_TRACE("on_new_consumer_request(resp)");
	return 0;
}

int MediaSoupAPI::new_data_consumer_response()
{
	return 0;
}

int MediaSoupAPI::connect_webrtc_transport(const string& id, const nlohmann::json& dtlsParameters)
{
	if (transport_list_.find(id) == transport_list_.end())
	{
		lberror("Invalid transport id:%s", id.c_str());
		return -1;
	}

	shared_ptr<JsonTransportParam>& ptp = transport_list_[id];
	auto request = std::make_shared<signaling::ConnectWebRtcTransportRequest>();
	request->data = signaling::ConnectWebRtcTransportRequest::Data();
	request->data->transportId = ptp->id;
	std::string err;
	auto ice = fromJsonString<signaling::ConnectWebRtcTransportRequest::ICEParameters>(ptp->ice_params.dump(), err);
	MSC_CHECK_ERROR(err, "parser ICEParameters failed");
	err.clear();
	request->data->iceParameters = *ice;
	//request->data->dtlsParameters = signaling::ConnectWebRtcTransportRequest::DTLSParameters();
	//auto dtlsp = fromJsonString<signaling::ConnectWebRtcTransportRequest::DTLSParameters>(ptp->dtls_params.dump(), err);
	//MSC_CHECK_ERROR(err, "parser DTLSParameters failed");dtlsParameters;
	string dtlsparam = dtlsParameters.dump();
	request->data->dtlsParameters = *fromJsonString<signaling::ConnectWebRtcTransportRequest::DTLSParameters>(dtlsParameters.dump(), err);
	/*if (ptp->bsend_transport)
	{
		request->data->dtlsParameters->role = "server";
	}
	else
	{
		request->data->dtlsParameters->role = "client";
	}*/
	//ms_sinal_->send(request->toJsonStr());
	std::string response = ms_sinal_->send_request_and_recv_response(request->toJsonStr());
	//MSC_CHECK_RESPONSE(response, "recv ConnectWebRtcTransportRequest failed");
	//MSC_CHECK_RESPONSE(response, "parser ConnectWebRtcTransportRequest failed");
	return 0;
}

int MediaSoupAPI::on_connect_webrtc_transport_response(const string& resp)
{
	//std::string err;
	//auto response = fromJsonString <signaling::BasicResponse>(resp, err);
	MSC_CHECK_RESPONSE(resp, "parser connect webrtc transport response failed");
	return 0;
}

int MediaSoupAPI::produce(const string& id, const string& kind, const nlohmann::json& rtp_param)
{
	MSC_FUNC_TRACE("produce(%s, %s)", id.c_str(), kind.c_str());
	auto request = std::make_shared<signaling::ProduceRequest>();
	request->data = signaling::ProduceRequest::Data();
	request->data->transportId = id;
	request->data->kind = kind;
	if (transport_list_.find(id) == transport_list_.end())
	{
		lberror("Invalid id:%s", id.c_str());
		return -1;
	}
	//auto rtpcap = getSendingRtpParameters(kind, extend_rtp_capabilities_);
	std::string err;
	request->data->rtpParameters = *fromJsonString<signaling::ProduceRequest::RTPParameters>(rtp_param.dump(), err);
	MSC_CHECK_ERROR(err, "RTPParameters failed");
	//transport_list_[id]->sctp_params
	auto response = ms_sinal_->send_request_and_recv_response(request->toJsonStr());
	return on_produce_response(response);
}

int MediaSoupAPI::on_produce_response(const string& resp)
{
	MSC_FUNC_TRACE("on_produce_response()");
	MSC_CHECK_RESPONSE(resp, "parser on_produce_response failed");
	return 0;
}

shared_ptr<JsonTransportParam> MediaSoupAPI::get_send_transport_param()
{
	for (auto it = transport_list_.begin(); it != transport_list_.end(); it++)
	{
		if (it->second->bsend_transport)
		{
			return it->second;
		}
	}

	return shared_ptr<JsonTransportParam>();
}

shared_ptr<JsonTransportParam> MediaSoupAPI::get_recv_transport_param()
{
	for (auto it = transport_list_.begin(); it != transport_list_.end(); it++)
	{
		if (!it->second->bsend_transport)
		{
			return it->second;
		}
	}

	return shared_ptr<JsonTransportParam>();
}

shared_ptr<JsonTransportParam> MediaSoupAPI::get_transport_param_by_id(const string& id)
{
	auto it = transport_list_.find(id);
	if (it == transport_list_.end())
	{
		RTC_LOG(LS_INFO) << "get_transport_param_by_id failed " << id;
		return shared_ptr<JsonTransportParam>();
	}
	return transport_list_[id];
}
// IMSAPI
int MediaSoupAPI::on_recv_message(const string& resp)
{
	return 0;
}

void MediaSoupAPI::register_method_handle(const char* even_name, std::function<void(std::string msg)> func)
{
	lbcheck_ptr(even_name, void(), "even_name == %p", even_name);
	lbcheck_ptr(func, void(), "func == %p", func);
	method_handle_list_[even_name] = func;
}

int MediaSoupAPI::send_base_response(const int64_t id, bool bis_ok)
{
	auto response = std::make_shared<signaling::BasicResponse>();
	response->response = true;
	response->id = id;
	response->ok = bis_ok; 
	auto msg = response->toJsonStr();
	RTC_LOG(LS_INFO) << "send_base_response:" << msg;
	return ms_sinal_->send(msg);
}

int MediaSoupAPI::on_recv_request(const shared_ptr<MSMessage>& msg)
{
	for (auto it = method_handle_list_.begin(); it != method_handle_list_.end(); it++)
	{
		if (it->first == msg->method)
		{
			it->second(msg->json);
			return 0;
		}
	}
	/*if (MS_NEW_CONSUMER == msg->method)
	{
		return on_new_consumer_request(msg->json);
	}
	else if (MS_NEW_DATA_CONSUMER == msg->method)
	{
		return on_new_data_consumer_request(msg->json);
	}
	else*/
	{
		lberror("Invalid msg:%s", msg->json.c_str());
		return -1;
	}
}

int MediaSoupAPI::on_recv_response(const shared_ptr<MSMessage>& msg)
{
	return 0;
	if (MS_GET_ROUTER_RTP_CAPABILITIES == msg->method)
	{
		return on_router_rtp_capabilities_response(msg->json);
	}
	else if (MS_CREATE_WEBRTC_TRANSPORT == msg->method)
	{
		return on_create_webrtc_transport_response(msg->json);
	}
	else if (MS_JOIN == msg->method)
	{
		return on_join_response(msg->json);
	}
	else if (MS_CONNECT_WEBRTC_TRANSPORT == msg->method)
	{
		return on_connect_webrtc_transport_response(msg->json);
	}
	else if (MS_PRODUCE == msg->method)
	{
		return on_produce_response(msg->json);
	}
	else
	{
		lberror("Invalid method:%s, msg:%s", msg->method.c_str(), msg->json.c_str());
		return -1;
	}
}

int MediaSoupAPI::on_recv_notify(const shared_ptr<MSMessage>& msg)
{
	if (msg)
	{
		if (promise_ && MS_MEDIASOUP_VERSION == msg->method)
		{
			promise_->set_value(msg->json);
		}
	}
	return 0;
}

int MediaSoupAPI::load_capabilities(nlohmann::json& router_rtp_capabilities)
{
	validateRtpCapabilities(router_rtp_capabilities);
	native_rtp_capabilities_ = PeerConnectionClient::GetNativeRtpCapabilities();
	//native_rtp_capabilities_ = nlohmann::json::parse(rtp_cap_str);
	//validateRtpCapabilities(native_rtp_capabilities_);
	extend_rtp_capabilities_ = getExtendedRtpCapabilities(native_rtp_capabilities_, router_rtp_capabilities);
	recv_rtp_capabilities_ = getRecvRtpCapabilities(extend_rtp_capabilities_);
	validateRtpCapabilities(recv_rtp_capabilities_);
	sctp_capabilities_ = PeerConnectionClient::GetNativeSctpCapabilities();
	validateSctpCapabilities(sctp_capabilities_);
	can_produce_["audio"] = canSend("audio", extend_rtp_capabilities_);
	can_produce_["video"] = canSend("video", extend_rtp_capabilities_);
	lbinfo("load rtp capabilities success");
	loaded_ = true;
	return 0;
}

int MediaSoupAPI::parser_transport_param(std::shared_ptr<signaling::CreateWebRtcTransportResponse> transportInfo, JsonTransportParam& json_param)
{
	if (!transportInfo)
	{
		lberror("Invalid transportInfo:%p ptr", transportInfo);
		return -1;
	}
	json_param.ice_params = nlohmann::json::parse(transportInfo->data->iceParameters->toJsonStr());
	json_param.ice_candidates = nlohmann::json::array();
	for (auto& candidate : transportInfo->data->iceCandidates.value()) {
		RTC_LOG(LS_INFO) << "candidate:" << candidate.toJsonStr();
		json_param.ice_candidates.emplace_back(nlohmann::json::parse(candidate.toJsonStr()));
	}
	transportInfo->data->dtlsParameters->role = "auto";
	json_param.dtls_params = nlohmann::json::parse(transportInfo->data->dtlsParameters->toJsonStr());
	json_param.sctp_params = nlohmann::json::parse(transportInfo->data->sctpParameters->toJsonStr());

	// Validate arguments.
	validateIceParameters(const_cast<nlohmann::json&>(json_param.ice_params));
	validateIceCandidates(const_cast<nlohmann::json&>(json_param.ice_candidates));
	validateDtlsParameters(const_cast<nlohmann::json&>(json_param.dtls_params));

	if (!json_param.sctp_params.is_null())
		validateSctpParameters(const_cast<nlohmann::json&>(json_param.sctp_params));

	/*if (json_param.dtls_params.find("role") != json_param.dtls_params.end() && json_param.dtls_params["role"].get<std::string>() != "auto")
	{
		this->forcedLocalDtlsRole =
			json_param.dtls_params["role"].get<std::string>() == "server" ? "client" : "server";
	}*/
	return 0;
}