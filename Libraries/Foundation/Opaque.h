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
struct OpaqueFunctions
{
    template <int BufferSizeInBytes>
    static void construct(uint8_t* buffer);
    static void destruct(T& obj);
    static void moveConstruct(uint8_t* buffer, T&& obj);
    static void moveAssign(T& pthis, T&& obj);
};

template <typename T, int N = sizeof(void*)>
struct OpaqueUniqueObject
{
    static constexpr int BufferSizeInBytes = N;

    OpaqueUniqueObject() { OpaqueUniqueObjectOps::template construct<N>(buffer); }
    ~OpaqueUniqueObject() { OpaqueUniqueObjectOps::destruct(get()); }
    OpaqueUniqueObject(OpaqueUniqueObject&& other)
    {
        OpaqueUniqueObjectOps::moveConstruct(buffer, forward<T>(other.get()));
    }
    OpaqueUniqueObject& operator=(OpaqueUniqueObject&& other)
    {
        OpaqueUniqueObjectOps::moveAssign(get(), forward<T>(other.get()));
        return *this;
    }

    // Disallow copy construction and copy assignment
    OpaqueUniqueObject(const OpaqueUniqueObject&)            = delete;
    OpaqueUniqueObject& operator=(const OpaqueUniqueObject&) = delete;

    T&       get() { return reinterpret_cast<T&>(buffer); }
    const T& get() const { return reinterpret_cast<const T&>(buffer); }

  private:
    using OpaqueUniqueObjectOps = OpaqueFunctions<T>;
    alignas(uint64_t) uint8_t buffer[N];
};

template <typename HandleType, HandleType InvalidSentinel, typename CloseReturnType,
          CloseReturnType (*DeleteFunc)(HandleType&)>
struct OpaqueUniqueTaggedHandle
{
    static constexpr HandleType   InvalidHandle() { return InvalidSentinel; }
    [[nodiscard]] CloseReturnType assign(const HandleType& externalHandle)
    {
        if (close())
        {
            handle = externalHandle;
            return true;
        }
        return false;
    }

    OpaqueUniqueTaggedHandle()                                                 = default;
    OpaqueUniqueTaggedHandle(const OpaqueUniqueTaggedHandle& v)                = delete;
    OpaqueUniqueTaggedHandle& operator=(const OpaqueUniqueTaggedHandle& other) = delete;
    OpaqueUniqueTaggedHandle(OpaqueUniqueTaggedHandle&& v) : handle(v.handle) { v.detach(); }
    OpaqueUniqueTaggedHandle(const HandleType& externalHandle) : handle(externalHandle) {}
    ~OpaqueUniqueTaggedHandle() { (void)close(); }

    [[nodiscard]] CloseReturnType assignMovingFrom(OpaqueUniqueTaggedHandle& other)
    {
        if (close())
        {
            handle = other.handle;
            other.detach();
            return true;
        }
        return false;
    }
    OpaqueUniqueTaggedHandle& operator=(OpaqueUniqueTaggedHandle&& other)
    {
        (void)(assignMovingFrom(other));
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
