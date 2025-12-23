#include "thread_pool.h"

namespace Boidsish {

ThreadPool::ThreadPool(size_t threads) : stop_(false) {
    for(size_t i = 0; i < threads; ++i) {
        workers_.emplace_back([this] {
            this->worker_loop();
        });
    }
}

void ThreadPool::worker_loop() {
    while(true) {
        std::shared_ptr<Task> task;
        {
            std::unique_lock<std::mutex> lock(this->queue_mutex_);
            this->condition_.wait(lock, [this] {
                return this->stop_ || !this->tasks_.empty();
            });

            if(this->stop_) {
                return;
            }

            task = this->tasks_.top();
            this->tasks_.pop();
        }

        if(!task->cancelled) {
            task->func();
        } else {
            task->func = nullptr;
        }
    }
}

ThreadPool::~ThreadPool() {
    {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        stop_ = true;
    }
    condition_.notify_all();
    for(std::thread &worker: workers_) {
        worker.join();
    }
}

} // namespace Boidsish
