// ##控制不同服务器的开关宏
#define SERVER_3

#ifdef  SERVER_1 //原生
#include <cstdio>
#include <iostream>
#include <cstring>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

#define MAX_EVENTS 10
#define PORT 8888

int main() {
    int listen_fd, conn_fd, epoll_fd, event_count;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(client_addr);
    struct epoll_event events[MAX_EVENTS], event;

    // 创建监听套接字
    if ((listen_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
        perror("socket");

    // 设置端口重用
    const int opt = 1;
    if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1)
        perror("setsockopt");

    // 设置服务器地址和端口
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    // 绑定监听套接字到服务器地址和端口
    if (bind(listen_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1)
        perror("bind");

    // 监听连接
    if (listen(listen_fd, 5) == -1)
        perror("listen");

    // 创建 epoll 实例
    if ((epoll_fd = epoll_create1(0)) == -1)
        perror("epoll_create1");

    // 添加监听套接字到 epoll 实例中, 默认水平触发
    event.events  = EPOLLIN; 
    event.data.fd = listen_fd;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, listen_fd, &event) == -1)
        perror("epoll_ctl");

    while (true) {
        // 等待事件发生
        event_count = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        if (event_count == -1){
            perror("socket");
            return -1;
        }
        // 处理事件
        for (int i = 0; i < event_count; i++) {
            // 如果事件对应的文件描述符是监听套接字，表示有新连接到达
            if (events[i].data.fd == listen_fd) {
                conn_fd = accept(listen_fd, (struct sockaddr *)&client_addr, &addr_len);
                if (conn_fd == -1) {
                    perror("accept");
                    continue;
                }
                // 将新连接的套接字添加到 epoll 实例中
                event.events  = EPOLLIN;
                event.data.fd = conn_fd;
                if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, conn_fd, &event) == -1)
                    perror("epoll_ctl");
            } else {
                // 如果事件对应的文件描述符不是监听套接字，表示有数据可读
                char buf[1024];
                int len = read(events[i].data.fd, buf, sizeof(buf) - 1);
                if (len <= 0) {
                    // 发生错误或连接关闭，关闭连接
                    close(events[i].data.fd);
                } else {
                    buf[len] = '\0';
                    // 处理接收到的消息
                    std::cout << "接收到消息：" << buf << std::endl;
                    // 发送回声消息给客户端
                    // write(events[i].data.fd, buf, len);
                    close(events[i].data.fd);
                }
            }
        }
    }
    // 关闭监听套接字和 epoll 实例
    close(listen_fd);
    close(epoll_fd);
    return 0;
}

#endif

#ifdef SERVER_2

#include "../src/fiber/include/monsoon.h"
#include <cstdio>
#include <cstring>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>
static int sock_listen_fd = -1;
void test_accept();
void error(const char *msg) {
    printf("errno:%d\n",errno);
    perror(msg);
    printf("erreur...\n");
    exit(1);
}
void watch_io_read() {
    monsoon::IOManager::GetThis()->addEvent(sock_listen_fd, monsoon::Event::READ, test_accept);
}
// 接受连接的函数。
void test_accept() {
    struct sockaddr_in addr; // 可能是sockaddr_un结构，用于UNIX域套接字。
    memset(&addr, 0, sizeof(addr)); // 清零地址结构体。
    socklen_t len = sizeof(addr); // 地址结构体的大小。
    int fd = accept(sock_listen_fd, (struct sockaddr *)&addr, &len); // 接受连接。
    if (fd < 0) {
        // 接受失败时，输出错误。
        std::cout << "fd = " << fd << "accept false" << std::endl;
    } else {
        // 设置文件描述符为非阻塞模式。
        fcntl(fd, F_SETFL, O_NONBLOCK);
        // 为新接受的连接添加一个读取事件。
        monsoon::IOManager::GetThis()->addEvent(fd, monsoon::READ, [fd]() {
            char buffer[1024];
            memset(buffer, 0, sizeof(buffer)); // 清零缓冲区。
            while (true) {
                // 接收数据。
                int ret = recv(fd, buffer, sizeof(buffer), 0);
                if (ret > 0) {
                    // 如果接收到数据，就将其发送回客户端。
                    ret = send(fd, buffer, ret, 0);
                }
                if (ret <= 0) {
                    // 如果recv返回0，表示客户端已关闭连接；如果返回负值，表示出错。
                    if (errno == EAGAIN) // 如果错误是EAGAIN，则继续循环。
                        continue;
                    break; // 否则退出循环。
                }
                // 关闭文件描述符，因为这是一个短连接服务器，处理完即关闭。
                close(fd);
            }
        });
    }
    // 重新调度watch_io_read函数，以便继续监听新的连接请求。
    monsoon::IOManager::GetThis()->scheduler(watch_io_read);
}

void test_iomanager() {
    int portno = 12345; // 服务器监听的端口号。
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    // 创建监听套接字。
    sock_listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_listen_fd < 0) {
        // 创建套接字失败时，输出错误并退出。
        const char *msg = "Error creating socket..";
        error(msg);
    }
    int yes = 1;
    // 设置套接字选项，避免地址已被使用的错误。
    setsockopt(sock_listen_fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    memset((char *)&server_addr, 0, sizeof(server_addr)); // 清零地址结构体。
    server_addr.sin_family = AF_INET; // 设置地址类型为IPv4。
    server_addr.sin_port = htons(portno); // 设置端口号。
    server_addr.sin_addr.s_addr = INADDR_ANY; // 设置IP地址为任意。

    // 绑定套接字到地址。
    if (bind(sock_listen_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
        error("Error binding socket..\n");
    // 监听套接字。
    if (listen(sock_listen_fd, 1024) < 0) {
        error("Error listening..\n");
    }
    // 输出服务器启动信息。
    printf("epoll echo server listening for connections on port: %d\n", portno);

    // 将监听套接字设置为非阻塞模式。
    fcntl(sock_listen_fd, F_SETFL, O_NONBLOCK);
    // 创建IOManager实例。
    monsoon::IOManager iom;
    // 添加一个读取事件，以便当有新连接时可以调用test_accept。
    iom.addEvent(sock_listen_fd, monsoon::READ, test_accept);
}

int main(int argc, char *argv[]) {
    test_iomanager();
    return 0;
}


#endif

#ifdef SERVER_3
 /**
 /**
  * @file test_tcp_server.cc
  * @brief 基于libevent库实现tcp_server
  * @version 0.1
  * @date 2024-04-02
  */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <event2/event.h>
#include <event2/bufferevent.h>
#include <event2/buffer.h>

#define LISTEN_PORT 8000
#define LIATEN_BACKLOG 32

/*********************************************************************************
*                   函数声明
**********************************************************************************/
//accept回调函数
void do_accept_cb(evutil_socket_t listener, short event, void *arg);
//read 回调函数
void read_cb(struct bufferevent *bev, void *arg);
//error回调函数
void error_cb(struct bufferevent *bev, short event, void *arg);
/*                   函数体
**********************************************************************************/
//accept回调函数
void do_accept_cb(evutil_socket_t listener, short event, void *arg)
{
  //传入的event_base指针
  struct event_base *base = (struct event_base*)arg;
  
  struct sockaddr_in sin; //声明地址
  socklen_t slen = sizeof(sin);  //地址长度声明
  
  //接收客户端，并返回新的套接字conn_fd用于通讯
  evutil_socket_t  conn_fd= accept(listener, (struct sockaddr *)&sin, &slen);
  if (conn_fd< 0)
  {
    perror("error accept");
    return;
  }
  printf("new client[%u]  connect success \n", conn_fd);
  
  //使用新的连接套接字conn_fd建立bufferevent，并将bufferevent也安插到base上
  struct bufferevent *bev = bufferevent_socket_new(base, conn_fd, BEV_OPT_CLOSE_ON_FREE);
  bufferevent_setcb(bev, read_cb, NULL, error_cb, arg); //设置回调函数read_cb \ error_cb
  bufferevent_enable(bev, EV_READ | EV_PERSIST);//启用永久read/write
  //bufferevent_enable(bev,EV_WRITE); //默认情况下，EV_WRITE是开启的
}
//read 回调函数
void read_cb(struct bufferevent *bev, void *arg)
{  
  struct evbuffer* input = bufferevent_get_input(bev); //获取读evbuffer的情况
  int n = evbuffer_get_length(input); //获取evbuffer中字节个数

  char buf[1024];
  bufferevent_read(bev,buf,n); //从缓冲区中读走n个字节的数据
  buf[n] = '\0';
  printf("read message = %s",buf);
  
  bufferevent_write(bev,buf,n); //回射信息给client
}
//error回调函数
void error_cb(struct bufferevent *bev, short event, void *arg)
{
  //通过传入参数bev找到socket fd
  evutil_socket_t fd = bufferevent_getfd(bev);
  //cout << "fd = " << fd << endl;
  if (event & BEV_EVENT_TIMEOUT)
  {
    printf("Timed out\n"); //if bufferevent_set_timeouts() called
  }
  else if (event & BEV_EVENT_EOF)
  {
    printf("connection closed\n");
  }
  else if (event & BEV_EVENT_ERROR)
  {
    printf("some other error\n");
  }
  bufferevent_free(bev);
}
 
int main()
{
  evutil_socket_t listener;
  listener = socket(AF_INET, SOCK_STREAM, 0);
  evutil_make_listen_socket_reuseable(listener);
  
  struct sockaddr_in sin;
  sin.sin_family = AF_INET;
  sin.sin_addr.s_addr = 0;
  sin.sin_port = htons(LISTEN_PORT);
  if (bind(listener, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
    perror("bind");
    return 1;
  }
  if (listen(listener, 1000) < 0) {
    perror("listen");
    return 1;
  }
  
  printf("Listening...\n");
  
  evutil_make_socket_nonblocking(listener);
  
  struct event_base *base = event_base_new();
  //监听套接字listener可读事件：一旦有client连接，就会调用do_accept_cb回调函数
  struct event *listen_event = event_new(base, listener, EV_READ | EV_PERSIST, do_accept_cb, (void*)base);

  event_add(listen_event, NULL);
  event_base_dispatch(base);

  return 0;
}

#endif