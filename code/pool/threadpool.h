#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <mutex>
#include <condition_variable>
#include <queue>
#include <thread>
#include <functional>
#include <assert.h>
class ThreadPool {
public:
    explicit ThreadPool(size_t threadCount = 8): pool_(std::make_shared<Pool>()) {  // make_shared:传递右值，功能是在动态内存中分配一个对象并初始化它，返回指向此对象的shared_ptr
            assert(threadCount > 0);
            for(size_t i = 0; i < threadCount; i++) {
                std::thread([this] {    // 若采用&，需要this->pool_->mtx
                    std::unique_lock<std::mutex> locker(pool_->mtx);
                    while(true) {
                        if(!pool_->tasks.empty()) {
                            auto task = std::move(pool_->tasks.front());    // 左值变右值,资产转移
                            pool_->tasks.pop();
                            locker.unlock();
                            task(); // 处理任务：task是一个可调用对象
                            locker.lock();
                        } 
                        else if(pool_->isClosed) break;
                        else pool_->cond.wait(locker);
                    }
                }).detach();
            }
    }

    ThreadPool() = default;

    ThreadPool(ThreadPool&&) = default;
    
    ~ThreadPool() {
        if(static_cast<bool>(pool_)) {
            {
                std::lock_guard<std::mutex> locker(pool_->mtx);
                pool_->isClosed = true;
            }
            pool_->cond.notify_all();
        }
    }

    template<class F>
    void AddTask(F&& task) {
        {
            std::lock_guard<std::mutex> locker(pool_->mtx);
            pool_->tasks.emplace(std::forward<F>(task));    // 完美转发，防止task变为左值
        }
        pool_->cond.notify_one();
    }

private:
    struct Pool {
        std::mutex mtx;
        std::condition_variable cond;
        bool isClosed;
        std::queue<std::function<void()>> tasks;    // 工作队列
    };
    std::shared_ptr<Pool> pool_;
};


#endif //THREADPOOL_H