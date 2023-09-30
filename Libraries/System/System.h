// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../Foundation/Objects/Opaque.h"
#include "../Foundation/Strings/String.h"

namespace SC
{
struct SystemDebug;
struct SystemDirectories;
struct SystemFunctions;
struct SystemDynamicLibrary;
struct SystemDynamicLibraryTraits;
} // namespace SC

struct SC::SystemDynamicLibraryTraits
{
    using Handle                    = void*;   // HANDLE
    static constexpr Handle Invalid = nullptr; // INVALID_HANDLE_VALUE
    static ReturnCode       releaseHandle(Handle& handle);
};

struct SC::SystemDynamicLibrary : public SC::UniqueTaggedHandleTraits<SC::SystemDynamicLibraryTraits>
{
    ReturnCode load(StringView fullPath);
    template <typename R, typename... Args>
    ReturnCode getSymbol(StringView symbolName, R (*&symbol)(Args...)) const
    {
        return loadSymbol(symbolName, reinterpret_cast<void*&>(symbol));
    }

  private:
    ReturnCode loadSymbol(StringView symbolName, void*& symbol) const;
};

struct SC::SystemDebug
{
    // Support deleting locked PDB files
    [[nodiscard]] static bool       isDebuggerConnected();
    [[nodiscard]] static ReturnCode unlockFileFromAllProcesses(StringView fileName);
    [[nodiscard]] static ReturnCode deleteForcefullyUnlockedFile(StringView fileName);

  private:
    struct Internal;
};

struct SC::SystemDirectories
{
    static const int StaticPathSize = 1024 * sizeof(native_char_t);

    SmallString<StaticPathSize> executableFile; // Full path (native encoding) to executable file including extension
    SmallString<StaticPathSize> applicationRootDirectory; // Full path to (native encoding) Application directory

    [[nodiscard]] bool init();
};

struct SC::SystemFunctions
{
    SystemFunctions() = default;
    ~SystemFunctions();

    [[nodiscard]] ReturnCode  initNetworking();
    [[nodiscard]] ReturnCode  shutdownNetworking();
    [[nodiscard]] static bool isNetworkingInited();

  private:
    struct Internal;
    SystemFunctions(const SystemFunctions&)            = delete;
    SystemFunctions& operator=(const SystemFunctions&) = delete;
    SystemFunctions(SystemFunctions&&)                 = delete;
    SystemFunctions& operator=(SystemFunctions&&)      = delete;
};
