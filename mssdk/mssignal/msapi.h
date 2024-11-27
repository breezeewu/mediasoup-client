#pragma once
#include "json/json.hpp"
#include "mssignal/mssignal.h"
#include "mssignal/microdef.h"
#include "sdputil/RemoteSdp.hpp"
#include "IRoomObserver.h"
#include <future>
#include <map>

class MSOptions
{
public:
	bool				use_simulcast = true;
	bool				use_sharing_simulcast = true;
	bool				force_tcp = false;
	bool				produce = true;
	bool				consume = true;
	bool				force_h264 = true;
	bool				force_vp9  = false;
	std::string			svc;
	bool				datachannel = true;
	std::string			throttle_secret;
	std::string			e2e_key;
};

class JsonTransportParam
{
public:
	std::string			id;
	nlohmann::json		ice_params;
	nlohmann::json		ice_candidates;
	nlohmann::json		dtls_params;
	nlohmann::json		sctp_params;
	nlohmann::json		extend_rtp_capabilities;
	bool				bsend_transport;

	std::shared_ptr<mediasoupclient::Sdp::RemoteSdp> sdp;
	//nlohmann::json		
};

class MediaSoupAPI :public IMediaSoupAPI, public std::enable_shared_from_this<MediaSoupAPI>
{
public:
	MediaSoupAPI();

	~MediaSoupAPI();

	int init(const string url, MSOptions* popt, vi::peer_info* pusr_info = NULL);

	int get_router_rtp_capabilities();

	int on_router_rtp_capabilities_response(const string& resp);

	int create_send_transport();

	int create_recv_transport();

	int create_webrtc_transport(bool bproduce, bool bconsuming);

	int on_create_webrtc_transport_response(const string& resp);

	int on_create_send_transport_response(const string& resp);
	int on_create_recv_transport_response(const string& resp);

	int join();

	int on_join_response(const string& resp);

	int on_new_consumer_request(const string& req);

	int new_consumer_response();

	int on_new_data_consumer_request(const string& req);

	int new_data_consumer_response();

	int connect_webrtc_transport(const string& id, const nlohmann::json& dtlsParameters);

	int on_connect_webrtc_transport_response(const string& resp);

	int produce(const string& id, const string& kind, const nlohmann::json& rtp_param);

	int on_produce_response(const string& resp);

	shared_ptr<JsonTransportParam> get_send_transport_param();

	shared_ptr<JsonTransportParam> get_recv_transport_param();
	shared_ptr<JsonTransportParam> get_transport_param_by_id(const string& id);
	// IMSAPI
	int on_recv_message(const string& resp);

	void register_method_handle(const char* even_name, std::function<void(std::string msg)> func);

	int send_base_response(const int64_t id, bool bis_ok = true);

	const map<string, vi::peer_info>& get_peer_info_list() { return peer_list_; }

protected:
	virtual int on_recv_request(const shared_ptr<MSMessage>& msg);

	virtual int on_recv_response(const shared_ptr<MSMessage>& msg);

	virtual int on_recv_notify(const shared_ptr<MSMessage>& msg);

	int load_capabilities(nlohmann::json& router_rtp_capabilities);

	int parser_transport_param(std::shared_ptr<signaling::CreateWebRtcTransportResponse> transportInfo, JsonTransportParam& json_param);

	nlohmann::json		native_rtp_capabilities_;
	nlohmann::json		extend_rtp_capabilities_;
	nlohmann::json		recv_rtp_capabilities_;
	nlohmann::json		sctp_capabilities_;

	std::map<std::string, bool> can_produce_;
	bool					loaded_;
	unique_ptr<MSOptions>	ms_options_;

protected:
	shared_ptr<MSSignal>	ms_sinal_;
	string					url_;
	unique_ptr<vi::peer_info>	user_info_;
	map<string, vi::peer_info>		peer_list_;
	//string					send_tansport_id;
	//string					recv_
	//shared_ptr <JsonTransportParam>		send_transport_param_;
	//shared_ptr<JsonTransportParam>		recv_transport_param_;
	map<string, shared_ptr<JsonTransportParam>>			transport_list_;
	unique_ptr<promise<string>>		promise_;
	map<string, std::function<void(std::string msg)>>	method_handle_list_;
};

