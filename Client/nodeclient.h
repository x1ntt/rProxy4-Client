#pragma once

#include "handler.h"
#include "cmd.h"
#include "rpoll.h"

class Handler;
class NodeClient : public GNET::Active, public std::enable_shared_from_this<NodeClient> {
public:
	NodeClient(std::string host, int port)
		: Active(host, port), _running(true), _cur_sid(0) {
	}

	~NodeClient() {
	}

	void del_handler(Sid sid);
	void safe_send(Buffer& buffer);
	bool Auth(us16 cid, std::string note);
	void Handle(Buffer& buf, Sid sid);
	void run();
	void stop();

private:
	Buffer _recv_buffer;
	std::map<Sid, std::shared_ptr<Handler>> _handle_map;
	std::mutex _handle_mtx;
	std::mutex _send_mtx;
	Sid _cur_sid;
	bool _running;
};