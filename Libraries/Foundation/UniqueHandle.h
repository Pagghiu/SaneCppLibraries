// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../Foundation/Result.h"

namespace SC
{
//! @addtogroup group_foundation_utility
//! @{

/// @brief  Move only handle that has a special tag value flagging its invalid state. @n
///         Typically used to wrap Operating System specific handles.
/// @tparam Definition A struct declaring handle type, release function and invalid handle value.
///
/// Example:
///
/// ... definition in header
/// @snippet Libraries/File/File.h UniqueHandleDeclaration1Snippet
/// ... derive from it
/// @snippet Libraries/File/File.h UniqueHandleDeclaration2Snippet
/// ...declaration in .cpp file
/// @snippet Libraries/File/File.cpp UniqueHandleDefinitionSnippet
/// ...usage
/// @snippet Tests/Libraries/File/FileTest.cpp UniqueHandleExampleSnippet
template <typename Definition>
struct SC_COMPILER_EXPORT UniqueHandle
{
    using Handle = typename Definition::Handle;

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
    Result assign(UniqueHandle&& other)
    {
        if (other.handle == handle)
            return Result(false);
        if (close())
        {
            handle = other.handle;
            other.detach();
            return Result(true);
        }
        return Result(false);
    }

    /// @brief Copy assigns another UniqueHandle to this object, eventually closing existing handle.
    /// @param externalHandle The handle to be copy assigned
    /// @return Returns invalid result if close failed
    Result assign(const Handle& externalHandle)
    {
        if (handle == externalHandle)
            return Result(false);
        if (close())
        {
            handle = externalHandle;
            return Result(true);
        }
        return Result(false);
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
    Result get(Handle& outHandle, Result invalidReturnType) const
    {
        if (isValid())
        {
            outHandle = handle;
            return Result(true);
        }
        return invalidReturnType;
    }

    /// @brief Closes the handle by calling its OS specific close function
    /// @return `true` if the handle was closed correctly
    Result close()
    {
        if (isValid())
        {
            Handle handleCopy = handle;
            detach();
            return Definition::releaseHandle(handleCopy);
        }
        return Result(true);
    }

  protected:
    Handle handle = Invalid;
};

//! @}

} // namespace SC
