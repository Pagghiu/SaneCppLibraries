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

template <typename T, int N, int Alignment>
struct OpaqueFunctions
{
    using Handle = OpaqueHandle<N, Alignment>;
    using Object = T;
    static void construct(Handle& buffer);
    static void destruct(T& obj);
    static void moveConstruct(Handle& buffer, T&& obj);
    static void moveAssign(T& pthis, T&& obj);
};

template <typename T, int N = sizeof(void*), int Alignment = alignof(void*)>
struct OpaqueUniqueObject
{
    static constexpr int BufferSizeInBytes = N;

    OpaqueUniqueObject() { OpaqueOps::construct(buffer); }
    ~OpaqueUniqueObject() { OpaqueOps::destruct(get()); }
    OpaqueUniqueObject(OpaqueUniqueObject&& other) { OpaqueOps::moveConstruct(buffer, forward<T>(other.get())); }
    OpaqueUniqueObject& operator=(OpaqueUniqueObject&& other)
    {
        OpaqueOps::moveAssign(get(), forward<T>(other.get()));
        return *this;
    }

    // Disallow copy construction and copy assignment
    OpaqueUniqueObject(const OpaqueUniqueObject&)            = delete;
    OpaqueUniqueObject& operator=(const OpaqueUniqueObject&) = delete;

    T&       get() { return reinterpret_cast<T&>(buffer); }
    const T& get() const { return reinterpret_cast<const T&>(buffer); }

  private:
    using OpaqueOps = OpaqueFunctions<T, N, Alignment>;
    OpaqueHandle<N, Alignment> buffer;
};

template <typename HandleType, HandleType InvalidSentinel, typename CloseReturnType,
          CloseReturnType (*DeleteFunc)(HandleType&)>
struct UniqueTaggedHandle
{
    UniqueTaggedHandle()                                           = default;
    UniqueTaggedHandle(const UniqueTaggedHandle& v)                = delete;
    UniqueTaggedHandle& operator=(const UniqueTaggedHandle& other) = delete;
    UniqueTaggedHandle(UniqueTaggedHandle&& v) : handle(v.handle) { v.detach(); }
    UniqueTaggedHandle(const HandleType& externalHandle) : handle(externalHandle) {}
    ~UniqueTaggedHandle() { (void)close(); }

    [[nodiscard]] CloseReturnType assign(UniqueTaggedHandle&& other)
    {
        if (close())
        {
            handle = other.handle;
            other.detach();
            return true;
        }
        return false;
    }

    [[nodiscard]] CloseReturnType assign(const HandleType& externalHandle)
    {
        if (close())
        {
            handle = externalHandle;
            return true;
        }
        return false;
    }

    UniqueTaggedHandle& operator=(UniqueTaggedHandle&& other)
    {
        (void)(assign(forward<UniqueTaggedHandle>(other)));
        return *this;
    }

    [[nodiscard]]      operator bool() const { return handle != InvalidSentinel; }
    [[nodiscard]] bool isValid() const { return handle != InvalidSentinel; }
    void               detach() { handle = InvalidSentinel; }

    [[nodiscard]] CloseReturnType get(HandleType& outHandle, CloseReturnType invalidReturnType) const
    {
        if (isValid())
        {
            outHandle = handle;
            return true;
        }
        return invalidReturnType;
    }

    [[nodiscard]] CloseReturnType close()
    {
        if (isValid())
        {
            HandleType handleCopy = handle;
            detach();
            return DeleteFunc(handleCopy);
        }
        return true;
    }

  private:
    HandleType handle = InvalidSentinel;
};

} // namespace SC
