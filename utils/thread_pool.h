#pragma once
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <future>
#include <atomic>
#include <stdexcept>

class ThreadPool {
public:
    explicit ThreadPool(size_t numThreads);
    ~ThreadPool();

    template<class F, class... Args>
    auto enqueue(F&& f, Args&&... args) -> std::future<typename std::result_of<F(Args...)>::type>;

    void waitAll();
    size_t pendingTasks() const;
    void stop();

private:
    std::vector<std::thread>          workers_;
    std::queue<std::function<void()>> tasks_;
    mutable std::mutex                queueMutex_;
    std::condition_variable           condition_;
    std::atomic<bool>                 stopping_;
    std::atomic<size_t>               activeTasks_;
};

// ─── Template implementation ─────────────────────────────────────────────────
template<class F, class... Args>
auto ThreadPool::enqueue(F&& f, Args&&... args)
    -> std::future<typename std::result_of<F(Args...)>::type>
{
    using RetType = typename std::result_of<F(Args...)>::type;
    auto task = std::make_shared<std::packaged_task<RetType()>>(
        std::bind(std::forward<F>(f), std::forward<Args>(args)...));
    std::future<RetType> res = task->get_future();
    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        if (stopping_) throw std::runtime_error("ThreadPool is stopped");
        ++activeTasks_;
        tasks_.emplace([task, this]() {
            (*task)();
            --activeTasks_;
        });
    }
    condition_.notify_one();
    return res;
}
