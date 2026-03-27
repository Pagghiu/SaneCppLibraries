// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
// Embedded libcurl type definitions and dlopen loader for Linux backend.
// Modeled after Libraries/Async/Internal/AsyncLinuxAPI.h (io_uring pattern).
//
// By default we supply our own minimal libcurl struct and enum definitions.
// Define SC_HTTPCLIENT_INCLUDE_CURL_HEADER=1 to use the real <curl/curl.h> instead.

#pragma once
#include <dlfcn.h>
#include <stddef.h>
#include <string.h>

#if SC_HTTPCLIENT_INCLUDE_CURL_HEADER
#include <curl/curl.h>
#else

// Minimal libcurl type definitions (enough for the HttpClient backend)
typedef void CURL;
typedef void CURLM;

typedef int CURLcode;
#define CURLE_OK                  0

typedef int CURLoption;
// Offsets per curl documentation (CURLOPT_* base values)
#define CURLOPTTYPE_LONG          0
#define CURLOPTTYPE_OBJECTPOINT   10000
#define CURLOPTTYPE_FUNCTIONPOINT 20000
#define CURLOPTTYPE_OFF_T         30000

#define CURLOPT_URL              (CURLOPTTYPE_OBJECTPOINT + 2)
#define CURLOPT_POST             (CURLOPTTYPE_LONG + 47)
#define CURLOPT_CUSTOMREQUEST    (CURLOPTTYPE_OBJECTPOINT + 36)
#define CURLOPT_NOBODY           (CURLOPTTYPE_LONG + 44)
#define CURLOPT_POSTFIELDS       (CURLOPTTYPE_OBJECTPOINT + 15)
#define CURLOPT_POSTFIELDSIZE    (CURLOPTTYPE_LONG + 60)
#define CURLOPT_HTTPHEADER       (CURLOPTTYPE_OBJECTPOINT + 23)
#define CURLOPT_WRITEFUNCTION    (CURLOPTTYPE_FUNCTIONPOINT + 11)
#define CURLOPT_WRITEDATA        (CURLOPTTYPE_OBJECTPOINT + 1)
#define CURLOPT_READFUNCTION     (CURLOPTTYPE_FUNCTIONPOINT + 12)
#define CURLOPT_READDATA         (CURLOPTTYPE_OBJECTPOINT + 9)
#define CURLOPT_HEADERFUNCTION   (CURLOPTTYPE_FUNCTIONPOINT + 79)
#define CURLOPT_HEADERDATA       (CURLOPTTYPE_OBJECTPOINT + 29)
#define CURLOPT_TIMEOUT_MS       (CURLOPTTYPE_LONG + 155)
#define CURLOPT_FOLLOWLOCATION   (CURLOPTTYPE_LONG + 52)
#define CURLOPT_NOPROGRESS       (CURLOPTTYPE_LONG + 43)
#define CURLOPT_XFERINFOFUNCTION (CURLOPTTYPE_FUNCTIONPOINT + 219)
#define CURLOPT_XFERINFODATA     (CURLOPTTYPE_OBJECTPOINT + 57)

typedef int CURLINFO;
#define CURLINFO_LONG            0x200000
#define CURLINFO_RESPONSE_CODE   (CURLINFO_LONG + 2)

#define CURL_READFUNC_ABORT 0x10000000

struct curl_slist
{
    char*              data;
    struct curl_slist* next;
};

#endif // SC_HTTPCLIENT_INCLUDE_CURL_HEADER

// ── Loader struct: dlopen("libcurl.so") and resolve symbols via dlsym ──

struct HttpClientLinuxLibCurlLoader
{
    void* libcurlHandle = nullptr;

    // Function pointers for the libcurl functions we use
    CURL* (*curl_easy_init)()                                                = nullptr;
    void (*curl_easy_cleanup)(CURL*)                                         = nullptr;
    void (*curl_easy_reset)(CURL*)                                           = nullptr;
    int (*curl_easy_perform)(CURL*)                                          = nullptr;
    int (*curl_easy_setopt_long)(CURL*, int, long)                           = nullptr;
    int (*curl_easy_setopt_ptr)(CURL*, int, const void*)                     = nullptr;
    int (*curl_easy_getinfo_long)(CURL*, int, long*)                         = nullptr;
    struct curl_slist* (*curl_slist_append)(struct curl_slist*, const char*) = nullptr;
    void (*curl_slist_free_all)(struct curl_slist*)                          = nullptr;

    bool isValid() const { return libcurlHandle != nullptr; }

    [[nodiscard]] bool init()
    {
        if (libcurlHandle)
            return true;

        // Try common libcurl shared library names
        libcurlHandle = ::dlopen("libcurl.so.4", RTLD_NOW);
        if (libcurlHandle == nullptr)
            libcurlHandle = ::dlopen("libcurl.so", RTLD_NOW);
        if (libcurlHandle == nullptr)
            return false;

        // clang-format off
        curl_easy_init      = reinterpret_cast<decltype(curl_easy_init)>(::dlsym(libcurlHandle, "curl_easy_init"));
        curl_easy_cleanup   = reinterpret_cast<decltype(curl_easy_cleanup)>(::dlsym(libcurlHandle, "curl_easy_cleanup"));
        curl_easy_reset     = reinterpret_cast<decltype(curl_easy_reset)>(::dlsym(libcurlHandle, "curl_easy_reset"));
        curl_easy_perform   = reinterpret_cast<decltype(curl_easy_perform)>(::dlsym(libcurlHandle, "curl_easy_perform"));
        curl_slist_append   = reinterpret_cast<decltype(curl_slist_append)>(::dlsym(libcurlHandle, "curl_slist_append"));
        curl_slist_free_all = reinterpret_cast<decltype(curl_slist_free_all)>(::dlsym(libcurlHandle, "curl_slist_free_all"));
        // clang-format on

        // curl_easy_setopt is variadic, so we cast to specific function signatures
        auto setopt           = reinterpret_cast<void*>(::dlsym(libcurlHandle, "curl_easy_setopt"));
        curl_easy_setopt_long = reinterpret_cast<decltype(curl_easy_setopt_long)>(setopt);
        curl_easy_setopt_ptr  = reinterpret_cast<decltype(curl_easy_setopt_ptr)>(setopt);

        auto getinfo           = reinterpret_cast<void*>(::dlsym(libcurlHandle, "curl_easy_getinfo"));
        curl_easy_getinfo_long = reinterpret_cast<decltype(curl_easy_getinfo_long)>(getinfo);

        // Verify essential symbols
        if (curl_easy_init == nullptr or curl_easy_cleanup == nullptr or curl_easy_perform == nullptr)
        {
            ::dlclose(libcurlHandle);
            libcurlHandle = nullptr;
            return false;
        }

        return true;
    }

    void close()
    {
        if (libcurlHandle)
        {
            ::dlclose(libcurlHandle);
            libcurlHandle = nullptr;
        }
    }
};
