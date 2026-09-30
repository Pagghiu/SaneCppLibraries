// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once

#include "../Common/Result.h"

namespace SC
{
//! @addtogroup group_cryptography
//! @{

/// @brief Stable portable failures returned by the Cryptography library.
enum class CryptographyError : uint32_t
{
    OperationUnsupported = 1,
    BackendUnavailable,
    InvalidAeadType,
    InvalidCipherType,
    InvalidHashType,
    InvalidCipherOperation,
    InvalidKeySize,
    InvalidNonceSize,
    InvalidInitializationVectorSize,
    InvalidTagSize,
    SizeLimitExceeded,
    OutputCapacityExceeded,
    BufferOverlap,
    SessionNotInitialized,
    KeyNotSet,
    InvalidCiphertext,
    AuthenticationFailed,
    RandomGenerationFailed,
    BackendInitializationFailed,
    BackendConfigurationFailed,
    BackendOperationFailed,
    InternalCapacityExceeded,
    UnexpectedOutputSize,
    InvalidBlockInputSize,
};

/// @brief Stable operation or backend stage retained for a Cryptography failure.
enum class CryptographyErrorDetail : uint16_t
{
    None = 0,
    FillRandom,
    InitializeAead,
    SealAead,
    OpenAead,
    StartCipher,
    UpdateCipher,
    FinishCipher,
    SetHmacType,
    SetHmacKey,
    AddHmacData,
    FinalizeHmac,
    DeriveHkdf,
    ValidateAeadKey,
    ValidateAeadNonce,
    ValidateAeadTag,
    ValidateAeadMessageSize,
    ValidateAeadOutput,
    ValidateAeadPartialOverlap,
    ValidateAeadSealCiphertextOverlap,
    ValidateAeadSealTagOverlap,
    ValidateAeadOpenPlaintextOverlap,
    ValidateCipherKey,
    ValidateCipherInitializationVector,
    ValidateCipherOutput,
    ValidateCipherOverlap,
    ValidateCipherBlockInput,
    ValidateCipherFinalBlock,
    ValidateCipherPadding,
    ValidateHkdfOutput,
    AppleCommonCryptoRandomGenerate,
    AppleCommonCryptoAeadCreate,
    AppleCommonCryptoAeadEncrypt,
    AppleCommonCryptoCipherCreate,
    AppleCommonCryptoCipherUpdate,
    WindowsBCryptRandomGenerate,
    WindowsBCryptAeadOpenAlgorithm,
    WindowsBCryptAeadSetGcmMode,
    WindowsBCryptAeadQueryObjectLength,
    WindowsBCryptAeadKeyObjectCapacity,
    WindowsBCryptAeadGenerateKey,
    WindowsBCryptAeadEncrypt,
    WindowsBCryptAeadDecrypt,
    WindowsBCryptCipherOpenAlgorithm,
    WindowsBCryptCipherSetCbcMode,
    WindowsBCryptCipherQueryObjectLength,
    WindowsBCryptCipherKeyObjectCapacity,
    WindowsBCryptCipherGenerateKey,
    WindowsBCryptCipherEncrypt,
    WindowsBCryptCipherDecrypt,
    WindowsBCryptHmacOpenAlgorithm,
    WindowsBCryptHmacQueryObjectLength,
    WindowsBCryptHmacObjectCapacity,
    WindowsBCryptHmacCreate,
    WindowsBCryptHmacUpdate,
    WindowsBCryptHmacFinalize,
    LinuxGetRandom,
    LinuxAFAlgCreateAlgorithmSocket,
    LinuxAFAlgBindAlgorithmSocket,
    LinuxAFAlgAcceptOperationSocket,
    LinuxAFAlgSetKey,
    LinuxAFAlgSetAeadAuthenticationSize,
    LinuxAFAlgAeadSealSend,
    LinuxAFAlgAeadSealReceive,
    LinuxAFAlgAeadOpenSend,
    LinuxAFAlgAeadOpenReceive,
    LinuxAFAlgCipherSend,
    LinuxAFAlgCipherReceive,
    LinuxAFAlgHmacSend,
    LinuxAFAlgHmacReceive,
    OpenSSL3RuntimeUnavailable,
    OpenSSL3AeadFetchCipher,
    OpenSSL3AeadCreateContext,
    OpenSSL3AeadInitialize,
    OpenSSL3AeadAssociatedDataUpdate,
    OpenSSL3AeadPayloadUpdate,
    OpenSSL3AeadSealFinalize,
    OpenSSL3AeadGetAuthenticationTag,
    OpenSSL3AeadSetAuthenticationTag,
    OpenSSL3AeadOpenFinalize,
    OpenSSL3CipherFetch,
    OpenSSL3CipherCreateContext,
    OpenSSL3CipherInitialize,
    OpenSSL3CipherSetPadding,
    OpenSSL3CipherUpdate,
    OpenSSL3HmacFetch,
    OpenSSL3HmacCreateContext,
    OpenSSL3HmacInitialize,
    OpenSSL3HmacUpdate,
    OpenSSL3HmacFinalize,
};

/// @brief Kind of the scalar diagnostic context stored in ResultCryptography.
enum class CryptographyErrorContextKind : uint16_t
{
    None = 0,
    AppleCommonCryptoStatus,
    WindowsNtStatus,
    PosixErrno,
    ExpectedBytes,
    RequiredBytes,
    MaximumBytes,
    ActualBytes,
};

/// @brief A typed scalar payload for a Cryptography error.
union CryptographyErrorContext
{
    struct AppleCommonCryptoStatus
    {
    };
    struct PosixErrno
    {
    };
    struct ExpectedBytes
    {
    };
    struct RequiredBytes
    {
    };
    struct MaximumBytes
    {
    };
    struct ActualBytes
    {
    };

    int32_t  appleCommonCryptoStatus;
    uint32_t windowsNtStatus;
    int32_t  posixErrno;
    uint32_t expectedBytes;
    uint32_t requiredBytes;
    uint32_t maximumBytes;
    uint32_t actualBytes;

    constexpr CryptographyErrorContext(uint32_t value = 0) : windowsNtStatus(value) {}
    constexpr CryptographyErrorContext(AppleCommonCryptoStatus, int32_t value) : appleCommonCryptoStatus(value) {}
    constexpr CryptographyErrorContext(PosixErrno, int32_t value) : posixErrno(value) {}
    constexpr CryptographyErrorContext(ExpectedBytes, uint32_t value) : expectedBytes(value) {}
    constexpr CryptographyErrorContext(RequiredBytes, uint32_t value) : requiredBytes(value) {}
    constexpr CryptographyErrorContext(MaximumBytes, uint32_t value) : maximumBytes(value) {}
    constexpr CryptographyErrorContext(ActualBytes, uint32_t value) : actualBytes(value) {}
};

/// @brief Stable category assigned to errors owned by Cryptography.
static constexpr ResultCategory CryptographyResultCategory = ResultCategory(9);

/// @brief Cryptography result retaining a stable operation detail and one scalar diagnostic context.
/// @details The composed Result is authoritative. Plain and foreign conversions clear Cryptography-specific context,
/// while same-domain copies retain it. Conversion to Result preserves only the category/error identity.
struct [[nodiscard]] ResultCryptography
{
    Result                       result;
    CryptographyErrorDetail      detail      = CryptographyErrorDetail::None;
    CryptographyErrorContextKind contextKind = CryptographyErrorContextKind::None;
    CryptographyErrorContext     context     = {};

    constexpr ResultCryptography() : result(true) {}
    explicit constexpr ResultCryptography(bool valid) : result(valid) {}
    constexpr ResultCryptography(CryptographyError       error,
                                 CryptographyErrorDetail detail = CryptographyErrorDetail::None)
        : result(Result::Error(CryptographyResultCategory, error)), detail(detail)
    {}
    constexpr ResultCryptography(CryptographyError error, CryptographyErrorDetail detail,
                                 CryptographyErrorContextKind contextKind, CryptographyErrorContext context)
        : result(Result::Error(CryptographyResultCategory, error)), detail(detail), contextKind(contextKind),
          context(context)
    {}
    constexpr ResultCryptography(Result result) : result(result) {}

    template <typename ResultLike>
    constexpr ResultCryptography(const ResultLike& other) : result(other.toResult())
    {}

    static constexpr ResultCryptography withAppleCommonCryptoStatus(CryptographyError       error,
                                                                    CryptographyErrorDetail detail, int32_t status)
    {
        return {error, detail, CryptographyErrorContextKind::AppleCommonCryptoStatus,
                CryptographyErrorContext(CryptographyErrorContext::AppleCommonCryptoStatus{}, status)};
    }

    static constexpr ResultCryptography withWindowsNtStatus(CryptographyError error, CryptographyErrorDetail detail,
                                                            uint32_t status)
    {
        return {error, detail, CryptographyErrorContextKind::WindowsNtStatus, CryptographyErrorContext(status)};
    }

    static constexpr ResultCryptography withPosixErrno(CryptographyError error, CryptographyErrorDetail detail,
                                                       int32_t errorNumber)
    {
        return {error, detail, CryptographyErrorContextKind::PosixErrno,
                CryptographyErrorContext(CryptographyErrorContext::PosixErrno{}, errorNumber)};
    }

    static constexpr ResultCryptography withExpectedBytes(CryptographyError error, CryptographyErrorDetail detail,
                                                          uint32_t expectedBytes)
    {
        return {error, detail, CryptographyErrorContextKind::ExpectedBytes,
                CryptographyErrorContext(CryptographyErrorContext::ExpectedBytes{}, expectedBytes)};
    }

    static constexpr ResultCryptography withRequiredBytes(CryptographyError error, CryptographyErrorDetail detail,
                                                          uint32_t requiredBytes)
    {
        return {error, detail, CryptographyErrorContextKind::RequiredBytes,
                CryptographyErrorContext(CryptographyErrorContext::RequiredBytes{}, requiredBytes)};
    }

    static constexpr ResultCryptography withMaximumBytes(CryptographyError error, CryptographyErrorDetail detail,
                                                         uint32_t maximumBytes)
    {
        return {error, detail, CryptographyErrorContextKind::MaximumBytes,
                CryptographyErrorContext(CryptographyErrorContext::MaximumBytes{}, maximumBytes)};
    }

    static constexpr ResultCryptography withActualBytes(CryptographyError error, CryptographyErrorDetail detail,
                                                        uint32_t actualBytes)
    {
        return {error, detail, CryptographyErrorContextKind::ActualBytes,
                CryptographyErrorContext(CryptographyErrorContext::ActualBytes{}, actualBytes)};
    }

    explicit constexpr operator bool() const { return static_cast<bool>(result); }
    constexpr          operator Result() const { return result; }
    constexpr Result   toResult() const { return result; }
    constexpr bool isError(CryptographyError error) const { return result.isError(CryptographyResultCategory, error); }
};

//! @}
} // namespace SC
