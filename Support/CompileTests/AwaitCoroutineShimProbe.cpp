// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT

#define SC_AWAIT_ENABLE_NO_STDLIB_COROUTINE 1
#include "Libraries/Await/Internal/AwaitCoroutine.h"

struct AwaitCoroutineShimTask
{
    struct promise_type;
    using Handle = SC::AwaitCoroutineTypedHandle<promise_type>;

    AwaitCoroutineShimTask() = default;
    explicit AwaitCoroutineShimTask(Handle handle) : coroutine(handle) {}

    AwaitCoroutineShimTask(const AwaitCoroutineShimTask&)            = delete;
    AwaitCoroutineShimTask& operator=(const AwaitCoroutineShimTask&) = delete;

    AwaitCoroutineShimTask(AwaitCoroutineShimTask&& other) noexcept : coroutine(other.coroutine)
    {
        other.coroutine = nullptr;
    }

    AwaitCoroutineShimTask& operator=(AwaitCoroutineShimTask&& other) noexcept
    {
        if (this != &other)
        {
            destroy();
            coroutine       = other.coroutine;
            other.coroutine = nullptr;
        }
        return *this;
    }

    ~AwaitCoroutineShimTask() { destroy(); }

    void resume()
    {
        if (coroutine)
        {
            coroutine.resume();
        }
    }

    void destroy()
    {
        if (coroutine)
        {
            coroutine.destroy();
            coroutine = nullptr;
        }
    }

    Handle coroutine = {};
};

struct AwaitCoroutineShimTask::promise_type
{
    static void* operator new(decltype(sizeof(0)), void* storage) noexcept { return storage; }
    static void  operator delete(void*, decltype(sizeof(0))) noexcept {}

    AwaitCoroutineShimTask get_return_object() noexcept
    {
        return AwaitCoroutineShimTask(AwaitCoroutineShimTask::Handle::from_promise(*this));
    }

    SC::AwaitSuspendAlways initial_suspend() noexcept { return {}; }
    SC::AwaitSuspendAlways final_suspend() noexcept { return {}; }

    void return_void() noexcept {}
    void unhandled_exception() noexcept {}
};

AwaitCoroutineShimTask awaitCoroutineShimProbe(void* storage) { co_return; }

int main()
{
    alignas(64) unsigned char storage[1024] = {};
    AwaitCoroutineShimTask    task          = awaitCoroutineShimProbe(storage);
    task.resume();
    return 0;
}
