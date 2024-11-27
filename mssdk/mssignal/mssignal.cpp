#include "mssignal.h"
#include "mssignal/signaling_models.h"
#include <websocket/WSPPClient.h>
#include "rtc_base/thread.h"
#include "rtc_base/logging.h"
MSSignal::MSSignal(shared_ptr<IMediaSoupAPI> msapi):ms_api_(msapi)
{
	task_queue_ = rtc::Thread::Create();
}

MSSignal::~MSSignal()
{
	
}

int MSSignal::ThreadProc()
{
	while (IsAlive())
	{
		if (ws_client_ && msg_size() > 0)
		{
			lock_guard<recursive_mutex> lock(mutex_);
			shared_ptr<MSMessage> pmsg = get_msg(send_list_);
			if (pmsg)
			{
				ws_client_->send_text(pmsg->json);
				if (pmsg->id != 0)
				{
					ack_list_[pmsg->id] = pmsg;
				}
			}
		}
		else
		{
			continue;
		}

		ThreadSleep(50);
	};
	return 0;
}

int MSSignal::connect(const string url)
{
	lock_guard<recursive_mutex> lock(mutex_);
	url_ = url;
	if (NULL == ws_client_)
	{
		ws_client_.reset(new WSPPClient());
	}

	int ret = ws_client_->connect_sync(url_, shared_from_this(), "protoo");
	if (ret != 0)
	{
		lberror("connect failed ret:%d", ret);
		return ret;
	}

	return Run();
}

void MSSignal::close()
{
	lock_guard<recursive_mutex> lock(mutex_);
	send_list_.clear();
	ack_list_.clear();
}

int MSSignal::send(const string& msg)
{
	return send(msg, std::shared_ptr<std::promise<std::string>>());
}

string MSSignal::send_request_and_recv_response(string request)
{
	std::shared_ptr<std::promise<std::string>> msg_promise(new std::promise<std::string>());
	std::future<std::string> future = msg_promise->get_future();
	send(request, msg_promise);
	std::string response = future.get();
	//RTC_LOG(LS_INFO) << "recv response:" << response;
	return response;
}

int MSSignal::send(const string& request, std::shared_ptr<std::promise<std::string>> msg_promise, std::function<void(std::string response)> callback)
{
	lock_guard<recursive_mutex> lock(mutex_);
	shared_ptr<MSMessage> pmsg = message_from_json(request);
	RTC_LOG(LS_INFO) << "MSClient send msg:" << request << ", id:" << pmsg->id;
	pmsg->msg_promise = msg_promise;
	pmsg->func = callback;
	send_list_[pmsg->id] = pmsg;
	return 0;
}

/*int MSSignal::send(const string& request, std::shared_ptr<std::promise<std::string>> msg_promise)
{
	lock_guard<recursive_mutex> lock(mutex_);
	shared_ptr<MSMessage> pmsg = message_from_json(request);

	pmsg->msg_promise = msg_promise;
	send_list_[pmsg->id] = pmsg;
	return 0;
}*/

void MSSignal::on_open()
{

}

void MSSignal::on_fail(int errorCode, const string& reason)
{

}

void MSSignal::on_close(int closeCode, const string& reason)
{
	if (ws_client_)
	{
		ws_client_->close();
	}
}

bool MSSignal::on_validate()
{
	return true;
}

void MSSignal::on_text_message(const string& text)
{
	shared_ptr<MSMessage> pmsg = message_from_json(text);
	RTC_LOG(LS_INFO) << "MSClient recv:" << text;
	auto it = ack_list_.find(pmsg->id);
	if (it != ack_list_.end())
	{
		shared_ptr<MSMessage> req = it->second;
		if (req)
		{
			pmsg->method		= req->method;
			pmsg->func			= req->func;
			pmsg->msg_promise	= req->msg_promise;
			if (pmsg->msg_promise)
			{
				pmsg->msg_promise->set_value(text);
				RTC_LOG(LS_INFO) << "recv response id:" << pmsg->id;
			}
			if (pmsg->func)
			{
				pmsg->func(text);
			}
		}
		ack_list_.erase(it);
	}
	else
	{
		RTC_LOG(LS_INFO) << pmsg->id << " not found in ack list";
	}
#if 1
	on_recv_msg(pmsg);
#else
	task_queue_->PostTask(RTC_FROM_HERE, [wself = weak_from_this(), pmsg](){
		auto self = wself.lock();
		self->on_recv_msg(pmsg);
	});
#endif
}

void MSSignal::on_binary_message(const vector<uint8_t>& data)
{

}

bool MSSignal::on_ping(const string& text)
{
	return true;
}

void MSSignal::on_pong(const string& text)
{

}

void MSSignal::on_pong_timeout(const string& text)
{

}

shared_ptr<MSMessage> MSSignal::get_msg(unordered_map<int64_t, shared_ptr<MSMessage>>& msg_list)
{
	lock_guard<recursive_mutex> lock(mutex_);
	if (msg_list.size() > 0)
	{
		auto it = msg_list.begin();
		shared_ptr<MSMessage> msg = it->second;
		msg_list.erase(it);
		return msg;
	}
	else
	{
		return shared_ptr<MSMessage>();
	}
}

size_t MSSignal::msg_size()
{
	lock_guard<recursive_mutex> lock(mutex_);
	return send_list_.size();
}

shared_ptr<MSMessage> MSSignal::message_from_json(string json)
{
	std::string err;
	auto chdr = fromJsonString<signaling::CommonHeader>(json, err);
	shared_ptr<MSMessage> pms(new MSMessage());
	pms->id = chdr->id.value_or(-1);
	pms->json = json;
	pms->method = chdr->method.value_or("");
	//pms->func = callback;
	pms->msg_type = chdr->msgType();
	return pms;
}

int MSSignal::on_recv_msg(const shared_ptr<MSMessage>& msg)
{
	//lock_guard<recursive_mutex> lock(mutex_);

	switch (msg->msg_type)
	{
		case MSG_TYPE_REQUEST:
		{
			return ms_api_->on_recv_request(msg);
		}
		break;
		case MSG_TYPE_RESPONSE:
		{
			return ms_api_->on_recv_response(msg);
		}
		break;
		case MSG_TYPE_NOTIFY:
		{
			return ms_api_->on_recv_notify(msg);
		}
		break;
		default:
		{
			assert(0);
			return -1;
		}
		break;
	}
}
