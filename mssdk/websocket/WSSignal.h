#include "lazythread.h"
#include "IWSClient.h"

class WSSignal:public LazyThread, public IWSObserver
{
public:
	WSSignal();
	~WSSignal();

	virtual int ThreadProc();

	int connect(string url, string subproto);

	int send(string request);

	int recv(int id, string& response, string key = "id");

protected:

};