#include "lst_timer.h"
#include <libgen.h>
#include <cstdio>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <assert.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <stdlib.h>

const int FD_LIMIT = 65535;
const int MAX_EVENT_NUMBER = 1024;
const int TIMESLOT = 5;

static int pipefd[2];
static sort_timer_lst timer_lst;
static int epollfd = 0;

// 设置非阻塞
int setnonblocking(int fd)
{
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

void addfd(int epollfd, int fd)
{
    epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLET;   // 边沿触发
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}

void sig_handler(int sig)
{
    int save_errno = errno;
    int msg = sig;
    send(pipefd[1], (char*)&msg, 1, 0); // 发送信号
    errno = save_errno;
}

void addsig(int sig)
{
    struct sigaction sa;
    memset(&sa,'\0',sizeof(sa));
    sa.sa_handler = sig_handler;
    sa.sa_flags |= SA_RESTART;  // SA_RESTART信号其实就是作用于当我程序进行运行的时候 这个当前的程序处于阻塞状态
    sigfillset(&sa.sa_mask);    // 若再有这个信号，则阻塞
    assert(sigaction(sig, &sa, nullptr) != -1);
}

void timer_handler()
{
    // 定时器处理函数，实际上就是调用tick()函数
    timer_lst.tick();   // 处理过期任务
    // 因为一次 alarm 调用只会引起一次SIGALARM 信号，所以我们要重新定时，以不断触发 SIGALARM信号。
    alarm(TIMESLOT);    // 计时5s
}

// 定时器回调函数，它删除非活动连接socket上的注册事件，并关闭之。
void cb_func(client_data* user_data)
{
    epoll_ctl(epollfd, EPOLL_CTL_DEL, user_data->sockfd, 0);
    assert(user_data);  // 如果该用户断开，则会报错
    close(user_data->sockfd);
    printf("close fd %d\n",user_data->sockfd);
}


int main(int argc, char** argv)
{
    if(argc <= 1)
    {
        printf("usage: %s port_number\n", basename(argv[0]));   //basename作用是得到特定的路径中的最后一个'/',后面的内容
        return 1;
    }
    int port = atoi(argv[1]);   // 获取端口号

    int ret = 0;
    sockaddr_in address;
    bzero(&address, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    address.sin_addr.s_addr = INADDR_ANY;

    int listenfd = socket(AF_INET, SOCK_STREAM, 0);
    assert(listenfd>=0);
    
    ret = bind(listenfd, (sockaddr*)&address, sizeof(address));
    assert(ret!=-1);

    ret = listen(listenfd, 5);
    assert(ret!=-1);

    epoll_event events[MAX_EVENT_NUMBER];   // 事件数组
    int epollfd = epoll_create(7);
    assert(epollfd);
    addfd(epollfd, listenfd);

    // 创建管道
    ret = socketpair(PF_UNIX, SOCK_STREAM, 0, pipefd);  // 本地套接字通信
    assert(ret!=-1);
    setnonblocking(pipefd[1]);  // 设置非阻塞
    addfd(epollfd, pipefd[0]);  // 将读添加到epoll对象中

    // 设置信号处理函数 
    addsig(SIGALRM);    // 定时
    addsig(SIGTERM);    // kill中断
    bool stop_server = false;   

    client_data* users = new client_data[FD_LIMIT]; // 最大连接数
    bool timeout = false;
    alarm(TIMESLOT);    // 定时开始

    while(!stop_server)
    {
        int num = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1); // 阻塞等待
        if(num < 0 && (errno != EINTR)) // EINTR：中断
        {
            printf("epoll failure!\n");
            break;
        }

        // 循环遍历事件数组
        for(int i=0;i<num;i++)
        {
            int sockfd = events[i].data.fd;
            if(sockfd == listenfd)
            {
                sockaddr_in cli_address;
                socklen_t len = sizeof(cli_address);
                int cfd = accept(listenfd, (sockaddr*)&cli_address, &len);
                addfd(epollfd, cfd);
                users[cfd].address = cli_address;
                users[cfd].sockfd = cfd;

                // 创建定时器，设置其回调函数与超时时间，然后绑定定时器与用户数据，最后将定时器添加到链表timer_lst中
                util_timer* timer = new util_timer;
                timer->user_data = &users[cfd];
                timer->cb_func = cb_func;   // 回调函数
                time_t cur = time(0);   // 当前时间记录
                timer->expire = cur + 3*TIMESLOT;
                users[cfd].timer = timer;
                timer_lst.add_timer(timer);
            }
            else if(sockfd == pipefd[0] && events[i].events & EPOLLIN)  // 检测到pipefd[0]的读事件
            {
                // 处理信号
                int sig;
                char signals[1024];
                ret = recv(pipefd[0], signals, sizeof(signals), 0);
                if(ret==-1 || ret==0)   // 异常或断开
                    continue;
                else
                {
                    for(int i=0;i<ret;i++)
                    {
                        switch(signals[i])
                        {
                            case SIGALRM:
                                // 用timeout变量标记有定时任务需要处理，但不立即处理定时任务
                                // 这是因为定时任务的优先级不是很高，我们优先处理其他更重要的任务。(I/O优先级跟高)
                                timeout = true;
                                break;
                            case SIGTERM:
                                stop_server = true; // kill
                        }
                    }
                }
            }
            else if(events[i].events & EPOLLIN) // 其他的读事件，则进行打印操作
            {
                memset(users[i].buf, '\0', BUFFER_SIZE);    // 清空读缓存区
                ret = recv(sockfd, users[i].buf, BUFFER_SIZE-1, 0);
                printf( "get %d bytes of client data %s from %d\n", ret, users[sockfd].buf, sockfd );
                util_timer* timer = users[sockfd].timer;
                if(ret < 0)
                {
                    // 如果发生读错误，则关闭连接，并移除其对应的定时器
                    if(errno != EAGAIN)
                    {
                        cb_func(&users[sockfd]);
                        if(timer)
                            timer_lst.del_timer(timer);
                    }
                }
                else if(ret == 0)
                {
                    // 如果对方已经关闭连接，则我们也关闭连接，并移除对应的定时器。
                    cb_func(&users[sockfd]);
                    if(timer)
                        timer_lst.del_timer(timer);
                }
                else
                {
                    // 如果某个客户端上有数据可读，则我们要调整该连接对应的定时器，以延迟该连接被关闭的时间。
                    if(timer)
                    {
                        time_t cur = time(0);
                        timer->expire = cur + 3*TIMESLOT;
                        printf( "adjust timer once\n" );
                        timer_lst.adjust_timer( timer );
                    }
                }
            }
        }
        // 最后处理定时事件，因为I/O事件有更高的优先级。当然，这样做将导致定时任务不能精准的按照预定的时间执行。
        if(timeout)
        {
            timer_handler();
            timeout = false;
        }
    }
    close(listenfd);
    close(pipefd[0]);
    close(pipefd[1]);
    delete[] users;
    return 0;
}