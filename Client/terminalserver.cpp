#include "terminalserver.h"

int TerminalServer::_cnt = 0;

bool TerminalServer::run() {
	char buf[RECV_MAX];
	//_cnt++;
	int ret = Recv(buf, RECV_MAX);
	//_cnt--;
	//LOG_D("cnd_: %d", _cnt);
	if (ret <= 0) {
		if (WSAGetLastError() == WSAEWOULDBLOCK) return true;
		if (!_hd.expired()) {
			_hd.lock()->terminal_close();
		}
		else {
			LOG_W("出现意料之外的情况");	// 确实会出现
		}
		return false;
	}

	if (!_hd.expired()) {
		_hd.lock()->send_to_server(Buffer().push_back(buf, ret));
	}
	return true;
}