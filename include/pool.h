#pragma once

#include <vector>
#include <cstdint>
#include <cassert>
#include <type_traits>
#include <memory>
#include <algorithm>

namespace Boidsish {

template <typename T>
class IPool {
public:
    virtual ~IPool() = default;
    virtual bool IsValid(uint32_t id) const = 0;
    virtual T* Get(uint32_t id) = 0;
    virtual const T* Get(uint32_t id) const = 0;
};

/**
 * @brief A handle that acts like a smart pointer to an object in a Pool.
 */
template <typename T>
class PoolHandle {
public:
    PoolHandle() : id_(0), pool_(nullptr) {}
    PoolHandle(uint32_t id, IPool<T>* pool) : id_(id), pool_(pool) {}

    bool IsValid() const {
        return id_ != 0 && pool_ != nullptr && pool_->IsValid(id_);
    }

    explicit operator bool() const {
        return IsValid();
    }

    T* operator->() const {
        assert(IsValid());
        return pool_->Get(id_);
    }

    T& operator*() const {
        assert(IsValid());
        return *pool_->Get(id_);
    }

    T* Get() const {
        return IsValid() ? pool_->Get(id_) : nullptr;
    }

    bool operator==(const PoolHandle& other) const {
        return id_ == other.id_ && pool_ == other.pool_;
    }

    bool operator!=(const PoolHandle& other) const {
        return !(*this == other);
    }

    uint32_t GetId() const { return id_; }

private:
    uint32_t id_;
    IPool<T>* pool_;
};

/**
 * @brief A pool that stores objects of type T in contiguous memory.
 * Uses a sparse-dense array (SoA) approach to maintain locality during iteration
 * while allowing stable handles.
 */
template <typename T>
class Pool : public IPool<T> {
public:
    using Handle = PoolHandle<T>;

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
        return Handle(id, this);
    }

    void Free(Handle handle) {
        if (!IsValid(handle.GetId())) return;
        FreeById(handle.GetId());
    }

    void FreeById(uint32_t id) {
        if (!IsValid(id)) return;
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
        if (generations_[sparse_index] == 0) generations_[sparse_index] = 1;
        free_slots_.push_back(sparse_index);
    }

    bool IsValid(uint32_t id) const override {
        if (id == 0) return false;
        uint32_t index = UnpackIndex(id);
        if (index >= generations_.size()) return false;
        uint16_t generation = UnpackGeneration(id);
        return generations_[index] == generation;
    }

    T* Get(uint32_t id) override {
        if (!IsValid(id)) return nullptr;
        return &data_[sparse_[UnpackIndex(id)]];
    }

    const T* Get(uint32_t id) const override {
        if (!IsValid(id)) return nullptr;
        return &data_[sparse_[UnpackIndex(id)]];
    }

    size_t Size() const { return data_.size(); }

    auto begin() { return data_.begin(); }
    auto end() { return data_.end(); }
    auto begin() const { return data_.begin(); }
    auto end() const { return data_.end(); }

    template <typename Func>
    void ForEach(Func&& func) {
        for (auto& item : data_) {
            func(item);
        }
    }

    template <typename Func>
    void ForEach(Func&& func) const {
        for (const auto& item : data_) {
            func(item);
        }
    }

    void Clear() {
        data_.clear();
        dense_to_sparse_.clear();
        for (auto& gen : generations_) {
            gen++;
            if (gen == 0) gen = 1;
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
        if (!IsValid(id)) return nullptr;
        return std::shared_ptr<T>(Get(id), [](T*){});
    }

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

    static uint32_t UnpackIndex(uint32_t id) {
        return (id & 0xFFFFF) - 1;
    }

    static uint16_t UnpackGeneration(uint32_t id) {
        return static_cast<uint16_t>(id >> 20);
    }

    std::vector<T> data_;
    std::vector<uint32_t> dense_to_sparse_;
    std::vector<uint32_t> sparse_;
    std::vector<uint16_t> generations_;
    std::vector<uint32_t> free_slots_;
};

} // namespace Boidsish
