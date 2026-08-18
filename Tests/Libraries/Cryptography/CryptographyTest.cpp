// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "Libraries/Cryptography/Cryptography.h"
#include "Libraries/Common/PlatformMacrosType.h"
#include "Libraries/Strings/StringView.h"
#include "Libraries/Testing/Testing.h"

#include <stdlib.h>
#include <string.h>

namespace SC
{
struct CryptographyTest;
}

struct SC::CryptographyTest : public SC::TestCase
{
    static bool sameBytes(Span<const uint8_t> actual, Span<const uint8_t> expected)
    {
        return actual.sizeInBytes() == expected.sizeInBytes() and
               memcmp(actual.data(), expected.data(), actual.sizeInBytes()) == 0;
    }

    static bool anyNonZero(Span<const uint8_t> bytes)
    {
        for (auto value : bytes)
        {
            if (value != 0)
                return true;
        }
        return false;
    }

    Cryptography::Features features;

    void printUnsupported(StringSpan sectionName)
    {
        report.console.print("CryptographyTest - Skipping ");
        report.console.print(sectionName);
        report.console.printLine(": unsupported on this backend");
        report.console.flush();
    }

    static constexpr uint8_t zeroKey16[16]   = {0};
    static constexpr uint8_t zeroKey32[32]   = {0};
    static constexpr uint8_t zeroIV16[16]    = {0};
    static constexpr uint8_t zeroNonce12[12] = {0};
    static constexpr uint8_t zeroPlain16[16] = {0};

    static constexpr uint8_t aes128CbcExpected[32] = {
        0x66, 0xe9, 0x4b, 0xd4, 0xef, 0x8a, 0x2c, 0x3b, 0x88, 0x4c, 0xfa, 0x59, 0xca, 0x34, 0x2b, 0x2e,
        0x94, 0x34, 0xde, 0xc2, 0xd0, 0x0f, 0xda, 0xc7, 0x65, 0xf0, 0x0c, 0x0c, 0x11, 0x62, 0x8c, 0xd1,
    };
    static constexpr uint8_t aes256CbcExpected[32] = {
        0xdc, 0x95, 0xc0, 0x78, 0xa2, 0x40, 0x89, 0x89, 0xad, 0x48, 0xa2, 0x14, 0x92, 0x84, 0x20, 0x87,
        0xf3, 0xc0, 0x03, 0xdd, 0xc4, 0xa7, 0xb8, 0xa9, 0x4b, 0xae, 0xdf, 0xfc, 0x3d, 0x21, 0x4c, 0x38,
    };
    static constexpr uint8_t aes128GcmExpectedCipher[16] = {
        0x03, 0x88, 0xda, 0xce, 0x60, 0xb6, 0xa3, 0x92, 0xf3, 0x28, 0xc2, 0xb9, 0x71, 0xb2, 0xfe, 0x78,
    };
    static constexpr uint8_t aes128GcmExpectedTag[16] = {
        0xab, 0x6e, 0x47, 0xd4, 0x2c, 0xec, 0x13, 0xbd, 0xf5, 0x3a, 0x67, 0xb2, 0x12, 0x57, 0xbd, 0xdf,
    };
    static constexpr uint8_t aes256GcmExpectedCipher[16] = {
        0xce, 0xa7, 0x40, 0x3d, 0x4d, 0x60, 0x6b, 0x6e, 0x07, 0x4e, 0xc5, 0xd3, 0xba, 0xf3, 0x9d, 0x18,
    };
    static constexpr uint8_t aes256GcmExpectedTag[16] = {
        0xd0, 0xd1, 0xc8, 0xa7, 0x99, 0x99, 0x6b, 0xf0, 0x26, 0x5b, 0x98, 0xb5, 0xd4, 0x8a, 0xb9, 0x19,
    };
    static constexpr uint8_t hmacSha256Expected[32] = {
        0xb0, 0x34, 0x4c, 0x61, 0xd8, 0xdb, 0x38, 0x53, 0x5c, 0xa8, 0xaf, 0xce, 0xaf, 0x0b, 0xf1, 0x2b,
        0x88, 0x1d, 0xc2, 0x00, 0xc9, 0x83, 0x3d, 0xa7, 0x26, 0xe9, 0x37, 0x6c, 0x2e, 0x32, 0xcf, 0xf7,
    };
    static constexpr uint8_t hmacSha384Expected[48] = {
        0xaf, 0xd0, 0x39, 0x44, 0xd8, 0x48, 0x95, 0x62, 0x6b, 0x08, 0x25, 0xf4, 0xab, 0x46, 0x90, 0x7f,
        0x15, 0xf9, 0xda, 0xdb, 0xe4, 0x10, 0x1e, 0xc6, 0x82, 0xaa, 0x03, 0x4c, 0x7c, 0xeb, 0xc5, 0x9c,
        0xfa, 0xea, 0x9e, 0xa9, 0x07, 0x6e, 0xde, 0x7f, 0x4a, 0xf1, 0x52, 0xe8, 0xb2, 0xfa, 0x9c, 0xb6,
    };
    static constexpr uint8_t hmacSha256EmptyExpected[32] = {
        0xb6, 0x13, 0x67, 0x9a, 0x08, 0x14, 0xd9, 0xec, 0x77, 0x2f, 0x95, 0xd7, 0x78, 0xc3, 0x5f, 0xc5,
        0xff, 0x16, 0x97, 0xc4, 0x93, 0x71, 0x56, 0x53, 0xc6, 0xc7, 0x12, 0x14, 0x42, 0x92, 0xc5, 0xad,
    };
    static constexpr uint8_t hkdfSha256Expected[42] = {
        0x3c, 0xb2, 0x5f, 0x25, 0xfa, 0xac, 0xd5, 0x7a, 0x90, 0x43, 0x4f, 0x64, 0xd0, 0x36,
        0x2f, 0x2a, 0x2d, 0x2d, 0x0a, 0x90, 0xcf, 0x1a, 0x5a, 0x4c, 0x5d, 0xb0, 0x2d, 0x56,
        0xec, 0xc4, 0xc5, 0xbf, 0x34, 0x00, 0x72, 0x08, 0xd5, 0xb8, 0x87, 0x18, 0x58, 0x65,
    };
    static constexpr uint8_t hkdfSha384Expected[42] = {
        0xb2, 0x7f, 0x1a, 0xe7, 0x75, 0xdc, 0x7d, 0x1f, 0x18, 0xcf, 0x16, 0xda, 0x95, 0xdc,
        0x6a, 0xa9, 0x65, 0xdc, 0xd7, 0x8c, 0xd3, 0x75, 0xeb, 0xe4, 0x9f, 0x7d, 0x91, 0xae,
        0x31, 0x03, 0x93, 0x2f, 0x30, 0x41, 0x71, 0x4d, 0xdc, 0x87, 0x64, 0x28, 0x78, 0x77,
    };

    void testFeatures();
    void testSecureRandom();
    void testAes128CbcPkcs7();
    void testAes256CbcPkcs7();
    void testCbcStreaming();
    void testCbcOutputRetry();
    void testCbcEmptyAndMalformed();
    void testCbcInvalidPaddingConsumesSession();
    void testCbcDeterministicStress();
    void testAes128Gcm();
    void testAes256Gcm();
    void testAes128GcmWithAad();
    void testAes128GcmWrongTag();
    void testAes128GcmEmptyAndInPlace();
    void testHmacSha256();
    void testHmacSha384();
    void testHmacStreamingAndLifecycle();
    void testExplicitReset();
    void testHkdfSha256();
    void testHkdfSha384();
    void testHkdfBounds();
    void testInvalidInputs();
    void testOverlapRejected();

    CryptographyTest(SC::TestReport& report) : TestCase(report, "CryptographyTest")
    {
        using namespace SC;

        SC_TEST_EXPECT(Cryptography::queryFeatures(features));

        if (test_section("Features"))
        {
            testFeatures();
        }

        if (test_section("Secure Random"))
        {
            testSecureRandom();
        }

        if (test_section("AES128 CBC PKCS7 Known Answer"))
        {
            testAes128CbcPkcs7();
        }

        if (test_section("AES256 CBC PKCS7 Known Answer"))
        {
            testAes256CbcPkcs7();
        }

        if (test_section("CBC Streaming"))
        {
            testCbcStreaming();
        }

        if (test_section("CBC Output Retry"))
        {
            testCbcOutputRetry();
        }

        if (test_section("CBC Empty And Malformed"))
        {
            testCbcEmptyAndMalformed();
        }

        if (test_section("CBC Invalid Padding Consumes Session"))
        {
            testCbcInvalidPaddingConsumesSession();
        }

        if (test_section("CBC Deterministic Stress"))
        {
            testCbcDeterministicStress();
        }

        if (test_section("AES128 GCM Known Answer"))
        {
            testAes128Gcm();
        }

        if (test_section("AES256 GCM Known Answer"))
        {
            testAes256Gcm();
        }

        if (test_section("AES128 GCM With AAD"))
        {
            testAes128GcmWithAad();
        }

        if (test_section("AES128 GCM Wrong Tag"))
        {
            testAes128GcmWrongTag();
        }

        if (test_section("AES128 GCM Empty And In Place"))
        {
            testAes128GcmEmptyAndInPlace();
        }

        if (test_section("HMAC SHA256"))
        {
            testHmacSha256();
        }

        if (test_section("HMAC SHA384"))
        {
            testHmacSha384();
        }

        if (test_section("HMAC Streaming And Lifecycle"))
        {
            testHmacStreamingAndLifecycle();
        }

        if (test_section("Explicit Reset"))
        {
            testExplicitReset();
        }

        if (test_section("HKDF SHA256"))
        {
            testHkdfSha256();
        }

        if (test_section("HKDF SHA384"))
        {
            testHkdfSha384();
        }

        if (test_section("HKDF Bounds"))
        {
            testHkdfBounds();
        }

        if (test_section("Invalid Inputs"))
        {
            testInvalidInputs();
        }

        if (test_section("Overlap Rejected"))
        {
            testOverlapRejected();
        }
    }
};

namespace SC
{
constexpr uint8_t CryptographyTest::zeroKey16[16];
constexpr uint8_t CryptographyTest::zeroKey32[32];
constexpr uint8_t CryptographyTest::zeroIV16[16];
constexpr uint8_t CryptographyTest::zeroNonce12[12];
constexpr uint8_t CryptographyTest::zeroPlain16[16];

constexpr uint8_t CryptographyTest::aes128CbcExpected[32];
constexpr uint8_t CryptographyTest::aes256CbcExpected[32];
constexpr uint8_t CryptographyTest::aes128GcmExpectedCipher[16];
constexpr uint8_t CryptographyTest::aes128GcmExpectedTag[16];
constexpr uint8_t CryptographyTest::aes256GcmExpectedCipher[16];
constexpr uint8_t CryptographyTest::aes256GcmExpectedTag[16];
constexpr uint8_t CryptographyTest::hmacSha256Expected[32];
constexpr uint8_t CryptographyTest::hmacSha384Expected[48];
constexpr uint8_t CryptographyTest::hmacSha256EmptyExpected[32];
constexpr uint8_t CryptographyTest::hkdfSha256Expected[42];
constexpr uint8_t CryptographyTest::hkdfSha384Expected[42];

void CryptographyTest::testFeatures()
{
    SC_TEST_EXPECT(features.secureRandom);
#if SC_PLATFORM_LINUX
    const char* requireAesGcm = ::getenv("SC_CRYPTOGRAPHY_REQUIRE_AES_GCM");
    if (requireAesGcm != nullptr and requireAesGcm[0] == '1' and requireAesGcm[1] == '\0')
    {
        SC_TEST_EXPECT(features.aes128Gcm);
        SC_TEST_EXPECT(features.aes256Gcm);
    }
#endif
#if SC_PLATFORM_APPLE or SC_PLATFORM_WINDOWS
    SC_TEST_EXPECT(features.aes128CbcPkcs7);
    SC_TEST_EXPECT(features.aes256CbcPkcs7);
    SC_TEST_EXPECT(features.hmacSha256);
    SC_TEST_EXPECT(features.hmacSha384);
#endif
    SC_TEST_EXPECT(features.hkdfSha256 == features.hmacSha256);
    SC_TEST_EXPECT(features.hkdfSha384 == features.hmacSha384);
}

void CryptographyTest::testSecureRandom()
{
    SC_TEST_EXPECT(Cryptography::Random::fill({}));

    if (features.secureRandom)
    {
        //! [CryptographyRandomSnippet]
        uint8_t randomBytes[32] = {0};
        SC_TEST_EXPECT(Cryptography::Random::fill(randomBytes));
        SC_TEST_EXPECT(anyNonZero(randomBytes));
        //! [CryptographyRandomSnippet]
    }
    else
    {
        printUnsupported("Secure Random");
    }
}

void SC::CryptographyTest::testAes128CbcPkcs7()
{
    if (features.aes128CbcPkcs7)
    {
        //! [CryptographyCbcSnippet]
        Cryptography::Cipher cipher;
        SC_TEST_EXPECT(cipher.start(Cryptography::CipherType::AES128CBCPKCS7, Cryptography::Cipher::Operation::Encrypt,
                                    zeroKey16, zeroIV16));

        uint8_t ciphertext[32] = {0};
        size_t  updateBytes    = 0;
        size_t  finishBytes    = 0;
        SC_TEST_EXPECT(cipher.update(zeroPlain16, ciphertext, updateBytes));
        SC_TEST_EXPECT(
            cipher.finish(Span<uint8_t>(ciphertext + updateBytes, sizeof(ciphertext) - updateBytes), finishBytes));
        SC_TEST_EXPECT(updateBytes + finishBytes == sizeof(aes128CbcExpected));
        SC_TEST_EXPECT(sameBytes(Span<const uint8_t>(ciphertext, updateBytes + finishBytes), aes128CbcExpected));

        Cryptography::Cipher decrypt;
        SC_TEST_EXPECT(decrypt.start(Cryptography::CipherType::AES128CBCPKCS7, Cryptography::Cipher::Operation::Decrypt,
                                     zeroKey16, zeroIV16));

        uint8_t plaintext[32] = {0};
        size_t  decryptUpdate = 0;
        size_t  decryptFinish = 0;
        SC_TEST_EXPECT(decrypt.update(aes128CbcExpected, plaintext, decryptUpdate));
        SC_TEST_EXPECT(
            decrypt.finish(Span<uint8_t>(plaintext + decryptUpdate, sizeof(plaintext) - decryptUpdate), decryptFinish));
        SC_TEST_EXPECT(decryptUpdate + decryptFinish == sizeof(zeroPlain16));
        SC_TEST_EXPECT(sameBytes(Span<const uint8_t>(plaintext, decryptUpdate + decryptFinish), zeroPlain16));
        //! [CryptographyCbcSnippet]
    }
    else
    {
        printUnsupported("AES128 CBC PKCS7 Known Answer");
    }
}

void CryptographyTest::testAes256CbcPkcs7()
{
    if (not features.aes256CbcPkcs7)
    {
        printUnsupported("AES256 CBC PKCS7 Known Answer");
        return;
    }

    Cryptography::Cipher cipher;
    SC_TEST_EXPECT(cipher.start(Cryptography::CipherType::AES256CBCPKCS7, Cryptography::Cipher::Operation::Encrypt,
                                zeroKey32, zeroIV16));

    uint8_t ciphertext[32] = {0};
    size_t  updateBytes    = 0;
    size_t  finishBytes    = 0;
    SC_TEST_EXPECT(cipher.update(zeroPlain16, ciphertext, updateBytes));
    SC_TEST_EXPECT(
        cipher.finish(Span<uint8_t>(ciphertext + updateBytes, sizeof(ciphertext) - updateBytes), finishBytes));
    SC_TEST_EXPECT(updateBytes + finishBytes == sizeof(aes256CbcExpected));
    SC_TEST_EXPECT(sameBytes(Span<const uint8_t>(ciphertext, sizeof(ciphertext)), aes256CbcExpected));

    Cryptography::Cipher decrypt;
    SC_TEST_EXPECT(decrypt.start(Cryptography::CipherType::AES256CBCPKCS7, Cryptography::Cipher::Operation::Decrypt,
                                 zeroKey32, zeroIV16));
    uint8_t plaintext[32] = {0};
    SC_TEST_EXPECT(decrypt.update(aes256CbcExpected, plaintext, updateBytes));
    SC_TEST_EXPECT(
        decrypt.finish(Span<uint8_t>(plaintext + updateBytes, sizeof(plaintext) - updateBytes), finishBytes));
    SC_TEST_EXPECT(updateBytes + finishBytes == sizeof(zeroPlain16));
    SC_TEST_EXPECT(sameBytes(Span<const uint8_t>(plaintext, sizeof(zeroPlain16)), zeroPlain16));
}

void CryptographyTest::testCbcStreaming()
{
    if (not features.aes128CbcPkcs7)
    {
        printUnsupported("CBC Streaming");
        return;
    }

    uint8_t plaintext[113];
    for (size_t idx = 0; idx < sizeof(plaintext); ++idx)
        plaintext[idx] = static_cast<uint8_t>(idx * 17 + 3);

    Cryptography::Cipher encrypt;
    SC_TEST_EXPECT(encrypt.start(Cryptography::CipherType::AES128CBCPKCS7, Cryptography::Cipher::Operation::Encrypt,
                                 zeroKey16, zeroIV16));

    static constexpr size_t chunks[]        = {5, 11, 1, 64, 32};
    uint8_t                 ciphertext[128] = {0};
    size_t                  inputOffset     = 0;
    size_t                  outputOffset    = 0;
    for (size_t chunk : chunks)
    {
        size_t written = 0;
        SC_TEST_EXPECT(encrypt.update(Span<const uint8_t>(plaintext + inputOffset, chunk),
                                      Span<uint8_t>(ciphertext + outputOffset, sizeof(ciphertext) - outputOffset),
                                      written));
        inputOffset += chunk;
        outputOffset += written;
    }
    size_t finishBytes = 0;
    SC_TEST_EXPECT(
        encrypt.finish(Span<uint8_t>(ciphertext + outputOffset, sizeof(ciphertext) - outputOffset), finishBytes));
    outputOffset += finishBytes;
    SC_TEST_EXPECT(inputOffset == sizeof(plaintext));
    SC_TEST_EXPECT(outputOffset == sizeof(ciphertext));

    Cryptography::Cipher decrypt;
    SC_TEST_EXPECT(decrypt.start(Cryptography::CipherType::AES128CBCPKCS7, Cryptography::Cipher::Operation::Decrypt,
                                 zeroKey16, zeroIV16));
    static constexpr size_t decryptChunks[] = {7, 9, 33, 1, 78};
    uint8_t                 decrypted[128]  = {0};
    inputOffset                             = 0;
    outputOffset                            = 0;
    for (size_t chunk : decryptChunks)
    {
        size_t written = 0;
        SC_TEST_EXPECT(decrypt.update(Span<const uint8_t>(ciphertext + inputOffset, chunk),
                                      Span<uint8_t>(decrypted + outputOffset, sizeof(decrypted) - outputOffset),
                                      written));
        inputOffset += chunk;
        outputOffset += written;
    }
    size_t decryptFinish = 0;
    SC_TEST_EXPECT(
        decrypt.finish(Span<uint8_t>(decrypted + outputOffset, sizeof(decrypted) - outputOffset), decryptFinish));
    SC_TEST_EXPECT(inputOffset == sizeof(ciphertext));
    SC_TEST_EXPECT(outputOffset + decryptFinish == sizeof(plaintext));
    SC_TEST_EXPECT(sameBytes(Span<const uint8_t>(decrypted, sizeof(plaintext)), plaintext));
}

void CryptographyTest::testCbcOutputRetry()
{
    if (not features.aes128CbcPkcs7)
    {
        printUnsupported("CBC Output Retry");
        return;
    }

    Cryptography::Cipher cipher;
    SC_TEST_EXPECT(cipher.start(Cryptography::CipherType::AES128CBCPKCS7, Cryptography::Cipher::Operation::Encrypt,
                                zeroKey16, zeroIV16));

    size_t written = 0;
    SC_TEST_EXPECT(cipher.update(Span<const uint8_t>(zeroPlain16, 5), {}, written));
    SC_TEST_EXPECT(written == 0);

    uint8_t tooSmall[15] = {0};
    SC_TEST_EXPECT(not cipher.update(Span<const uint8_t>(zeroPlain16 + 5, 11), tooSmall, written));
    SC_TEST_EXPECT(written == 0);

    uint8_t ciphertext[32] = {0};
    SC_TEST_EXPECT(cipher.update(Span<const uint8_t>(zeroPlain16 + 5, 11), ciphertext, written));
    SC_TEST_EXPECT(written == 16);
    size_t finishBytes = 0;
    SC_TEST_EXPECT(cipher.finish(Span<uint8_t>(ciphertext + written, sizeof(ciphertext) - written), finishBytes));
    SC_TEST_EXPECT(written + finishBytes == sizeof(aes128CbcExpected));
    SC_TEST_EXPECT(sameBytes(ciphertext, aes128CbcExpected));

    Cryptography::Cipher decrypt;
    SC_TEST_EXPECT(decrypt.start(Cryptography::CipherType::AES128CBCPKCS7, Cryptography::Cipher::Operation::Decrypt,
                                 zeroKey16, zeroIV16));
    SC_TEST_EXPECT(not decrypt.update(aes128CbcExpected, tooSmall, written));
    SC_TEST_EXPECT(written == 0);
    uint8_t plaintext[16] = {0};
    SC_TEST_EXPECT(decrypt.update(aes128CbcExpected, plaintext, written));
    SC_TEST_EXPECT(written == sizeof(plaintext));
    uint8_t finalPlaintext[16] = {0};
    SC_TEST_EXPECT(decrypt.finish(finalPlaintext, finishBytes));
    SC_TEST_EXPECT(finishBytes == 0);
    SC_TEST_EXPECT(sameBytes(plaintext, zeroPlain16));
}

void CryptographyTest::testCbcEmptyAndMalformed()
{
    if (not features.aes128CbcPkcs7)
    {
        printUnsupported("CBC Empty And Malformed");
        return;
    }

    Cryptography::Cipher encrypt;
    SC_TEST_EXPECT(encrypt.start(Cryptography::CipherType::AES128CBCPKCS7, Cryptography::Cipher::Operation::Encrypt,
                                 zeroKey16, zeroIV16));
    size_t  written         = 0;
    uint8_t emptyCipher[16] = {0};
    SC_TEST_EXPECT(encrypt.update({}, {}, written));
    SC_TEST_EXPECT(written == 0);
    SC_TEST_EXPECT(encrypt.finish(emptyCipher, written));
    SC_TEST_EXPECT(written == sizeof(emptyCipher));

    Cryptography::Cipher decrypt;
    SC_TEST_EXPECT(decrypt.start(Cryptography::CipherType::AES128CBCPKCS7, Cryptography::Cipher::Operation::Decrypt,
                                 zeroKey16, zeroIV16));
    uint8_t emptyPlain[16];
    memset(emptyPlain, 0xA5, sizeof(emptyPlain));
    SC_TEST_EXPECT(decrypt.update(emptyCipher, {}, written));
    SC_TEST_EXPECT(written == 0);
    SC_TEST_EXPECT(decrypt.finish(emptyPlain, written));
    SC_TEST_EXPECT(written == 0);
    SC_TEST_EXPECT(emptyPlain[0] == 0xA5 and emptyPlain[sizeof(emptyPlain) - 1] == 0xA5);

    Cryptography::Cipher malformed;
    SC_TEST_EXPECT(malformed.start(Cryptography::CipherType::AES128CBCPKCS7, Cryptography::Cipher::Operation::Decrypt,
                                   zeroKey16, zeroIV16));
    SC_TEST_EXPECT(malformed.update(Span<const uint8_t>(emptyCipher, 15), {}, written));
    SC_TEST_EXPECT(not malformed.finish(emptyPlain, written));
    SC_TEST_EXPECT(not malformed.update({}, {}, written));
}

void CryptographyTest::testCbcInvalidPaddingConsumesSession()
{
    if (not features.aes128CbcPkcs7)
    {
        printUnsupported("CBC Invalid Padding Consumes Session");
        return;
    }

    uint8_t corrupted[32];
    memcpy(corrupted, aes128CbcExpected, sizeof(corrupted));
    corrupted[15] ^= 0x01;

    Cryptography::Cipher cipher;
    SC_TEST_EXPECT(cipher.start(Cryptography::CipherType::AES128CBCPKCS7, Cryptography::Cipher::Operation::Decrypt,
                                zeroKey16, zeroIV16));
    uint8_t output[32] = {0};
    size_t  written    = 0;
    SC_TEST_EXPECT(cipher.update(corrupted, output, written));
    SC_TEST_EXPECT(written == 16);
    SC_TEST_EXPECT(not cipher.finish(Span<uint8_t>(output + written, sizeof(output) - written), written));
    SC_TEST_EXPECT(not cipher.update({}, output, written));
}

void CryptographyTest::testCbcDeterministicStress()
{
    if (not features.aes128CbcPkcs7)
    {
        printUnsupported("CBC Deterministic Stress");
        return;
    }

    static constexpr size_t   messageSizes[] = {0,  1,  2,  7,  15, 16,  17,  31,  32,  33,  47,
                                                48, 49, 63, 64, 65, 127, 128, 129, 255, 256, 257};
    static constexpr uint32_t chunkSeeds[]   = {1, 0x12345678, 0x9e3779b9, 0xffffffff};

    uint8_t plaintext[257];
    uint8_t expectedCiphertext[288];
    uint8_t chunkedCiphertext[288];
    uint8_t decrypted[288];

    for (size_t messageSize : messageSizes)
    {
        for (size_t idx = 0; idx < messageSize; ++idx)
            plaintext[idx] = static_cast<uint8_t>((idx * 29 + messageSize * 17 + 11) & 0xff);

        Cryptography::Cipher reference;
        SC_TEST_EXPECT(reference.start(Cryptography::CipherType::AES128CBCPKCS7,
                                       Cryptography::Cipher::Operation::Encrypt, zeroKey16, zeroIV16));

        size_t expectedSize = 0;
        size_t written      = 0;
        SC_TEST_EXPECT(reference.update(Span<const uint8_t>(plaintext, messageSize), expectedCiphertext, written));
        expectedSize += written;
        SC_TEST_EXPECT(reference.finish(
            Span<uint8_t>(expectedCiphertext + expectedSize, sizeof(expectedCiphertext) - expectedSize), written));
        expectedSize += written;

        for (uint32_t initialSeed : chunkSeeds)
        {
            Cryptography::Cipher encrypt;
            SC_TEST_EXPECT(encrypt.start(Cryptography::CipherType::AES128CBCPKCS7,
                                         Cryptography::Cipher::Operation::Encrypt, zeroKey16, zeroIV16));

            uint32_t seed         = initialSeed ^ static_cast<uint32_t>(messageSize);
            size_t   inputOffset  = 0;
            size_t   outputOffset = 0;
            while (inputOffset < messageSize)
            {
                seed             = seed * 1664525u + 1013904223u;
                size_t chunkSize = static_cast<size_t>(seed % 37u) + 1;
                if (chunkSize > messageSize - inputOffset)
                    chunkSize = messageSize - inputOffset;

                SC_TEST_EXPECT(encrypt.update(
                    Span<const uint8_t>(plaintext + inputOffset, chunkSize),
                    Span<uint8_t>(chunkedCiphertext + outputOffset, sizeof(chunkedCiphertext) - outputOffset),
                    written));
                inputOffset += chunkSize;
                outputOffset += written;
            }
            SC_TEST_EXPECT(encrypt.finish(
                Span<uint8_t>(chunkedCiphertext + outputOffset, sizeof(chunkedCiphertext) - outputOffset), written));
            outputOffset += written;
            SC_TEST_EXPECT(outputOffset == expectedSize);
            SC_TEST_EXPECT(sameBytes(Span<const uint8_t>(chunkedCiphertext, outputOffset),
                                     Span<const uint8_t>(expectedCiphertext, expectedSize)));

            Cryptography::Cipher decrypt;
            SC_TEST_EXPECT(decrypt.start(Cryptography::CipherType::AES128CBCPKCS7,
                                         Cryptography::Cipher::Operation::Decrypt, zeroKey16, zeroIV16));

            seed         = initialSeed ^ static_cast<uint32_t>(expectedSize) ^ 0xa5a5a5a5u;
            inputOffset  = 0;
            outputOffset = 0;
            while (inputOffset < expectedSize)
            {
                seed             = seed * 22695477u + 1u;
                size_t chunkSize = static_cast<size_t>(seed % 41u) + 1;
                if (chunkSize > expectedSize - inputOffset)
                    chunkSize = expectedSize - inputOffset;

                SC_TEST_EXPECT(decrypt.update(Span<const uint8_t>(chunkedCiphertext + inputOffset, chunkSize),
                                              Span<uint8_t>(decrypted + outputOffset, sizeof(decrypted) - outputOffset),
                                              written));
                inputOffset += chunkSize;
                outputOffset += written;
            }
            SC_TEST_EXPECT(
                decrypt.finish(Span<uint8_t>(decrypted + outputOffset, sizeof(decrypted) - outputOffset), written));
            outputOffset += written;
            SC_TEST_EXPECT(outputOffset == messageSize);
            SC_TEST_EXPECT(
                sameBytes(Span<const uint8_t>(decrypted, outputOffset), Span<const uint8_t>(plaintext, messageSize)));
        }
    }

    for (size_t malformedSize = 1; malformedSize < 16; ++malformedSize)
    {
        Cryptography::Cipher malformed;
        SC_TEST_EXPECT(malformed.start(Cryptography::CipherType::AES128CBCPKCS7,
                                       Cryptography::Cipher::Operation::Decrypt, zeroKey16, zeroIV16));
        size_t written = 0;
        SC_TEST_EXPECT(malformed.update(Span<const uint8_t>(expectedCiphertext, malformedSize), {}, written));
        SC_TEST_EXPECT(written == 0);
        SC_TEST_EXPECT(not malformed.finish(decrypted, written));
        SC_TEST_EXPECT(written == 0);
    }
}

void SC::CryptographyTest::testAes128Gcm()
{
    if (features.aes128Gcm)
    {
        //! [CryptographyAeadSnippet]
        Cryptography::Aead aead;
        SC_TEST_EXPECT(aead.init(Cryptography::AeadType::AES128GCM, zeroKey16));

        uint8_t ciphertext[16] = {0};
        uint8_t tag[16]        = {0};
        size_t  bytesWritten   = 0;

        SC_TEST_EXPECT(aead.seal(zeroNonce12, {}, zeroPlain16, ciphertext, tag, bytesWritten));
        SC_TEST_EXPECT(bytesWritten == sizeof(ciphertext));
        SC_TEST_EXPECT(sameBytes(ciphertext, aes128GcmExpectedCipher));
        SC_TEST_EXPECT(sameBytes(tag, aes128GcmExpectedTag));

        uint8_t decrypted[16] = {0};
        size_t  plainBytes    = 0;
        SC_TEST_EXPECT(
            aead.open(zeroNonce12, {}, aes128GcmExpectedCipher, aes128GcmExpectedTag, decrypted, plainBytes));
        SC_TEST_EXPECT(plainBytes == sizeof(decrypted));
        SC_TEST_EXPECT(sameBytes(decrypted, zeroPlain16));
        //! [CryptographyAeadSnippet]
    }
    else
    {
        printUnsupported("AES128 GCM Known Answer");
        Cryptography::Aead aead;
        auto               res = aead.init(Cryptography::AeadType::AES128GCM, zeroKey16);
        SC_TEST_EXPECT(not res);
    }
}

void CryptographyTest::testAes256Gcm()
{
    if (not features.aes256Gcm)
    {
        printUnsupported("AES256 GCM Known Answer");
        return;
    }

    Cryptography::Aead aead;
    SC_TEST_EXPECT(aead.init(Cryptography::AeadType::AES256GCM, zeroKey32));

    uint8_t ciphertext[16] = {0};
    uint8_t tag[16]        = {0};
    size_t  bytesWritten   = 0;
    SC_TEST_EXPECT(aead.seal(zeroNonce12, {}, zeroPlain16, ciphertext, tag, bytesWritten));
    SC_TEST_EXPECT(bytesWritten == sizeof(ciphertext));
    SC_TEST_EXPECT(sameBytes(ciphertext, aes256GcmExpectedCipher));
    SC_TEST_EXPECT(sameBytes(tag, aes256GcmExpectedTag));

    uint8_t decrypted[16] = {0};
    SC_TEST_EXPECT(aead.open(zeroNonce12, {}, ciphertext, tag, decrypted, bytesWritten));
    SC_TEST_EXPECT(bytesWritten == sizeof(decrypted));
    SC_TEST_EXPECT(sameBytes(decrypted, zeroPlain16));
}

void CryptographyTest::testAes128GcmWithAad()
{
    if (not features.aes128Gcm)
    {
        printUnsupported("AES128 GCM With AAD");
        return;
    }

    const auto key            = "\xfe\xff\xe9\x92\x86\x65\x73\x1c\x6d\x6a\x8f\x94\x67\x30\x83\x08"_a8;
    const auto nonce          = "\xca\xfe\xba\xbe\xfa\xce\xdb\xad\xde\xca\xf8\x88"_a8;
    const auto aad            = "\xfe\xed\xfa\xce\xde\xad\xbe\xef\xfe\xed\xfa\xce\xde\xad\xbe\xef\xab\xad\xda\xd2"_a8;
    const auto plaintext      = "\xd9\x31\x32\x25\xf8\x84\x06\xe5\xa5\x59\x09\xc5\xaf\xf5\x26\x9a"
                                "\x86\xa7\xa9\x53\x15\x34\xf7\xda\x2e\x4c\x30\x3d\x8a\x31\x8a\x72"
                                "\x1c\x3c\x0c\x95\x95\x68\x09\x53\x2f\xcf\x0e\x24\x49\xa6\xb5\x25"
                                "\xb1\x6a\xed\xf5\xaa\x0d\xe6\x57\xba\x63\x7b\x39\x1a\xaf\xd2\x55"_a8;
    const auto expectedCipher = "\x42\x83\x1e\xc2\x21\x77\x74\x24\x4b\x72\x21\xb7\x84\xd0\xd4\x9c"
                                "\xe3\xaa\x21\x2f\x2c\x02\xa4\xe0\x35\xc1\x7e\x23\x29\xac\xa1\x2e"
                                "\x21\xd5\x14\xb2\x54\x66\x93\x1c\x7d\x8f\x6a\x5a\xac\x84\xaa\x05"
                                "\x1b\xa3\x0b\x39\x6a\x0a\xac\x97\x3d\x58\xe0\x91\x47\x3f\x59\x85"_a8;
    const auto expectedTag    = "\xda\x80\xce\x83\x0c\xfd\xa0\x2d\xa2\xa2\x18\xa1\x74\x4f\x4c\x76"_a8;

    Cryptography::Aead aead;
    SC_TEST_EXPECT(aead.init(Cryptography::AeadType::AES128GCM, key.toBytesSpan()));
    uint8_t ciphertext[64] = {0};
    uint8_t tag[16]        = {0};
    size_t  bytesWritten   = 0;
    SC_TEST_EXPECT(
        aead.seal(nonce.toBytesSpan(), aad.toBytesSpan(), plaintext.toBytesSpan(), ciphertext, tag, bytesWritten));
    SC_TEST_EXPECT(bytesWritten == sizeof(ciphertext));
    SC_TEST_EXPECT(sameBytes(ciphertext, expectedCipher.toBytesSpan()));
    SC_TEST_EXPECT(sameBytes(tag, expectedTag.toBytesSpan()));

    uint8_t decrypted[64] = {0};
    SC_TEST_EXPECT(aead.open(nonce.toBytesSpan(), aad.toBytesSpan(), ciphertext, tag, decrypted, bytesWritten));
    SC_TEST_EXPECT(bytesWritten == sizeof(decrypted));
    SC_TEST_EXPECT(sameBytes(decrypted, plaintext.toBytesSpan()));

    uint8_t inPlace[64];
    memcpy(inPlace, plaintext.toBytesSpan().data(), sizeof(inPlace));
    SC_TEST_EXPECT(aead.seal(nonce.toBytesSpan(), aad.toBytesSpan(), inPlace, inPlace, tag, bytesWritten));
    SC_TEST_EXPECT(bytesWritten == sizeof(inPlace));
    SC_TEST_EXPECT(sameBytes(inPlace, expectedCipher.toBytesSpan()));
    SC_TEST_EXPECT(sameBytes(tag, expectedTag.toBytesSpan()));
    SC_TEST_EXPECT(aead.open(nonce.toBytesSpan(), aad.toBytesSpan(), inPlace, tag, inPlace, bytesWritten));
    SC_TEST_EXPECT(bytesWritten == sizeof(inPlace));
    SC_TEST_EXPECT(sameBytes(inPlace, plaintext.toBytesSpan()));

#if SC_PLATFORM_LINUX
    uint8_t maximumAad[4096] = {0};
    SC_TEST_EXPECT(aead.seal(zeroNonce12, maximumAad, {}, {}, tag, bytesWritten));
    SC_TEST_EXPECT(bytesWritten == 0);
    SC_TEST_EXPECT(aead.open(zeroNonce12, maximumAad, {}, tag, {}, bytesWritten));
    SC_TEST_EXPECT(bytesWritten == 0);

    uint8_t oversizedAad[4097] = {0};
    bytesWritten               = 77;
    SC_TEST_EXPECT(not aead.seal(zeroNonce12, oversizedAad, {}, {}, tag, bytesWritten));
    SC_TEST_EXPECT(bytesWritten == 0);
#endif
}

void SC::CryptographyTest::testAes128GcmWrongTag()
{
    if (features.aes128Gcm)
    {
        Cryptography::Aead aead;
        SC_TEST_EXPECT(aead.init(Cryptography::AeadType::AES128GCM, zeroKey16));

        uint8_t badTag[16];
        memcpy(badTag, aes128GcmExpectedTag, sizeof(badTag));
        badTag[0] ^= 0xFF;

        uint8_t decrypted[16];
        memset(decrypted, 0xA5, sizeof(decrypted));
        size_t plainBytes = 0;
        auto   res        = aead.open(zeroNonce12, {}, aes128GcmExpectedCipher, badTag, decrypted, plainBytes);
        SC_TEST_EXPECT(not res);
        SC_TEST_EXPECT(plainBytes == 0);
        SC_TEST_EXPECT(not anyNonZero(decrypted));
    }
    else
    {
        printUnsupported("AES128 GCM Wrong Tag");
    }
}

void CryptographyTest::testAes128GcmEmptyAndInPlace()
{
    if (not features.aes128Gcm)
    {
        printUnsupported("AES128 GCM Empty And In Place");
        return;
    }

    static constexpr uint8_t emptyTag[16] = {
        0x58, 0xe2, 0xfc, 0xce, 0xfa, 0x7e, 0x30, 0x61, 0x36, 0x7f, 0x1d, 0x57, 0xa4, 0xe7, 0x45, 0x5a,
    };

    Cryptography::Aead aead;
    SC_TEST_EXPECT(aead.init(Cryptography::AeadType::AES128GCM, zeroKey16));

    uint8_t tag[16]      = {0};
    size_t  bytesWritten = 77;
    SC_TEST_EXPECT(aead.seal(zeroNonce12, {}, {}, {}, tag, bytesWritten));
    SC_TEST_EXPECT(bytesWritten == 0);
    SC_TEST_EXPECT(sameBytes(tag, emptyTag));
    SC_TEST_EXPECT(aead.open(zeroNonce12, {}, {}, tag, {}, bytesWritten));
    SC_TEST_EXPECT(bytesWritten == 0);

    uint8_t inPlace[16];
    memcpy(inPlace, zeroPlain16, sizeof(inPlace));
    SC_TEST_EXPECT(aead.seal(zeroNonce12, {}, inPlace, inPlace, tag, bytesWritten));
    SC_TEST_EXPECT(bytesWritten == sizeof(inPlace));
    SC_TEST_EXPECT(sameBytes(inPlace, aes128GcmExpectedCipher));
    SC_TEST_EXPECT(sameBytes(tag, aes128GcmExpectedTag));

    SC_TEST_EXPECT(aead.open(zeroNonce12, {}, inPlace, tag, inPlace, bytesWritten));
    SC_TEST_EXPECT(bytesWritten == sizeof(inPlace));
    SC_TEST_EXPECT(sameBytes(inPlace, zeroPlain16));
}

void SC::CryptographyTest::testHmacSha256()
{
    if (features.hmacSha256)
    {
        //! [CryptographyHmacSnippet]
        Cryptography::Hmac hmac;
        SC_TEST_EXPECT(hmac.setType(Cryptography::HashType::SHA256));
        SC_TEST_EXPECT(hmac.setKey(
            "\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b"_a8.toBytesSpan()));
        SC_TEST_EXPECT(hmac.add("Hi There"_a8.toBytesSpan()));

        Cryptography::MacResult result;
        SC_TEST_EXPECT(hmac.getMac(result));
        SC_TEST_EXPECT(sameBytes(result.toBytesSpan(), hmacSha256Expected));
        //! [CryptographyHmacSnippet]
    }
    else
    {
        printUnsupported("HMAC SHA256");
    }
}

void SC::CryptographyTest::testHmacSha384()
{
    if (features.hmacSha384)
    {
        Cryptography::Hmac hmac;
        SC_TEST_EXPECT(hmac.setType(Cryptography::HashType::SHA384));
        SC_TEST_EXPECT(hmac.setKey(
            "\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b"_a8.toBytesSpan()));
        SC_TEST_EXPECT(hmac.add("Hi There"_a8.toBytesSpan()));

        Cryptography::MacResult result;
        SC_TEST_EXPECT(hmac.getMac(result));
        SC_TEST_EXPECT(sameBytes(result.toBytesSpan(), hmacSha384Expected));
    }
    else
    {
        printUnsupported("HMAC SHA384");
    }
}

void CryptographyTest::testHmacStreamingAndLifecycle()
{
    if (not features.hmacSha256)
    {
        printUnsupported("HMAC Streaming And Lifecycle");
        return;
    }

    Cryptography::Hmac empty;
    SC_TEST_EXPECT(empty.setType(Cryptography::HashType::SHA256));
    SC_TEST_EXPECT(empty.setKey({}));
    Cryptography::MacResult emptyResult;
    SC_TEST_EXPECT(empty.getMac(emptyResult));
    SC_TEST_EXPECT(sameBytes(emptyResult.toBytesSpan(), hmacSha256EmptyExpected));
    SC_TEST_EXPECT(not empty.add({}));

    Cryptography::Hmac streaming;
    SC_TEST_EXPECT(streaming.setType(Cryptography::HashType::SHA256));
    SC_TEST_EXPECT(streaming.setKey(
        "\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b"_a8.toBytesSpan()));
    SC_TEST_EXPECT(streaming.add("Hi "_a8.toBytesSpan()));
    SC_TEST_EXPECT(streaming.add("There"_a8.toBytesSpan()));
    Cryptography::MacResult streamingResult;
    SC_TEST_EXPECT(streaming.getMac(streamingResult));
    SC_TEST_EXPECT(sameBytes(streamingResult.toBytesSpan(), hmacSha256Expected));
}

void CryptographyTest::testExplicitReset()
{
    Cryptography::Cipher cipher;
    cipher.reset();
    cipher.reset();

    if (features.aes128CbcPkcs7)
    {
        SC_TEST_EXPECT(cipher.start(Cryptography::CipherType::AES128CBCPKCS7, Cryptography::Cipher::Operation::Encrypt,
                                    zeroKey16, zeroIV16));
        cipher.reset();
        size_t bytesWritten = 77;
        SC_TEST_EXPECT(not cipher.update(zeroPlain16, {}, bytesWritten));
        SC_TEST_EXPECT(bytesWritten == 0);
    }

    Cryptography::Hmac hmac;
    hmac.reset();
    hmac.reset();

    if (features.hmacSha256)
    {
        SC_TEST_EXPECT(hmac.setType(Cryptography::HashType::SHA256));
        SC_TEST_EXPECT(hmac.setKey(zeroKey16));
        SC_TEST_EXPECT(hmac.add(zeroPlain16));
        hmac.reset();
        SC_TEST_EXPECT(not hmac.add(zeroPlain16));
    }
}

void SC::CryptographyTest::testHkdfSha256()
{
    if (features.hkdfSha256)
    {
        //! [CryptographyHkdfSnippet]
        uint8_t output[42] = {0};
        SC_TEST_EXPECT(Cryptography::Hkdf::derive(
            Cryptography::HashType::SHA256, "\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c"_a8.toBytesSpan(),
            "\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b\x0b"_a8.toBytesSpan(),
            "\xf0\xf1\xf2\xf3\xf4\xf5\xf6\xf7\xf8\xf9"_a8.toBytesSpan(), output));
        SC_TEST_EXPECT(sameBytes(output, hkdfSha256Expected));
        //! [CryptographyHkdfSnippet]
    }
    else
    {
        printUnsupported("HKDF SHA256");
    }
}

void CryptographyTest::testHkdfSha384()
{
    if (features.hkdfSha384)
    {
        uint8_t output[42] = {0};
        SC_TEST_EXPECT(Cryptography::Hkdf::derive(
            Cryptography::HashType::SHA384,
            "\xa0\xa1\xa2\xa3\xa4\xa5\xa6\xa7\xa8\xa9\xaa\xab\xac\xad\xae\xaf"_a8.toBytesSpan(),
            "\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e\x0f\x10"_a8.toBytesSpan(),
            "\xb0\xb1\xb2\xb3\xb4\xb5\xb6\xb7"_a8.toBytesSpan(), output));
        SC_TEST_EXPECT(sameBytes(output, hkdfSha384Expected));
    }
    else
    {
        printUnsupported("HKDF SHA384");
    }
}

void CryptographyTest::testHkdfBounds()
{
    if (not features.hkdfSha384)
    {
        printUnsupported("HKDF Bounds");
        return;
    }

    uint8_t byte = 0xA5;
    SC_TEST_EXPECT(Cryptography::Hkdf::derive(Cryptography::HashType::SHA384, {}, {}, {}, {}));
    SC_TEST_EXPECT(byte == 0xA5);
    SC_TEST_EXPECT(
        not Cryptography::Hkdf::derive(Cryptography::HashType::SHA384, {}, {}, {}, Span<uint8_t>(&byte, 255 * 48 + 1)));
    SC_TEST_EXPECT(byte == 0xA5);
}

void CryptographyTest::testInvalidInputs()
{
    Cryptography::Hmac hmac;
    SC_TEST_EXPECT(not hmac.setType(static_cast<Cryptography::HashType>(255)));

    Cryptography::Aead aead;
    auto               badAead = aead.init(Cryptography::AeadType::AES128GCM, Span<const uint8_t>(zeroKey32, 15));
    SC_TEST_EXPECT(not badAead);

    Cryptography::Cipher cipher;
    auto badCipher = cipher.start(Cryptography::CipherType::AES128CBCPKCS7, Cryptography::Cipher::Operation::Encrypt,
                                  zeroKey16, Span<const uint8_t>(zeroIV16, 8));
    SC_TEST_EXPECT(not badCipher);

    auto badOperation = cipher.start(Cryptography::CipherType::AES128CBCPKCS7,
                                     static_cast<Cryptography::Cipher::Operation>(255), zeroKey16, zeroIV16);
    SC_TEST_EXPECT(not badOperation);

    uint8_t outputByte = 0;
    size_t  written    = 77;
    SC_TEST_EXPECT(not cipher.update({}, Span<uint8_t>(&outputByte, 1), written));
    SC_TEST_EXPECT(written == 0);
    written = 77;
    SC_TEST_EXPECT(not cipher.finish(Span<uint8_t>(&outputByte, 1), written));
    SC_TEST_EXPECT(written == 0);

    if (features.aes128Gcm)
    {
        SC_TEST_EXPECT(aead.init(Cryptography::AeadType::AES128GCM, zeroKey16));
        uint8_t output[16] = {0};
        uint8_t tag[16]    = {0};
        written            = 0;
        SC_TEST_EXPECT(not aead.seal(Span<const uint8_t>(zeroNonce12, 11), {}, zeroPlain16, output, tag, written));
        SC_TEST_EXPECT(not aead.seal(zeroNonce12, {}, zeroPlain16, Span<uint8_t>(output, 15), tag, written));
        SC_TEST_EXPECT(not aead.seal(zeroNonce12, {}, zeroPlain16, output, Span<uint8_t>(tag, 15), written));

        uint8_t overlap[32] = {0};
        SC_TEST_EXPECT(not aead.seal(zeroNonce12, {}, Span<const uint8_t>(overlap, 16), Span<uint8_t>(overlap + 1, 16),
                                     tag, written));
    }
}

void CryptographyTest::testOverlapRejected()
{
    uint8_t buffer[32] = {0};
    memcpy(buffer, zeroPlain16, sizeof(zeroPlain16));

    Cryptography::Cipher partialOverlap;
    SC_TEST_EXPECT(partialOverlap.start(Cryptography::CipherType::AES128CBCPKCS7,
                                        Cryptography::Cipher::Operation::Encrypt, zeroKey16, zeroIV16));
    size_t bytesWritten = 0;
    SC_TEST_EXPECT(not partialOverlap.update(Span<const uint8_t>(buffer, sizeof(zeroPlain16)),
                                             Span<uint8_t>(buffer + 4, sizeof(zeroPlain16)), bytesWritten));

    Cryptography::Cipher exactOverlap;
    SC_TEST_EXPECT(exactOverlap.start(Cryptography::CipherType::AES128CBCPKCS7,
                                      Cryptography::Cipher::Operation::Encrypt, zeroKey16, zeroIV16));
    SC_TEST_EXPECT(not exactOverlap.update(Span<const uint8_t>(buffer, sizeof(zeroPlain16)),
                                           Span<uint8_t>(buffer, sizeof(zeroPlain16)), bytesWritten));
}

void runCryptographyTest(SC::TestReport& report) { CryptographyTest test(report); }

} // namespace SC
