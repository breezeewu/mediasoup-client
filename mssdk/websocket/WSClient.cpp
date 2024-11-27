#include "WSClient.h"
#include <websocketpp/client.hpp>
#include <websocketpp/config/asio_client.hpp>
#include <websocketpp/transport/asio/endpoint.hpp>
using Client = websocketpp::client<websocketpp::config::asio_client>;

WSClient::WSClient()
{
	ws_client_ = NULL;
}

WSClient::~WSClient()
{
	if (ws_client_)
	{
		close();
		ws_client_.release();
	}
}

int WSClient::init()
{
	deinit();
	ws_client_.reset(new WebSocketClient());
	ws_client_->clear_access_channels(websocketpp::log::alevel::all);
	ws_client_->clear_error_channels(websocketpp::log::elevel::all);

	ws_client_->init_asio();
	//auto handle = bind(&on_tls_init, "", ::_1);
	//ws_client_->set_tls_init_handler(handle);
	ws_client_->start_perpetual();

	_thread = websocketpp::lib::make_shared<websocketpp::lib::thread>(&Client::run, ws_client_.get());
	//_thread = websocketpp::lib::make_shared<websocketpp::lib::thread>(&TLSClient::run, ws_client_);
	return 0;
}

void WSClient::deinit()
{
	close();
	if (ws_client_)
	{
		ws_client_->stop_perpetual();
		ws_client_.release();
	}
	
	if (_thread && _thread->joinable()) {
		_thread->join();
	}
}

int WSClient::connect(string const& uri, shared_ptr<IWSObserver> observer, const string& subprotocol)
{
	websocketpp::lib::error_code ec;
	ws_observer_ = observer;
	if (NULL == ws_client_)
	{
		init();
	}
	WebSocketClient::connection_ptr con = ws_client_->get_connection(uri, ec);

	if (ec) {
		auto msg = ec.message();
		lberror("> Connect initialization error:%s", ec.message().c_str());
		return -1;
	}

	if (!subprotocol.empty()) {
		con->add_subprotocol(subprotocol, ec);
		if (ec) {
			lberror("> add subprotocol error:%s", ec.message().c_str());
			return -1;
		}
	}
	handl_ = con->get_handle();
	//int newId = _nextId++;
	//ConnectionMetadata<WebSocketClient>::ptr metadataPtr = websocketpp::lib::make_shared<ConnectionMetadata<WebSocketClient>>(newId, con->get_handle(), uri, observer);
	//_connectionList[newId] = metadataPtr;

	con->set_open_handler(websocketpp::lib::bind(
		&WSClient::on_open,
		//&ConnectionMetadata<WebSocketClient>::onOpen,
		this,
		ws_client_.get(),
		websocketpp::lib::placeholders::_1
	));
	con->set_fail_handler(websocketpp::lib::bind(
		&WSClient::on_fail,
		this,
		//metadataPtr,
		ws_client_.get(),
		websocketpp::lib::placeholders::_1
	));
	con->set_close_handler(websocketpp::lib::bind(
		&WSClient::on_close,
		this,
		ws_client_.get(),
		websocketpp::lib::placeholders::_1
	));
	con->set_message_handler(websocketpp::lib::bind(
		&WSClient::on_message,
		this,
		ws_client_.get(),
		websocketpp::lib::placeholders::_1,
		websocketpp::lib::placeholders::_2
	));

	con->set_ping_handler(websocketpp::lib::bind(
		&WSClient::on_ping,
		this,
		ws_client_.get(),
		websocketpp::lib::placeholders::_1,
		websocketpp::lib::placeholders::_2
	));

	con->set_pong_handler(websocketpp::lib::bind(
		&WSClient::on_pong,
		this,
		ws_client_.get(),
		websocketpp::lib::placeholders::_1,
		websocketpp::lib::placeholders::_2
	));

	con->set_pong_timeout_handler(websocketpp::lib::bind(
		&WSClient::on_pong_timeout,
		this,
		ws_client_.get(),
		websocketpp::lib::placeholders::_1,
		websocketpp::lib::placeholders::_2
	));

	ws_client_->connect(con);
	//ws_client_->run();
	return 0;
}

void WSClient::close(int code, const string& reason)
{
	if (ws_client_) {
		websocketpp::lib::error_code ec;
		ws_client_->stop();
		ws_client_->close(handl_, code, reason, ec);
		if (ec) {
			lberror("Error initiating close:%s", ec.message().c_str());
		}
	}
	
}

int WSClient::send_text(const string& data)
{
	websocketpp::lib::error_code ec;
	assert(ws_client_);
	ws_client_->send(handl_, data, websocketpp::frame::opcode::text, ec);
	if (ec) {
		lberror("Error send text:%s", ec.message().c_str());
		return -1;
	}
	return 0;
}

int WSClient::send_binary(const vector<uint8_t>& data)
{
	websocketpp::lib::error_code ec;
	assert(ws_client_);
	ws_client_->send(handl_, data.data(), data.size(), websocketpp::frame::opcode::binary, ec);
	if (ec) {
		lberror("Error send text:%s", ec.message().c_str());
		return -1;
	}
	return 0;
}

int WSClient::send_ping(const string& data)
{
	websocketpp::lib::error_code ec;
	assert(ws_client_);
	ws_client_->send(handl_, data, websocketpp::frame::opcode::ping, ec);
	if (ec) {
		lberror("Error send ping:%s", ec.message().c_str());
		return -1;
	}
	return 0;
}

void WSClient::send_pong(const string& data)
{
	websocketpp::lib::error_code ec;
	assert(ws_client_);
	ws_client_->send(handl_, data, websocketpp::frame::opcode::pong , ec);
	if (ec) {
		lberror("Error send pong:%s", ec.message().c_str());
	}
}

void WSClient::on_open(WebSocketClient* c, websocketpp::connection_hdl hdl)
{
	lbtrace("on_open success\n");
	if (ws_observer_)
	{
		ws_observer_->on_open();
	}
}

void WSClient::on_fail(WebSocketClient* c, websocketpp::connection_hdl hdl)
{
	if (ws_observer_)
	{
		WebSocketClient::connection_ptr con = c->get_con_from_hdl(hdl);
		//_server = con->get_response_header("Server");
		std::string err_reason = con->get_ec().message();
		int errorCode = con->get_ec().value();
		ws_observer_->on_fail(errorCode, err_reason);
	}
}

void WSClient::on_close(WebSocketClient* c, websocketpp::connection_hdl hdl)
{
	lbtrace("on_close success\n");
	if (ws_observer_)
	{
		WebSocketClient::connection_ptr con = c->get_con_from_hdl(hdl);
		int closeCode = con->get_remote_close_code();
		std::string close_reason = con->get_remote_close_reason();
		ws_observer_->on_close(closeCode, close_reason);
	}
}

bool WSClient::on_validate(WebSocketClient* c, websocketpp::connection_hdl hdl)
{
	if (ws_observer_)
	{
		return ws_observer_->on_validate();
	}

	return false;
}

void WSClient::on_message(WebSocketClient* c, websocketpp::connection_hdl hdl, WebSocketClient::message_ptr msg)
{
	if (msg->get_opcode() == websocketpp::frame::opcode::text) {
		lbtrace("on_message:%s\n", msg->get_payload().c_str());
		if (ws_observer_)
		{
			ws_observer_->on_text_message(msg->get_payload());
		}
		
	}
	else if (msg->get_opcode() == websocketpp::frame::opcode::binary) {
		std::vector<uint8_t> data(msg->get_payload().begin(), msg->get_payload().end());
		if (ws_observer_)
		{
			ws_observer_->on_binary_message(data);
		}
	}
}

bool WSClient::on_ping(WebSocketClient* c, websocketpp::connection_hdl hdl, const string& text)
{
	if (ws_observer_)
	{
		return ws_observer_->on_ping(text);
	}
	return false;
}

void WSClient::on_pong(WebSocketClient* c, websocketpp::connection_hdl hdl, const string& text)
{
	if (ws_observer_)
	{
		ws_observer_->on_pong(text);
	}
}

void WSClient::on_pong_timeout(WebSocketClient* c, websocketpp::connection_hdl hdl, const string& text)
{
	if (ws_observer_)
	{
		ws_observer_->on_pong_timeout(text);
	}
}