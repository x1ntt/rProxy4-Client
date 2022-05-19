#include "handler.h"

bool Handler::run() {
	if (_ts.use_count() == 0) {
		_ts = std::shared_ptr<TerminalServer>(new TerminalServer(_proxy_server, _proxy_port, weak_from_this()));
		if (_ts->IsError()) {
			LOG_W("终端服务器链接错误");
			// 关闭对象 通知请求端这个链接有错
			terminal_close();
			return false;
		}
		_ts->set_unblock();
		Thread::Pool::add_task(_ts);
	}

	std::unique_lock<std::mutex> kp(_send_mtx);

	if (_send_buff.size() == 0) return true;
	// LOG_D("--> Terminal [sid=%d], [size=%zd]", _sid, _send_buff.size());
	int ret = _ts->terminal_send(_send_buff);
	if (ret > 0) {
		_send_buff.erase(0, ret);
	}
	else {
		terminal_close();
		LOG_W("发送到terminal失败 %d", shared_from_this().use_count());
	}

	return true;
}

void Handler::send_to_server(Buffer& buf) {
	if (!_nc.expired()) {
		// LOG_D("--> NodeServer [sid=%d], [size=%zd]", _sid, buf.size());
		_nc.lock()->safe_send(Buffer() << _sid << buf);
	}
};

void Handler::terminal_close() {
	if (!_nc.expired()) {
		_nc.lock()->del_handler(_sid);
	}
}