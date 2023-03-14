#include "lst_timer.h"

util_timer::util_timer():prev(nullptr),next(nullptr){}

sort_timer_lst::sort_timer_lst():head(nullptr),tail(nullptr){}

sort_timer_lst::~sort_timer_lst()
{
    util_timer* tmp = head;
    while(tmp)
    {
        head = tmp->next;
        delete tmp;
        tmp = head;
    }
}

// 将定时器timer添加到链表中
void sort_timer_lst::add_timer(util_timer* timer)
{
    if(!timer)
        return;
    if(!head)
    {
        head = tail = timer;
        return;
    }
    // 若目标定时器超时时间最小，则放在头，否则调用重载函数add_timer()，放在中间合适的位置
    if(timer->expire < head->expire)
    {
        timer->next = head;
        head->prev = timer;
        head = timer;
        return;
    }
    add_timer(timer, head);
}

void sort_timer_lst::add_timer(util_timer* timer, util_timer* lst_head)
{
    util_timer* prev = lst_head;
    util_timer* tmp = prev->next;
    while(tmp)
    {
        if(timer->expire < tmp->expire)
        {
            prev->next = timer;
            timer->next = tmp;
            tmp->prev = timer;
            timer->prev = prev;
            return;
        }
        prev=tmp;
        tmp=tmp->next;
    }
    // 到尾部了
    prev->next = timer;
    timer->next = nullptr;
    timer->prev = prev;
    tail = timer;   
}

// 当某个定时任务发生变化，需要调整位置（只考虑延长时间）
void sort_timer_lst::adjust_timer(util_timer* timer)
{
    if(!timer)
        return;
    decltype(timer) tmp = timer->next;
    // 如果在尾结点（本身就是最长的）或新的超时时间小于后面的超时时间，则不需要调整
    if(!tmp || timer->expire < tmp->expire)
        return;
    if(timer == head)
    {
        head = head->next;  // 取出头部
        head->prev = nullptr;
        timer->next = nullptr;
        add_timer(timer, head);
    }
    else
    {
        timer->prev->next = timer->next;
        timer->next->prev = timer->prev;
        add_timer(head,timer->next);
    }
}

void sort_timer_lst::del_timer(util_timer* timer)
{
    if(!timer)
        return;
    // 若只有一个定时器
    if(timer == head && timer == tail)
    {
        delete timer;
        head=tail=nullptr;
        return;
    }
    if(timer == head)
    {
        head = head->next;
        head->prev = nullptr;
        delete timer;
        return;
    }
    if(timer == tail)
    {
        tail = tail->prev;
        tail->next = nullptr;
        delete timer;
        return;
    }
    timer->prev->next = timer->next;
    timer->next->prev = timer->prev;
    return;
}

// SIGALARM信号每次被触发就在其信号处理函数中执行一次tick函数，以处理链表上的到期任务
void sort_timer_lst::tick()
{
    if(!head)
        return;
    printf("timer tick\n");
    time_t cur = time(nullptr);    //获取当前系统时间
    auto tmp = head;
    // 从头节点开始依次处理每个定时器，直到遇到一个尚未到期的定时器
    while(tmp)
    {
        // 未超时
        if(cur < tmp->expire)
            break;
        // 调用定时器的回调函数，以执行定时任务
        tmp->cb_func(tmp->user_data);
        // 执行完定时器中的任务之后，就将他从链表中删除（关闭连接），并重置链表
        head = head->next;
        if(head)
            head->prev = nullptr;
        delete tmp;
        tmp = head;
    }
}