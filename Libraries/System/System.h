// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../Foundation/Result.h"
#include "../Foundation/UniqueHandle.h"
#include "../Strings/SmallString.h"

namespace SC
{
struct SystemDebug;
struct SystemDirectories;
struct SystemFunctions;
struct SystemDynamicLibrary;
struct SystemDynamicLibraryDefinition;
} // namespace SC

struct SC::SystemDynamicLibraryDefinition
{
    using Handle = void*; // HANDLE
    static Result releaseHandle(Handle& handle);

    static constexpr Handle Invalid = nullptr; // INVALID_HANDLE_VALUE
};

struct SC::SystemDynamicLibrary : public SC::UniqueHandle<SC::SystemDynamicLibraryDefinition>
{
    Result load(StringView fullPath);
    template <typename R, typename... Args>
    Result getSymbol(StringView symbolName, R (*&symbol)(Args...)) const
    {
        return loadSymbol(symbolName, reinterpret_cast<void*&>(symbol));
    }

  private:
    Result loadSymbol(StringView symbolName, void*& symbol) const;
};

struct SC::SystemDebug
{
    // Support deleting locked PDB files
    [[nodiscard]] static bool   isDebuggerConnected();
    [[nodiscard]] static Result unlockFileFromAllProcesses(StringView fileName);
    [[nodiscard]] static Result deleteForcefullyUnlockedFile(StringView fileName);

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

    [[nodiscard]] Result      initNetworking();
    [[nodiscard]] Result      shutdownNetworking();
    [[nodiscard]] static bool isNetworkingInited();

  private:
    struct Internal;
    SystemFunctions(const SystemFunctions&)            = delete;
    SystemFunctions& operator=(const SystemFunctions&) = delete;
    SystemFunctions(SystemFunctions&&)                 = delete;
    SystemFunctions& operator=(SystemFunctions&&)      = delete;
};
