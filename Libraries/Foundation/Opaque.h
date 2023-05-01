// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "Language.h"

namespace SC
{
template <typename T, size_t E, size_t R = sizeof(T)>
void static_assert_size()
{
    static_assert(R <= E, "Size mismatch");
}

template <int N, int Alignment>
struct OpaqueHandle
{
    template <typename T>
    T& reinterpret_as()
    {
        static_assert_size<T, N>();
        static_assert(alignof(T) <= Alignment, "Increase Alignment of OpaqueHandle");
        return *reinterpret_cast<T*>(bytes);
    }

  private:
    alignas(Alignment) char bytes[N];
};

template <typename T, typename Sizes, int A = alignof(void*)>
struct OpaqueTraits
{
    using Object = T;
#if SC_PLATFORM_WINDOWS
    static constexpr int Size = Sizes::Windows;
#elif SC_PLATFORM_APPLE
    static constexpr int Size = Sizes::Apple;
#else
    static constexpr int Size = Sizes::Default;
#endif
    static constexpr int Alignment = A;
    using Handle                   = OpaqueHandle<Size, Alignment>;
};

template <typename Traits>
struct OpaqueFuncs
{
    using Handle = typename Traits::Handle;
    using Object = typename Traits::Object;

    static constexpr int Size      = Traits::Size;
    static constexpr int Alignment = Traits::Alignment;

    static void construct(Handle& buffer);
    static void destruct(Object& obj);
    static void moveConstruct(Handle& buffer, Object&& obj);
    static void moveAssign(Object& pthis, Object&& obj);
};

template <typename OpaqueOps>
struct OpaqueUniqueObject
{
    using T                        = typename OpaqueOps::Object;
    static constexpr int N         = OpaqueOps::Size;
    static constexpr int Alignment = OpaqueOps::Alignment;

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
    OpaqueHandle<N, Alignment> buffer;
};

template <typename Traits>
struct UniqueTaggedHandleTraits
{
    using Handle                    = typename Traits::Handle;
    static constexpr Handle Invalid = Traits::Invalid;
    using CloseReturnType           = typename ReturnType<decltype(Traits::releaseHandle)>::type;

    UniqueTaggedHandleTraits()                                                 = default;
    UniqueTaggedHandleTraits(const UniqueTaggedHandleTraits& v)                = delete;
    UniqueTaggedHandleTraits& operator=(const UniqueTaggedHandleTraits& other) = delete;
    UniqueTaggedHandleTraits(UniqueTaggedHandleTraits&& v) : handle(v.handle) { v.detach(); }
    UniqueTaggedHandleTraits(const Handle& externalHandle) : handle(externalHandle) {}
    ~UniqueTaggedHandleTraits() { (void)close(); }

    [[nodiscard]] CloseReturnType assign(UniqueTaggedHandleTraits&& other)
    {
        if (close())
        {
            handle = other.handle;
            other.detach();
            return true;
        }
        return false;
    }

    [[nodiscard]] CloseReturnType assign(const Handle& externalHandle)
    {
        if (close())
        {
            handle = externalHandle;
            return true;
        }
        return false;
    }

    UniqueTaggedHandleTraits& operator=(UniqueTaggedHandleTraits&& other)
    {
        (void)(assign(forward<UniqueTaggedHandleTraits>(other)));
        return *this;
    }

    [[nodiscard]] bool isValid() const { return handle != Invalid; }

    void detach() { handle = Invalid; }

    [[nodiscard]] CloseReturnType get(Handle& outHandle, CloseReturnType invalidReturnType) const
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
            Handle handleCopy = handle;
            detach();
            return Traits::releaseHandle(handleCopy);
        }
        return true;
    }

  protected:
    Handle handle = Invalid;
};

} // namespace SC
