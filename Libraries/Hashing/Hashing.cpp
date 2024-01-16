// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "Hashing.h"

#if SC_PLATFORM_APPLE
#include <CommonCrypto/CommonDigest.h>

#if SC_COMPILER_CLANG
// CC_MD5_* are deprecated
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#endif
SC::Hashing::Hashing()
{
    static_assert(sizeof(CC_MD5_CTX) <= sizeof(buffer), "Check size");    // 92
    static_assert(sizeof(CC_SHA1_CTX) <= sizeof(buffer), "Check size");   // 96
    static_assert(sizeof(CC_SHA256_CTX) == sizeof(buffer), "Check size"); // 104
    static_assert(alignof(CC_MD5_CTX) <= alignof(uint64_t), "Check size");
    static_assert(CC_MD5_DIGEST_LENGTH == Result::MD5_DIGEST_LENGTH, "Check Size");
    static_assert(CC_SHA1_DIGEST_LENGTH == Result::SHA1_DIGEST_LENGTH, "Check Size");
    static_assert(CC_SHA256_DIGEST_LENGTH == Result::SHA256_DIGEST_LENGTH, "Check Size");
}

SC::Hashing::~Hashing() {}

bool SC::Hashing::setType(Type newType)
{
    inited = true;
    type   = newType;
    switch (type)
    {
    case TypeMD5: CC_MD5_Init(reinterpret_cast<CC_MD5_CTX*>(buffer)); return true;
    case TypeSHA1: CC_SHA1_Init(reinterpret_cast<CC_SHA1_CTX*>(buffer)); return true;
    case TypeSHA256: CC_SHA256_Init(reinterpret_cast<CC_SHA256_CTX*>(buffer)); return true;
    }
    return false;
}

bool SC::Hashing::update(Span<const uint8_t> data)
{
    if (not inited)
        return false;
    switch (type)
    {
    case TypeMD5:
        CC_MD5_Update(reinterpret_cast<CC_MD5_CTX*>(buffer), data.data(), static_cast<CC_LONG>(data.sizeInBytes()));
        break;
    case TypeSHA1:
        CC_SHA1_Update(reinterpret_cast<CC_SHA1_CTX*>(buffer), data.data(), static_cast<CC_LONG>(data.sizeInBytes()));
        break;
    case TypeSHA256:
        CC_SHA256_Update(reinterpret_cast<CC_SHA256_CTX*>(buffer), data.data(),
                         static_cast<CC_LONG>(data.sizeInBytes()));
        break;
    }
    return true;
}

bool SC::Hashing::finalize(Result& res)
{
    if (!inited)
        return false;
    switch (type)
    {
    case TypeMD5:
        CC_MD5_Final(res.hash, reinterpret_cast<CC_MD5_CTX*>(buffer));
        res.size = Result::MD5_DIGEST_LENGTH;
        break;
    case TypeSHA1:
        CC_SHA1_Final(res.hash, reinterpret_cast<CC_SHA1_CTX*>(buffer));
        res.size = Result::SHA1_DIGEST_LENGTH;
        break;
    case TypeSHA256:
        CC_SHA256_Final(res.hash, reinterpret_cast<CC_SHA256_CTX*>(buffer));
        res.size = Result::SHA256_DIGEST_LENGTH;
        break;
    }
    return true;
}
#if SC_COMPILER_CLANG
#pragma clang diagnostic pop
#endif
#elif SC_PLATFORM_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <wincrypt.h>

struct SC::Hashing::CryptoPrivate
{
    HCRYPTPROV hCryptProv = 0;
    HCRYPTHASH hHash      = 0;

    static auto getFromType(Hashing::Type type)
    {
        switch (type)
        {
        case TypeMD5: return CALG_MD5;
        case TypeSHA1: return CALG_SHA1;
        case TypeSHA256: return CALG_SHA_256;
        }
        return CALG_MD5;
    }
};

SC::Hashing::Hashing()
{
    memset(buffer, 0, sizeof(buffer));
    CryptoPrivate& self = *reinterpret_cast<CryptoPrivate*>(buffer);
    if (!CryptAcquireContext(&self.hCryptProv, NULL, MS_ENH_RSA_AES_PROV, PROV_RSA_AES, CRYPT_VERIFYCONTEXT))
    {
        return;
    }
}

SC::Hashing::~Hashing()
{
    CryptoPrivate& self = *reinterpret_cast<CryptoPrivate*>(buffer);
    if (inited)
    {
        CryptDestroyHash(self.hHash);
    }
    CryptReleaseContext(self.hCryptProv, 0);
}

bool SC::Hashing::setType(Type newType)
{
    CryptoPrivate& self = *reinterpret_cast<CryptoPrivate*>(buffer);
    if (inited)
    {
        CryptDestroyHash(self.hHash);
    }
    inited = false;
    type   = newType;
    if (!CryptCreateHash(self.hCryptProv, CryptoPrivate::getFromType(newType), 0, 0, &self.hHash))
    {
        CryptReleaseContext(self.hCryptProv, 0);
        return false;
    }
    inited = true;
    return true;
}

bool SC::Hashing::update(Span<const uint8_t> data)
{
    if (not inited)
        return false;
    CryptoPrivate& self = *reinterpret_cast<CryptoPrivate*>(buffer);
    if (!CryptHashData(self.hHash, data.data(), static_cast<DWORD>(data.sizeInBytes()), 0))
    {
        return false;
    }
    return true;
}

bool SC::Hashing::finalize(Result& res)
{
    CryptoPrivate& self     = *reinterpret_cast<CryptoPrivate*>(buffer);
    DWORD          hashSize = sizeof(Result::hash);
    if (!CryptGetHashParam(self.hHash, HP_HASHVAL, res.hash, &hashSize, 0))
    {
        return false;
    }
    res.size = hashSize;
    return true;
}

#elif SC_PLATFORM_LINUX
#include <linux/if_alg.h>
#include <sys/socket.h>
#include <unistd.h>

#include "../Foundation/Deferred.h"

SC::Hashing::Hashing() {}

SC::Hashing::~Hashing()
{
    if (inited)
    {
        ::close(hashSocket);
        ::close(mainSocket);
    }
}

bool SC::Hashing::setType(Type newType)
{
    struct sockaddr_alg sa = {0};
    switch (newType)
    {
    case TypeMD5: sa = {.salg_family = AF_ALG, .salg_type = "hash", .salg_name = "md5"}; break;
    case TypeSHA1: sa = {.salg_family = AF_ALG, .salg_type = "hash", .salg_name = "sha1"}; break;
    case TypeSHA256: sa = {.salg_family = AF_ALG, .salg_type = "hash", .salg_name = "sha256"}; break;
    }
    if (inited)
    {
        if (hashSocket != -1)
            ::close(hashSocket);
        hashSocket = -1;

        if (mainSocket != -1)
            ::close(mainSocket);
        mainSocket = -1;
    }

    inited = false;
    type   = newType;

    mainSocket = ::socket(AF_ALG, SOCK_SEQPACKET, 0);
    if (mainSocket == -1)
    {
        return false;
    }
    auto deferCloseSock = MakeDeferred(
        [&]
        {
            ::close(mainSocket);
            mainSocket = -1;
        });

    if (::bind(mainSocket, reinterpret_cast<struct sockaddr*>(&sa), sizeof(sa)) == -1)
    {
        return false;
    }

    hashSocket = ::accept(mainSocket, NULL, 0);
    if (hashSocket == -1)
    {
        return false;
    }

    deferCloseSock.disarm();
    inited = true;
    return true;
}

bool SC::Hashing::update(Span<const uint8_t> data)
{
    if (!inited)
        return false;

    if (::send(hashSocket, data.data(), data.sizeInBytes(), MSG_MORE) == -1)
    {
        return false;
    }

    return true;
}

bool SC::Hashing::finalize(Result& res)
{
    if (!inited)
        return false;

    if (::recv(hashSocket, res.hash, sizeof(res.hash), 0) == -1)
    {
        return false;
    }
    switch (type)
    {
    case TypeMD5: res.size = Result::MD5_DIGEST_LENGTH; break;
    case TypeSHA1: res.size = Result::SHA1_DIGEST_LENGTH; break;
    case TypeSHA256: res.size = Result::SHA256_DIGEST_LENGTH; break;
    }

    return true;
}

#else

SC::Hashing::Hashing() {}
SC::Hashing::~Hashing() {}
bool SC::Hashing::setType(Type newType) { return false; }
bool SC::Hashing::update(Span<const uint8_t> data) { return false; }
bool SC::Hashing::finalize(Result& res) { return false; }

#endif
