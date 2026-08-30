// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once

// Platform-neutral fixed-storage interface to the optional OpenSSL 3 implementation.
// Cryptography.cpp includes this after Cryptography.h, so Result, Span, and Cryptography are already available.

namespace SC
{
namespace detail
{
struct OpenSSL3AeadBackend
{
    static constexpr size_t StorageSize = 64;
    alignas(void*) uint8_t storage[StorageSize];

    OpenSSL3AeadBackend();
    ~OpenSSL3AeadBackend();

    void   reset();
    Result init(Cryptography::AeadType type, Span<const uint8_t> key);
    Result seal(Span<const uint8_t> nonce, Span<const uint8_t> aad, Span<const uint8_t> plaintext,
                Span<uint8_t> ciphertext, Span<uint8_t> tag, size_t& bytesWritten);
    Result open(Span<const uint8_t> nonce, Span<const uint8_t> aad, Span<const uint8_t> ciphertext,
                Span<const uint8_t> tag, Span<uint8_t> plaintext, size_t& bytesWritten);
};

struct OpenSSL3CipherBackend
{
    static constexpr size_t StorageSize = 64;
    alignas(void*) uint8_t storage[StorageSize];

    OpenSSL3CipherBackend();
    ~OpenSSL3CipherBackend();

    void   reset();
    Result start(Cryptography::CipherType type, Cryptography::Cipher::Operation operation, Span<const uint8_t> key,
                 Span<const uint8_t> iv);
    Result update(Span<const uint8_t> input, Span<uint8_t> output, size_t& bytesWritten);
    Result finish(Span<uint8_t> output, size_t& bytesWritten);
};

struct OpenSSL3HmacBackend
{
    static constexpr size_t StorageSize = 48;
    alignas(void*) uint8_t storage[StorageSize];

    OpenSSL3HmacBackend();
    ~OpenSSL3HmacBackend();

    void   reset();
    Result setType(Cryptography::HashType type);
    Result setKey(Span<const uint8_t> key);
    Result add(Span<const uint8_t> data);
    Result getMac(Cryptography::MacResult& result);
};
} // namespace detail
} // namespace SC
