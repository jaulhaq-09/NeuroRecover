#include "thread_pool.h"

ThreadPool::ThreadPool(size_t numThreads) : stopping_(false), activeTasks_(0) {
    for (size_t i = 0; i < numThreads; ++i) {
        workers_.emplace_back([this] {
            while (true) {
                std::function<void()> task;
                {
                    std::unique_lock<std::mutex> lock(queueMutex_);
                    condition_.wait(lock, [this] {
                        return stopping_.load() || !tasks_.empty();
                    });
                    if (stopping_ && tasks_.empty()) return;
                    task = std::move(tasks_.front());
                    tasks_.pop();
                }
                task();
            }
        });
    }
}

ThreadPool::~ThreadPool() { stop(); }

void ThreadPool::stop() {
    stopping_ = true;
    condition_.notify_all();
    for (auto& w : workers_) if (w.joinable()) w.join();
}

void ThreadPool::waitAll() {
    while (activeTasks_ > 0 || !tasks_.empty())
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
}

size_t ThreadPool::pendingTasks() const {
    std::lock_guard<std::mutex> lock(queueMutex_);
    return tasks_.size();
}
