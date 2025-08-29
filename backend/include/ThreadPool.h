#pragma once

#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <future>
#include <functional>
#include <atomic>

class ThreadPool {
public:
    explicit ThreadPool(size_t threadCount);
    ~ThreadPool();

    // Submit a task and get a future result
    template<typename Func, typename... Args>
    auto submit(Func&& f, Args&&... args) -> std::future<decltype(f(args...))>;

private:
    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;

    std::mutex queueMutex_;
    std::condition_variable condition_;
    std::atomic<bool> stop_{ false };

    void workerThread();
};

inline ThreadPool::ThreadPool(size_t threadCount) {
    for (size_t i = 0; i < threadCount; ++i) {
        workers_.emplace_back([this] { workerThread(); });
    }
}

inline ThreadPool::~ThreadPool() {
    stop_ = true;
    condition_.notify_all();

    for (auto& thread : workers_) {
        if (thread.joinable()) thread.join();
    }
}

inline void ThreadPool::workerThread() {
    while (!stop_) {
        std::function<void()> task;
        {
            std::unique_lock lock(queueMutex_);
            condition_.wait(lock, [this] {
                return stop_ || !tasks_.empty();
            });
            if (stop_ && tasks_.empty()) return;
            task = std::move(tasks_.front());
            tasks_.pop();
        }
        task();
    }
}

template<typename Func, typename... Args>
auto ThreadPool::submit(Func&& f, Args&&... args)
    -> std::future<decltype(f(args...))> {
    using ReturnType = decltype(f(args...));

    auto taskPtr = std::make_shared<std::packaged_task<ReturnType()>>(
        std::bind(std::forward<Func>(f), std::forward<Args>(args)...)
    );

    std::future<ReturnType> res = taskPtr->get_future();
    {
        std::unique_lock lock(queueMutex_);
        tasks_.emplace([taskPtr]() { (*taskPtr)(); });
    }
    condition_.notify_one();
    return res;
}
