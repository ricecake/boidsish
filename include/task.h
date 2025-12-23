#pragma once

#include <functional>
#include <atomic>
#include <future>
#include <memory>

namespace Boidsish {

enum class TaskPriority {
    LOW,
    MEDIUM,
    HIGH
};

struct Task {
    std::function<void()> func;
    TaskPriority priority;
    std::atomic<bool> cancelled;

    Task(std::function<void()> f, TaskPriority p)
        : func(std::move(f)), priority(p), cancelled(false) {}

    // So we can use this in a priority_queue
    bool operator<(const Task& other) const {
        return priority < other.priority;
    }
};

template<typename T>
struct TaskHandle {
    std::future<T> future;
    std::shared_ptr<Task> task;

    void cancel() {
        if (task) {
            task->cancelled = true;
        }
    }
};

}
