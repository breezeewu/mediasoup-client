#include "WSPPClient.h"
#include "WSSClient.h"
#include "WSClient.h"
#include <websocketpp/client.hpp>

WSPPClient::WSPPClient()
{
	ws_status_ = WS_STATUS_INIT;
}

WSPPClient::~WSPPClient()
{

}

int WSPPClient::connect(string const& url, shared_ptr<IWSObserver> observer, const string& subprotocol)
{
	websocketpp::uri_ptr location = websocketpp::lib::make_shared<websocketpp::uri>(url);
	close();
	observer_ = observer;
	if (location->get_scheme() == "https" || location->get_scheme() == "wss")
	{
		ws_client_.reset(new WSSClient());
	}
	else
	{
		ws_client_.reset(new WSClient());
	}
	ws_status_ = WS_STATUS_CONNECTING;
	int ret = ws_client_->connect(url, shared_from_this(), subprotocol);
	
	return ret;
}

int WSPPClient::connect_sync(string const& uri, shared_ptr<IWSObserver> observer, const string& subprotocol)
{
	std::lock_guard<std::recursive_mutex> lock(mutex_);
	int ret = connect(uri, observer, subprotocol);
	if (0 != ret)
	{
		lberror("connect(uri:%s, observer:%p, subprotocol:%s) failed, ret:%d", uri.c_str(), observer, subprotocol.c_str(), ret);
		return ret;
	}
	promise_ = new std::promise<int>();
	ret = promise_->get_future().get();
	delete promise_;
	return ret;
}

void WSPPClient::close(int code, const string& reason)
{
	std::lock_guard<std::recursive_mutex> lock(mutex_);
	if (ws_client_)
	{
		ws_client_->close(code, reason);
		ws_client_.release();
	}
	ws_status_ = WS_STATUS_DISCONNECT;
}

int WSPPClient::send_text(const string& data)
{
	//std::lock_guard<std::recursive_mutex> lock(mutex_);
	if (ws_client_)
	{
		return ws_client_->send_text(data);
	}
	return -1;
}

int WSPPClient::send_binary(const vector<uint8_t>& data)
{
	std::lock_guard<std::recursive_mutex> lock(mutex_);
	if (ws_client_)
	{
		return ws_client_->send_binary(data);
	}
	return -1;
}


int WSPPClient::send_ping(const string& data)
{
	std::lock_guard<std::recursive_mutex> lock(mutex_);
	if (ws_client_)
	{
		return ws_client_->send_ping(data);
	}
	return -1;
}

void WSPPClient::send_pong(const string& data)
{
	std::lock_guard<std::recursive_mutex> lock(mutex_);
	if (ws_client_)
	{
		ws_client_->send_text(data);
	}
}

WEBSOCKET_STATUS WSPPClient::status()
{
	return ws_status_;
}

string WSPPClient::recv_text()
{
	std::lock_guard<std::recursive_mutex> lock(mutex_);
	if (recv_text_que_.size() > 0)
	{
		string msg = recv_text_que_.front();
		recv_text_que_.pop();
		return msg;
	}
	else
	{
		return string();
	}
}

vector<uint8_t> WSPPClient::recv_binary()
{
	std::lock_guard<std::recursive_mutex> lock(mutex_);
	vector<uint8_t> data = recv_binary_que_.front();
	recv_binary_que_.pop();
	return data;
}

void WSPPClient::on_open()
{
	ws_status_ = WS_STATUS_CONNECTED;
	if (observer_)
	{
		observer_->on_open();
	}

	if (promise_)
	{
		promise_->set_value(0);
	}
}

void WSPPClient::on_fail(int errorCode, const string& reason)
{
	ws_status_ = WS_STATUS_DISCONNECT;
	if (observer_)
	{
		observer_->on_fail(errorCode, reason);
	}
	
	if (promise_)
	{
		promise_->set_value(errorCode);
	}
}

void WSPPClient::on_close(int closeCode, const string& reason)
{
	ws_status_ = WS_STATUS_DISCONNECT;
	if (observer_)
	{
		observer_->on_fail(closeCode, reason);
	}
}

bool WSPPClient::on_validate()
{
	if (observer_)
	{
		return observer_->on_validate();
	}
	return false;
}

void WSPPClient::on_text_message(const string& text)
{
	std::lock_guard<std::recursive_mutex> lock(mutex_);
	recv_text_que_.push(text);
	if (observer_)
	{
		observer_->on_text_message(text);
	}
}

void WSPPClient::on_binary_message(const vector<uint8_t>& data)
{
	recv_binary_que_.push(data);
	if (observer_)
	{
		observer_->on_binary_message(data);
	}
}

bool WSPPClient::on_ping(const string& text)
{
	if (observer_)
	{
		return observer_->on_ping(text);
	}
	return false;
}

void WSPPClient::on_pong(const string& text)
{
	if (observer_)
	{
		observer_->on_pong(text);
	}
}

void WSPPClient::on_pong_timeout(const string& text)
{
	if (observer_)
	{
		observer_->on_pong_timeout(text);
	}
}