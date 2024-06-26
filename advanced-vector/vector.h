#pragma once
#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <memory>
#include <new>
#include <utility>

template <typename T>
class RawMemory {
public:
    RawMemory() = default;
    explicit RawMemory(size_t capacity) : buffer_(Allocate(capacity)), capacity_(capacity) {}
    RawMemory(const RawMemory&) = delete;
    RawMemory(RawMemory&& other) noexcept {
        buffer_ = std::exchange(other.buffer_, nullptr);
        capacity_ = std::exchange(other.capacity_, 0);
    }
    
    ~RawMemory() { Deallocate(buffer_); }
    
    RawMemory& operator=(const RawMemory& rhs) = delete;
    RawMemory& operator=(RawMemory&& rhs) noexcept {
        if (this != &rhs) {
            buffer_ = std::exchange(rhs.buffer_, nullptr);
            capacity_ = std::exchange(rhs.capacity_, 0);
        }
        return *this;
    }
    T* operator+(size_t offset) noexcept {
        assert(offset <= capacity_);
        return buffer_ + offset;
    }
    const T* operator+(size_t offset) const noexcept {
        return const_cast<RawMemory&>(*this) + offset;
    }
    const T& operator[](size_t index) const noexcept {
        return const_cast<RawMemory&>(*this)[index];
    }
    T& operator[](size_t index) noexcept {
        assert(index < capacity_);
        return buffer_[index];
    }
    
    T* GetAddress() noexcept { return buffer_; }
    const T* GetAddress() const noexcept { return buffer_; }
    size_t Capacity() const { return capacity_; }
    
    void Swap(RawMemory& other) noexcept {
        std::swap(buffer_, other.buffer_);
        std::swap(capacity_, other.capacity_);
    }
    
private:
    static T* Allocate(size_t n) { return n != 0 ? static_cast<T*>(operator new(n * sizeof(T))) : nullptr; }
    static void Deallocate(T* buf) noexcept { operator delete(buf); }
    
    T* buffer_ = nullptr;
    size_t capacity_ = 0;
};

template <typename T>
class Vector {
public:
    using iterator = T*;
    using const_iterator = const T*;
    
    Vector() = default;
    explicit Vector(size_t size) : data_(size), size_(size) {
        std::uninitialized_value_construct_n(begin(), Size());
    }
    Vector(const Vector& other) : data_(other.size_), size_(other.size_) {
        std::uninitialized_copy_n(other.begin(), Size(), begin());
    }
    Vector(Vector&& other) noexcept
        : data_(std::exchange(other.data_, RawMemory<T>())), size_(std::exchange(other.size_, 0)) {
    }
    
    ~Vector() {
        std::destroy_n(begin(), Size());
    }
    
    Vector& operator=(const Vector& rhs) {
        if (this != &rhs) {
            if (rhs.Size() > Capacity()) {
                Vector rhs_copy(rhs);
                Swap(rhs_copy);
            } else {
                size_t size_delta = std::abs(static_cast<int>(rhs.Size() - Size()));
                if (rhs.Size() < Size()) {
                    std::copy_n(rhs.begin(), rhs.Size(), begin());
                    std::destroy_n(begin() + rhs.Size(), size_delta);
                } else {
                    std::copy_n(rhs.begin(), Size(), begin());
                    std::uninitialized_copy_n(begin() + Size(), size_delta, begin() + Size());
                }
                size_ = rhs.Size();
            }
        }
        return *this;
    }
    Vector& operator=(Vector&& rhs) noexcept {
        if (this != &rhs) {
            data_ = std::exchange(rhs.data_, RawMemory<T>());
            size_ = std::exchange(rhs.size_, 0);
        }
        return *this;
    }
    const T& operator[](size_t index) const noexcept {
        return const_cast<Vector&>(*this)[index];
    }
    T& operator[](size_t index) noexcept {
        assert(index < size_);
        return data_[index];
    }
    
    iterator begin() noexcept { return Data(); }
    iterator end() noexcept { return Data() + Size(); }
    const_iterator begin() const noexcept { return Data(); }
    const_iterator end() const noexcept { return Data() + Size(); }
    const_iterator cbegin() const noexcept { return begin(); }
    const_iterator cend() const noexcept { return end(); }
    
    size_t Capacity() const noexcept { return data_.Capacity(); }
    size_t Size() const noexcept { return size_; }
    iterator Data() noexcept { return data_.GetAddress(); }
    const_iterator Data() const noexcept { return data_.GetAddress(); }
    
    void Reserve(size_t new_capacity) {
        if (new_capacity > Capacity()) {
            RawMemory<T> new_data(new_capacity);
            
            if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
                std::uninitialized_move_n(begin(), Size(), new_data.GetAddress());
            } else {
                std::uninitialized_copy_n(begin(), Size(), new_data.GetAddress());
            }
            std::destroy_n(begin(), Size());
            
            data_.Swap(new_data);
        }
    }
    
    void Resize(size_t new_size) {
        if (new_size > Capacity()) {
            Reserve(new_size);
            std::uninitialized_value_construct_n(begin(), new_size - Size());
        } else {
            std::destroy_n(begin() + new_size, Size() - new_size);
        }
        size_ = new_size;
    }
    
    template <typename... Args>
    T& EmplaceBack(Args&&... args) {
        if (Size() == Capacity()) {
            RawMemory<T> new_data(Size() == 0 ? 1 : 2 * Size());
            new (new_data.GetAddress() + Size()) T(std::forward<Args>(args)...);
            
            if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
                std::uninitialized_move_n(begin(), Size(), new_data.GetAddress());
            } else {
                std::uninitialized_copy_n(begin(), Size(), new_data.GetAddress());
            }
            std::destroy_n(begin(), Size());
            
            data_.Swap(new_data);
        } else {
            new (end()) T(std::forward<Args>(args)...);
        }
        
        ++size_;
        return *(end() - 1);
    }
    
    template <typename U>
    void PushBack(U&& value) {
        EmplaceBack(std::forward<U>(value));
    }
    
    void PopBack() noexcept {
        --size_;
        std::destroy_at(end());
    }
    
    template <typename... Args>
    iterator Emplace(const_iterator pos, Args&&... args) {
        if (Size() == Capacity()) {
            RawMemory<T> new_data((Size() == 0) ? 1 : 2 * Size());
            iterator new_pos = new_data.GetAddress() + std::distance(cbegin(), pos);
            new (new_pos) T(std::forward<Args>(args)...);
            
            if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
                try {
                    std::uninitialized_move(begin(), const_cast<iterator>(pos), new_data.GetAddress());
                } catch (...) {
                    std::destroy_at(new_pos);
                    throw;
                }
                try {
                    std::uninitialized_move(const_cast<iterator>(pos), end(), new_pos + 1);
                } catch (...) {
                    std::destroy(new_data.GetAddress(), new_pos + 1);
                    throw;
                }
            } else {
                try {
                    std::uninitialized_copy(cbegin(), pos, new_data.GetAddress());
                } catch (...) {
                    std::destroy_at(new_pos);
                    throw;
                }
                try {
                    std::uninitialized_copy(pos, cend(), new_pos + 1);
                } catch (...) {
                    std::destroy(new_data.GetAddress(), new_pos + 1);
                    throw;
                }
            }
            std::destroy_n(begin(), Size());
            
            data_.Swap(new_data);
            
            ++size_;
            return begin() + std::distance(begin(), new_pos);
        } else {
            if (pos != cend()) {
                T temp(std::forward<Args>(args)...);
                new (end()) T(std::move(*(end() - 1)));
                std::move_backward(const_cast<iterator>(pos), end() - 1, end());
                *const_cast<iterator>(pos) = std::move(temp);
            } else {
                new (end()) T(std::forward<Args>(args)...);
            }
            
            ++size_;
            return begin() + std::distance(cbegin(), pos);
        }
    }
    
    template <typename U>
    iterator Insert(const_iterator pos, U&& value) {
        return Emplace(pos, std::forward<U>(value));
    }
    
    iterator Erase(const_iterator pos) noexcept(std::is_nothrow_move_assignable_v<T>) {
        std::move(const_cast<iterator>(pos) + 1, end(), const_cast<iterator>(pos));
        
        --size_;
        std::destroy_at(end());
        
        return begin() + std::distance(cbegin(), pos);
    }
    
    void Swap(Vector<T>& other) noexcept {
        std::swap(data_, other.data_);
        std::swap(size_, other.size_);
    }
    
private:
    RawMemory<T> data_;
    size_t size_ = 0;
};
