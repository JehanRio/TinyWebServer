#ifndef THREADPOOL_H
#define THREADPOOL_H
#include <pthread.h>
#include <list>
#include "locker.h"
#include "http_conn.h"

// 线程池类，定义成模板类是为了代码的复用，模板参数T就是任务类
template<typename T>
class threadpool
{
public:
    threadpool(int thread_num=8,int max_requests=10000);
    ~threadpool();
    bool append(T* request);    // 添加任务
    
private:
    static void* worker(void* arg);
    void run();

private:
    int m_thread_num;                   // 线程数
    pthread_t* m_threads;               // 线程池数组
    int m_max_request;                  // 请求队列中最多允许的，等待处理的请求数量
    std::list<T*> m_workqueue;          // 请求队列
    locker m_queuelocker;               // 互斥锁
    sem m_queuestat;                    // 信号量用来判断是否有任务需要处理
    bool m_stop;                        // 是否结束线程
};


#endif