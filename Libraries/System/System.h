// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../Foundation/Result.h"
#include "../Foundation/UniqueHandle.h"
#include "../Strings/SmallString.h"

namespace SC
{
struct SystemDirectories;
struct SystemDynamicLibrary;
namespace detail
{
struct SystemDynamicLibraryDefinition;
}
} // namespace SC

//! @defgroup group_system System
//! @copybrief library_system (see @ref library_system for more details)

//! @addtogroup group_system
//! @{

/// @brief Definition of native dynamic library handle for SystemDynamicLibrary
struct SC::detail::SystemDynamicLibraryDefinition
{
    using Handle = void*; // HANDLE
    static Result releaseHandle(Handle& handle);

    static constexpr Handle Invalid = nullptr; // INVALID_HANDLE_VALUE
};

/// @brief Loads dynamic libraries to obtain and invoke functions in current process.
struct SC::SystemDynamicLibrary : public SC::UniqueHandle<SC::detail::SystemDynamicLibraryDefinition>
{
    /// @brief Loads a dynamic library at given path
    /// @param fullPath Path where dynamic library exists
    /// @return Valid Result if dynamic library has been loaded successfully
    [[nodiscard]] Result load(StringView fullPath);

    /// @brief Obtains a function pointer exported from the dynamic library, casting to the wanted signature
    /// @tparam R Return type of the function exported from dynamic library
    /// @tparam Args Arguments to the function exported from dynamic library
    /// @param symbolName Name of the function
    /// @param[out] symbol The function pointer that has been read from the dynamic library
    /// @return `true` if a symbol with the given `symbolName` exists in the library
    template <typename R, typename... Args>
    [[nodiscard]] Result getSymbol(StringView symbolName, R (*&symbol)(Args...)) const
    {
        return loadSymbol(symbolName, reinterpret_cast<void*&>(symbol));
    }

  private:
    Result loadSymbol(StringView symbolName, void*& symbol) const;
};

/// @brief Reports location of system directories (executable / application root)
struct SC::SystemDirectories
{
    /// @brief Absolute executable path with extension (UTF16 on Windows, UTF8 elsewhere)
    StringView getExecutablePath() const { return executableFile.view(); }

    /// @brief Absolute Application path with extension (UTF16 on Windows, UTF8 elsewhere)
    /// @note on macOS this is different from SystemDirectories::getExecutablePath
    StringView getApplicationPath() const { return applicationRootDirectory.view(); }

    /// @brief Initializes the paths
    /// @return `true` if paths have been initialized correctly
    [[nodiscard]] bool init();

  private:
    static const int StaticPathSize = 1024 * sizeof(native_char_t);

    SmallString<StaticPathSize> executableFile;
    SmallString<StaticPathSize> applicationRootDirectory;
};

//! @}
