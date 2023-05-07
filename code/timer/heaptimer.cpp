#include "heaptimer.h"

void HeapTimer::SwapNode_(size_t i, size_t j) {
    assert(i >= 0 && i <heap_.size());
    assert(j >= 0 && j <heap_.size());
    swap(heap_[i], heap_[j]);
    ref_[heap_[i].id] = i;    // 结点内部id所在索引位置也要变化
    ref_[heap_[j].id] = j;    
}

void HeapTimer::siftup_(size_t i) {
    assert(i >= 0 && i < heap_.size());
    size_t parent = (i-1) / 2;
    while(parent >= 0) {
        if(heap_[parent] > heap_[i]) {
            SwapNode_(i, parent);
            i = parent;
            parent = (i-1)/2;
        } else {
            break;
        }
    }
}

// false：不需要下滑  true：下滑成功
bool HeapTimer::siftdown_(size_t i, size_t n) {
    assert(i >= 0 && i < heap_.size());
    assert(n >= 0 && n <= heap_.size());    // n:共几个结点
    auto index = i;
    auto child = 2*index+1;
    while(child < n) {
        if(child+1 < n && heap_[child+1] < heap_[child]) {
            child++;
        }
        if(heap_[child] < heap_[index]) {
            SwapNode_(index, child);
            index = child;
            child = 2*child+1;
        }
        break;  // 需要跳出循环
    }
    return index > i;
}

// 删除指定位置的结点
void HeapTimer::del_(size_t index) {
    assert(index >= 0 && index < heap_.size());
    // 将要删除的结点换到队尾，然后调整堆
    size_t tmp = index;
    size_t n = heap_.size() - 1;
    assert(tmp <= n);
    // 如果就在队尾，就不用移动了
    if(index < heap_.size()-1) {
        SwapNode_(tmp, heap_.size()-1);
        if(!siftdown_(tmp, n)) {
            siftup_(tmp);
        }
    }
    ref_.erase(heap_.back().id);
    heap_.pop_back();
}

// 调整指定id的结点
void HeapTimer::adjust(int id, int newExpires) {
    assert(!heap_.empty() && ref_.count(id));
    heap_[ref_[id]].expires = Clock::now() + MS(newExpires);
    siftdown_(ref_[id], heap_.size());
}

void HeapTimer::add(int id, int timeOut, const TimeoutCallBack& cb) {
    assert(id >= 0);
    // 如果有，则调整
    if(ref_.count(id)) {
        int tmp = ref_[id];
        heap_[tmp].expires = Clock::now() + MS(timeOut);
        heap_[tmp].cb = cb;
        if(!siftdown_(tmp, heap_.size())) {
            siftup_(tmp);
        }
    } else {
        size_t n = heap_.size();
        ref_[id] = n;
        // 这里应该算是结构体的默认构造？
        heap_.push_back({id, Clock::now() + MS(timeOut), cb});  // 右值
        siftup_(n);
    }
}

// 删除指定id，并触发回调函数
void HeapTimer::doWork(int id) {
    if(heap_.empty() || ref_.count(id) == 0) {
        return;
    }
    size_t i = ref_[id];
    auto node = heap_[i];
    node.cb();  // 触发回调函数
    del_(i);
}

void HeapTimer::tick() {
    /* 清除超时结点 */
    if(heap_.empty()) {
        return;
    }
    while(!heap_.empty()) {
        TimerNode node = heap_.front();
        if(std::chrono::duration_cast<MS>(node.expires - Clock::now()).count() > 0) { 
            break; 
        }
        node.cb();
        pop();
    }
}

void HeapTimer::pop() {
    assert(!heap_.empty());
    del_(0);
}

void HeapTimer::clear() {
    ref_.clear();
    heap_.clear();
}

int HeapTimer::GetNextTick() {
    tick();
    size_t res = -1;
    if(!heap_.empty()) {
        res = std::chrono::duration_cast<MS>(heap_.front().expires - Clock::now()).count();
        if(res < 0) { res = 0; }
    }
    return res;
}