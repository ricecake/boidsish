#pragma once

#include <atomic>
#include <condition_variable>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <thread>
#include <vector>

#include <task_thread_pool.hpp>

namespace Boidsish {

	static inline task_thread_pool::task_thread_pool pool;

	enum class TaskPriority { LOW, MEDIUM, HIGH };

	// Forward declaration
	class ThreadPool;

	template <typename R>
	class TaskHandle {
	public:
		TaskHandle(std::future<R>&& future, std::shared_ptr<std::atomic<bool>> cancelled_flag):
			future_(std::move(future)), cancelled_flag_(cancelled_flag) {}

		// Non-copyable
		TaskHandle(const TaskHandle&) = delete;
		TaskHandle& operator=(const TaskHandle&) = delete;

		// Movable
		TaskHandle(TaskHandle&&) = default;
		TaskHandle& operator=(TaskHandle&&) = default;

		R get() { return future_.get(); }

		bool is_ready() const {
			return future_.valid() && future_.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
		}

		void cancel() {
			if (cancelled_flag_) {
				cancelled_flag_->store(true, std::memory_order_relaxed);
			}
		}

	private:
		std::future<R>                     future_;
		std::shared_ptr<std::atomic<bool>> cancelled_flag_;
	};

	class ThreadPool {
	public:
		ThreadPool() { dispatcher_thread_ = std::thread(&ThreadPool::dispatcherLoop, this); }

		~ThreadPool() {
			{
				std::unique_lock<std::mutex> lock(queue_mutex_);
				stop_ = true;
			}
			condition_.notify_one();
			if (dispatcher_thread_.joinable()) {
				dispatcher_thread_.join();
			}
		}

		template <class F, class... Args, typename R = std::invoke_result_t<F, Args...>>
		auto enqueue(TaskPriority priority, F&& f, Args&&... args) -> TaskHandle<R> {
			auto cancelled_flag = std::make_shared<std::atomic<bool>>(false);

			auto packaged_task = std::make_shared<std::packaged_task<R()>>(
				std::bind(std::forward<F>(f), std::forward<Args>(args)...)
			);

			std::future<R> res = packaged_task->get_future();

			auto wrapper_func = [packaged_task, cancelled_flag]() {
				if (!cancelled_flag->load(std::memory_order_relaxed)) {
					(*packaged_task)();
				}
			};

			{
				std::unique_lock<std::mutex> lock(queue_mutex_);
				if (stop_) {
					throw std::runtime_error("enqueue on stopped ThreadPool");
				}
				tasks_.push({std::move(wrapper_func), priority});
			}
			condition_.notify_one();

			return TaskHandle<R>(std::move(res), cancelled_flag);
		}

	private:
		struct Task {
			std::function<void()> func;
			TaskPriority          priority;

			bool operator<(const Task& other) const { return priority < other.priority; }
		};

		void dispatcherLoop() {
			while (true) {
				std::unique_lock<std::mutex> lock(queue_mutex_);
				condition_.wait(lock, [this] { return stop_ || !tasks_.empty(); });

				if (stop_ && tasks_.empty()) {
					return;
				}

				Task task = std::move(const_cast<Task&>(tasks_.top()));
				tasks_.pop();
				lock.unlock();

				Boidsish::pool.submit_detach(std::move(task.func));
			}
		}

		std::priority_queue<Task> tasks_;
		std::mutex                queue_mutex_;
		std::condition_variable   condition_;
		bool                      stop_ = false;
		std::thread               dispatcher_thread_;
	};

} // namespace Boidsish
