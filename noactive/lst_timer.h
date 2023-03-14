#ifndef LST_TIMER
#define LST_TIMER

#include <stdio.h>
#include <time.h>
#include <arpa/inet.h>

const int BUFFER_SIZE = 64;
class util_timer;       // 前向声明

// 用户数据结构
struct client_data
{
    sockaddr_in address;        // 客户端socket地址
    int sockfd;                 // socket文件描述符
    char buf[BUFFER_SIZE];      // 读缓存
    util_timer* timer;          // 定时器
};

// 定时器类
class util_timer
{
public:
    util_timer();

    time_t expire;      // 任务超时时间，这里使用绝对时间
    void (*cb_func)(client_data*);  // 任务回调函数，回调函数处理的客户数
    client_data* user_data;
    util_timer* prev;   // 指向前一个定时器
    util_timer* next;   // 指向后一个定时器
};
  
class sort_timer_lst
{
public:
    sort_timer_lst();
    ~sort_timer_lst();
    void add_timer(util_timer* timer);
    void adjust_timer(util_timer* timer);
    void del_timer(util_timer* timer);
    void tick();
private:
    util_timer* head;       // 头结点，超时时间最小
    util_timer* tail;       // 尾结点
    void add_timer(util_timer* timer, util_timer* lst_head);
};
#endif