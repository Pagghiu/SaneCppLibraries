// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once

#include "OpenSSL3API.h"

#include <dlfcn.h>

namespace SC
{
namespace detail
{
struct OpenSSL3RuntimePosix
{
    void*       library = nullptr;
    OpenSSL3API api;

    OpenSSL3RuntimePosix()
    {
#if SC_PLATFORM_NETBSD || SC_PLATFORM_FREEBSD
        // The base system library can use a different SONAME from the upstream OpenSSL 3 build.
        if (load("/usr/lib/libcrypto.so"))
            return;
#endif
        if (load("libcrypto.so.3"))
            return;
#if SC_PLATFORM_OPENBSD
        // OpenBSD resolves the versioned library in the port's private directory.
        if (load("/usr/local/lib/eopenssl35/libcrypto.so"))
            return;
        load("libcrypto.so");
#endif
    }

    bool load(const char* path)
    {
        library = ::dlopen(path, RTLD_NOW | RTLD_LOCAL);
        if (api.resolve(library, resolveSymbol))
            return true;
        if (library != nullptr)
            ::dlclose(library);
        library = nullptr;
        return false;
    }

    static void* resolveSymbol(void* library, const char* name) { return ::dlsym(library, name); }
};

inline OpenSSL3API& openSSL3API()
{
    // A valid handle intentionally remains loaded for the process lifetime. This avoids invalidating provider objects
    // owned by sessions and keeps dynamic-library ownership separate from the platform-neutral EVP adapter.
    static OpenSSL3RuntimePosix runtime;
    return runtime.api;
}
} // namespace detail
} // namespace SC
