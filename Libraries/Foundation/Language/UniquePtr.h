// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once

namespace SC
{
template <typename T>
struct UniquePtr
{
    UniquePtr() = default;
    ~UniquePtr() { delete ptr; }
    UniquePtr(UniquePtr&& other) : ptr(other.ptr) { other.ptr = nullptr; }
    UniquePtr& operator=(UniquePtr&& other)
    {
        delete ptr;
        ptr       = other.ptr;
        other.ptr = nullptr;
        return *this;
    }
    UniquePtr(const UniquePtr&)            = delete;
    UniquePtr& operator=(const UniquePtr&) = delete;

    T*       get() { return ptr; }
    const T* get() const { return ptr; }
    T*       operator->() { return ptr; }
    const T* operator->() const { return ptr; }
    T&       operator*() { return *ptr; }
    const T& operator*() const { return *ptr; }

    bool isValid() const { return ptr; }

  private:
    T* ptr = nullptr;
    template <typename Q, typename... Args>
    friend UniquePtr<Q> MakeUnique(Args&&... args);

    UniquePtr(T* ptr) : ptr(ptr){};
};

template <typename T, typename... Args>
UniquePtr<T> MakeUnique(Args&&... args)
{
    return UniquePtr<T>(new T(forward<Args>(args)...));
}
} // namespace SC