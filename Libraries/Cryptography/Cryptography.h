// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once

#include "../Common/CompilerMacrosExport.h"
#ifndef SC_EXPORT_LIBRARY_CRYPTOGRAPHY
#define SC_EXPORT_LIBRARY_CRYPTOGRAPHY 0
#endif
#define SC_CRYPTOGRAPHY_EXPORT SC_COMPILER_LIBRARY_EXPORT(SC_EXPORT_LIBRARY_CRYPTOGRAPHY)

#include "../Common/CompilerMacrosLifetimeBound.h"
#include "../Common/CompilerMove.h"
#include "../Common/OpaqueObject.h"
#include "../Common/Result.h"
#include "../Common/Span.h"

namespace SC
{
//! @defgroup group_cryptography Cryptography
//! @copybrief library_cryptography (see @ref library_cryptography for more details)

//! @addtogroup group_cryptography
//! @{

/// @brief OS-backed symmetric cryptography primitives.
/// @n
/// The library only wraps operating system cryptography APIs and keeps the core interface synchronous,
/// allocation-free and span-based.
/// @warning This is a Draft, unaudited low-level primitive API. Prefer AEAD, keep keys and nonces under an explicit
/// protocol policy, and use AES-CBC only for compatibility with an existing authenticated construction.
struct SC_CRYPTOGRAPHY_EXPORT Cryptography
{
    struct Features
    {
        bool secureRandom   = false;
        bool aes128Gcm      = false;
        bool aes256Gcm      = false;
        bool aes128CbcPkcs7 = false;
        bool aes256CbcPkcs7 = false;
        bool hmacSha256     = false;
        bool hmacSha384     = false;
        bool hkdfSha256     = false;
        bool hkdfSha384     = false;
    };

    enum class HashType : uint8_t
    {
        SHA256,
        SHA384,
    };

    enum class AeadType : uint8_t
    {
        AES128GCM,
        AES256GCM,
    };

    enum class CipherType : uint8_t
    {
        AES128CBCPKCS7,
        AES256CBCPKCS7,
    };

    struct MacResult
    {
        uint8_t bytes[48] = {0};
        size_t  size      = 0;

        Span<const uint8_t> toBytesSpan() const SC_LANGUAGE_LIFETIME_BOUND { return {bytes, size}; }
    };

    /// @brief Query which primitives are available on the current backend.
    static Result queryFeatures(Features& outFeatures);

    struct SC_CRYPTOGRAPHY_EXPORT Random
    {
        /// @brief Fill output with cryptographically secure random bytes.
        static Result fill(Span<uint8_t> output);
    };

    struct SC_CRYPTOGRAPHY_EXPORT Aead
    {
      private:
        struct Internal;
        struct InternalDefinition
        {
            static constexpr int Windows = 4600;
            static constexpr int Apple   = sizeof(void*);
            static constexpr int Linux   = 64;
            static constexpr int Default = Linux;

            static constexpr size_t Alignment = alignof(void*);

            using Object = Internal;
        };

      public:
        using InternalOpaque = OpaqueObject<InternalDefinition>;

      private:
        InternalOpaque internal;

      public:
        Aead();
        ~Aead();
        Aead(const Aead&)            = delete;
        Aead(Aead&&)                 = delete;
        Aead& operator=(const Aead&) = delete;
        Aead& operator=(Aead&&)      = delete;

        /// @brief Initialize an AEAD context with a single key.
        Result init(AeadType type, Span<const uint8_t> key);

        /// @brief Encrypt a single message using AEAD.
        /// @param nonce Nonce/IV for the message. Must be 12 bytes for AES-GCM and unique for every message under a
        /// key.
        /// @param aad Additional authenticated data. The Linux AF_ALG backend accepts at most 4096 bytes.
        /// @param plaintext Input plaintext.
        /// @param ciphertext Output ciphertext span. Must be at least plaintext size. Exact overlap is allowed; partial
        /// overlap is rejected.
        /// @param tag Output authentication tag. Must be 16 bytes.
        /// @param[out] bytesWritten Number of ciphertext bytes written.
        Result seal(Span<const uint8_t> nonce, Span<const uint8_t> aad, Span<const uint8_t> plaintext,
                    Span<uint8_t> ciphertext, Span<uint8_t> tag, size_t& bytesWritten);

        /// @brief Decrypt a single message using AEAD.
        /// @param nonce Nonce/IV for the message. Must be 12 bytes for AES-GCM.
        /// @param aad Additional authenticated data. The Linux AF_ALG backend accepts at most 4096 bytes.
        /// @param ciphertext Input ciphertext.
        /// @param tag Input authentication tag. Must be 16 bytes.
        /// @param plaintext Output plaintext span. Must be at least ciphertext size. Exact overlap is allowed; partial
        /// overlap is rejected. Authentication failure clears the complete output span.
        /// @param[out] bytesWritten Number of plaintext bytes written.
        Result open(Span<const uint8_t> nonce, Span<const uint8_t> aad, Span<const uint8_t> ciphertext,
                    Span<const uint8_t> tag, Span<uint8_t> plaintext, size_t& bytesWritten);

      private:
        friend decltype(internal);
    };

    struct SC_CRYPTOGRAPHY_EXPORT Cipher
    {
        enum class Operation : uint8_t
        {
            Encrypt,
            Decrypt,
        };

      private:
        struct Internal;
        struct InternalDefinition
        {
            static constexpr int Windows = 4600;
            static constexpr int Apple   = 512;
            static constexpr int Linux   = 128;
            static constexpr int Default = Linux;

            static constexpr size_t Alignment = alignof(void*);

            using Object = Internal;
        };

      public:
        using InternalOpaque = OpaqueObject<InternalDefinition>;

      private:
        InternalOpaque internal;

      public:
        Cipher();
        ~Cipher();
        Cipher(const Cipher&)            = delete;
        Cipher(Cipher&&)                 = delete;
        Cipher& operator=(const Cipher&) = delete;
        Cipher& operator=(Cipher&&)      = delete;

        /// @brief Start a legacy AES-CBC PKCS#7 operation.
        /// @warning AES-CBC does not authenticate ciphertext. Use only when required by an existing authenticated
        /// protocol. Encryption IVs must be unpredictable and freshly generated for each message.
        Result start(CipherType type, Operation operation, Span<const uint8_t> key, Span<const uint8_t> iv);

        /// @brief Process the next chunk of bytes.
        /// @note Input and output must not overlap. An output span of input size plus 15 bytes is always sufficient.
        /// An insufficient output span fails without consuming input and can be retried.
        Result update(Span<const uint8_t> input, Span<uint8_t> output, size_t& bytesWritten);

        /// @brief Finalize the operation and flush the remaining bytes / padding.
        /// @note Output must have at least 16 bytes. Success and invalid ciphertext or padding consume the session.
        Result finish(Span<uint8_t> output, size_t& bytesWritten);

        /// @brief Discard the current operation and clear library-owned session state.
        /// @note This operation is idempotent.
        void reset();

      private:
        friend decltype(internal);
    };

    struct SC_CRYPTOGRAPHY_EXPORT Hmac
    {
      private:
        struct Internal;
        struct InternalDefinition
        {
            static constexpr int Windows = 2600;
            static constexpr int Apple   = 512;
            static constexpr int Linux   = 64;
            static constexpr int Default = Linux;

            static constexpr size_t Alignment = alignof(void*);

            using Object = Internal;
        };

      public:
        using InternalOpaque = OpaqueObject<InternalDefinition>;

      private:
        InternalOpaque internal;

      public:
        Hmac();
        ~Hmac();
        Hmac(const Hmac&)            = delete;
        Hmac(Hmac&&)                 = delete;
        Hmac& operator=(const Hmac&) = delete;
        Hmac& operator=(Hmac&&)      = delete;

        /// @brief Select which hash family to use for HMAC.
        /// @note Calling setType resets any previously configured key or input state.
        Result setType(HashType type);

        /// @brief Set the HMAC key and reset the current running MAC state.
        Result setKey(Span<const uint8_t> key);

        /// @brief Add more message bytes to the running HMAC computation.
        Result add(Span<const uint8_t> data);

        /// @brief Finalize the current HMAC computation.
        /// @note Finalization consumes the session. Call setKey again before computing another MAC.
        Result getMac(MacResult& result);

        /// @brief Discard the current HMAC computation and clear library-owned session state.
        /// @note This operation is idempotent. The selected hash type is preserved.
        void reset();

      private:
        friend decltype(internal);
    };

    struct SC_CRYPTOGRAPHY_EXPORT Hkdf
    {
        /// @brief Derive output keying material with RFC5869 HKDF built on top of the OS HMAC primitive.
        /// @note Output is limited to 255 times the selected hash output size. An empty salt uses HashLen zero bytes.
        static Result derive(HashType type, Span<const uint8_t> salt, Span<const uint8_t> ikm, Span<const uint8_t> info,
                             Span<uint8_t> output);
    };
};

//! @}
} // namespace SC
