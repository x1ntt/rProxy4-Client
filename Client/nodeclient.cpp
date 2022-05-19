#include "nodeclient.h"

void NodeClient::del_handler(Sid sid) {
	std::unique_lock<std::mutex> kp(_handle_mtx);
	if (_handle_map.find(sid) != _handle_map.end()) {	// handle����������û�н��������Ի�ts������
		LOG_D("[sid=%d] ����, use: %d", sid, _handle_map[sid].use_count());
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
		SendN(send_buf.data(), send_buf.size()); // ��Ϊû���û����ķ��ͻ���������Ҫ��֤���ݰ������Ļ�������Ҫȷ���������
		// send_buf.dump();
	}
}

bool NodeClient::Auth(us16 cid, std::string note) {
	// ��ʱȥ����������֤
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
			// �����ָ�� ��Ϊ���ᵼ�´�����������Ժ��Ծͺ���
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
				LOG_I("��¼�ɹ�~ �����cid: %d", cid);
			}
			else {
				LOG_E("��¼ʧ��cid��ռ��");
				stop();
			}
			break;
		}
		case CMD::CMD_SESSION_END: {
			if (buf.size() != sizeof(Sid)) return;
			Sid end_sid;
			buf >> end_sid;
			LOG_D("���ԶϿ� [sid=%d]", end_sid);
			if (_handle_map.find(end_sid) != _handle_map.end()) {
				_handle_map.erase(end_sid);
			}
			break;
		}
		default: {
			LOG_W("δ֪�������: %d", cmdid);
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
			LOG_D("�����˶Ͽ�");
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
				LOG_E("���յ������Э��");
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