#include "threadpool.h"
#include "locker.h"
#include <exception>
#include <cstdio>

template<typename T>
threadpool<T>::threadpool(int thread_num, int max_requests):
    m_thread_num(thread_num), m_max_request(max_requests),
    m_stop(false), m_threads(nullptr)
{
    if((thread_num<=0) || (max_requests<=0))
        throw std::exception();

    m_threads = new pthread_t[m_thread_num];
    if(!m_threads)
        throw std::exception();
    
    // 创建thread_number个线程，并将它们设置为线程脱离
    for(int i=0;i<thread_num;i++)
    {
        printf("create the %d thread\n",i);
        if(pthread_create(m_threads+i,NULL,worker,this))
        {
            delete[] m_threads;
            throw std::exception();
        }
        if(pthread_detach(m_threads[i]))
        {
            delete[] m_threads;
            throw std::exception();
        }
    }
}

template<typename T>
threadpool<T>::~threadpool()
{
    delete[] m_threads;
    m_stop = true;
}

template<typename T>
bool threadpool<T>::append(T* request)
{
    m_queuelocker.lock();
    if(m_workqueue.size() > m_max_request)  // 已经超出最大值，报错
    {
        m_queuelocker.unlock();
        return false;
    }

    m_workqueue.push_back(request);
    m_queuelocker.unlock();
    m_queuestat.post(); // 任务信号量+1
    return true;    
}

// 子线程执行线程池中的任务（pool->run）
template<typename T>
void* threadpool<T>::worker(void* arg)
{
    threadpool* pool=(threadpool*)arg;
    pool->run();
    return pool;
}

template<typename T>
void threadpool<T>::run()
{
    while(!m_stop)
    {
        m_queuestat.wait(); // 任务信号量-1
        m_queuelocker.lock();
        // if(m_workqueue.empty)
        // {
        //     m_queuelocker.unlock();
        //     continue;
        // }
        T* request = m_workqueue.front();
        m_workqueue.pop_front();
        m_queuelocker.unlock();
        // if(!request)
        // {
        //     continue;
        // }
        request->process(); // 任务类的执行
    }
}

template class threadpool<http_conn>;