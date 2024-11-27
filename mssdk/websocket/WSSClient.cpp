#include <windows.h>
#include "WSSClient.h"
#include <websocketpp/client.hpp>
#include <websocketpp/config/asio_client.hpp>
#include <websocketpp/transport/asio/endpoint.hpp>
#include <websocketpp/transport/asio/security/tls.hpp>
using TLSClient = websocketpp::client<websocketpp::config::asio_tls_client>;
using WSClient = websocketpp::client<websocketpp::config::asio_client>;
typedef websocketpp::lib::shared_ptr<websocketpp::lib::asio::ssl::context> context_ptr;

using websocketpp::lib::placeholders::_1;
using websocketpp::lib::placeholders::_2;

/// Verify that one of the subject alternative names matches the given hostname
bool verify_subject_alternative_name(const char* hostname, X509* cert) {
	STACK_OF(GENERAL_NAME)* san_names = NULL;

	san_names = (STACK_OF(GENERAL_NAME)*) X509_get_ext_d2i(cert, NID_subject_alt_name, NULL, NULL);
	if (san_names == NULL) {
		return false;
	}

	int san_names_count = sk_GENERAL_NAME_num(san_names);

	bool result = false;

	for (int i = 0; i < san_names_count; i++) {
		const GENERAL_NAME* current_name = sk_GENERAL_NAME_value(san_names, i);

		if (current_name->type != GEN_DNS) {
			continue;
		}

		char const* dns_name = (char const*)ASN1_STRING_get0_data(current_name->d.dNSName);

		// Make sure there isn't an embedded NUL character in the DNS name
		if (ASN1_STRING_length(current_name->d.dNSName) != strlen(dns_name)) {
			break;
		}
		// Compare expected hostname with the CN
		result = (strcmp(hostname, dns_name) == 0);
	}
	sk_GENERAL_NAME_pop_free(san_names, GENERAL_NAME_free);

	return result;
}

/// Verify that the certificate common name matches the given hostname
bool verify_common_name(char const* hostname, X509* cert) {
	// Find the position of the CN field in the Subject field of the certificate
	int common_name_loc = X509_NAME_get_index_by_NID(X509_get_subject_name(cert), NID_commonName, -1);
	if (common_name_loc < 0) {
		return false;
	}

	// Extract the CN field
	X509_NAME_ENTRY* common_name_entry = X509_NAME_get_entry(X509_get_subject_name(cert), common_name_loc);
	if (common_name_entry == NULL) {
		return false;
	}

	// Convert the CN field to a C string
	ASN1_STRING* common_name_asn1 = X509_NAME_ENTRY_get_data(common_name_entry);
	if (common_name_asn1 == NULL) {
		return false;
	}

	char const* common_name_str = (char const*)ASN1_STRING_get0_data(common_name_asn1);

	// Make sure there isn't an embedded NUL character in the CN
	if (ASN1_STRING_length(common_name_asn1) != strlen(common_name_str)) {
		return false;
	}

	// Compare expected hostname with the CN
	return (strcmp(hostname, common_name_str) == 0);
}

/**
 * This code is derived from examples and documentation found ato00po
 * http://www.boost.org/doc/libs/1_61_0/doc/html/boost_asio/example/cpp03/ssl/client.cpp
 * and
 * https://github.com/iSECPartners/ssl-conservatory
 */
bool verify_certificate(const char * hostname, bool preverified, asio::ssl::verify_context& ctx) {
    // The verify callback can be used to check whether the certificate that is
    // being presented is valid for the peer. For example, RFC 2818 describes
    // the steps involved in doing this for HTTPS. Consult the OpenSSL
    // documentation for more details. Note that the callback is called once
    // for each certificate in the certificate chain, starting from the root
    // certificate authority.

	// Retrieve the depth of the current cert in the chain. 0 indicates the
	// actual server cert, upon which we will perform extra validation
	// (specifically, ensuring that the hostname matches. For other certs we
	// will use the 'preverified' flag from Asio, which incorporates a number of
	// non-implementation specific OpenSSL checking, such as the formatting of
	// certs and the trusted status based on the CA certs we imported earlier.
	int depth = X509_STORE_CTX_get_error_depth(ctx.native_handle());

	// if we are on the final cert and everything else checks out, ensure that
	// the hostname is present on the list of SANs or the common name (CN).
	if (depth == 0 && preverified) {
		X509* cert = X509_STORE_CTX_get_current_cert(ctx.native_handle());

		if (verify_subject_alternative_name(hostname, cert)) {
			return true;
		}
		else if (verify_common_name(hostname, cert)) {
			return true;
		}
		else {
			return false;
		}
	}

	return preverified;
}

context_ptr on_tls_init(const char* hostname, websocketpp::connection_hdl) {
	context_ptr ctx = websocketpp::lib::make_shared<asio::ssl::context>(asio::ssl::context::sslv23);

	try {
		ctx->set_options(asio::ssl::context::default_workarounds |
			asio::ssl::context::no_sslv2 |
			asio::ssl::context::no_sslv3 |
			asio::ssl::context::single_dh_use);

		ctx->set_verify_mode(asio::ssl::verify_none);

		//ctx->set_verify_mode(boost::asio::ssl::verify_peer);
		ctx->set_verify_callback(bind(&verify_certificate, hostname, ::_1, ::_2));

		// Here we load the CA certificates of all CA's that this client trusts.
		//CString str;
		//str.Format("%sca-chain.cert.pem", theApp.m_lpszAppPath);
		//ctx->load_verify_file(str.GetBufferSetLength(str.GetLength()+1));

	}
	catch (std::exception& e) {
		std::cout << e.what() << std::endl;
	}
	return ctx;
}

WSSClient::WSSClient()
{
	ws_client_ = NULL;
}

WSSClient::~WSSClient()
{
	if (ws_client_)
	{
		close();
		ws_client_.release();
	}
}

int WSSClient::init()
{
	deinit();
	ws_client_.reset(new TLSClient());
	ws_client_->clear_access_channels(websocketpp::log::alevel::all);
	ws_client_->clear_error_channels(websocketpp::log::elevel::all);

	ws_client_->init_asio();
	auto handle = bind(&on_tls_init, "", ::_1);
	ws_client_->set_tls_init_handler(handle);
	ws_client_->start_perpetual();
	
	_thread = websocketpp::lib::make_shared<websocketpp::lib::thread>(&TLSClient::run, ws_client_.get());
	return 0;
}

void WSSClient::deinit()
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

int WSSClient::connect(string const& uri, shared_ptr<IWSObserver> observer, const string& subprotocol)
{
	websocketpp::lib::error_code ec;
	ws_observer_ = observer;
	if (NULL == ws_client_)
	{
		init();
	}
	TLSClient::connection_ptr con = ws_client_->get_connection(uri, ec);

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
	//ConnectionMetadata<TLSClient>::ptr metadataPtr = websocketpp::lib::make_shared<ConnectionMetadata<TLSClient>>(newId, con->get_handle(), uri, observer);
	//_connectionList[newId] = metadataPtr;

	con->set_open_handler(websocketpp::lib::bind(
		&WSSClient::on_open,
		//&ConnectionMetadata<TLSClient>::onOpen,
		this,
		ws_client_.get(),
		websocketpp::lib::placeholders::_1
	));
	con->set_fail_handler(websocketpp::lib::bind(
		&WSSClient::on_fail,
		this,
		//metadataPtr,
		ws_client_.get(),
		websocketpp::lib::placeholders::_1
	));
	con->set_close_handler(websocketpp::lib::bind(
		&WSSClient::on_close,
		this,
		ws_client_.get(),
		websocketpp::lib::placeholders::_1
	));
	con->set_message_handler(websocketpp::lib::bind(
		&WSSClient::on_message,
		this,
		ws_client_.get(),
		websocketpp::lib::placeholders::_1,
		websocketpp::lib::placeholders::_2
	));

	con->set_ping_handler(websocketpp::lib::bind(
		&WSSClient::on_ping,
		this,
		ws_client_.get(),
		websocketpp::lib::placeholders::_1,
		websocketpp::lib::placeholders::_2
	));

	con->set_pong_handler(websocketpp::lib::bind(
		&WSSClient::on_pong,
		this,
		ws_client_.get(),
		websocketpp::lib::placeholders::_1,
		websocketpp::lib::placeholders::_2
	));

	con->set_pong_timeout_handler(websocketpp::lib::bind(
		&WSSClient::on_pong_timeout,
		this,
		ws_client_.get(),
		websocketpp::lib::placeholders::_1,
		websocketpp::lib::placeholders::_2
	));

	ws_client_->connect(con);
	//ws_client_->run();
	return 0;
}

void WSSClient::close(int code, const string& reason)
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

int WSSClient::send_text(const string& data)
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

int WSSClient::send_binary(const vector<uint8_t>& data)
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

int WSSClient::send_ping(const string& data)
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

void WSSClient::send_pong(const string& data)
{
	websocketpp::lib::error_code ec;
	assert(ws_client_);
	ws_client_->send(handl_, data, websocketpp::frame::opcode::pong , ec);
	if (ec) {
		lberror("Error send pong:%s", ec.message().c_str());
	}
}

void WSSClient::on_open(TLSClient* c, websocketpp::connection_hdl hdl)
{
	lbtrace("on_open success\n");
	if (ws_observer_)
	{
		ws_observer_->on_open();
	}
}

void WSSClient::on_fail(TLSClient* c, websocketpp::connection_hdl hdl)
{
	if (ws_observer_)
	{
		TLSClient::connection_ptr con = c->get_con_from_hdl(hdl);
		//_server = con->get_response_header("Server");
		std::string err_reason = con->get_ec().message();
		int errorCode = con->get_ec().value();
		ws_observer_->on_fail(errorCode, err_reason);
	}
}

void WSSClient::on_close(TLSClient* c, websocketpp::connection_hdl hdl)
{
	lbtrace("on_close success\n");
	if (ws_observer_)
	{
		TLSClient::connection_ptr con = c->get_con_from_hdl(hdl);
		int closeCode = con->get_remote_close_code();
		std::string close_reason = con->get_remote_close_reason();
		ws_observer_->on_close(closeCode, close_reason);
	}
}

bool WSSClient::on_validate(TLSClient* c, websocketpp::connection_hdl hdl)
{
	if (ws_observer_)
	{
		return ws_observer_->on_validate();
	}

	return false;
}

void WSSClient::on_message(TLSClient* c, websocketpp::connection_hdl hdl, TLSClient::message_ptr msg)
{
	if (ws_observer_)
	{
		if (msg->get_opcode() == websocketpp::frame::opcode::text) {
			lbtrace("on_message:%s\n", msg->get_payload().c_str());
			ws_observer_->on_text_message(msg->get_payload());
		}
		else if (msg->get_opcode() == websocketpp::frame::opcode::binary) {
			std::vector<uint8_t> data(msg->get_payload().begin(), msg->get_payload().end());
			ws_observer_->on_binary_message(data);
		}
	}
}

bool WSSClient::on_ping(TLSClient* c, websocketpp::connection_hdl hdl, const string& text)
{
	if (ws_observer_)
	{
		return ws_observer_->on_ping(text);
	}
	return false;
}

void WSSClient::on_pong(TLSClient* c, websocketpp::connection_hdl hdl, const string& text)
{
	if (ws_observer_)
	{
		ws_observer_->on_pong(text);
	}
}

void WSSClient::on_pong_timeout(TLSClient* c, websocketpp::connection_hdl hdl, const string& text)
{
	if (ws_observer_)
	{
		ws_observer_->on_pong_timeout(text);
	}
}