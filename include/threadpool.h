#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <mutex>
#include <condition_variable>
#include <queue>
#include <thread>
#include <functional>
#include <cassert>

class ThreadPool {
public:
    explicit ThreadPool(size_t threadCount = 8);
    ThreadPool() = default;
    ThreadPool(ThreadPool&&) = default;
    ~ThreadPool();

    // 模板要在头文件实现
    template<class F>
    void AddTask(F&& task) {
        {
            std::lock_guard<std::mutex> locker(pool_->mtx);

            // 添加任务到任务队列
            pool_->tasks.emplace(std::forward<F>(task));
        }

        // 通知一个线程来取任务
        pool_->cond.notify_one();
    }

private:
    // 内部定义
    struct Pool {
        std::mutex mtx;
        std::condition_variable cond;
        bool isClosed;
        std::queue<std::function<void()>> tasks;  // 队列（保存的是任务）
    };
    std::shared_ptr<Pool> pool_;  // 池子
};

#endif