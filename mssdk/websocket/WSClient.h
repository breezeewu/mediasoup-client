#pragma once
#include "IWSClient.h"
#include <websocketpp/client.hpp>
#include <websocketpp/config/asio_client.hpp>
#ifndef lberror
#define lbtrace printf
#define lberror printf
#endif
//using TLSClient = websocketpp::client<websocketpp::config::asio_tls_client>;
using WebSocketClient = websocketpp::client<websocketpp::config::asio_client>;

class WSClient:public IWSClient
{
public:
	WSClient();

	~WSClient();

	virtual int connect(string const& uri, shared_ptr<IWSObserver> observer, const string& subprotocol = "");

	virtual void close(int code = 0, const string& reason = "");

	virtual int send_text(const string& data);

	virtual int send_binary(const vector<uint8_t>& data);

	virtual int send_ping(const string& data);

	virtual void send_pong(const string& data);

protected:
	virtual void on_open(WebSocketClient* c, websocketpp::connection_hdl hdl);

	virtual void on_fail(WebSocketClient* c, websocketpp::connection_hdl hdl);

	virtual void on_close(WebSocketClient* c, websocketpp::connection_hdl hdl);

	virtual bool on_validate(WebSocketClient* c, websocketpp::connection_hdl hdl);

	virtual void on_message(WebSocketClient* c, websocketpp::connection_hdl hdl, WebSocketClient::message_ptr msg);

	//virtual void on_binary_message(WebSocketClient* c, websocketpp::connection_hdl hdl, const vector<uint8_t>& data);

	virtual bool on_ping(WebSocketClient* c, websocketpp::connection_hdl hdl, const string& text);

	virtual void on_pong(WebSocketClient* c, websocketpp::connection_hdl hdl, const string& text);

	virtual void on_pong_timeout(WebSocketClient* c, websocketpp::connection_hdl hdl, const string& text);

	int init();

	void deinit();
protected:
	std::unique_ptr<WebSocketClient>			ws_client_;
	shared_ptr<IWSObserver>						ws_observer_;
	websocketpp::connection_hdl					handl_;
	websocketpp::lib::shared_ptr<websocketpp::lib::thread> _thread;
};