// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once

#include "OpenSSL3API.h"

namespace SC
{
namespace detail
{
struct OpenSSL3RuntimeWindows
{
    HMODULE     library = nullptr;
    OpenSSL3API api;

    OpenSSL3RuntimeWindows()
    {
#if defined(_M_ARM64) || defined(__aarch64__)
        static const char* candidates[] = {"libcrypto-3-arm64.dll", "libcrypto-3-aarch64.dll", "libcrypto-3.dll"};
#elif defined(_WIN64)
        static const char* candidates[] = {"libcrypto-3-x64.dll", "libcrypto-3.dll"};
#else
        static const char* candidates[] = {"libcrypto-3.dll"};
#endif
        for (const char* candidate : candidates)
        {
            library = ::LoadLibraryExA(candidate, nullptr, LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
            if (api.resolve(library, resolveSymbol))
                return;
            if (library != nullptr)
                ::FreeLibrary(library);
            library = nullptr;
        }
    }

    static void* resolveSymbol(void* library, const char* name)
    {
        return reinterpret_cast<void*>(::GetProcAddress(static_cast<HMODULE>(library), name));
    }
};

inline OpenSSL3API& openSSL3API()
{
    static OpenSSL3RuntimeWindows runtime;
    return runtime.api;
}
} // namespace detail
} // namespace SC
