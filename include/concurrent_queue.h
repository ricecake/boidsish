#pragma once

#include <queue>
#include <mutex>
#include <condition_variable>

namespace Boidsish {
    template <typename T>
    class ConcurrentQueue {
    public:
        void push(T value) {
            std::lock_guard<std::mutex> lock(mutex_);
            queue_.push(std::move(value));
        }

        bool try_pop(T& value) {
            std::lock_guard<std::mutex> lock(mutex_);
            if (queue_.empty()) {
                return false;
            }
            value = std::move(queue_.front());
            queue_.pop();
            return true;
        }

        bool empty() const {
            std::lock_guard<std::mutex> lock(mutex_);
            return queue_.empty();
        }

    private:
        std::queue<T> queue_;
        mutable std::mutex mutex_;
    };
}
