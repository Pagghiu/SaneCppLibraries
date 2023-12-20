// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../Foundation/TypeTraits.h" // ReturnType<T>

namespace SC
{
//! @addtogroup group_foundation_utility
//! @{

/// @brief  Move only handle that has a special tag value flagging its invalid state. @n
///         Typically used to wrap Operating System specific handles.
/// @tparam Definition A struct declaring handle type, release function and invalid handle value.
///
/// Example:
/**
 * @code{.cpp}
    // ... definition in header
    struct PosixFileDescriptorDefinition
    {
        using Handle = int; // fd
        static Result releaseHandle(Handle& handle);

        static constexpr Handle Invalid = -1; // invalid fd
    };

    using PosixFileDescriptor = UniqueHandle<PosixFileDescriptorDefinition>;

    // ...declaration in .cpp file

    Result PosixFileDescriptorDefinition::releaseHandle(Handle& handle)
    {
        if (::close(handle) != 0)
        {
            return Result::Error("releaseHandle - close failed");
        }
        return Result(true);
    }

    // ... usage
    PosixFileDescriptor myDescriptor;

    const int nativeFd = ::open(filePath.getNullTerminatedNative(), flags, access);

    // Assign the native handle to UniqueHandle (will release the existing one, if any)
    myDescriptor.assign(nativeFd);

    // UniqueHandle can only be moved, but not copied
    PosixFileDescriptor otherDescriptor = move(myDescriptor);
    // PosixFileDescriptor otherDescriptor = myDescriptor; // <- Doesn't compile

    // Explicitly close (or it will be automatically released on scope close / destructor)
    otherDescriptor.close()

    // If detach() is called, the handle will be made invalid without releasing it
    otherDescriptor.detach()

    // Check handle for validity
    if(otherDescriptor.isValid())
    {
        // ... do something
    }
 @endcode
*/
template <typename Definition>
struct UniqueHandle
{
    using Handle          = typename Definition::Handle;
    using CloseReturnType = typename TypeTraits::ReturnType<decltype(Definition::releaseHandle)>::type;

    static constexpr Handle Invalid = Definition::Invalid;

    UniqueHandle()                                     = default;
    UniqueHandle(const UniqueHandle& v)                = delete;
    UniqueHandle& operator=(const UniqueHandle& other) = delete;
    UniqueHandle(UniqueHandle&& v) : handle(v.handle) { v.detach(); }
    UniqueHandle(const Handle& externalHandle) : handle(externalHandle) {}
    ~UniqueHandle() { (void)close(); }

    /// @brief Move assigns another UniqueHandle to this object, eventually closing existing handle.
    /// @param other The handle to be move-assigned
    /// @return Returns invalid result if close failed
    [[nodiscard]] CloseReturnType assign(UniqueHandle&& other)
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

    UniqueHandle& operator=(UniqueHandle&& other)
    {
        (void)(assign(forward<UniqueHandle>(other)));
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

//! @}

} // namespace SC
