// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once

// Platform-neutral OpenSSL 3 ABI declarations for the optional runtime backend.
// Platform code owns loading the dynamic library and passes its symbol resolver to resolve().
// Define SC_CRYPTOGRAPHY_OPENSSL3_USE_HEADERS=1 to verify these declarations against installed OpenSSL headers.

#include <stddef.h>
#include <stdint.h>

#ifndef SC_CRYPTOGRAPHY_OPENSSL3_USE_HEADERS
#define SC_CRYPTOGRAPHY_OPENSSL3_USE_HEADERS 0
#endif

#if SC_CRYPTOGRAPHY_OPENSSL3_USE_HEADERS
#include <openssl/core.h>
#include <openssl/crypto.h>
#include <openssl/err.h>
#include <openssl/evp.h>

namespace SC
{
namespace detail
{
using OpenSSL3Cipher         = ::EVP_CIPHER;
using OpenSSL3CipherCtx      = ::EVP_CIPHER_CTX;
using OpenSSL3LibraryContext = ::OSSL_LIB_CTX;
using OpenSSL3Mac            = ::EVP_MAC;
using OpenSSL3MacCtx         = ::EVP_MAC_CTX;
using OpenSSL3Param          = ::OSSL_PARAM;
} // namespace detail
} // namespace SC
#else
namespace SC
{
namespace detail
{
struct OpenSSL3Cipher;
struct OpenSSL3CipherCtx;
struct OpenSSL3LibraryContext;
struct OpenSSL3Mac;
struct OpenSSL3MacCtx;

struct OpenSSL3Param
{
    const char*  key;
    unsigned int dataType;
    void*        data;
    size_t       dataSize;
    size_t       returnSize;
};
} // namespace detail
} // namespace SC
#endif

namespace SC
{
namespace detail
{
struct OpenSSL3API
{
    using ResolveSymbol = void* (*)(void* library, const char* name);

    void* library = nullptr;

    unsigned int (*versionMajor)() = nullptr;

    OpenSSL3Cipher* (*cipherFetch)(OpenSSL3LibraryContext*, const char*, const char*) = nullptr;
    void (*cipherFree)(OpenSSL3Cipher*)                                               = nullptr;

    OpenSSL3CipherCtx* (*cipherContextNew)()                                     = nullptr;
    void (*cipherContextFree)(OpenSSL3CipherCtx*)                                = nullptr;
    int (*cipherInit)(OpenSSL3CipherCtx*, const OpenSSL3Cipher*, const uint8_t*, const uint8_t*, int,
                      const OpenSSL3Param*)                                      = nullptr;
    int (*cipherUpdate)(OpenSSL3CipherCtx*, uint8_t*, int*, const uint8_t*, int) = nullptr;
    int (*cipherFinal)(OpenSSL3CipherCtx*, uint8_t*, int*)                       = nullptr;
    int (*cipherSetPadding)(OpenSSL3CipherCtx*, int)                             = nullptr;
    int (*cipherGetParams)(OpenSSL3CipherCtx*, OpenSSL3Param*)                   = nullptr;
    int (*cipherSetParams)(OpenSSL3CipherCtx*, const OpenSSL3Param*)             = nullptr;

    OpenSSL3Mac* (*macFetch)(OpenSSL3LibraryContext*, const char*, const char*) = nullptr;
    void (*macFree)(OpenSSL3Mac*)                                               = nullptr;

    OpenSSL3MacCtx* (*macContextNew)(OpenSSL3Mac*)                                = nullptr;
    void (*macContextFree)(OpenSSL3MacCtx*)                                       = nullptr;
    int (*macInit)(OpenSSL3MacCtx*, const uint8_t*, size_t, const OpenSSL3Param*) = nullptr;
    int (*macUpdate)(OpenSSL3MacCtx*, const uint8_t*, size_t)                     = nullptr;
    int (*macFinal)(OpenSSL3MacCtx*, uint8_t*, size_t*, size_t)                   = nullptr;

    OpenSSL3Param (*paramUtf8String)(const char*, char*, size_t)  = nullptr;
    OpenSSL3Param (*paramOctetString)(const char*, void*, size_t) = nullptr;
    OpenSSL3Param (*paramEnd)()                                   = nullptr;

    unsigned long (*errorPeek)() = nullptr;
    int (*errorSetMark)()        = nullptr;
    int (*errorPopToMark)()      = nullptr;
    void (*errorClear)()         = nullptr;

    bool isValid() const { return library != nullptr; }

    template <typename Function>
    static bool resolveOne(void* library, ResolveSymbol resolveSymbol, Function& function, const char* name)
    {
        function = reinterpret_cast<Function>(resolveSymbol(library, name));
        return function != nullptr;
    }

    bool resolve(void* libraryHandle, ResolveSymbol resolveSymbol)
    {
        library = nullptr;
        if (libraryHandle == nullptr or resolveSymbol == nullptr)
            return false;

        bool valid = true;
#define SC_OPENSSL3_RESOLVE(field, name) valid = resolveOne(libraryHandle, resolveSymbol, field, name) and valid
        SC_OPENSSL3_RESOLVE(versionMajor, "OPENSSL_version_major");

        SC_OPENSSL3_RESOLVE(cipherFetch, "EVP_CIPHER_fetch");
        SC_OPENSSL3_RESOLVE(cipherFree, "EVP_CIPHER_free");
        SC_OPENSSL3_RESOLVE(cipherContextNew, "EVP_CIPHER_CTX_new");
        SC_OPENSSL3_RESOLVE(cipherContextFree, "EVP_CIPHER_CTX_free");
        SC_OPENSSL3_RESOLVE(cipherInit, "EVP_CipherInit_ex2");
        SC_OPENSSL3_RESOLVE(cipherUpdate, "EVP_CipherUpdate");
        SC_OPENSSL3_RESOLVE(cipherFinal, "EVP_CipherFinal_ex");
        SC_OPENSSL3_RESOLVE(cipherSetPadding, "EVP_CIPHER_CTX_set_padding");
        SC_OPENSSL3_RESOLVE(cipherGetParams, "EVP_CIPHER_CTX_get_params");
        SC_OPENSSL3_RESOLVE(cipherSetParams, "EVP_CIPHER_CTX_set_params");

        SC_OPENSSL3_RESOLVE(macFetch, "EVP_MAC_fetch");
        SC_OPENSSL3_RESOLVE(macFree, "EVP_MAC_free");
        SC_OPENSSL3_RESOLVE(macContextNew, "EVP_MAC_CTX_new");
        SC_OPENSSL3_RESOLVE(macContextFree, "EVP_MAC_CTX_free");
        SC_OPENSSL3_RESOLVE(macInit, "EVP_MAC_init");
        SC_OPENSSL3_RESOLVE(macUpdate, "EVP_MAC_update");
        SC_OPENSSL3_RESOLVE(macFinal, "EVP_MAC_final");

        SC_OPENSSL3_RESOLVE(paramUtf8String, "OSSL_PARAM_construct_utf8_string");
        SC_OPENSSL3_RESOLVE(paramOctetString, "OSSL_PARAM_construct_octet_string");
        SC_OPENSSL3_RESOLVE(paramEnd, "OSSL_PARAM_construct_end");

        SC_OPENSSL3_RESOLVE(errorPeek, "ERR_peek_error");
        SC_OPENSSL3_RESOLVE(errorSetMark, "ERR_set_mark");
        SC_OPENSSL3_RESOLVE(errorPopToMark, "ERR_pop_to_mark");
        SC_OPENSSL3_RESOLVE(errorClear, "ERR_clear_error");
#undef SC_OPENSSL3_RESOLVE

        if (valid and versionMajor() == 3)
            library = libraryHandle;
        return isValid();
    }
};

#if SC_CRYPTOGRAPHY_OPENSSL3_USE_HEADERS
static_assert(__is_same(decltype(OpenSSL3API::versionMajor), decltype(&::OPENSSL_version_major)));
static_assert(__is_same(decltype(OpenSSL3API::cipherFetch), decltype(&::EVP_CIPHER_fetch)));
static_assert(__is_same(decltype(OpenSSL3API::cipherFree), decltype(&::EVP_CIPHER_free)));
static_assert(__is_same(decltype(OpenSSL3API::cipherContextNew), decltype(&::EVP_CIPHER_CTX_new)));
static_assert(__is_same(decltype(OpenSSL3API::cipherContextFree), decltype(&::EVP_CIPHER_CTX_free)));
static_assert(__is_same(decltype(OpenSSL3API::cipherInit), decltype(&::EVP_CipherInit_ex2)));
static_assert(__is_same(decltype(OpenSSL3API::cipherUpdate), decltype(&::EVP_CipherUpdate)));
static_assert(__is_same(decltype(OpenSSL3API::cipherFinal), decltype(&::EVP_CipherFinal_ex)));
static_assert(__is_same(decltype(OpenSSL3API::cipherSetPadding), decltype(&::EVP_CIPHER_CTX_set_padding)));
static_assert(__is_same(decltype(OpenSSL3API::cipherGetParams), decltype(&::EVP_CIPHER_CTX_get_params)));
static_assert(__is_same(decltype(OpenSSL3API::cipherSetParams), decltype(&::EVP_CIPHER_CTX_set_params)));
static_assert(__is_same(decltype(OpenSSL3API::macFetch), decltype(&::EVP_MAC_fetch)));
static_assert(__is_same(decltype(OpenSSL3API::macFree), decltype(&::EVP_MAC_free)));
static_assert(__is_same(decltype(OpenSSL3API::macContextNew), decltype(&::EVP_MAC_CTX_new)));
static_assert(__is_same(decltype(OpenSSL3API::macContextFree), decltype(&::EVP_MAC_CTX_free)));
static_assert(__is_same(decltype(OpenSSL3API::macInit), decltype(&::EVP_MAC_init)));
static_assert(__is_same(decltype(OpenSSL3API::macUpdate), decltype(&::EVP_MAC_update)));
static_assert(__is_same(decltype(OpenSSL3API::macFinal), decltype(&::EVP_MAC_final)));
static_assert(__is_same(decltype(OpenSSL3API::paramUtf8String), decltype(&::OSSL_PARAM_construct_utf8_string)));
static_assert(__is_same(decltype(OpenSSL3API::paramOctetString), decltype(&::OSSL_PARAM_construct_octet_string)));
static_assert(__is_same(decltype(OpenSSL3API::paramEnd), decltype(&::OSSL_PARAM_construct_end)));
static_assert(__is_same(decltype(OpenSSL3API::errorPeek), decltype(&::ERR_peek_error)));
static_assert(__is_same(decltype(OpenSSL3API::errorSetMark), decltype(&::ERR_set_mark)));
static_assert(__is_same(decltype(OpenSSL3API::errorPopToMark), decltype(&::ERR_pop_to_mark)));
static_assert(__is_same(decltype(OpenSSL3API::errorClear), decltype(&::ERR_clear_error)));
#endif

struct OpenSSL3ErrorScope
{
    OpenSSL3API& api;
    bool         hadErrors;
    bool         marked;

    explicit OpenSSL3ErrorScope(OpenSSL3API& api)
        : api(api), hadErrors(api.errorPeek() != 0), marked(hadErrors and api.errorSetMark() == 1)
    {}

    ~OpenSSL3ErrorScope()
    {
        if (marked)
            api.errorPopToMark();
        else if (not hadErrors)
            api.errorClear();
    }
};
} // namespace detail
} // namespace SC
