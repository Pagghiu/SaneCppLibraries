// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#ifdef SC_FOUNDATION_UNIQUEHANDLE_DEFINITION_H
#if SC_FOUNDATION_UNIQUEHANDLE_DEFINITION_H != 1
#error "UniqueHandle.h has been included multiple times in different versions."
#endif
#else
#define SC_FOUNDATION_UNIQUEHANDLE_DEFINITION_H 1 // Increment to indicate a new version of the file

#include "CompilerMacrosExport.h" // SC_FOUNDATION_EXPORT
#include "CompilerMove.h"         // forward<UniqueHandle>(other)

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
struct SC_FOUNDATION_EXPORT UniqueHandle
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
    [[nodiscard]] bool assign(UniqueHandle&& other)
    {
        if (other.handle == handle)
            return false;
        if (close())
        {
            handle = other.handle;
            other.detach();
            return true;
        }
        return false;
    }

    /// @brief Copy assigns another UniqueHandle to this object, eventually closing existing handle.
    /// @param externalHandle The handle to be copy assigned
    /// @return Returns invalid result if close failed
    [[nodiscard]] bool assign(const Handle& externalHandle)
    {
        if (handle == externalHandle)
            return false;
        if (close())
        {
            handle = externalHandle;
            return true;
        }
        return false;
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
    template <typename T>
    T get(Handle& outHandle, T invalidReturnType) const
    {
        if (isValid())
        {
            outHandle = handle;
            return T(true);
        }
        return invalidReturnType;
    }

    /// @brief Closes the handle by calling its OS specific close function
    /// @return `true` if the handle was closed correctly
    auto close()
    {
        if (isValid())
        {
            Handle handleCopy = handle;
            detach();
            return Definition::releaseHandle(handleCopy);
        }
        // Get the return type of releaseHandle and return a valid value, since we didn't need to close anything
        return decltype(Definition::releaseHandle(handle))(true);
    }

  protected:
    Handle handle = Invalid;
};

//! @}

} // namespace SC

#endif