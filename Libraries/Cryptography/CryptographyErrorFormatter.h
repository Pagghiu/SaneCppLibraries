// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once

#include "../Common/ResultErrorFormatter.h"
#include "CryptographyError.h"

namespace SC
{
namespace detail
{
inline bool appendCryptographyError(ResultErrorFormatter& formatter, CryptographyError error)
{
    switch (error)
    {
    case CryptographyError::OperationUnsupported: formatter.append("Cryptographic operation is unsupported"); break;
    case CryptographyError::BackendUnavailable: formatter.append("Cryptographic backend is unavailable"); break;
    case CryptographyError::InvalidAeadType: formatter.append("AEAD type is invalid"); break;
    case CryptographyError::InvalidCipherType: formatter.append("Cipher type is invalid"); break;
    case CryptographyError::InvalidHashType: formatter.append("Hash type is invalid"); break;
    case CryptographyError::InvalidCipherOperation: formatter.append("Cipher operation is invalid"); break;
    case CryptographyError::InvalidKeySize: formatter.append("Key size is invalid"); break;
    case CryptographyError::InvalidNonceSize: formatter.append("Nonce size is invalid"); break;
    case CryptographyError::InvalidInitializationVectorSize:
        formatter.append("Initialization vector size is invalid");
        break;
    case CryptographyError::InvalidTagSize: formatter.append("Authentication tag size is invalid"); break;
    case CryptographyError::SizeLimitExceeded: formatter.append("Cryptographic size limit exceeded"); break;
    case CryptographyError::OutputCapacityExceeded: formatter.append("Output capacity exceeded"); break;
    case CryptographyError::BufferOverlap: formatter.append("Buffer overlap is not allowed"); break;
    case CryptographyError::SessionNotInitialized: formatter.append("Cryptographic session is not initialized"); break;
    case CryptographyError::KeyNotSet: formatter.append("HMAC key is not set"); break;
    case CryptographyError::InvalidCiphertext: formatter.append("Ciphertext is invalid"); break;
    case CryptographyError::AuthenticationFailed: formatter.append("Authentication failed"); break;
    case CryptographyError::RandomGenerationFailed: formatter.append("Secure random generation failed"); break;
    case CryptographyError::BackendInitializationFailed:
        formatter.append("Cryptographic backend initialization failed");
        break;
    case CryptographyError::BackendConfigurationFailed:
        formatter.append("Cryptographic backend configuration failed");
        break;
    case CryptographyError::BackendOperationFailed: formatter.append("Cryptographic backend operation failed"); break;
    case CryptographyError::InternalCapacityExceeded:
        formatter.append("Cryptographic internal capacity exceeded");
        break;
    case CryptographyError::UnexpectedOutputSize: formatter.append("Cryptographic output size was unexpected"); break;
    case CryptographyError::InvalidBlockInputSize: formatter.append("Cipher block input size is invalid"); break;
    default: return false;
    }
    return true;
}

inline bool appendCryptographyErrorDetail(ResultErrorFormatter& formatter, CryptographyErrorDetail detail)
{
    switch (detail)
    {
    case CryptographyErrorDetail::None: return true;
    case CryptographyErrorDetail::FillRandom: formatter.append("fill secure random bytes"); break;
    case CryptographyErrorDetail::InitializeAead: formatter.append("initialize AEAD session"); break;
    case CryptographyErrorDetail::SealAead: formatter.append("seal AEAD message"); break;
    case CryptographyErrorDetail::OpenAead: formatter.append("open AEAD message"); break;
    case CryptographyErrorDetail::StartCipher: formatter.append("start cipher session"); break;
    case CryptographyErrorDetail::UpdateCipher: formatter.append("update cipher session"); break;
    case CryptographyErrorDetail::FinishCipher: formatter.append("finish cipher session"); break;
    case CryptographyErrorDetail::SetHmacType: formatter.append("select HMAC hash type"); break;
    case CryptographyErrorDetail::SetHmacKey: formatter.append("set HMAC key"); break;
    case CryptographyErrorDetail::AddHmacData: formatter.append("add HMAC data"); break;
    case CryptographyErrorDetail::FinalizeHmac: formatter.append("finalize HMAC"); break;
    case CryptographyErrorDetail::DeriveHkdf: formatter.append("derive HKDF output"); break;
    case CryptographyErrorDetail::ValidateAeadKey: formatter.append("validate AEAD key"); break;
    case CryptographyErrorDetail::ValidateAeadNonce: formatter.append("validate AEAD nonce"); break;
    case CryptographyErrorDetail::ValidateAeadTag: formatter.append("validate AEAD authentication tag"); break;
    case CryptographyErrorDetail::ValidateAeadMessageSize: formatter.append("validate AEAD message size"); break;
    case CryptographyErrorDetail::ValidateAeadOutput: formatter.append("validate AEAD output capacity"); break;
    case CryptographyErrorDetail::ValidateAeadPartialOverlap: formatter.append("validate AEAD partial overlap"); break;
    case CryptographyErrorDetail::ValidateAeadSealCiphertextOverlap:
        formatter.append("validate AEAD ciphertext overlap");
        break;
    case CryptographyErrorDetail::ValidateAeadSealTagOverlap:
        formatter.append("validate AEAD authentication tag overlap");
        break;
    case CryptographyErrorDetail::ValidateAeadOpenPlaintextOverlap:
        formatter.append("validate AEAD plaintext overlap");
        break;
    case CryptographyErrorDetail::ValidateCipherKey: formatter.append("validate cipher key"); break;
    case CryptographyErrorDetail::ValidateCipherInitializationVector:
        formatter.append("validate cipher initialization vector");
        break;
    case CryptographyErrorDetail::ValidateCipherOutput: formatter.append("validate cipher output capacity"); break;
    case CryptographyErrorDetail::ValidateCipherOverlap: formatter.append("validate cipher buffer overlap"); break;
    case CryptographyErrorDetail::ValidateCipherBlockInput: formatter.append("validate cipher block input"); break;
    case CryptographyErrorDetail::ValidateCipherFinalBlock: formatter.append("validate cipher final block"); break;
    case CryptographyErrorDetail::ValidateCipherPadding: formatter.append("validate cipher padding"); break;
    case CryptographyErrorDetail::ValidateHkdfOutput: formatter.append("validate HKDF output size"); break;
    case CryptographyErrorDetail::AppleCommonCryptoRandomGenerate:
        formatter.append("Apple CommonCrypto generate random bytes");
        break;
    case CryptographyErrorDetail::AppleCommonCryptoAeadCreate:
        formatter.append("Apple CommonCrypto create AEAD cryptor");
        break;
    case CryptographyErrorDetail::AppleCommonCryptoAeadEncrypt:
        formatter.append("Apple CommonCrypto encrypt AEAD blocks");
        break;
    case CryptographyErrorDetail::AppleCommonCryptoCipherCreate:
        formatter.append("Apple CommonCrypto create cipher");
        break;
    case CryptographyErrorDetail::AppleCommonCryptoCipherUpdate:
        formatter.append("Apple CommonCrypto update cipher");
        break;
    case CryptographyErrorDetail::WindowsBCryptRandomGenerate:
        formatter.append("Windows BCrypt generate random bytes");
        break;
    case CryptographyErrorDetail::WindowsBCryptAeadOpenAlgorithm:
        formatter.append("Windows BCrypt open AEAD algorithm");
        break;
    case CryptographyErrorDetail::WindowsBCryptAeadSetGcmMode:
        formatter.append("Windows BCrypt set AEAD GCM mode");
        break;
    case CryptographyErrorDetail::WindowsBCryptAeadQueryObjectLength:
        formatter.append("Windows BCrypt query AEAD object length");
        break;
    case CryptographyErrorDetail::WindowsBCryptAeadKeyObjectCapacity:
        formatter.append("Windows BCrypt validate AEAD key-object capacity");
        break;
    case CryptographyErrorDetail::WindowsBCryptAeadGenerateKey:
        formatter.append("Windows BCrypt generate AEAD key");
        break;
    case CryptographyErrorDetail::WindowsBCryptAeadEncrypt:
        formatter.append("Windows BCrypt encrypt AEAD message");
        break;
    case CryptographyErrorDetail::WindowsBCryptAeadDecrypt:
        formatter.append("Windows BCrypt decrypt AEAD message");
        break;
    case CryptographyErrorDetail::WindowsBCryptCipherOpenAlgorithm:
        formatter.append("Windows BCrypt open cipher algorithm");
        break;
    case CryptographyErrorDetail::WindowsBCryptCipherSetCbcMode:
        formatter.append("Windows BCrypt set cipher CBC mode");
        break;
    case CryptographyErrorDetail::WindowsBCryptCipherQueryObjectLength:
        formatter.append("Windows BCrypt query cipher object length");
        break;
    case CryptographyErrorDetail::WindowsBCryptCipherKeyObjectCapacity:
        formatter.append("Windows BCrypt validate cipher key-object capacity");
        break;
    case CryptographyErrorDetail::WindowsBCryptCipherGenerateKey:
        formatter.append("Windows BCrypt generate cipher key");
        break;
    case CryptographyErrorDetail::WindowsBCryptCipherEncrypt:
        formatter.append("Windows BCrypt encrypt cipher blocks");
        break;
    case CryptographyErrorDetail::WindowsBCryptCipherDecrypt:
        formatter.append("Windows BCrypt decrypt cipher blocks");
        break;
    case CryptographyErrorDetail::WindowsBCryptHmacOpenAlgorithm:
        formatter.append("Windows BCrypt open HMAC algorithm");
        break;
    case CryptographyErrorDetail::WindowsBCryptHmacQueryObjectLength:
        formatter.append("Windows BCrypt query HMAC object length");
        break;
    case CryptographyErrorDetail::WindowsBCryptHmacObjectCapacity:
        formatter.append("Windows BCrypt validate HMAC object capacity");
        break;
    case CryptographyErrorDetail::WindowsBCryptHmacCreate: formatter.append("Windows BCrypt create HMAC"); break;
    case CryptographyErrorDetail::WindowsBCryptHmacUpdate: formatter.append("Windows BCrypt update HMAC"); break;
    case CryptographyErrorDetail::WindowsBCryptHmacFinalize: formatter.append("Windows BCrypt finalize HMAC"); break;
    case CryptographyErrorDetail::LinuxGetRandom: formatter.append("Linux getrandom"); break;
    case CryptographyErrorDetail::LinuxAFAlgCreateAlgorithmSocket:
        formatter.append("Linux AF_ALG create algorithm socket");
        break;
    case CryptographyErrorDetail::LinuxAFAlgBindAlgorithmSocket:
        formatter.append("Linux AF_ALG bind algorithm socket");
        break;
    case CryptographyErrorDetail::LinuxAFAlgAcceptOperationSocket:
        formatter.append("Linux AF_ALG accept operation socket");
        break;
    case CryptographyErrorDetail::LinuxAFAlgSetKey: formatter.append("Linux AF_ALG set key"); break;
    case CryptographyErrorDetail::LinuxAFAlgSetAeadAuthenticationSize:
        formatter.append("Linux AF_ALG set AEAD authentication size");
        break;
    case CryptographyErrorDetail::LinuxAFAlgAeadSealSend: formatter.append("Linux AF_ALG send AEAD seal input"); break;
    case CryptographyErrorDetail::LinuxAFAlgAeadSealReceive:
        formatter.append("Linux AF_ALG receive AEAD seal output");
        break;
    case CryptographyErrorDetail::LinuxAFAlgAeadOpenSend: formatter.append("Linux AF_ALG send AEAD open input"); break;
    case CryptographyErrorDetail::LinuxAFAlgAeadOpenReceive:
        formatter.append("Linux AF_ALG receive AEAD open output");
        break;
    case CryptographyErrorDetail::LinuxAFAlgCipherSend: formatter.append("Linux AF_ALG send cipher input"); break;
    case CryptographyErrorDetail::LinuxAFAlgCipherReceive:
        formatter.append("Linux AF_ALG receive cipher output");
        break;
    case CryptographyErrorDetail::LinuxAFAlgHmacSend: formatter.append("Linux AF_ALG send HMAC input"); break;
    case CryptographyErrorDetail::LinuxAFAlgHmacReceive: formatter.append("Linux AF_ALG receive HMAC output"); break;
    case CryptographyErrorDetail::OpenSSL3RuntimeUnavailable: formatter.append("OpenSSL 3 load runtime"); break;
    case CryptographyErrorDetail::OpenSSL3AeadFetchCipher: formatter.append("OpenSSL 3 fetch AEAD cipher"); break;
    case CryptographyErrorDetail::OpenSSL3AeadCreateContext: formatter.append("OpenSSL 3 create AEAD context"); break;
    case CryptographyErrorDetail::OpenSSL3AeadInitialize: formatter.append("OpenSSL 3 initialize AEAD context"); break;
    case CryptographyErrorDetail::OpenSSL3AeadAssociatedDataUpdate:
        formatter.append("OpenSSL 3 add AEAD associated data");
        break;
    case CryptographyErrorDetail::OpenSSL3AeadPayloadUpdate:
        formatter.append("OpenSSL 3 transform AEAD payload");
        break;
    case CryptographyErrorDetail::OpenSSL3AeadSealFinalize: formatter.append("OpenSSL 3 finalize AEAD seal"); break;
    case CryptographyErrorDetail::OpenSSL3AeadGetAuthenticationTag:
        formatter.append("OpenSSL 3 get AEAD authentication tag");
        break;
    case CryptographyErrorDetail::OpenSSL3AeadSetAuthenticationTag:
        formatter.append("OpenSSL 3 set AEAD authentication tag");
        break;
    case CryptographyErrorDetail::OpenSSL3AeadOpenFinalize: formatter.append("OpenSSL 3 finalize AEAD open"); break;
    case CryptographyErrorDetail::OpenSSL3CipherFetch: formatter.append("OpenSSL 3 fetch cipher"); break;
    case CryptographyErrorDetail::OpenSSL3CipherCreateContext:
        formatter.append("OpenSSL 3 create cipher context");
        break;
    case CryptographyErrorDetail::OpenSSL3CipherInitialize:
        formatter.append("OpenSSL 3 initialize cipher context");
        break;
    case CryptographyErrorDetail::OpenSSL3CipherSetPadding: formatter.append("OpenSSL 3 disable cipher padding"); break;
    case CryptographyErrorDetail::OpenSSL3CipherUpdate: formatter.append("OpenSSL 3 update cipher"); break;
    case CryptographyErrorDetail::OpenSSL3HmacFetch: formatter.append("OpenSSL 3 fetch HMAC"); break;
    case CryptographyErrorDetail::OpenSSL3HmacCreateContext: formatter.append("OpenSSL 3 create HMAC context"); break;
    case CryptographyErrorDetail::OpenSSL3HmacInitialize: formatter.append("OpenSSL 3 initialize HMAC context"); break;
    case CryptographyErrorDetail::OpenSSL3HmacUpdate: formatter.append("OpenSSL 3 update HMAC"); break;
    case CryptographyErrorDetail::OpenSSL3HmacFinalize: formatter.append("OpenSSL 3 finalize HMAC"); break;
    default: return false;
    }
    return true;
}

inline void appendSignedCryptographyContext(ResultErrorFormatter& formatter, int32_t value)
{
    if (value < 0)
    {
        formatter.append("-");
        formatter.append(static_cast<uint64_t>(-static_cast<int64_t>(value)));
    }
    else
        formatter.append(static_cast<uint64_t>(value));
}

inline ResultErrorFormat formatCryptographyErrorWithContext(CryptographyError error, CryptographyErrorDetail detail,
                                                            CryptographyErrorContextKind contextKind,
                                                            CryptographyErrorContext context, Span<char> output)
{
    ResultErrorFormatter formatter(output);
    if (not appendCryptographyError(formatter, error))
        return ResultErrorFormatter::failure(ResultErrorFormatStatus::UnknownError, output);
    if (detail != CryptographyErrorDetail::None)
    {
        formatter.append(" (detail: ");
        if (not appendCryptographyErrorDetail(formatter, detail))
            return ResultErrorFormatter::failure(ResultErrorFormatStatus::UnknownError, output);
        formatter.append(")");
    }
    switch (contextKind)
    {
    case CryptographyErrorContextKind::None: break;
    case CryptographyErrorContextKind::AppleCommonCryptoStatus:
        formatter.append(" (CommonCrypto status: ");
        appendSignedCryptographyContext(formatter, context.appleCommonCryptoStatus);
        formatter.append(")");
        break;
    case CryptographyErrorContextKind::WindowsNtStatus:
        formatter.append(" (NTSTATUS: ");
        formatter.append(static_cast<uint64_t>(context.windowsNtStatus));
        formatter.append(")");
        break;
    case CryptographyErrorContextKind::PosixErrno:
        formatter.append(" (POSIX errno: ");
        appendSignedCryptographyContext(formatter, context.posixErrno);
        formatter.append(")");
        break;
    case CryptographyErrorContextKind::ExpectedBytes:
        formatter.append(" (expected: ");
        formatter.append(static_cast<uint64_t>(context.expectedBytes));
        formatter.append(" bytes)");
        break;
    case CryptographyErrorContextKind::RequiredBytes:
        formatter.append(" (required capacity: ");
        formatter.append(static_cast<uint64_t>(context.requiredBytes));
        formatter.append(" bytes)");
        break;
    case CryptographyErrorContextKind::MaximumBytes:
        formatter.append(" (maximum: ");
        formatter.append(static_cast<uint64_t>(context.maximumBytes));
        formatter.append(" bytes)");
        break;
    case CryptographyErrorContextKind::ActualBytes:
        formatter.append(" (actual: ");
        formatter.append(static_cast<uint64_t>(context.actualBytes));
        formatter.append(" bytes)");
        break;
    default: return ResultErrorFormatter::failure(ResultErrorFormatStatus::UnknownError, output);
    }
    return formatter.finish();
}
} // namespace detail

inline ResultErrorFormat formatCryptographyError(CryptographyError error, Span<char> output)
{
    return detail::formatCryptographyErrorWithContext(error, CryptographyErrorDetail::None,
                                                      CryptographyErrorContextKind::None, {}, output);
}

inline ResultErrorFormat formatCryptographyError(Result result, Span<char> output)
{
    if (result)
        return ResultErrorFormatter::failure(ResultErrorFormatStatus::NotAnError, output);
    if (result.category() != CryptographyResultCategory)
        return ResultErrorFormatter::failure(ResultErrorFormatStatus::ForeignCategory, output);
    return formatCryptographyError(static_cast<CryptographyError>(result.errorValue()), output);
}

inline ResultErrorFormat formatCryptographyError(ResultCryptography result, Span<char> output)
{
    if (result)
        return ResultErrorFormatter::failure(ResultErrorFormatStatus::NotAnError, output);
    if (result.result.category() != CryptographyResultCategory)
        return ResultErrorFormatter::failure(ResultErrorFormatStatus::ForeignCategory, output);
    return detail::formatCryptographyErrorWithContext(static_cast<CryptographyError>(result.result.errorValue()),
                                                      result.detail, result.contextKind, result.context, output);
}
} // namespace SC
