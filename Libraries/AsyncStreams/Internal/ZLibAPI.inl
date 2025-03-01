// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "ZLibAPI.h"

#ifdef _WIN32
#include <Windows.h>
#include <stdlib.h>
#else
#include <dlfcn.h>
#endif

struct SC::ZLibAPI::Internal
{
    template <typename Func>
    static Result loadSymbol(ZLibAPI& zlib, Func& sym, const char* name)
    {
#ifdef _WIN32
        HMODULE hmodule;
        memcpy(&hmodule, &zlib.library, sizeof(HMODULE));
        auto func = ::GetProcAddress(hmodule, name);
        memcpy(&sym, &func, sizeof(func));
        static_assert(sizeof(Func) == sizeof(void*), "");
#else
        sym = reinterpret_cast<Func>(::dlsym(zlib.library, name));
#endif
        if (!sym)
        {
            return Result::Error("Failed to load zlib symbol");
        }
        return Result(true);
    }

#if _WIN32
    static Result GetClrCompressionPath(char* pathBuffer, size_t bufferSize)
    {
        const char* subKey    = "SOFTWARE\\Microsoft\\NET Framework Setup\\NDP\\v4\\Full";
        const char* valueName = "InstallPath";

        HKEY  hKey;
        DWORD valueType;
        DWORD valueSize = static_cast<DWORD>(bufferSize);

        // Open the registry key
        if (::RegOpenKeyExA(HKEY_LOCAL_MACHINE, subKey, 0, KEY_READ, &hKey) != ERROR_SUCCESS)
        {
            return Result::Error("GetClrCompressionPath: Failed to open registry key.");
        }

        // Query the InstallPath value
        if (::RegQueryValueExA(hKey, valueName, NULL, &valueType, (LPBYTE)pathBuffer, &valueSize) != ERROR_SUCCESS)
        {
            ::RegCloseKey(hKey);
            return Result::Error("GetClrCompressionPath: Failed to read registry value");
        }

        ::RegCloseKey(hKey);

        // Check the type of the registry value
        if (valueType != REG_SZ)
        {
            return Result::Error("GetClrCompressionPath: Unexpected registry value type.");
        }

        // Construct the path to clrcompression.dll
        if (::strcat_s(pathBuffer, bufferSize, "clrcompression.dll") != 0)
        {
            return Result::Error("GetClrCompressionPath: Not enough space");
        }
        return Result(true);
    }
#endif
};

SC::Result SC::ZLibAPI::load(const char* libPath)
{
    if (library != nullptr)
        return Result(true);
#if _WIN32
    const char* zlib_library_path = "zlib1.dll";
#elif __APPLE__
    const char* zlib_library_path = "libz.dylib";
#else
    const char* zlib_library_path = "libz.so.1";
#endif

    if (libPath == nullptr)
    {
        libPath = zlib_library_path;
    }

#ifdef _WIN32
    library = ::LoadLibraryA(libPath);
    if (library == nullptr)
    {
        char clr_zlib_path[MAX_PATH] = {0};
        SC_TRY(Internal::GetClrCompressionPath(clr_zlib_path, sizeof(clr_zlib_path)));
        library = ::LoadLibraryA(clr_zlib_path);
    }
#else
    library = ::dlopen(libPath, RTLD_NOW);
#endif

    if (!library)
    {
        return Result::Error("Failed to load zlib library");
    }
    // Load functions
    SC_TRY(Internal::loadSymbol(*this, pDeflate, "deflate"));
    SC_TRY(Internal::loadSymbol(*this, pDeflateEnd, "deflateEnd"));
    SC_TRY(Internal::loadSymbol(*this, pInflate, "inflate"));
    SC_TRY(Internal::loadSymbol(*this, pInflateEnd, "inflateEnd"));
    SC_TRY(Internal::loadSymbol(*this, pDeflateInit2, "deflateInit2_"));
    SC_TRY(Internal::loadSymbol(*this, pInflateInit2, "inflateInit2_"));
    return Result(true);
}

void SC::ZLibAPI::unload()
{
    if (library)
    {
#ifdef _WIN32
        HMODULE hmodule;
        memcpy(&hmodule, &library, sizeof(HMODULE));
        ::FreeLibrary(hmodule);
#else
        ::dlclose(library);
#endif
        library = nullptr;
    }
}
