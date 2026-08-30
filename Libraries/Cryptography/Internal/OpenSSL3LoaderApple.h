// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once

#include "OpenSSL3API.h"

#include <dlfcn.h>

namespace SC
{
namespace detail
{
struct OpenSSL3RuntimeApple
{
    void*       library = nullptr;
    OpenSSL3API api;

    OpenSSL3RuntimeApple()
    {
        static const char* candidates[] = {
            "libcrypto.3.dylib",
            "/opt/homebrew/opt/openssl@3/lib/libcrypto.3.dylib",
            "/usr/local/opt/openssl@3/lib/libcrypto.3.dylib",
            "/opt/local/lib/libcrypto.3.dylib",
        };
        for (const char* candidate : candidates)
        {
            library = ::dlopen(candidate, RTLD_NOW | RTLD_LOCAL);
            if (api.resolve(library, resolveSymbol))
                return;
            if (library != nullptr)
                ::dlclose(library);
            library = nullptr;
        }
    }

    static void* resolveSymbol(void* library, const char* name) { return ::dlsym(library, name); }
};

inline OpenSSL3API& openSSL3API()
{
    static OpenSSL3RuntimeApple runtime;
    return runtime.api;
}
} // namespace detail
} // namespace SC
