#pragma once
#include <string>
#include <vector>
#include <memory>
using namespace std;
enum WEBSOCKET_STATUS
{
	WS_STATUS_UNINIT = -1,
	WS_STATUS_INIT = 0,
	WS_STATUS_CONNECTING,
	WS_STATUS_CONNECTED,
	WS_STATUS_DISCONNECT
};

class IWSObserver {
public:
	virtual ~IWSObserver() {}

	virtual void on_open() = 0;

	virtual void on_fail(int errorCode, const string& reason) = 0;

	virtual void on_close(int closeCode, const string& reason) = 0;

	virtual bool on_validate() = 0;

	virtual void on_text_message(const string& text) = 0;

	virtual void on_binary_message(const vector<uint8_t>& data) = 0;

	virtual bool on_ping(const string& text) = 0;

	virtual void on_pong(const string& text) = 0;

	virtual void on_pong_timeout(const string& text) = 0;
};

class IWSClient
{
public:
	virtual int connect(string const& uri, shared_ptr<IWSObserver> observer, const string& subprotocol = "") = 0;

	virtual int connect_sync(string const& uri, shared_ptr<IWSObserver> observer, const string& subprotocol = "") { return -1; };

	virtual void close(int code = 0, const string& reason = "") = 0;

	virtual int send_text(const string& data) = 0;

	virtual int send_binary(const vector<uint8_t>& data) = 0;

	virtual int send_ping(const string& data) = 0;

	virtual void send_pong(const string& data) = 0;

	virtual WEBSOCKET_STATUS status() { return WS_STATUS_UNINIT; };

	virtual string recv_text() { return string(); }

	virtual vector<uint8_t> recv_binary() { return vector<uint8_t>(); }
};

