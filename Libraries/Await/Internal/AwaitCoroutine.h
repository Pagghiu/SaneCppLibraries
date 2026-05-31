// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once

#ifndef SC_AWAIT_ENABLE_NO_STDLIB_COROUTINE
#define SC_AWAIT_ENABLE_NO_STDLIB_COROUTINE 0
#endif

#if SC_AWAIT_ENABLE_NO_STDLIB_COROUTINE

namespace std
{
template <typename Result, typename... Arguments>
struct coroutine_traits
{
    using promise_type = typename Result::promise_type;
};

template <typename Promise = void>
struct coroutine_handle;

template <>
struct coroutine_handle<void>
{
    constexpr coroutine_handle() noexcept = default;
    constexpr coroutine_handle(decltype(nullptr)) noexcept {}

    static constexpr coroutine_handle from_address(void* address) noexcept
    {
        coroutine_handle handle;
        handle.coro = address;
        return handle;
    }

    constexpr void* address() const noexcept { return coro; }

    constexpr explicit operator bool() const noexcept { return coro != nullptr; }

    constexpr bool operator==(decltype(nullptr)) const noexcept { return coro == nullptr; }
    constexpr bool operator!=(decltype(nullptr)) const noexcept { return coro != nullptr; }

    coroutine_handle& operator=(decltype(nullptr)) noexcept
    {
        coro = nullptr;
        return *this;
    }

    void resume() const noexcept { __builtin_coro_resume(coro); }

    void destroy() const noexcept { __builtin_coro_destroy(coro); }

    bool done() const noexcept { return __builtin_coro_done(coro); }

    void* coro = nullptr;
};

template <typename Promise>
struct coroutine_handle
{
    constexpr coroutine_handle() noexcept = default;
    constexpr coroutine_handle(decltype(nullptr)) noexcept {}

    static constexpr coroutine_handle from_address(void* address) noexcept
    {
        coroutine_handle handle;
        handle.coro = address;
        return handle;
    }

    static coroutine_handle from_promise(Promise& promise) noexcept
    {
        coroutine_handle handle;
#if defined(_MSC_VER)
        handle.coro = __builtin_coro_promise(&promise, 0, true);
#else
        handle.coro = __builtin_coro_promise(&promise, alignof(Promise), true);
#endif
        return handle;
    }

    constexpr void* address() const noexcept { return coro; }

    Promise& promise() const noexcept
    {
#if defined(_MSC_VER)
        return *static_cast<Promise*>(__builtin_coro_promise(coro, 0, false));
#else
        return *static_cast<Promise*>(__builtin_coro_promise(coro, alignof(Promise), false));
#endif
    }

    constexpr explicit operator bool() const noexcept { return coro != nullptr; }
    constexpr          operator coroutine_handle<>() const noexcept { return coroutine_handle<>::from_address(coro); }

    constexpr bool operator==(decltype(nullptr)) const noexcept { return coro == nullptr; }
    constexpr bool operator!=(decltype(nullptr)) const noexcept { return coro != nullptr; }

    coroutine_handle& operator=(decltype(nullptr)) noexcept
    {
        coro = nullptr;
        return *this;
    }

    void resume() const noexcept { coroutine_handle<>::from_address(coro).resume(); }
    void destroy() const noexcept { coroutine_handle<>::from_address(coro).destroy(); }
    bool done() const noexcept { return coroutine_handle<>::from_address(coro).done(); }

    void* coro = nullptr;
};

struct suspend_always
{
    constexpr bool await_ready() const noexcept { return false; }
    constexpr void await_suspend(coroutine_handle<>) const noexcept {}
    constexpr void await_resume() const noexcept {}
};

struct suspend_never
{
    constexpr bool await_ready() const noexcept { return true; }
    constexpr void await_suspend(coroutine_handle<>) const noexcept {}
    constexpr void await_resume() const noexcept {}
};
} // namespace std

#else
#include <coroutine>
#endif

namespace SC
{
using AwaitCoroutineHandle = std::coroutine_handle<>;

template <typename Promise>
using AwaitCoroutineTypedHandle = std::coroutine_handle<Promise>;

using AwaitSuspendAlways = std::suspend_always;
} // namespace SC
