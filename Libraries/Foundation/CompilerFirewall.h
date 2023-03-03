// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "Language.h"

namespace SC
{
template <int N, int Alignment>
struct OpaqueHandle
{
    template <typename T>
    T& reinterpret_as()
    {
        static_assert(sizeof(T) <= N, "Increase size of OpaqueHandle");
        static_assert(alignof(T) <= Alignment, "Increase Alignment of OpaqueHandle");
        return *reinterpret_cast<T*>(bytes);
    }

  private:
    alignas(Alignment) char bytes[N];
};
template <typename T>
struct CompilerFirewallFuncs
{
    template <int BufferSizeInBytes>
    static void construct(uint8_t* buffer);
    static void destruct(T& obj);
    static void moveConstruct(uint8_t* buffer, T&& obj);
    static void moveAssign(T& pthis, T&& obj);
};

template <typename T, int N = sizeof(void*)>
struct CompilerFirewall
{
    static constexpr int BufferSizeInBytes = N;

    CompilerFirewall() { CompilerFirewallOps::template construct<N>(buffer); }
    ~CompilerFirewall() { CompilerFirewallOps::destruct(get()); }
    CompilerFirewall(CompilerFirewall&& other) { CompilerFirewallOps::moveConstruct(buffer, forward<T>(other.get())); }
    CompilerFirewall& operator=(CompilerFirewall&& other)
    {
        CompilerFirewallOps::moveAssign(get(), forward<T>(other.get()));
        return *this;
    }

    // Disallow copy construction and copy assignment
    CompilerFirewall(const CompilerFirewall&)            = delete;
    CompilerFirewall& operator=(const CompilerFirewall&) = delete;

    T&       get() { return reinterpret_cast<T&>(buffer); }
    const T& get() const { return reinterpret_cast<const T&>(buffer); }

  private:
    using CompilerFirewallOps = CompilerFirewallFuncs<T>;
    alignas(uint64_t) uint8_t buffer[N];
};

} // namespace SC
