// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../Foundation/Language/Span.h"
namespace SC
{
struct Hashing
{
    struct Result
    {
        static constexpr auto MD5_DIGEST_LENGTH    = 16;
        static constexpr auto SHA1_DIGEST_LENGTH   = 20;
        static constexpr auto SHA256_DIGEST_LENGTH = 32;

        uint8_t hash[32] = {0};
        size_t  size     = 0;

        Span<const uint8_t> toBytesSpan() const { return {hash, size}; }
    };
    enum Type
    {
        TypeMD5,
        TypeSHA1,
        TypeSHA256
    };

    Hashing();
    ~Hashing();

    Hashing(const Hashing&)            = delete;
    Hashing(Hashing&&)                 = delete;
    Hashing& operator=(const Hashing&) = delete;
    Hashing& operator=(Hashing&&)      = delete;

    [[nodiscard]] bool update(Span<const uint8_t> data);
    [[nodiscard]] bool finalize(Result& res);
    [[nodiscard]] bool setType(Type newType);

  private:
#if SC_PLATFORM_APPLE
    alignas(uint64_t) char buffer[104];
#elif SC_PLATFORM_WINDOWS
    alignas(uint64_t) char buffer[16];
    struct CryptoPrivate;
    bool destroyHash();
#endif
    bool inited = false;
    Type type   = TypeMD5;
};
} // namespace SC
