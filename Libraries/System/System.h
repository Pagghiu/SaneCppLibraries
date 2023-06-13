// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../Foundation/Opaque.h"
#include "../Foundation/String.h"

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
    ReturnCode getSymbol(StringView symbolName, R (*&symbol)(Args...))
    {
        return loadSymbol(symbolName, reinterpret_cast<void*&>(symbol));
    }

  private:
    ReturnCode loadSymbol(StringView symbolName, void*& symbol);
};

struct SC::SystemDebug
{
    [[nodiscard]] static bool   printBacktrace();
    [[nodiscard]] static bool   printBacktrace(void** backtraceBuffer, size_t backtraceBufferSizeInBytes);
    [[nodiscard]] static size_t captureBacktrace(size_t framesToSkip, void** backtraceBuffer,
                                                 size_t backtraceBufferSizeInBytes, uint32_t* hash);
};

struct SC::SystemDirectories
{
    static const int StaticPathSize = 1024 * sizeof(utf_char_t);

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
