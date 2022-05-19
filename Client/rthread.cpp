#include "rthread.h"

namespace Thread{
	std::queue<std::weak_ptr<Runnable>> Pool::_tasks;
	std::mutex Pool::_tasks_mtx;
	std::condition_variable Pool::_tasks_cv;
	bool Pool::running;
	int Pool::_debug_cnt = 0;
	std::mutex Pool::_debug_mtx;
};
