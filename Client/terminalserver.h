#pragma once

#include "handler.h"
#include "rpoll.h"
#include "rthread.h"

class Handler;
class TerminalServer : public GNET::Active, public Thread::Runnable {
public:
	TerminalServer(std::string host, int port, std::weak_ptr<Handler> hd)
		: Active(host, port), _hd(hd) {}
	~TerminalServer() {
		OnClose();
	}
	int terminal_send(Buffer& buf) {
		//LOG_D("·¢ËÍ: %d", buf.size());
		return Send(buf.data(), buf.size());
	}

	bool run();

private:
	std::weak_ptr<Handler> _hd;
	static int _cnt;
};