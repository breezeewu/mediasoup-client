#pragma once
#include <list>
#include <mutex>
#include <unordered_map>
#include "lazyutil/lazythread.h"
//#include "../websocket/IWSClient.h"
#include "websocket/IWSClient.h"
#include <rtc_base/thread.h>
#include "mssignal/signaling_models.h"

using namespace signaling;

class MSMessage
{
public:
	MSMessage() {}
	~MSMessage() {}

	int64_t		id = 0;
	string		method;
	MSGTYPE		msg_type = MSG_TYPE_UNKNOW;
	string		json;
	std::function<void(std::string msg)> func;
	std::shared_ptr<std::promise<std::string>> msg_promise;
};

class IMediaSoupAPI
{
public:
	virtual ~IMediaSoupAPI() {}

	//virtual int on_recv_message() = 0;
	virtual int on_recv_request(const shared_ptr<MSMessage>& msg) = 0;

	virtual int on_recv_response(const shared_ptr<MSMessage>& msg) = 0;

	virtual int on_recv_notify(const shared_ptr<MSMessage>& msg) = 0;
};

class MSSignal:public LazyThread, public IWSObserver, public std::enable_shared_from_this<MSSignal>
{
public:
	MSSignal(shared_ptr<IMediaSoupAPI> msapi);
	~MSSignal();

	virtual int ThreadProc();

	int connect(const string url);

	void close();

	int send(const string& msg);

	string send_request_and_recv_response(string request);

	int send(const string& request, std::shared_ptr<std::promise<std::string>> msg_promise, std::function<void(std::string response)> callback = nullptr);

	//int send(const string& request, std::shared_ptr<std::promise<std::string>> msg_promise);
	//string recv();

	virtual void on_open();

	virtual void on_fail(int errorCode, const string& reason);

	virtual void on_close(int closeCode, const string& reason);

	virtual bool on_validate();

	virtual void on_text_message(const string& text);

	virtual void on_binary_message(const vector<uint8_t>& data);

	virtual bool on_ping(const string& text);

	virtual void on_pong(const string& text);

	virtual void on_pong_timeout(const string& text);

protected:
	shared_ptr<MSMessage> get_msg(unordered_map<int64_t, shared_ptr<MSMessage>>& msg_list);

	size_t msg_size();

	shared_ptr<MSMessage> message_from_json(string json);

	int on_recv_msg(const shared_ptr<MSMessage>& msg);

protected:
	unique_ptr<rtc::Thread>								task_queue_;
	unordered_map<int64_t, shared_ptr<MSMessage>>		send_list_;
	unordered_map<int64_t, shared_ptr<MSMessage>>		ack_list_;
	vector<shared_ptr<MSMessage>>						recv_list_;
	shared_ptr<IWSClient>								ws_client_;
	recursive_mutex										mutex_;
	string												url_;
	shared_ptr<IMediaSoupAPI>							ms_api_;
};