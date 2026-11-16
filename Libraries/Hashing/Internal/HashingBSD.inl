// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
// BSD native headers define generic types and macros that can collide with OpenSSL in unity builds.
// Keep their types private, derive each function signature from the system declaration, and bind
// it to the native C symbol while restoring the caller's preprocessor state.
#include <sys/cdefs.h>
#include <sys/types.h>

namespace SC
{
namespace
{
constexpr size_t bsdHashingMD5Length    = Hashing::Result::MD5_DIGEST_LENGTH;
constexpr size_t bsdHashingSHA1Length   = Hashing::Result::SHA1_DIGEST_LENGTH;
constexpr size_t bsdHashingSHA256Length = Hashing::Result::SHA256_DIGEST_LENGTH;
} // namespace
} // namespace SC

// Save system digest macros so this implementation can be composed into a unity build.
#define SC_HASHING_BSD_PRAGMA(x) _Pragma(#x)
#define SC_HASHING_BSD_PUSH(x)   SC_HASHING_BSD_PRAGMA(push_macro(#x))
#define SC_HASHING_BSD_POP(x)    SC_HASHING_BSD_PRAGMA(pop_macro(#x))
// clang-format off
#define SC_HASHING_BSD_MACROS(X) \
    X(MD5_BLOCK_LENGTH) X(MD5_DIGEST_LENGTH) X(MD5_DIGEST_STRING_LENGTH) X(MD5Init) X(MD5Update) \
    X(MD5Pad) X(MD5Final) X(MD5Transform) X(MD5End) X(MD5Fd) X(MD5FdChunk) X(MD5File) \
    X(MD5FileChunk) X(MD5Data) X(SHA1_BLOCK_LENGTH) X(SHA1_DIGEST_LENGTH) \
    X(SHA1_DIGEST_STRING_LENGTH) X(SHA1_CTX) X(SHA1Init) X(SHA1Update) X(SHA1Final) X(SHA1_Init) \
    X(SHA1_Update) X(SHA1_Final) X(SHA1_End) X(SHA1_Fd) X(SHA1_FdChunk) X(SHA1_File) \
    X(SHA1_FileChunk) X(SHA1_Data) X(SHA1_Transform) X(sha1_block) X(SHA_BLOCK) X(SHA_CBLOCK) \
    X(SHA_LBLOCK) X(SHA_LAST_BLOCK) X(SHA_LENGTH_BLOCK) X(SHA_DIGEST_LENGTH) X(SHA_Init) \
    X(SHA_Update) X(SHA_Final) X(SHA_End) X(SHA_Fd) X(SHA_FdChunk) X(SHA_File) X(SHA_FileChunk) \
    X(SHA_Data) X(sha_block) X(SHA224_BLOCK_LENGTH) X(SHA224_DIGEST_LENGTH) \
    X(SHA224_DIGEST_STRING_LENGTH) X(SHA256_BLOCK_LENGTH) X(SHA256_DIGEST_LENGTH) \
    X(SHA256_DIGEST_STRING_LENGTH) X(SHA256_Init) X(SHA256_Update) X(SHA256_Final) X(SHA256_End) \
    X(SHA256_Fd) X(SHA256_FdChunk) X(SHA256_File) X(SHA256_FileChunk) X(SHA256_Data) \
    X(SHA256_Transform) X(SHA384_BLOCK_LENGTH) X(SHA384_DIGEST_LENGTH) \
    X(SHA384_DIGEST_STRING_LENGTH) X(SHA512_BLOCK_LENGTH) X(SHA512_DIGEST_LENGTH) \
    X(SHA512_DIGEST_STRING_LENGTH)
// clang-format on
SC_HASHING_BSD_MACROS(SC_HASHING_BSD_PUSH)
#undef MD5Init
#undef MD5Update
#undef MD5Final
#undef SHA1Init
#undef SHA1Update
#undef SHA1Final
#undef SHA1_Init
#undef SHA1_Update
#undef SHA1_Final
#undef SHA256_Init
#undef SHA256_Update
#undef SHA256_Final
#pragma push_macro("__BEGIN_DECLS")
#pragma push_macro("__END_DECLS")
#undef __BEGIN_DECLS
#undef __END_DECLS
#define __BEGIN_DECLS                                                                                                  \
    extern "C++"                                                                                                       \
    {
#define __END_DECLS }
#pragma push_macro("_MD5_H_")
#pragma push_macro("_SYS_MD5_H_")
#pragma push_macro("_SYS_SHA1_H_")
#pragma push_macro("__SHA2_H__")
#pragma push_macro("_SHA_H_")
#pragma push_macro("_SHA256_H_")
#pragma push_macro("_SHA1_H")
#pragma push_macro("_SHA2_H")
#undef _MD5_H_
#undef _SYS_MD5_H_
#undef _SYS_SHA1_H_
#undef __SHA2_H__
#undef _SHA_H_
#undef _SHA256_H_
#undef _SHA1_H
#undef _SHA2_H
namespace sc_hashing_bsd_native
{
#include <md5.h>
#if SC_PLATFORM_NETBSD || SC_PLATFORM_OPENBSD
#include <sha1.h>
#include <sha2.h>
#else
#include <sha.h>
#include <sha256.h>
#endif
} // namespace sc_hashing_bsd_native
#pragma pop_macro("_SHA2_H")
#pragma pop_macro("_SHA1_H")
#pragma pop_macro("_SHA256_H_")
#pragma pop_macro("_SHA_H_")
#pragma pop_macro("__SHA2_H__")
#pragma pop_macro("_SYS_SHA1_H_")
#pragma pop_macro("_SYS_MD5_H_")
#pragma pop_macro("_MD5_H_")
#pragma pop_macro("__END_DECLS")
#pragma pop_macro("__BEGIN_DECLS")

#define SC_HASHING_BSD_STRINGIFY_IMPL(x) #x
#define SC_HASHING_BSD_STRINGIFY(x)      SC_HASHING_BSD_STRINGIFY_IMPL(x)
namespace SC
{
namespace
{
using BSDHashingMD5Context = ::sc_hashing_bsd_native::MD5_CTX;
#if SC_PLATFORM_OPENBSD
using BSDHashingSHA256Context = ::sc_hashing_bsd_native::SHA2_CTX;
#else
using BSDHashingSHA256Context = ::sc_hashing_bsd_native::SHA256_CTX;
#endif
#if SC_PLATFORM_NETBSD || SC_PLATFORM_OPENBSD
using BSDHashingSHA1Context = ::sc_hashing_bsd_native::SHA1_CTX;

constexpr size_t bsdHashingNativeSHA1Length = SHA1_DIGEST_LENGTH;

extern "C" decltype(::sc_hashing_bsd_native::SHA1Init)   bsdHashingSHA1Init __asm__(SC_HASHING_BSD_STRINGIFY(SHA1Init));
extern "C" decltype(::sc_hashing_bsd_native::SHA1Update) bsdHashingSHA1Update __asm__(
    SC_HASHING_BSD_STRINGIFY(SHA1Update));
extern "C" decltype(::sc_hashing_bsd_native::SHA1Final) bsdHashingSHA1Final __asm__(
    SC_HASHING_BSD_STRINGIFY(SHA1Final));
#else
using BSDHashingSHA1Context = ::sc_hashing_bsd_native::SHA_CTX;

constexpr size_t bsdHashingNativeSHA1Length = SHA_DIGEST_LENGTH;

extern "C" decltype(::sc_hashing_bsd_native::SHA1_Init) bsdHashingSHA1Init __asm__(SC_HASHING_BSD_STRINGIFY(SHA1_Init));
extern "C" decltype(::sc_hashing_bsd_native::SHA1_Update) bsdHashingSHA1Update __asm__(
    SC_HASHING_BSD_STRINGIFY(SHA1_Update));
extern "C" decltype(::sc_hashing_bsd_native::SHA1_Final) bsdHashingSHA1Final __asm__(
    SC_HASHING_BSD_STRINGIFY(SHA1_Final));
#endif
constexpr size_t bsdHashingNativeMD5Length    = MD5_DIGEST_LENGTH;
constexpr size_t bsdHashingNativeSHA256Length = SHA256_DIGEST_LENGTH;

extern "C" decltype(::sc_hashing_bsd_native::MD5Init)   bsdHashingMD5Init __asm__(SC_HASHING_BSD_STRINGIFY(MD5Init));
extern "C" decltype(::sc_hashing_bsd_native::MD5Update) bsdHashingMD5Update __asm__(
    SC_HASHING_BSD_STRINGIFY(MD5Update));
extern "C" decltype(::sc_hashing_bsd_native::MD5Final) bsdHashingMD5Final __asm__(SC_HASHING_BSD_STRINGIFY(MD5Final));

#if SC_PLATFORM_OPENBSD
extern "C" decltype(::sc_hashing_bsd_native::SHA256Init)   bsdHashingSHA256Init __asm__("SHA256Init");
extern "C" decltype(::sc_hashing_bsd_native::SHA256Update) bsdHashingSHA256Update __asm__("SHA256Update");
extern "C" decltype(::sc_hashing_bsd_native::SHA256Final)  bsdHashingSHA256Final __asm__("SHA256Final");
#else
extern "C" decltype(::sc_hashing_bsd_native::SHA256_Init) bsdHashingSHA256Init __asm__(
    SC_HASHING_BSD_STRINGIFY(SHA256_Init));
extern "C" decltype(::sc_hashing_bsd_native::SHA256_Update) bsdHashingSHA256Update __asm__(
    SC_HASHING_BSD_STRINGIFY(SHA256_Update));
extern "C" decltype(::sc_hashing_bsd_native::SHA256_Final) bsdHashingSHA256Final __asm__(
    SC_HASHING_BSD_STRINGIFY(SHA256_Final));
#endif
} // namespace
} // namespace SC
#undef SC_HASHING_BSD_STRINGIFY
#undef SC_HASHING_BSD_STRINGIFY_IMPL
SC_HASHING_BSD_MACROS(SC_HASHING_BSD_POP)
#undef SC_HASHING_BSD_MACROS
#undef SC_HASHING_BSD_POP
#undef SC_HASHING_BSD_PUSH
#undef SC_HASHING_BSD_PRAGMA

SC::Hashing::Hashing()
{
    static_assert(sizeof(BSDHashingMD5Context) <= sizeof(buffer), "MD5 context must fit");
    static_assert(sizeof(BSDHashingSHA1Context) <= sizeof(buffer), "SHA1 context must fit");
    static_assert(sizeof(BSDHashingSHA256Context) <= sizeof(buffer), "SHA256 context must fit");
    static_assert(alignof(BSDHashingMD5Context) <= alignof(uint64_t), "MD5 context alignment");
    static_assert(alignof(BSDHashingSHA1Context) <= alignof(uint64_t), "SHA1 context alignment");
    static_assert(alignof(BSDHashingSHA256Context) <= alignof(uint64_t), "SHA256 context alignment");
    static_assert(bsdHashingNativeMD5Length == bsdHashingMD5Length, "MD5 digest length");
    static_assert(bsdHashingNativeSHA1Length == bsdHashingSHA1Length, "SHA1 digest length");
    static_assert(bsdHashingNativeSHA256Length == bsdHashingSHA256Length, "SHA256 digest length");
}

SC::Hashing::~Hashing() {}

bool SC::Hashing::setType(Type newType)
{
    inited = false;
    type   = newType;
    switch (type)
    {
    case TypeMD5: {
        auto& context = *reinterpret_cast<BSDHashingMD5Context*>(buffer);
        placementNew(context);
        bsdHashingMD5Init(&context);
        break;
    }
    case TypeSHA1: {
        auto& context = *reinterpret_cast<BSDHashingSHA1Context*>(buffer);
        placementNew(context);
        bsdHashingSHA1Init(&context);
        break;
    }
    case TypeSHA256: {
        auto& context = *reinterpret_cast<BSDHashingSHA256Context*>(buffer);
        placementNew(context);
#if SC_PLATFORM_NETBSD
        if (bsdHashingSHA256Init(&context) != 1)
            return false;
#else
        bsdHashingSHA256Init(&context);
#endif
        break;
    }
    default: return false;
    }
    inited = true;
    return true;
}

bool SC::Hashing::add(Span<const uint8_t> data)
{
    if (not inited)
        return false;
    if (data.empty())
        return true;
    if (type == TypeSHA256)
    {
#if SC_PLATFORM_NETBSD
        return bsdHashingSHA256Update(reinterpret_cast<BSDHashingSHA256Context*>(buffer), data.data(),
                                      data.sizeInBytes()) == 1;
#else
        bsdHashingSHA256Update(reinterpret_cast<BSDHashingSHA256Context*>(buffer), data.data(), data.sizeInBytes());
        return true;
#endif
    }

    constexpr size_t maxChunk  = static_cast<unsigned int>(-1);
    const uint8_t*   current   = data.data();
    size_t           remaining = data.sizeInBytes();
    while (remaining > 0)
    {
        const size_t chunk = remaining < maxChunk ? remaining : maxChunk;
        if (type == TypeMD5)
            bsdHashingMD5Update(reinterpret_cast<BSDHashingMD5Context*>(buffer), current,
                                static_cast<unsigned int>(chunk));
        else
            bsdHashingSHA1Update(reinterpret_cast<BSDHashingSHA1Context*>(buffer), current,
                                 static_cast<unsigned int>(chunk));
        current += chunk;
        remaining -= chunk;
    }
    return true;
}

bool SC::Hashing::getHash(Result& res)
{
    if (not inited)
        return false;
    switch (type)
    {
    case TypeMD5:
        bsdHashingMD5Final(res.hash, reinterpret_cast<BSDHashingMD5Context*>(buffer));
        res.size = bsdHashingMD5Length;
        return true;
    case TypeSHA1:
        bsdHashingSHA1Final(res.hash, reinterpret_cast<BSDHashingSHA1Context*>(buffer));
        res.size = bsdHashingSHA1Length;
        return true;
    case TypeSHA256:
#if SC_PLATFORM_NETBSD
        if (bsdHashingSHA256Final(res.hash, reinterpret_cast<BSDHashingSHA256Context*>(buffer)) != 1)
            return false;
#else
        bsdHashingSHA256Final(res.hash, reinterpret_cast<BSDHashingSHA256Context*>(buffer));
#endif
        res.size = bsdHashingSHA256Length;
        return true;
    }
    return false;
}
