#pragma once

#include "buffer.h"
#include "memory"
#include "terminalserver.h"
#include "nodeclient.h"
#include "rthread.h"

class NodeClient;
class TerminalServer;
class Handler : public Thread::Runnable, public std::enable_shared_from_this<Handler> {
public:
	Handler(Sid sid, std::weak_ptr<NodeClient> nc)
		: _sid(sid), _nc(nc) {
		_proxy_server = "127.0.0.1";
		_proxy_port = 3128;
	}

	~Handler() {
		LOG_D("Handler Îö¹¹ [sid=%d]", _sid);
	}
	void process(Buffer buf, Sid sid) {
		std::unique_lock<std::mutex> kp(_send_mtx);
		_send_buff << buf;
	}

	bool run();

	void send_to_server(Buffer& buf);

	void terminal_close();

private:
	Sid _sid;
	std::shared_ptr<TerminalServer> _ts;
	std::weak_ptr<NodeClient> _nc;
	std::string _proxy_server;
	std::mutex _send_mtx;
	Buffer _send_buff;
	int _proxy_port;
};