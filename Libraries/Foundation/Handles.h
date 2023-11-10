// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../Foundation/TypeTraits.h" // ReturnType<T>

namespace SC
{
template <typename T, size_t E, size_t R = sizeof(T)>
void static_assert_size()
{
    static_assert(R <= E, "Size mismatch");
}
//! @addtogroup group_foundation_utility
//! @{

/// @brief  Holds an Operating System handle of size `N` to avoid public inclusion of header where it's defined.
///         This is a measure to lower compile time and reduce implementation details leaks.
///         For example it's used used to wrap SocketIPAddress, a Mutex and ConditionVariable.
/// @tparam N Size in bytes of the Operating System Handle
/// @tparam Alignment Alignment in Bytes of the operating system Handle
template <int N, int Alignment = alignof(void*)>
struct OpaqueHandle
{
    /// @brief  Access wanted OS Handle with it's actual type.
    ///         This is typically done in a .cpp file where the concrete type of the Handle is known
    ///         For example it is used inside  Mutex class like:
    /// @code
    /// pthread_mutex_t& myMutes = handle.reinterpret_as<pthread_mutex_t>()
    /// @endcode
    /// @tparam T Type of the handle. It will statically check size and alignment of requested type.
    /// @return A reference to actual OS handle
    template <typename T>
    T& reinterpret_as()
    {
        static_assert_size<T, N>();
        static_assert(alignof(T) <= Alignment, "Increase Alignment of OpaqueHandle");
        return *reinterpret_cast<T*>(bytes);
    }

    /// @brief  Access wanted OS Handle with it's actual type.
    ///         This is typically done in a .cpp file where the concrete type of the Handle is known
    ///         For example it is used inside  Mutex class like:
    /// @code
    /// pthread_mutex_t& myMutes = handle.reinterpret_as<pthread_mutex_t>()
    /// @endcode
    /// @tparam T Type of the handle. It will statically check size and alignment of requested type.
    /// @return A reference to actual OS handle
    template <typename T>
    const T& reinterpret_as() const
    {
        static_assert_size<T, N>();
        static_assert(alignof(T) <= Alignment, "Increase Alignment of OpaqueHandle");
        return *reinterpret_cast<const T*>(bytes);
    }

  private:
    alignas(Alignment) char bytes[N];
};

/// @brief Wraps an non-copyable (move-only) Operating System handle.
/// @tparam Definition A struct declaring handle type, release function and invalid handle value.
template <typename Definition>
struct UniqueTaggedHandle
{
    using Handle          = typename Definition::Handle;
    using CloseReturnType = typename ReturnType<decltype(Definition::releaseHandle)>::type;

    static constexpr Handle Invalid = Definition::Invalid;

    UniqueTaggedHandle()                                           = default;
    UniqueTaggedHandle(const UniqueTaggedHandle& v)                = delete;
    UniqueTaggedHandle& operator=(const UniqueTaggedHandle& other) = delete;
    UniqueTaggedHandle(UniqueTaggedHandle&& v) : handle(v.handle) { v.detach(); }
    UniqueTaggedHandle(const Handle& externalHandle) : handle(externalHandle) {}
    ~UniqueTaggedHandle() { (void)close(); }

    /// @brief Move assigns another UniqueHandle to this object, eventually closing existing handle.
    /// @param other The handle to be move-assigned
    /// @return Returns invalid result if close failed
    [[nodiscard]] CloseReturnType assign(UniqueTaggedHandle&& other)
    {
        if (other.handle == handle)
            return CloseReturnType(false);
        if (close())
        {
            handle = other.handle;
            other.detach();
            return CloseReturnType(true);
        }
        return CloseReturnType(false);
    }

    /// @brief Copy assigns another UniqueHandle to this object, eventually closing existing handle.
    /// @param externalHandle The handle to be copy assigned
    /// @return Returns invalid result if close failed
    [[nodiscard]] CloseReturnType assign(const Handle& externalHandle)
    {
        if (handle == externalHandle)
            return CloseReturnType(false);
        if (close())
        {
            handle = externalHandle;
            return CloseReturnType(true);
        }
        return CloseReturnType(false);
    }

    UniqueTaggedHandle& operator=(UniqueTaggedHandle&& other)
    {
        (void)(assign(forward<UniqueTaggedHandle>(other)));
        return *this;
    }

    /// @brief Check if current handle is valid
    /// @return `true` if handle is valid
    [[nodiscard]] bool isValid() const { return handle != Invalid; }

    /// @brief Detaches (sets to invalid) current handle, without closing it
    void detach() { handle = Invalid; }

    /// @brief Extracts the native operating system handle out.
    /// @param outHandle Output native OS handle
    /// @param invalidReturnType the value to be returned if this function fails
    /// @return invalidReturnType if isValid() == `false`
    [[nodiscard]] CloseReturnType get(Handle& outHandle, CloseReturnType invalidReturnType) const
    {
        if (isValid())
        {
            outHandle = handle;
            return CloseReturnType(true);
        }
        return invalidReturnType;
    }

    /// @brief Closes the handle by calling its OS specific close function
    /// @return `true` if the handle was closed correctly
    [[nodiscard]] CloseReturnType close()
    {
        if (isValid())
        {
            Handle handleCopy = handle;
            detach();
            return Definition::releaseHandle(handleCopy);
        }
        return CloseReturnType(true);
    }

  protected:
    Handle handle = Invalid;
};

//! }@

} // namespace SC
