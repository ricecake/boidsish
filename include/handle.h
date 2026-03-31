#pragma once

#include <algorithm>
#include <cassert>
#include <compare>
#include <cstdint>
#include <memory>
#include <mutex>
#include <vector>

namespace Boidsish {

	/**
	 * @brief A monostate pool that stores objects of type T in contiguous memory.
	 * * @tparam T The type of the resource.
	 * @tparam Tag A unique tag to create completely distinct pools for the same type T.
	 */
	template <typename T, typename Tag = T>
	class Pool {
	private:
		// All storage is static inline, ensuring one global instance per <T, Tag> pair.
		static inline std::vector<T>        data_;
		static inline std::vector<uint32_t> dense_to_sparse_;
		static inline std::vector<uint32_t> sparse_;
		static inline std::vector<uint16_t> generations_;
		static inline std::vector<uint32_t> free_slots_;
		static inline std::recursive_mutex  mutex_;

		static uint32_t Pack(uint32_t index, uint16_t generation) {
			return (static_cast<uint32_t>(generation) << 20) | ((index + 1) & 0xFFFFF);
		}

		static uint32_t UnpackIndex(uint32_t id) { return (id & 0xFFFFF) - 1; }

		static uint16_t UnpackGeneration(uint32_t id) { return static_cast<uint16_t>(id >> 20); }

		static void Grow() {
			size_t old_size = sparse_.size();
			size_t new_size = old_size == 0 ? 1024 : old_size * 2; // Lazy initialization
			sparse_.resize(new_size);
			generations_.resize(new_size, 1);
			for (size_t i = old_size; i < new_size; ++i) {
				free_slots_.push_back(static_cast<uint32_t>(new_size + old_size - 1 - i));
			}
		}

	public:
		/**
		 * @brief The bound handle that intrinsically routes to the parent Pool's static state.
		 */
		struct Handle {
			using ValueType = uint32_t;
			ValueType id = 0;

			constexpr Handle() = default;

			constexpr explicit Handle(ValueType id): id(id) {}

			bool IsValid() const { return Pool::IsValid(id); }

			explicit operator bool() const { return IsValid(); }

			auto operator<=>(const Handle&) const = default;
			bool operator==(const Handle&) const = default;

			T* operator->() const {
				assert(IsValid());
				return Pool::Get(id);
			}

			T& operator*() const {
				assert(IsValid());
				return *Pool::Get(id);
			}

			T* Get() const { return Pool::Get(id); }

			ValueType GetId() const { return id; }
		};

		/**
		 * @brief Represents a reserved contiguous block of the pool.
		 */
		struct Chunk {
			std::span<T>        memory;  // Lock-free write access to the objects
			std::vector<Handle> handles; // The specific handles for these objects
		};

		// ------------------------------------------------------------------------
		// Chunk Allocation API
		// ------------------------------------------------------------------------

		/**
		 * @brief Reserves a contiguous block of memory and returns it along with its handles.
		 * WARNING: Calling Free() on the pool while holding this span may invalidate the span
		 * due to the pool's swap-and-pop density maintenance.
		 */
		static Chunk AllocateChunk(size_t count) {
			if (count == 0)
				return {};

			std::lock_guard<std::recursive_mutex> lock(mutex_);

			// 1. Ensure we have enough free slots
			if (free_slots_.size() < count) {
				Grow(count - free_slots_.size());
			}

			// 2. Mark the starting position in the dense array
			size_t start_dense_index = data_.size();

			// 3. Expand the dense data.
			// For trivial types (PODs), this value-initializes (zeroes) the memory.
			data_.resize(start_dense_index + count);

			Chunk chunk;
			chunk.memory = std::span<T>(data_.data() + start_dense_index, count);
			chunk.handles.reserve(count);

			// 4. Map the sparse indices to the new dense chunk
			for (size_t i = 0; i < count; ++i) {
				uint32_t sparse_index = free_slots_.back();
				free_slots_.pop_back();

				uint32_t current_dense = static_cast<uint32_t>(start_dense_index + i);

				dense_to_sparse_.push_back(sparse_index);
				sparse_[sparse_index] = current_dense;

				uint32_t id = Pack(sparse_index, generations_[sparse_index]);
				chunk.handles.push_back(Handle(id));
			}

			return chunk;
		}

		static std::span<T> AllocateAnonymousChunk(size_t count) {
			if (count == 0)
				return {};

			std::lock_guard<std::recursive_mutex> lock(mutex_);

			if (free_slots_.size() < count) {
				Grow(count - free_slots_.size());
			}

			size_t start_dense_index = data_.size();
			data_.resize(start_dense_index + count);

			// CRITICAL: You must maintain the internal mapping even if the caller
			// throws away the handles, otherwise Free() will read out of bounds.
			for (size_t i = 0; i < count; ++i) {
				uint32_t sparse_index = free_slots_.back();
				free_slots_.pop_back();

				uint32_t current_dense = static_cast<uint32_t>(start_dense_index + i);
				dense_to_sparse_.push_back(sparse_index);
				sparse_[sparse_index] = current_dense;

				// We simply skip the 'Pack(sparse, gen)' step since no handle is returned.
			}

			return std::span<T>(data_.data() + start_dense_index, count);
		}

		static std::span<T> GetContiguousData() {
			std::lock_guard<std::recursive_mutex> lock(mutex_);
			return std::span<T>(data_.data(), data_.size());
		}

		static std::span<const T> GetContiguousDataConst() {
			std::lock_guard<std::recursive_mutex> lock(mutex_);
			return std::span<const T>(data_.data(), data_.size());
		}

		// ------------------------------------------------------------------------
		// Instance-like API for allocations
		// ------------------------------------------------------------------------

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

		void Free(Handle handle) { FreeById(handle.GetId()); }

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

		size_t Size() const {
			std::lock_guard<std::recursive_mutex> lock(mutex_);
			return data_.size();
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

		// ------------------------------------------------------------------------
		// Static Accessors routed through the Handle
		// ------------------------------------------------------------------------

		static bool IsValid(uint32_t id) {
			std::lock_guard<std::recursive_mutex> lock(mutex_);
			if (id == 0)
				return false;
			uint32_t index = UnpackIndex(id);
			if (index >= generations_.size())
				return false;
			return generations_[index] == UnpackGeneration(id);
		}

		static T* Get(uint32_t id) {
			std::lock_guard<std::recursive_mutex> lock(mutex_);
			if (!IsValid(id))
				return nullptr;
			return &data_[sparse_[UnpackIndex(id)]];
		}

		static const T* GetConst(uint32_t id) {
			std::lock_guard<std::recursive_mutex> lock(mutex_);
			if (!IsValid(id))
				return nullptr;
			return &data_[sparse_[UnpackIndex(id)]];
		}

		static std::shared_ptr<T> GetAsShared(uint32_t id) {
			std::lock_guard<std::recursive_mutex> lock(mutex_);
			if (!IsValid(id))
				return nullptr;
			return std::shared_ptr<T>(Get(id), [](T*) {});
		}
	};

} // namespace Boidsish

// Hash support for unordered maps
namespace std {
	template <typename T, typename Tag>
	struct hash<typename Boidsish::Pool<T, Tag>::Handle> {
		size_t operator()(const typename Boidsish::Pool<T, Tag>::Handle& h) const { return hash<uint32_t>{}(h.id); }
	};
} // namespace std