#include "nodeclient.h"

void NodeClient::del_handler(Sid sid) {
	std::unique_lock<std::mutex> kp(_handle_mtx);
	if (_handle_map.find(sid) != _handle_map.end()) {	// handle的生命周期没有结束，所以还ts还活着
		LOG_D("[sid=%d] 结束, use: %d", sid, _handle_map[sid].use_count());
		_handle_map.erase(sid);
		Buffer tmp;
		CMD::Make_session_end(tmp, sid);
		safe_send(tmp);
	}
}

void NodeClient::safe_send(Buffer& buffer) {
	std::unique_lock<std::mutex> kp(_send_mtx);
	Buffer send_buf;
	PackSize ps = buffer.size() + sizeof(PackSize);
	if (ps) {
		send_buf << ps << buffer;
		SendN(send_buf.data(), send_buf.size()); // 因为没有用户级的发送缓冲区，想要保证数据包完整的话，就需要确定发送完毕
		// send_buf.dump();
	}
}

bool NodeClient::Auth(us16 cid, std::string note) {
	// 暂时去掉了密码验证
	Buffer bf;
	CMD::Make_login(bf, cid, note);
	safe_send(bf);
	return true;
}

void NodeClient::Handle(Buffer& buf, Sid sid) {
	if (sid) {
		std::unique_lock<std::mutex> kp(_handle_mtx);
		if (_handle_map.find(sid) == _handle_map.end()) {
			if (_cur_sid < sid) {
				_cur_sid = sid;
				if (_cur_sid == MAX_SID) { _cur_sid = 0; }
			}else {
				return;
			}
			std::shared_ptr<Handler> hd = std::shared_ptr<Handler>(new Handler(sid, weak_from_this()));
			_handle_map[sid] = hd;
			Thread::Pool::add_task(hd);
			LOG_D("[sid=%d]", sid);
		}
		_handle_map[sid]->process(buf, sid);
		// LOG_D("<-- NodeServer [sid=%d], [size=%d]", sid, buf.size());
	}
	else {
		// CMD
		CmdId cmdid;
		if (buf.size() < sizeof(CmdId)) {
			// 错误的指令 因为不会导致传输崩溃，所以忽略就好了
			return;
		}
		buf >> cmdid;
		switch (cmdid) {
		case CMD::CMD_LOGIN_RE: {
			if (buf.size() != sizeof(Cid)) {
				return;
			}
			Cid cid;
			buf >> cid;
			if (cid) {
				LOG_I("登录成功~ 分配的cid: %d", cid);
			}
			else {
				LOG_E("登录失败cid被占用");
				stop();
			}
			break;
		}
		case CMD::CMD_SESSION_END: {
			if (buf.size() != sizeof(Sid)) return;
			Sid end_sid;
			buf >> end_sid;
			LOG_D("尝试断开 [sid=%d]", end_sid);
			if (_handle_map.find(end_sid) != _handle_map.end()) {
				_handle_map.erase(end_sid);
			}
			break;
		}
		default: {
			LOG_W("未知的命令号: %d", cmdid);
			buf.dump();
		}
		}
	}
}

void NodeClient::run() {
	// std::shared_ptr<char> recv_buf = std::shared_ptr<char>(new char[RECV_MAX]);
	char recv_buf[RECV_MAX];
	int ret = 0;
	while (_running) {
		ret = Recv(recv_buf, RECV_MAX);
		if (ret <= 0) {
			LOG_D("与服务端断开");
			reconnect();
			if (IsError()) {
				break;
			}
			continue;
		}
		_recv_buffer.push_back(recv_buf, ret);
		while (_recv_buffer.size() > (sizeof(PackSize) + sizeof(Sid))) {
			PackSize ps = *(PackSize*)_recv_buffer.data();
			if (ps <= (sizeof(PackSize) + sizeof(Sid))) {
				LOG_E("接收到错误的协议");
				_recv_buffer.dump();
				OnClose();
				return;
			}
			if (_recv_buffer.size() < ps) {
				break;
			}
			Sid sid;
			_recv_buffer >> ps >> sid;
			//if (sid > 5000 || ps > 5000) {
			//	_recv_buffer.dump();
			//}
			Buffer buf;
			buf.push_back(_recv_buffer.data(), ps - (sizeof(PackSize) + sizeof(Sid)));
			_recv_buffer.erase(0, ps - (sizeof(PackSize) + sizeof(Sid)));
			Handle(buf, sid);
		}
	}
}

void NodeClient::stop() {
	_running = false;
}