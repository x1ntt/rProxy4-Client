#include "rpoll.h"
#include "type.h"
#include "buffer.h"
#include "rthread.h"
#include "cmd.h"
#include "nodeclient.h"


#include <iostream>
#include <memory>
#include <mutex>

using namespace std;


int main(){
	WSADATA wsaData;
	WSAStartup(MAKEWORD(2, 2), &wsaData);

	shared_ptr<NodeClient> nc = shared_ptr<NodeClient>(new NodeClient("101.42.104.214", 7201));
	if (nc->IsError()) {
		nc->reconnect();
		return -1;
	}
	Thread::Pool::init();
	nc->Auth(1000, "win11 2333");
	nc->run();
	Thread::Pool::stop();
	return 0;
}

// ts的生命周期还需要认真检查 还是会出现无限发sent to terminal的情况 =
// 要确认数据确实是发出去了 这点必须确认
// 考虑故意造成end_session风暴用来解决ts
// 因为到某一阶段会出现几个请求突然通了的情况，需要判断是否是什么东西遇到瓶颈了 或者锁冲突
// 服务端有很多找不到请求端sid的情况，需要考虑sid是什么时候关闭的