#include "../include/threadpool.h"
#include <cassert>

// 使用线程池创建子线程
ThreadPool::ThreadPool(size_t threadCount) : pool_(std::make_shared<Pool>()) {
    assert(threadCount > 0);

    // 创建 threadCount 个子线程
    for(size_t i = 0; i < threadCount; i++) {
        std::thread([pool = pool_] {
            std::unique_lock<std::mutex> locker(pool->mtx);
            while(true) {
                if(!pool->tasks.empty()) {
                    // 从任务队列中取一个任务
                    auto task = std::move(pool->tasks.front());

                    // 移除该任务
                    pool->tasks.pop();

                    locker.unlock();

                    // 执行该任务
                    task();

                    locker.lock();
                } 
                else if(pool->isClosed) {
                    break;
                }
                else {
                    // 等待任务
                    pool->cond.wait(locker);
                }
            }
        }).detach();  // 分离子线程
    }
}

// 析构函数，关闭线程池并通知所有线程
ThreadPool::~ThreadPool() {
    if(static_cast<bool>(pool_)) {
        {
            std::lock_guard<std::mutex> locker(pool_->mtx);
            pool_->isClosed = true;
        }
        pool_->cond.notify_all();
    }
}
