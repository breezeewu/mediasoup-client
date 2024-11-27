#include "IWSClient.h"
#include <future>
#include <mutex>
#include <queue>

class WSPPClient:public IWSClient, public IWSObserver, public std::enable_shared_from_this<WSPPClient>
{
public:
	WSPPClient();
	~WSPPClient();

	virtual int connect(string const& uri, shared_ptr<IWSObserver> observer, const string& subprotocol = "");

	virtual int connect_sync(string const& uri, shared_ptr<IWSObserver> observer, const string& subprotocol = "");

	virtual void close(int code = 0, const string& reason = "");

	virtual int send_text(const string& data);

	virtual int send_binary(const vector<uint8_t>& data);

	virtual int send_ping(const string& data);

	virtual void send_pong(const string& data);

	virtual WEBSOCKET_STATUS status();

	virtual string recv_text();

	virtual vector<uint8_t> recv_binary();

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
	std::unique_ptr<IWSClient>		ws_client_;
	shared_ptr<IWSObserver>			observer_;
	std::promise<int>*				promise_;
	std::recursive_mutex			mutex_;
	std::queue<string>				recv_text_que_;
	std::queue<vector<uint8_t>>		recv_binary_que_;
	WEBSOCKET_STATUS				ws_status_;

};