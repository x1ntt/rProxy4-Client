#ifndef __RPOLL_H
#define __RPOLL_H

#ifdef __linux
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#else
#define FD_SETSIZE 1024
#include <winsock2.h>
#include <WS2tcpip.h>
#pragma comment(lib,"ws2_32.lib")
#endif

#include "type.h"
#include "log.h"

#include <string.h>

#include <iostream>
#include <map>
#include <vector>
#include <mutex>
#include <algorithm>

namespace GNET {
    using std::string;
    using std::cout;
    using std::endl;
    using std::map;


    enum {
        MAX_CONNECT = 1024,
        LISTEN_COUNT = 50
    };
    class Poll;
    class BaseNet {
    protected:
        string _host;
        int _port;
        int _sock_fd;
        unsigned int _flag;
        struct sockaddr_in _addr;
        char* _buffer; // 接收缓冲区    // 记得初始化
        int _packet_size;  // 实际包大小   记得初始化
        int _packet_pos;    // 已经接收的实际大小
    public:
        enum {
            NET_ERROR = 0x0001,     // 连接失败
            NEED_DELETE = 0x0002    // 有这个标志的对象会在poll函数中被删除
        };

        // 基础包结构，只有数据大小头
        typedef struct {
            unsigned short int data_len;
            char data[];
        }BasePacket;

        int& get_sock() { return _sock_fd; };
        void set_sock(int sock) { _sock_fd = sock; };
        string& get_host() { return _host; };
        void set_host(string host) { _host = host; };
        int& get_port() { return _port; };
        void set_port(int port) { _port = port; };
        struct sockaddr_in& get_sockaddr_in() { return _addr; };
        void set_sockaddr_in(struct sockaddr_in& addr) { memcpy(&_addr, &addr, sizeof(addr)); };
        int set_unblock() {
            int iMode = 1;
            return ioctlsocket(_sock_fd, FIONBIO, (u_long FAR*) & iMode);
        };

        BaseNet() : _flag(0), _buffer(NULL), _packet_size(0), _packet_pos(0), _sock_fd(0), _port(0) {
            memset(&_addr, 0, sizeof(struct sockaddr_in));
        };
        BaseNet(string host, int port) :_host(host), _port(port), _flag(0),
            _buffer(NULL),
            _packet_size(0),
            _packet_pos(0),
            _sock_fd(0) {
            memset(&_addr, 0, sizeof(struct sockaddr_in));
        };
        virtual ~BaseNet() {
            /*if (_buffer){
                free(_buffer);
            }*/
        };

        virtual void OnRecv() {
            LOG_D("BaseNet的OnRecv()");
        };

        // 不在析构函数中关闭套接字是因为有时delete对象保留连接的需要 所以把对象和连接分离比较好
        void OnClose() {
            // Poll::deregister_poll(this); 
            // 还是把BaseNet和Poll分离开吧 
            // 也就select需要取消注册 epoll在套接字被close的时候就自己取消监听了
            if (_buffer) {
                free(_buffer);
                _buffer = NULL;
                // 当套接字被关闭的时候，缓冲区肯定没用了
                // 但是因为可能copy，所以不能在析构函数中free 
                // 否则会导致delete副本之后所有的对象_buffer失效
            }
#ifdef __linux
            close(_sock_fd);
#else
            closesocket(_sock_fd);
#endif
        };

        // 会接收一个连接，并且返回 BaseNet 对象指针
        BaseNet* Accept() {
            BaseNet* bn = new BaseNet();
#ifdef __linux
            socklen_t lt = sizeof(_addr);
#else
            int lt = sizeof(_addr);
#endif
            int sock;
            struct sockaddr_in& addr = bn->get_sockaddr_in();
            if ((sock = accept(_sock_fd, (struct sockaddr*)&(addr), &lt)) == -1) {
                delete bn;
                return NULL;
            }
            bn->set_sock(sock);
            bn->set_port(ntohs(addr.sin_port));
            char ip[128];
            inet_ntop(AF_INET, &addr.sin_addr, ip, sizeof(ip));
            bn->set_host(ip);
            return bn;
        }

        int Recv(char* data, size_t len) {
            int ret = 0;
#ifdef __linux
            do {
                ret = recv(_sock_fd, data, len, 0);
            } while (ret == -1 && errno == EINTR);    // windows 大概是没有errno
#else
            ret = recv(_sock_fd, data, len, 0);
#endif
            return ret;
        }

        // 保证接受完 len 长度的数据才返回
        int RecvN(char* data, size_t len) {
            int ret = 0;
            int real_recv = 0;
            do {
                ret = Recv(data + real_recv, len - real_recv);
                if (ret <= 0) { return ret; }
                real_recv += ret;
            } while (real_recv != len);
            return real_recv;
        };

        int Send(const char* data, size_t len) {
            int ret = 0;
            ret = send(_sock_fd, data, len, 0);
            //if (ret != len && ret > 0) {
            //    printf("[!!!!!!!!] Send:  ret/len: %d/%d", ret, (int)len);
            //}
            return ret;
        }

        // 不发送完len长度的数据绝不返回 除非接收错误
        int SendN(const char* data, size_t len) {
            int ret = 0;
            int real_send = 0; // 总共发送的数据
            do {
                ret = Send(data + real_send, len - real_send);
                if (ret <= 0) { return ret; }
                real_send += ret;
            } while (real_send != len);
            return real_send;
        }

        unsigned int get_flag() { return _flag; };
        void SetError() { _flag |= NET_ERROR; }
        bool IsError() { return _flag & NET_ERROR; };
        void ClearError() { _flag = ~NET_ERROR & _flag; };
        void SetDelete() { _flag |= NEED_DELETE; }
        bool IsDelete() { return _flag & NEED_DELETE; };

    };

    class Passive : public BaseNet {
    public:
        Passive(string host, int port) :BaseNet(host, port) {
            _sock_fd = socket(AF_INET, SOCK_STREAM, 0);
            if (_sock_fd < 0) {
                LOG_E("创建服务套接字失败");
                SetError();
                return;
            }
            memset(&_addr, 0, sizeof(_addr));
            if (inet_pton(AF_INET, _host.c_str(), &(_addr.sin_addr)) <= 0) {
                LOG_E("主机地址有错误");
                SetError();
                return;
            }
            _addr.sin_family = AF_INET;
            _addr.sin_port = htons(_port);
            if (bind(_sock_fd, (sockaddr*)&_addr, sizeof(_addr)) < 0) {
                LOG_E("绑定错误");
                SetError();
                return;
            }
            listen(_sock_fd, LISTEN_COUNT); // 需要错误处理
            LOG_D("开始监听, %s:%d", _host.c_str(), _port);
        };
        virtual void OnRecv() {
            LOG_D("Passivs的虚方法");
        };
    };

    class Active : public BaseNet {
    public:
        Active(string host, int port) : BaseNet(host, port) {
            _reconnect_count = 0;

            _sock_fd = socket(AF_INET, SOCK_STREAM, 0);
            if (_sock_fd < 0) {
                LOG_E("创建连接套接字失败");
                SetError();
                return;
            }
            memset(&_addr, 0, sizeof(_addr));
            _addr.sin_family = AF_INET;
            _addr.sin_port = htons(_port);
            if (inet_pton(AF_INET, _host.c_str(), &_addr.sin_addr) <= 0) {
                LOG_E("主机地址有错误 ");
                SetError();
                return;
            }
            if (connect(_sock_fd, (struct sockaddr*)&_addr, sizeof(_addr)) < 0) {
                LOG_E("连接错误");
                SetError();
                return;
            }
            // printf("[Info]: 连接成功: %s:%d\n", _host.c_str(), _port);
        }

        bool reconnect(int max_reconnect_count = 3) {
            LOG_I("重连服务器...");
            int ret = 0;
            ClearError();
            while (((ret = connect(_sock_fd, (struct sockaddr*)&_addr, sizeof(_addr))) < 0) && (_reconnect_count < max_reconnect_count)) {
                _reconnect_count++;
                LOG_I("重连失败 %d", _reconnect_count);
            }
            if (ret < 0) {
                SetError();
                return false;
            }
            _reconnect_count = 0;
            ClearError();
            return true;
        }
    private:
        us16 _reconnect_count;
    };
};

#endif
