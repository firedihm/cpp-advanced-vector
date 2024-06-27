#pragma once

#include <stdexcept>
#include <utility>

// Исключение этого типа должно генерироватся при обращении к пустому optional
class BadOptionalAccess : public std::exception {
public:
    using exception::exception;
    
    virtual const char* what() const noexcept override {
        return "Bad optional access";
    }
};

template <typename T>
class Optional {
public:
    Optional() = default;
    Optional(const T& value) {
        new (data_) T(value);
        is_initialized_ = true;
    }
    Optional(T&& value) {
        new (data_) T(std::move(value));
        is_initialized_ = true;
    }
    Optional(const Optional& other) {
        if (other.HasValue()) {
            new (data_) T(*other);
            is_initialized_ = true;
        }
    }
    Optional(Optional&& other) {
        if (other.HasValue()) {
            new (data_) T(std::move(*other));
            is_initialized_ = true;
        }
    }
    
    ~Optional() noexcept { Reset(); }
    
    Optional& operator=(const T& rhs) {
        if (HasValue()) {
            **this = rhs;
        } else {
            new (data_) T(rhs);
            is_initialized_ = true;
        }
        return *this;
    }
    Optional& operator=(T&& rhs) {
        if (HasValue()) {
            **this = std::move(rhs);
        } else {
            new (data_) T(std::move(rhs));
            is_initialized_ = true;
        }
        return *this;
    }
    Optional& operator=(const Optional& rhs) {
        if (this != &rhs) {
            if (!rhs.HasValue()) {
                Reset();
            } else if (HasValue()) {
                **this = *rhs;
            } else {
                new (data_) T(*rhs);
                is_initialized_ = true;
            }
        }
        return *this;
    }
    Optional& operator=(Optional&& rhs) {
        if (!rhs.HasValue()) {
            Reset();
        } else if (HasValue()) {
            **this = std::move(*rhs);
        } else {
            new (data_) T(std::move(*rhs));
            is_initialized_ = true;
        }
        return *this;
    }
    T& operator*() & noexcept { return *reinterpret_cast<T*>(&data_[0]); }
    T&& operator*() && noexcept { return std::move(**this); }
    const T& operator*() const& noexcept { return *const_cast<Optional&>(*this); }
    T* operator->() noexcept { return reinterpret_cast<T*>(data_); }
    const T* operator->() const noexcept { return reinterpret_cast<const T*>(data_); }
    
    bool HasValue() const noexcept { return is_initialized_; }
    
    T& Value() & {
        if (!HasValue()) {
            throw BadOptionalAccess();
        }
        return **this;
    }
    T&& Value() && { return std::move(Value()); }
    const T& Value() const& { return const_cast<Optional&>(*this).Value(); }
    
    template <typename... Args>
    void Emplace(Args&&... values) noexcept {
        Reset();
        new (data_) T(std::forward<Args>(values)...);
        is_initialized_ = true;
    }
    
    void Reset() noexcept {
        if (HasValue()) {
            Value().~T();
            is_initialized_ = false;
        }
    }
    
private:
    // alignas нужен для правильного выравнивания блока памяти
    alignas(T) char data_[sizeof(T)];
    bool is_initialized_ = false;
};
