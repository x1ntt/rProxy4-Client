#ifndef THREAD_H
#define THREAD_H
/*
 *线程池的实现
 *by st
 *
 * */
#include "log.h"

#include <thread>
#include <queue>
#include <atomic>
#include <algorithm>
#include <mutex>
#include <memory>
#include <chrono>

#include <iostream>

namespace Thread {

	class Runnable {
	public:
		virtual bool run() { return false; };
	};

	class Pool {
	public:
		enum {
			THREAD_NUM = 50,
			MAX_QUEUE_SIZE = 100000
		};
		static void init() {
			running = true;
			for (int i = 0; i < THREAD_NUM; i++) {
				std::thread* t = new std::thread(exec_task, i);
				t->detach();
			}
		};
		static void stop() {
			running = false;
		};
		static void run() {}
		static void exec_task(int i) {
			while (running) {
				std::weak_ptr<Runnable> task;
				{
					std::unique_lock<std::mutex> kp(_tasks_mtx);
					while (_tasks.empty())
						_tasks_cv.wait(kp);
					task = fetch_task();
				}
				if (!task.expired()) {
					std::shared_ptr<Runnable> stask = task.lock();	// 此时因为没有加锁的关系 在别处被释放了，导致expired判断什么用也没有
					{
						std::unique_lock<std::mutex> kp(_debug_mtx);
						_debug_cnt++;
					}
					
					if (stask->run()) {	// 如果返回true则重新加回去 
						add_task(task);
					}
					{
						std::unique_lock<std::mutex> kp(_debug_mtx);
						_debug_cnt--;
					}
				}
				std::this_thread::sleep_for(std::chrono::milliseconds(1));
			}
		};

		static std::weak_ptr<Runnable> fetch_task() {
			std::weak_ptr<Runnable> tmp = _tasks.front();
			_tasks.pop();
			return tmp;
		};

		static void add_task(std::weak_ptr<Runnable> task) {
			std::unique_lock<std::mutex> kp(_tasks_mtx);
			if (_tasks.size() > MAX_QUEUE_SIZE) return;
			_tasks.push(task);
			_tasks_cv.notify_all();
		}

		~Pool() {
			stop();
		}
	private:
		static std::queue<std::weak_ptr<Runnable>> _tasks;
		static std::mutex _tasks_mtx;
		static std::condition_variable _tasks_cv;
		static bool running;
		static int _debug_cnt;
		static std::mutex _debug_mtx;
	};
};
#endif
