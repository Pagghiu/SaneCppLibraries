// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../../Foundation/Result.h"
#include "../../Foundation/UniqueHandle.h"
#include "../../Strings/StringView.h"

namespace SC
{
struct SystemDynamicLibrary;
namespace detail
{
struct SystemDynamicLibraryDefinition;
}
} // namespace SC

//! @addtogroup group_plugin
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
    Result load(StringView fullPath);

    /// @brief Obtains a function pointer exported from the dynamic library, casting to the wanted signature
    /// @tparam R Return type of the function exported from dynamic library
    /// @tparam Args Arguments to the function exported from dynamic library
    /// @param symbolName Name of the function
    /// @param[out] symbol The function pointer that has been read from the dynamic library
    /// @return `true` if a symbol with the given `symbolName` exists in the library
    template <typename R, typename... Args>
    Result getSymbol(StringView symbolName, R (*&symbol)(Args...)) const
    {
        return loadSymbol(symbolName, reinterpret_cast<void*&>(symbol));
    }

  private:
    Result loadSymbol(StringView symbolName, void*& symbol) const;
};

//! @}
