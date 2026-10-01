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
#include <stdint.h>
#include <string.h>

#if SC_HTTPCLIENT_INCLUDE_CURL_HEADER
#include <curl/curl.h>
#else

// Minimal libcurl type definitions (enough for the HttpClient backend)
typedef void    CURL;
typedef void    CURLM;
typedef int64_t curl_off_t;

typedef int CURLcode;
#define CURLE_OK 0

typedef int CURLoption;
typedef int CURLINFO;
typedef enum
{
    CURLVERSION_FIRST = 0
} CURLversion;

// The fields through features are the stable CURLVERSION_FIRST prefix.
struct curl_version_info_data
{
    CURLversion  age;
    const char*  version;
    unsigned int version_num;
    const char*  host;
    int          features;
};

#define CURL_VERSION_SSL          (1 << 2)
#define CURL_VERSION_HTTP2        (1 << 16)

// Offsets per curl documentation (CURLOPT_* base values)
#define CURLOPTTYPE_LONG          0
#define CURLOPTTYPE_OBJECTPOINT   10000
#define CURLOPTTYPE_FUNCTIONPOINT 20000
#define CURLOPTTYPE_OFF_T         30000
#define CURLOPTTYPE_VALUES        CURLOPTTYPE_LONG

#define CURLOPT_URL              (CURLOPTTYPE_OBJECTPOINT + 2)
#define CURLOPT_UPLOAD           (CURLOPTTYPE_LONG + 46)
#define CURLOPT_POST             (CURLOPTTYPE_LONG + 47)
#define CURLOPT_CUSTOMREQUEST    (CURLOPTTYPE_OBJECTPOINT + 36)
#define CURLOPT_NOBODY           (CURLOPTTYPE_LONG + 44)
#define CURLOPT_INFILESIZE       (CURLOPTTYPE_LONG + 14)
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
#define CURLOPT_MAXREDIRS        (CURLOPTTYPE_LONG + 68)
#define CURLOPT_SSL_VERIFYPEER   (CURLOPTTYPE_LONG + 64)
#define CURLOPT_CAINFO           (CURLOPTTYPE_OBJECTPOINT + 65)
#define CURLOPT_SSL_VERIFYHOST   (CURLOPTTYPE_LONG + 81)
#define CURLOPT_NOSIGNAL         (CURLOPTTYPE_LONG + 99)
#define CURLOPT_NOPROGRESS       (CURLOPTTYPE_LONG + 43)
#define CURLOPT_XFERINFOFUNCTION (CURLOPTTYPE_FUNCTIONPOINT + 219)
#define CURLOPT_XFERINFODATA     (CURLOPTTYPE_OBJECTPOINT + 57)
#define CURLOPT_HTTP_VERSION     (CURLOPTTYPE_VALUES + 84)
#define CURLOPT_PROXY            (CURLOPTTYPE_OBJECTPOINT + 4)
#define CURLOPT_NOPROXY          (CURLOPTTYPE_OBJECTPOINT + 177)

#define CURL_HTTP_VERSION_NONE              0
#define CURL_HTTP_VERSION_1_0               1
#define CURL_HTTP_VERSION_1_1               2
#define CURL_HTTP_VERSION_2_0               3
#define CURL_HTTP_VERSION_2TLS              4
#define CURL_HTTP_VERSION_2_PRIOR_KNOWLEDGE 5

#define CURLINFO_STRING         0x100000
#define CURLINFO_LONG           0x200000
#define CURLINFO_EFFECTIVE_URL  (CURLINFO_STRING + 1)
#define CURLINFO_RESPONSE_CODE  (CURLINFO_LONG + 2)
#define CURLINFO_REDIRECT_COUNT (CURLINFO_LONG + 20)
#define CURLINFO_HTTP_VERSION   (CURLINFO_LONG + 46)

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
    CURL* (*curl_easy_init)()        = nullptr;
    void (*curl_easy_cleanup)(CURL*) = nullptr;
    void (*curl_easy_reset)(CURL*)   = nullptr;

    CURLcode (*curl_easy_perform)(CURL*) = nullptr;

    CURLcode (*curl_easy_setopt)(CURL*, CURLoption, ...) = nullptr;
    CURLcode (*curl_easy_getinfo)(CURL*, CURLINFO, ...)  = nullptr;

    curl_version_info_data* (*curl_version_info)(CURLversion) = nullptr;

    struct curl_slist* (*curl_slist_append)(struct curl_slist*, const char*) = nullptr;

    void (*curl_slist_free_all)(struct curl_slist*) = nullptr;

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
        curl_easy_setopt    = reinterpret_cast<decltype(curl_easy_setopt)>(::dlsym(libcurlHandle, "curl_easy_setopt"));
        curl_easy_getinfo   = reinterpret_cast<decltype(curl_easy_getinfo)>(::dlsym(libcurlHandle, "curl_easy_getinfo"));
        curl_version_info   = reinterpret_cast<decltype(curl_version_info)>(::dlsym(libcurlHandle, "curl_version_info"));
        curl_slist_append   = reinterpret_cast<decltype(curl_slist_append)>(::dlsym(libcurlHandle, "curl_slist_append"));
        curl_slist_free_all = reinterpret_cast<decltype(curl_slist_free_all)>(::dlsym(libcurlHandle, "curl_slist_free_all"));
        // clang-format on

        // Verify essential symbols
        if (curl_easy_init == nullptr or curl_easy_cleanup == nullptr or curl_easy_reset == nullptr or
            curl_easy_perform == nullptr or curl_easy_setopt == nullptr or curl_easy_getinfo == nullptr or
            curl_version_info == nullptr or curl_slist_append == nullptr or curl_slist_free_all == nullptr)
        {
            ::dlclose(libcurlHandle);
            libcurlHandle = nullptr;
            return false;
        }

        return true;
    }

    bool supportsFeature(int feature) const
    {
        if (curl_version_info == nullptr)
            return false;
        const curl_version_info_data* version = curl_version_info(CURLVERSION_FIRST);
        return version != nullptr and (version->features & feature) != 0;
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
