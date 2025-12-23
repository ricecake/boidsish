#pragma once

#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <future>
#include <functional>
#include <memory>
#include <stdexcept>

#include "task.h"

namespace Boidsish {

// Custom comparator for the priority queue to handle std::shared_ptr<Task>
struct CompareTasks {
    bool operator()(const std::shared_ptr<Task>& a, const std::shared_ptr<Task>& b) const {
        return *a < *b;
    }
};

class ThreadPool {
public:
    ThreadPool(size_t threads);
    ~ThreadPool();

    template<class F, class... Args>
    auto enqueue(TaskPriority priority, F&& f, Args&&... args)
        -> TaskHandle<typename std::invoke_result_t<F, Args...>>;

private:
    void worker_loop();

    std::vector<std::thread> workers_;
    std::priority_queue<std::shared_ptr<Task>, std::vector<std::shared_ptr<Task>>, CompareTasks> tasks_;

    std::mutex queue_mutex_;
    std::condition_variable condition_;
    std::atomic<bool> stop_;
};

// Template implementation must be in the header file
template<class F, class... Args>
auto ThreadPool::enqueue(TaskPriority priority, F&& f, Args&&... args)
    -> TaskHandle<typename std::invoke_result_t<F, Args...>> {
    using return_type = typename std::invoke_result_t<F, Args...>;

    auto packaged_task = std::make_shared<std::packaged_task<return_type()>>(
        std::bind(std::forward<F>(f), std::forward<Args>(args)...)
    );

    TaskHandle<return_type> handle;
    handle.future = packaged_task->get_future();

    {
        std::unique_lock<std::mutex> lock(queue_mutex_);

        if(stop_) {
            throw std::runtime_error("enqueue on stopped ThreadPool");
        }

        auto task_func = [packaged_task](){ (*packaged_task)(); };
        handle.task = std::make_shared<Task>(task_func, priority);
        tasks_.emplace(handle.task);
    }

    condition_.notify_one();
    return handle;
}

} // namespace Boidsish
