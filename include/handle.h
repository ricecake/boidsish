#pragma once

#include <algorithm>
#include <cassert>
#include <compare>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <type_traits>
#include <vector>

namespace Boidsish {

	template <typename T>
	class IPool {
	public:
		virtual ~IPool() = default;
		virtual bool     IsValid(uint32_t id) const = 0;
		virtual T*       Get(uint32_t id) = 0;
		virtual const T* Get(uint32_t id) const = 0;

		virtual void lock() const = 0;
		virtual void unlock() const = 0;
	};

	template <typename T>
	struct PoolAccessor {
		// Function pointer to retrieve the static pool instance
		IPool<T>* (*get_pool)() = nullptr;
	};

	/**
	 * @brief A generic type-safe and tagged handle for resources.
	 *
	 * @tparam T The type of the resource this handle refers to.
	 * @tparam Tag A unique tag to differentiate handle types for different resource categories.
	 */
	template <typename T, typename Tag = T, PoolAccessor<T> pool_ = {}>
	struct Handle {
	public:
		using ValueType = uint32_t;
		using PoolType = decltype(pool_);

		ValueType id = 0;

		constexpr Handle() = default;

		constexpr explicit Handle(ValueType id): id(id) {}

		constexpr bool IsValid() const { return id != 0 && (pool_ == nullptr || pool_->IsValid(id)); }

		constexpr explicit operator bool() const { return IsValid(); }

		auto operator<=>(const Handle&) const = default;
		bool operator==(const Handle&) const = default;

		T* operator->() const {
			assert(IsValid());
			return pool_->Get(id);
		}

		T& operator*() const {
			assert(IsValid());
			return *pool_->Get(id);
		}

		T* Get() const { return IsValid() ? pool_->Get(id) : nullptr; }

		ValueType GetId() const { return id; }
	};

	/**
	 * @brief A pool that stores objects of type T in contiguous memory.
	 * Uses a sparse-dense array (SoA) approach to maintain locality during iteration
	 * while allowing stable handles.
	 */
	template <typename T>
	class Pool: public IPool<T> {
	public:
		using Handle = Handle<T, >;

		Pool(size_t initial_capacity = 1024) {
			data_.reserve(initial_capacity);
			dense_to_sparse_.reserve(initial_capacity);
			sparse_.resize(initial_capacity);
			generations_.resize(initial_capacity, 1);

			for (size_t i = 0; i < initial_capacity; ++i) {
				free_slots_.push_back(static_cast<uint32_t>(initial_capacity - 1 - i));
			}
		}

		template <typename... Args>
		Handle Allocate(Args&&... args) {
			std::lock_guard<std::recursive_mutex> lock(mutex_);
			if (free_slots_.empty()) {
				Grow();
			}

			uint32_t sparse_index = free_slots_.back();
			free_slots_.pop_back();

			uint32_t dense_index = static_cast<uint32_t>(data_.size());
			data_.emplace_back(std::forward<Args>(args)...);
			dense_to_sparse_.push_back(sparse_index);

			sparse_[sparse_index] = dense_index;

			uint32_t id = Pack(sparse_index, generations_[sparse_index]);
			return Handle(id);
		}

		void Free(Handle handle) {
			std::lock_guard<std::recursive_mutex> lock(mutex_);
			if (!IsValid(handle.GetId()))
				return;
			FreeById(handle.GetId());
		}

		void FreeById(uint32_t id) {
			std::lock_guard<std::recursive_mutex> lock(mutex_);
			if (!IsValid(id))
				return;
			uint32_t sparse_index = UnpackIndex(id);
			uint32_t dense_index = sparse_[sparse_index];

			uint32_t last_dense_index = static_cast<uint32_t>(data_.size() - 1);
			if (dense_index != last_dense_index) {
				data_[dense_index] = std::move(data_[last_dense_index]);
				uint32_t sparse_index_for_last = dense_to_sparse_[last_dense_index];
				sparse_[sparse_index_for_last] = dense_index;
				dense_to_sparse_[dense_index] = sparse_index_for_last;
			}

			data_.pop_back();
			dense_to_sparse_.pop_back();

			generations_[sparse_index]++;
			if (generations_[sparse_index] == 0)
				generations_[sparse_index] = 1;
			free_slots_.push_back(sparse_index);
		}

		bool IsValid(uint32_t id) const override {
			std::lock_guard<std::recursive_mutex> lock(mutex_);
			if (id == 0)
				return false;
			uint32_t index = UnpackIndex(id);
			if (index >= generations_.size())
				return false;
			uint16_t generation = UnpackGeneration(id);
			return generations_[index] == generation;
		}

		T* Get(uint32_t id) override {
			std::lock_guard<std::recursive_mutex> lock(mutex_);
			if (!IsValid(id))
				return nullptr;
			return &data_[sparse_[UnpackIndex(id)]];
		}

		const T* Get(uint32_t id) const override {
			std::lock_guard<std::recursive_mutex> lock(mutex_);
			if (!IsValid(id))
				return nullptr;
			return &data_[sparse_[UnpackIndex(id)]];
		}

		size_t Size() const {
			std::lock_guard<std::recursive_mutex> lock(mutex_);
			return data_.size();
		}

		// Warning: begin/end are not thread-safe if the pool can be modified during iteration.
		// Use ForEach instead.
		auto begin() { return data_.begin(); }

		auto end() { return data_.end(); }

		auto begin() const { return data_.begin(); }

		auto end() const { return data_.end(); }

		template <typename Func>
		void ForEach(Func&& func) {
			std::lock_guard<std::recursive_mutex> lock(mutex_);
			for (auto& item : data_) {
				func(item);
			}
		}

		template <typename Func>
		void ForEach(Func&& func) const {
			std::lock_guard<std::recursive_mutex> lock(mutex_);
			for (const auto& item : data_) {
				func(item);
			}
		}

		void Clear() {
			std::lock_guard<std::recursive_mutex> lock(mutex_);
			data_.clear();
			dense_to_sparse_.clear();
			for (auto& gen : generations_) {
				gen++;
				if (gen == 0)
					gen = 1;
			}
			free_slots_.clear();
			for (size_t i = 0; i < sparse_.size(); ++i) {
				free_slots_.push_back(static_cast<uint32_t>(sparse_.size() - 1 - i));
			}
		}

		/**
		 * @brief Creates a shared_ptr with a no-op deleter that points to an object in the pool.
		 * Useful for backward compatibility with systems expecting shared_ptrs.
		 */
		std::shared_ptr<T> GetAsShared(uint32_t id) {
			std::lock_guard<std::recursive_mutex> lock(mutex_);
			if (!IsValid(id))
				return nullptr;
			return std::shared_ptr<T>(Get(id), [](T*) {});
		}

		void lock() const override { mutex_.lock(); }

		void unlock() const override { mutex_.unlock(); }

	private:
		void Grow() {
			size_t old_size = sparse_.size();
			size_t new_size = old_size * 2;
			sparse_.resize(new_size);
			generations_.resize(new_size, 1);
			for (size_t i = old_size; i < new_size; ++i) {
				free_slots_.push_back(static_cast<uint32_t>(new_size + old_size - 1 - i));
			}
		}

		static uint32_t Pack(uint32_t index, uint16_t generation) {
			return (static_cast<uint32_t>(generation) << 20) | ((index + 1) & 0xFFFFF);
		}

		static uint32_t UnpackIndex(uint32_t id) { return (id & 0xFFFFF) - 1; }

		static uint16_t UnpackGeneration(uint32_t id) { return static_cast<uint16_t>(id >> 20); }

		std::vector<T>               data_;
		std::vector<uint32_t>        dense_to_sparse_;
		std::vector<uint32_t>        sparse_;
		std::vector<uint16_t>        generations_;
		std::vector<uint32_t>        free_slots_;
		mutable std::recursive_mutex mutex_;
	};

} // namespace Boidsish

// Hash support for using Handle in unordered maps
namespace std {
	template <typename T, typename Tag>
	struct hash<Boidsish::Handle<T, Tag>> {
		size_t operator()(const Boidsish::Handle<T, Tag>& h) const { return hash<uint32_t>{}(h.id); }
	};
} // namespace std
