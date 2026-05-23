// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT

#include <coroutine>

struct StdCppHeaderNoLinkTask
{
    struct promise_type
    {
        StdCppHeaderNoLinkTask        get_return_object() noexcept { return {}; }
        static StdCppHeaderNoLinkTask get_return_object_on_allocation_failure() noexcept { return {}; }
        std::suspend_never            initial_suspend() noexcept { return {}; }
        std::suspend_never            final_suspend() noexcept { return {}; }
        void                          return_void() noexcept {}
        void                          unhandled_exception() noexcept {}

        static void* operator new(decltype(sizeof(0)), void* memory) noexcept { return memory; }
        static void  operator delete(void*, decltype(sizeof(0))) noexcept {}
    };
};

StdCppHeaderNoLinkTask stdCppHeaderNoLinkProbe(void* storage) { co_return; }

int main()
{
    alignas(64) unsigned char storage[1024] = {};
    (void)stdCppHeaderNoLinkProbe(storage);
    return 0;
}
