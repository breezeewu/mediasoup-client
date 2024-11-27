#include "mssignal/mssignal.h"
#include "mssignal/signaling_models.h"
#include <iostream>
#include <functional>
#include "lazyutil/lazyexception.hpp"
#include "mssignal/msapi.h"
#include "peerconnection/media_control.h"
using namespace signaling;
int main()
{
	const string url = "wss://192.168.3.18:4443/?roomId=45672&peerId=gaaidqq3&consumerReplicas=undefined";
	vi::peer_info ui;
	ui.room_id = "45672";
	ui.peer_id = "swdfsdf";
	ui.display_name = "dawson";
	ui.device_flag = "mediasoup-client";
	ui.device_name = "mediasoup-client";
	ui.version = "1.0.0";
#if 0
	lbinit_log("./", LOG_LEVEL_INFO, LOG_OUTPUT_FILE_AND_CONSOLE, 0, 1, 0, 0);
	std::shared_ptr<MediaSoupAPI> msapi(new MediaSoupAPI());
	
	msapi->init("wss://192.168.3.18:4443/?roomId=45672&peerId=gaaidqq3&consumerReplicas=undefined", NULL, &ui);
	msapi->get_router_rtp_capabilities();
	msapi->create_send_transport();
	msapi->create_recv_transport();
	msapi->join();
#else
	std::shared_ptr<MediaControl> media_corl(new MediaControl());
	MessageBox(NULL, NULL, NULL, MB_OK);
	int ret = media_corl->join(url, &ui);
	lbcheck_result(ret, "media_corl->join(url:%s, &ui)", url.c_str());
	ret = media_corl->publish(true, true);
	lbcheck_result(ret, "media_corl->push(true, true)");
#endif
	while (1)
	{
		Sleep(100);
	};
	//LAZY_THROW_EXCEPTION("exec failed, ret:%d", -1);
	/*GetRouterRtpCapabilitiesRequest* prrc_req = new GetRouterRtpCapabilitiesRequest();
	std::string json = prrc_req->toJsonStr();
	std::string err;
	auto commhdr = fromJsonString<signaling::CommonHeader>(json, err);
	int64_t id = commhdr->id.value_or(-1);
	bool response = commhdr->response.value_or(false);
	bool request = commhdr->request.value_or(false);
	string method = commhdr->method.value_or("");*/
	//std::bind()
	//cout << "id:" << id << ", response:" << response << ", request:" << request << ", method:" << method << "\n" << commhdr->toJsonStr();
}