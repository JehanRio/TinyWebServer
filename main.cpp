#include <cstdio>
#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <signal.h>
#include "http_conn.h"
#include "threadpool.h"


const int MAX_FD = 65535;   // 最大的文件描述符个数
const int MAX_EVENT_NUMBER = 10000;  //监听的最大事件数量
// 添加信号捕捉
void addsig(int sig, void(handler)(int))
{
    struct sigaction sa;
    memset(&sa,'\0',sizeof(sa));
    sa.sa_handler = handler;
    sigfillset(&sa.sa_mask);    // 阻塞
    sigaction(sig, &sa, NULL);
}

// 添加文件描述符到epoll中
extern void addfd(int epollfd, int fd, bool one_shot);
// 从epoll中删除文件描述符
extern void removefd(int epollfd, int fd);

// 修改文件描述符
extern void modfd(int epollfd, int fd, int ev);

int main(int argc,char** argv)
{
    if(argc <= 1)
    {
        printf("按照如下格式运行：%s port_number\n",basename(argv[0]));
        exit(-1);
    }

    // 获取端口号
    int port = atoi(argv[1]);

    // 对SIGPIPE信号处理
    addsig(SIGPIPE,SIG_IGN);    // SIGPIPE:客户端关闭;SIG_IGN忽略信号 表示忽略该退出信号

    // 创建线程池，初始化线程池z
    threadpool<http_conn> *pool=nullptr;
    try
    {
        pool = new threadpool<http_conn>;
    }
    catch(...)
    {
        exit(-1);
    }
        
    // 创建一个数组用于保存所有的用户的客户端信息
    http_conn* users = new http_conn[MAX_FD];

    // 创建监听的套接字
    int listenfd = socket(AF_INET, SOCK_STREAM, 0);
    
    // 设置端口复用
    int reuse = 1;
    setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    // 绑定
    sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);
    bind(listenfd,(sockaddr*)&address, sizeof(address));

    // 监听
    listen(listenfd, 5);

    // 创建epoll对象,事件数组，添加监听文件描述符
    epoll_event events[MAX_EVENT_NUMBER];
    int epollfd = epoll_create(7);

    // 将监听的文件描述符添加到epoll对象中
    addfd(epollfd, listenfd, false);    // 监听描述符都为水平触发（因为要接收新的socket）
    http_conn::m_epollfd = epollfd;

    while(1)
    {
        int num = epoll_wait(epollfd, events, MAX_EVENT_NUMBER,-1);   // 阻塞等待;检测到了几个事件
        if(num < 0 && (errno != EINTR)) // EINTR：中断
        {
            printf("epoll failure!\n");
            break;
        }

        // 循环遍历事件数组
        for(int i=0;i<num;i++)
        {
            int sockfd = events[i].data.fd;
            if(sockfd == listenfd)  // 有客户端连接进来
            {
                sockaddr_in client_address;
                socklen_t len = sizeof(client_address);
                int cfd = accept(listenfd, (struct sockaddr*)&client_address, &len);
                
                if(http_conn::m_user_count >= MAX_FD)   // 目前连接数满了,
                {
                    close(cfd);
                    continue;
                }
                // 将新的客户数据初始化，放入数组中
                users[cfd].init(cfd, client_address);
            }
            else if(events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR))  // RDHUP: 读关闭；HUP:读写都关闭
            {
                // 对方异常断开或者错误等事件
                users[sockfd].close_conn();
            }
            else if(events[i].events & EPOLLIN) // 读事件发生
            {
                if(users[sockfd].read())    // 一次性把所有数据都读完
                {
                    pool->append(users+sockfd);
                }
                else
                {
                    users[sockfd].close_conn(); // 读失败
                }
            }
            else if(events[i].events & EPOLLOUT)    //写事件发生
            {
                if(!users[sockfd].write())  // 一次性写完所有数据 失败
                {
                    users[sockfd].close_conn(); // 写失败;或m_linger=false,不保持连接
                }
            }
        }
    }
    close(epollfd);
    close(listenfd);
    delete[] users;
    delete pool;

    return 0;
}