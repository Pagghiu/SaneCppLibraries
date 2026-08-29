// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once

#include "OpenSSL3API.h"

#include <dlfcn.h>

namespace SC
{
namespace detail
{
struct OpenSSL3RuntimeLinux
{
    void*       library = nullptr;
    OpenSSL3API api;

    OpenSSL3RuntimeLinux()
    {
        library = ::dlopen("libcrypto.so.3", RTLD_NOW | RTLD_LOCAL);
        if (not api.resolve(library, resolveSymbol))
        {
            if (library != nullptr)
                ::dlclose(library);
            library = nullptr;
        }
    }

    static void* resolveSymbol(void* library, const char* name) { return ::dlsym(library, name); }
};

inline OpenSSL3API& openSSL3API()
{
    // A valid handle intentionally remains loaded for the process lifetime. This avoids invalidating provider objects
    // owned by sessions and keeps dynamic-library ownership separate from the platform-neutral EVP adapter.
    static OpenSSL3RuntimeLinux runtime;
    return runtime.api;
}
} // namespace detail
} // namespace SC
