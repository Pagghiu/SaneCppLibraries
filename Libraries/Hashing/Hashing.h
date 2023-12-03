// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../Foundation/Span.h"
namespace SC
{
//! @defgroup group_hashing Hashing
//! @copybrief library_hashing (see  @ref library_hashing for more details)

//! @addtogroup group_hashing
//! @{

/// @brief Compute MD5, SHA1 or SHA256 hash for stream of data
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
        TypeMD5,   ///< Compute MD5 hash for the incoming stream of bytes
        TypeSHA1,  ///< Compute SHA1 hash for the incoming stream of bytes
        TypeSHA256 ///< Compute SHA256 hash for the incoming stream of bytes
    };

    /// @brief Initializes an Hashing struct
    Hashing();

    /// @brief Destroys an Hashing struct
    ~Hashing();

    Hashing(const Hashing&)            = delete;
    Hashing(Hashing&&)                 = delete;
    Hashing& operator=(const Hashing&) = delete;
    Hashing& operator=(Hashing&&)      = delete;

    /// @brief Add data to be hashed. Can be called multiple times before Hashing::finalize
    /// @param data Data to be hashed
    /// @return `true` if data has been hashed successfully
    [[nodiscard]] bool update(Span<const uint8_t> data);

    /// @brief Finalizes hash computation that has been pushed through Hashing::update
    /// @param[out] res Result object holding the actual Result::hash
    /// @return `true` if the final hash has been computed successfully
    [[nodiscard]] bool finalize(Result& res);

    /// @brief Set type of hash to compute
    /// @param newType MD5, SHA1, SHA256
    /// @return `true` if the hash type has been changed successfully
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
//! @}
} // namespace SC
