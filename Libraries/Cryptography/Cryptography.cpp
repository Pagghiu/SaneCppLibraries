// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "Cryptography.h"

#include <stdint.h>
#include <string.h>

#include "../Common/CompilerMinMax.h"
#include "../Common/Deferred.h"
#include "../Common/PlacementNew.h"
#include "../Common/PlatformMacrosType.h"

#if SC_PLATFORM_APPLE
#include <CommonCrypto/CommonCryptor.h>
#include <CommonCrypto/CommonDigest.h>
#include <CommonCrypto/CommonHMAC.h>
#include <CommonCrypto/CommonRandom.h>
#elif SC_PLATFORM_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <bcrypt.h>

#pragma comment(lib, "bcrypt.lib")
#elif SC_PLATFORM_LINUX
#include <errno.h>
#include <linux/if_alg.h>
#include <sys/random.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace SC
{
namespace
{

static constexpr size_t AESBlockSize = 16;
#if SC_PLATFORM_APPLE || SC_PLATFORM_WINDOWS || SC_PLATFORM_LINUX
static constexpr size_t GCMNonceSize = 12;
static constexpr size_t GCMTagSize   = 16;
#endif

static size_t digestSize(Cryptography::HashType type)
{
    switch (type)
    {
    case Cryptography::HashType::SHA256: return 32;
    case Cryptography::HashType::SHA384: return 48;
    }
    return 0;
}

static bool isValid(Cryptography::HashType type) { return digestSize(type) != 0; }

static bool isValid(Cryptography::AeadType type)
{
    switch (type)
    {
    case Cryptography::AeadType::AES128GCM:
    case Cryptography::AeadType::AES256GCM: return true;
    }
    return false;
}

static bool isValid(Cryptography::CipherType type)
{
    switch (type)
    {
    case Cryptography::CipherType::AES128CBCPKCS7:
    case Cryptography::CipherType::AES256CBCPKCS7: return true;
    }
    return false;
}

static bool isValid(Cryptography::Cipher::Operation operation)
{
    switch (operation)
    {
    case Cryptography::Cipher::Operation::Encrypt:
    case Cryptography::Cipher::Operation::Decrypt: return true;
    }
    return false;
}

#if SC_PLATFORM_APPLE || SC_PLATFORM_WINDOWS || SC_PLATFORM_LINUX
static size_t keySize(Cryptography::AeadType type)
{
    switch (type)
    {
    case Cryptography::AeadType::AES128GCM: return 16;
    case Cryptography::AeadType::AES256GCM: return 32;
    }
    return 0;
}
#endif

static size_t keySize(Cryptography::CipherType type)
{
    switch (type)
    {
    case Cryptography::CipherType::AES128CBCPKCS7: return 16;
    case Cryptography::CipherType::AES256CBCPKCS7: return 32;
    }
    return 0;
}

#if SC_PLATFORM_APPLE || SC_PLATFORM_WINDOWS || SC_PLATFORM_LINUX
static bool spansExactlyOverlap(Span<const uint8_t> input, Span<uint8_t> output)
{
    return input.data() == output.data() and input.sizeInBytes() == output.sizeInBytes();
}
#endif

static bool spansOverlap(Span<const uint8_t> input, Span<uint8_t> output)
{
    if (input.empty() or output.empty())
        return false;

    auto inputBegin  = reinterpret_cast<uintptr_t>(input.data());
    auto inputEnd    = inputBegin + input.sizeInBytes();
    auto outputBegin = reinterpret_cast<uintptr_t>(output.data());
    auto outputEnd   = outputBegin + output.sizeInBytes();
    return inputBegin < outputEnd and outputBegin < inputEnd;
}

#if SC_PLATFORM_APPLE || SC_PLATFORM_WINDOWS || SC_PLATFORM_LINUX
static Result validateOutputNoPartialOverlap(Span<const uint8_t> input, Span<uint8_t> output, const char* message)
{
    if (spansOverlap(input, output) and not spansExactlyOverlap(input, output))
        return Result::FromStableCharPointer(message);
    return Result(true);
}
#endif

static Result validateOutputNoOverlap(Span<const uint8_t> input, Span<uint8_t> output, const char* message)
{
    if (spansOverlap(input, output))
        return Result::FromStableCharPointer(message);
    return Result(true);
}

static Result validateKeySize(size_t actual, size_t expected, const char* message)
{
    if (actual != expected)
        return Result::FromStableCharPointer(message);
    return Result(true);
}

static void secureClear(Span<uint8_t> bytes)
{
    volatile uint8_t* current = bytes.data();
    for (size_t idx = 0; idx < bytes.sizeInBytes(); ++idx)
        current[idx] = 0;
}

struct CipherStreamState
{
    Cryptography::Cipher::Operation operation = Cryptography::Cipher::Operation::Encrypt;

    uint8_t pending[16] = {0};
    size_t  pendingSize = 0;

    void reset()
    {
        secureClear(pending);
        pendingSize = 0;
    }
};

static size_t cipherEncryptUpdateSize(const CipherStreamState& state, size_t inputSize)
{
    return (state.pendingSize + inputSize) / AESBlockSize * AESBlockSize;
}

static size_t cipherDecryptUpdateSize(const CipherStreamState& state, size_t inputSize)
{
    const size_t totalSize = state.pendingSize + inputSize;
    return totalSize > AESBlockSize ? (totalSize - 1) / AESBlockSize * AESBlockSize : 0;
}

template <typename Backend>
static Result cipherUpdateEncrypt(Backend& backend, Span<const uint8_t> input, Span<uint8_t> output,
                                  size_t& bytesWritten)
{
    CipherStreamState& state        = backend.stream;
    size_t             inputOffset  = 0;
    size_t             outputOffset = 0;

    if (state.pendingSize > 0)
    {
        const size_t toCopy = min(AESBlockSize - state.pendingSize, input.sizeInBytes());
        memcpy(state.pending + state.pendingSize, input.data(), toCopy);
        state.pendingSize += toCopy;
        inputOffset += toCopy;

        if (state.pendingSize == AESBlockSize)
        {
            size_t produced = 0;
            SC_TRY(backend.processBlocks(state.pending, output, produced));
            outputOffset += produced;
            state.pendingSize = 0;
        }
    }

    const size_t remaining = input.sizeInBytes() - inputOffset;
    const size_t fullBytes = remaining / AESBlockSize * AESBlockSize;
    if (fullBytes > 0)
    {
        size_t produced = 0;
        SC_TRY(backend.processBlocks(Span<const uint8_t>(input.data() + inputOffset, fullBytes),
                                     Span<uint8_t>(output.data() + outputOffset, output.sizeInBytes() - outputOffset),
                                     produced));
        inputOffset += fullBytes;
        outputOffset += produced;
    }

    const size_t tailSize = input.sizeInBytes() - inputOffset;
    if (tailSize > 0)
    {
        memcpy(state.pending, input.data() + inputOffset, tailSize);
        state.pendingSize = tailSize;
    }

    bytesWritten = outputOffset;
    return Result(true);
}

template <typename Backend>
static Result cipherUpdateDecrypt(Backend& backend, Span<const uint8_t> input, Span<uint8_t> output,
                                  size_t& bytesWritten)
{
    CipherStreamState& state        = backend.stream;
    size_t             inputOffset  = 0;
    size_t             outputOffset = 0;

    if (state.pendingSize > 0)
    {
        const size_t toCopy = min(AESBlockSize - state.pendingSize, input.sizeInBytes());
        memcpy(state.pending + state.pendingSize, input.data(), toCopy);
        state.pendingSize += toCopy;
        inputOffset += toCopy;

        if (state.pendingSize == AESBlockSize and inputOffset < input.sizeInBytes())
        {
            size_t produced = 0;
            SC_TRY(backend.processBlocks(state.pending, output, produced));
            outputOffset += produced;
            state.pendingSize = 0;
        }
    }

    const size_t remaining = input.sizeInBytes() - inputOffset;
    const size_t fullBytes = remaining > AESBlockSize ? (remaining - 1) / AESBlockSize * AESBlockSize : 0;
    if (fullBytes > 0)
    {
        size_t produced = 0;
        SC_TRY(backend.processBlocks(Span<const uint8_t>(input.data() + inputOffset, fullBytes),
                                     Span<uint8_t>(output.data() + outputOffset, output.sizeInBytes() - outputOffset),
                                     produced));
        inputOffset += fullBytes;
        outputOffset += produced;
    }

    const size_t tailSize = input.sizeInBytes() - inputOffset;
    if (tailSize > 0)
    {
        memcpy(state.pending + state.pendingSize, input.data() + inputOffset, tailSize);
        state.pendingSize += tailSize;
    }

    bytesWritten = outputOffset;
    return Result(true);
}

template <typename Backend>
static Result cipherUpdate(Backend& backend, Span<const uint8_t> input, Span<uint8_t> output, size_t& bytesWritten)
{
    bytesWritten = 0;
    SC_TRY_MSG(backend.initialized, "Cryptography::Cipher::update - not initialized");
    SC_TRY(validateOutputNoOverlap(input, output, "Cryptography::Cipher::update - overlap is not supported"));

    const size_t required = backend.stream.operation == Cryptography::Cipher::Operation::Encrypt
                                ? cipherEncryptUpdateSize(backend.stream, input.sizeInBytes())
                                : cipherDecryptUpdateSize(backend.stream, input.sizeInBytes());
    SC_TRY_MSG(output.sizeInBytes() >= required, "Cryptography::Cipher::update - insufficient output buffer");

    Result result = backend.stream.operation == Cryptography::Cipher::Operation::Encrypt
                        ? cipherUpdateEncrypt(backend, input, output, bytesWritten)
                        : cipherUpdateDecrypt(backend, input, output, bytesWritten);
    if (not result)
        backend.close();
    return result;
}

template <typename Backend>
static Result cipherFinish(Backend& backend, Span<uint8_t> output, size_t& bytesWritten)
{
    bytesWritten = 0;
    SC_TRY_MSG(backend.initialized, "Cryptography::Cipher::finish - not initialized");

    if (backend.stream.operation == Cryptography::Cipher::Operation::Encrypt)
    {
        SC_TRY_MSG(output.sizeInBytes() >= AESBlockSize, "Cryptography::Cipher::finish - insufficient output buffer");

        uint8_t       finalBlock[16];
        const uint8_t pad = static_cast<uint8_t>(AESBlockSize - backend.stream.pendingSize);
        memcpy(finalBlock, backend.stream.pending, backend.stream.pendingSize);
        memset(finalBlock + backend.stream.pendingSize, pad, pad);
        Result result = backend.processBlocks(finalBlock, output, bytesWritten);
        secureClear(finalBlock);
        backend.close();
        return result;
    }

    if (backend.stream.pendingSize != AESBlockSize)
    {
        backend.close();
        return Result::Error("Cryptography::Cipher::finish - invalid ciphertext");
    }
    SC_TRY_MSG(output.sizeInBytes() >= AESBlockSize, "Cryptography::Cipher::finish - insufficient output buffer");

    uint8_t block[16];
    size_t  produced = 0;
    Result  result   = backend.processBlocks(backend.stream.pending, block, produced);
    backend.close();
    if (not result)
    {
        secureClear(block);
        return result;
    }
    if (produced != AESBlockSize)
    {
        secureClear(block);
        return Result::Error("Cryptography::Cipher::finish - unexpected block size");
    }

    const uint8_t pad               = block[AESBlockSize - 1];
    uint8_t       paddingDifference = 0;
    for (size_t idx = 0; idx < AESBlockSize; ++idx)
    {
        const uint8_t mask = static_cast<uint8_t>(-static_cast<int>(idx < pad));
        paddingDifference |= static_cast<uint8_t>((block[AESBlockSize - idx - 1] ^ pad) & mask);
    }
    const bool validPadding = pad > 0 and pad <= AESBlockSize and paddingDifference == 0;
    if (not validPadding)
    {
        secureClear(block);
        return Result::Error("Cryptography::Cipher::finish - invalid ciphertext");
    }

    bytesWritten = AESBlockSize - pad;
    memcpy(output.data(), block, bytesWritten);
    secureClear(block);
    return Result(true);
}

#if SC_PLATFORM_WINDOWS
static constexpr size_t BcryptMaxInputSize = 0xffffffffu;
#endif

#if SC_PLATFORM_APPLE || SC_PLATFORM_WINDOWS || SC_PLATFORM_LINUX
#if SC_PLATFORM_WINDOWS
static constexpr size_t AeadMaxInputSize = BcryptMaxInputSize;
#elif SC_PLATFORM_APPLE
static constexpr size_t AeadMaxInputSize = static_cast<size_t>(0xfffffffeULL) * AESBlockSize;
#else
static constexpr size_t AeadMaxInputSize          = 0x7fffffffu;
static constexpr size_t AeadMaxAssociatedDataSize = 4096;
#endif

static Result validateAeadArguments(Span<const uint8_t> nonce, Span<const uint8_t> aad, Span<const uint8_t> input,
                                    Span<uint8_t> output, size_t tagSize)
{
    SC_TRY_MSG(nonce.sizeInBytes() == GCMNonceSize, "Cryptography::Aead - invalid nonce size");
    SC_TRY_MSG(tagSize == GCMTagSize, "Cryptography::Aead - invalid tag size");
    SC_TRY_MSG(aad.sizeInBytes() <= AeadMaxInputSize and input.sizeInBytes() <= AeadMaxInputSize and
                   output.sizeInBytes() <= AeadMaxInputSize,
               "Cryptography::Aead - message is too large for the backend");
    SC_TRY_MSG(output.sizeInBytes() >= input.sizeInBytes(), "Cryptography::Aead - insufficient output buffer");
    SC_TRY(validateOutputNoPartialOverlap(input, output, "Cryptography::Aead - partial overlap is not supported"));
    return Result(true);
}

static Result validateAeadSealArguments(Span<const uint8_t> nonce, Span<const uint8_t> aad,
                                        Span<const uint8_t> plaintext, Span<uint8_t> ciphertext, Span<uint8_t> tag)
{
    SC_TRY(validateAeadArguments(nonce, aad, plaintext, ciphertext, tag.sizeInBytes()));
    SC_TRY_MSG(not spansOverlap(nonce, ciphertext) and not spansOverlap(aad, ciphertext),
               "Cryptography::Aead::seal - ciphertext overlaps nonce or associated data");
    SC_TRY_MSG(not spansOverlap(nonce, tag) and not spansOverlap(aad, tag) and not spansOverlap(plaintext, tag) and
                   not spansOverlap(Span<const uint8_t>(ciphertext.data(), ciphertext.sizeInBytes()), tag),
               "Cryptography::Aead::seal - tag overlaps another argument");
    return Result(true);
}

static Result validateAeadOpenArguments(Span<const uint8_t> nonce, Span<const uint8_t> aad,
                                        Span<const uint8_t> ciphertext, Span<const uint8_t> tag,
                                        Span<uint8_t> plaintext)
{
    SC_TRY(validateAeadArguments(nonce, aad, ciphertext, plaintext, tag.sizeInBytes()));
    SC_TRY_MSG(not spansOverlap(nonce, plaintext) and not spansOverlap(aad, plaintext) and
                   not spansOverlap(tag, plaintext),
               "Cryptography::Aead::open - plaintext overlaps nonce, associated data, or tag");
    return Result(true);
}
#endif

#if SC_PLATFORM_APPLE
static void storeBigEndian64(uint8_t* destination, uint64_t value)
{
    for (size_t idx = 0; idx < sizeof(value); ++idx)
    {
        destination[sizeof(value) - idx - 1] = static_cast<uint8_t>(value);
        value >>= 8;
    }
}

static void gcmMultiply(const uint8_t left[AESBlockSize], const uint8_t right[AESBlockSize],
                        uint8_t result[AESBlockSize])
{
    uint8_t product[AESBlockSize] = {0};
    uint8_t value[AESBlockSize];
    memcpy(value, right, sizeof(value));

    for (size_t bitIndex = 0; bitIndex < AESBlockSize * 8; ++bitIndex)
    {
        const uint8_t bit  = static_cast<uint8_t>((left[bitIndex / 8] >> (7 - bitIndex % 8)) & 1);
        const uint8_t mask = static_cast<uint8_t>(0u - bit);
        for (size_t byteIndex = 0; byteIndex < AESBlockSize; ++byteIndex)
            product[byteIndex] ^= static_cast<uint8_t>(value[byteIndex] & mask);

        const uint8_t reductionMask = static_cast<uint8_t>(0u - (value[AESBlockSize - 1] & 1));
        for (size_t byteIndex = AESBlockSize - 1; byteIndex > 0; --byteIndex)
        {
            value[byteIndex] =
                static_cast<uint8_t>((value[byteIndex] >> 1) | static_cast<uint8_t>(value[byteIndex - 1] << 7));
        }
        value[0] = static_cast<uint8_t>((value[0] >> 1) ^ (0xe1u & reductionMask));
    }

    memcpy(result, product, AESBlockSize);
    secureClear(product);
    secureClear(value);
}

static void gcmHashBlock(uint8_t hash[AESBlockSize], const uint8_t block[AESBlockSize],
                         const uint8_t hashSubkey[AESBlockSize])
{
    for (size_t idx = 0; idx < AESBlockSize; ++idx)
        hash[idx] ^= block[idx];
    gcmMultiply(hash, hashSubkey, hash);
}

static void gcmHashSpan(uint8_t hash[AESBlockSize], Span<const uint8_t> input, const uint8_t hashSubkey[AESBlockSize])
{
    size_t offset = 0;
    while (input.sizeInBytes() - offset >= AESBlockSize)
    {
        gcmHashBlock(hash, input.data() + offset, hashSubkey);
        offset += AESBlockSize;
    }

    const size_t remaining = input.sizeInBytes() - offset;
    if (remaining > 0)
    {
        uint8_t finalBlock[AESBlockSize] = {0};
        memcpy(finalBlock, input.data() + offset, remaining);
        gcmHashBlock(hash, finalBlock, hashSubkey);
        secureClear(finalBlock);
    }
}

static void gcmHash(const uint8_t hashSubkey[AESBlockSize], Span<const uint8_t> aad, Span<const uint8_t> ciphertext,
                    uint8_t result[AESBlockSize])
{
    memset(result, 0, AESBlockSize);
    gcmHashSpan(result, aad, hashSubkey);
    gcmHashSpan(result, ciphertext, hashSubkey);

    uint8_t lengths[AESBlockSize];
    storeBigEndian64(lengths, static_cast<uint64_t>(aad.sizeInBytes()) * 8);
    storeBigEndian64(lengths + 8, static_cast<uint64_t>(ciphertext.sizeInBytes()) * 8);
    gcmHashBlock(result, lengths, hashSubkey);
    secureClear(lengths);
}

static void gcmInitialCounter(Span<const uint8_t> nonce, uint8_t counter[AESBlockSize])
{
    memcpy(counter, nonce.data(), GCMNonceSize);
    counter[12] = 0;
    counter[13] = 0;
    counter[14] = 0;
    counter[15] = 1;
}

static void gcmIncrementCounter(uint8_t counter[AESBlockSize])
{
    for (size_t idx = AESBlockSize; idx > GCMNonceSize; --idx)
    {
        counter[idx - 1] = static_cast<uint8_t>(counter[idx - 1] + 1);
        if (counter[idx - 1] != 0)
            break;
    }
}

static bool gcmTagsEqual(Span<const uint8_t> supplied, const uint8_t expected[AESBlockSize])
{
    volatile uint8_t difference = 0;
    for (size_t idx = 0; idx < AESBlockSize; ++idx)
        difference = static_cast<uint8_t>(difference | static_cast<uint8_t>(supplied[idx] ^ expected[idx]));
    return difference == 0;
}
#endif

#if SC_PLATFORM_WINDOWS
static bool bcryptSuccess(NTSTATUS status) { return status >= 0; }

static LPCWSTR bcryptHashName(Cryptography::HashType type)
{
    switch (type)
    {
    case Cryptography::HashType::SHA256: return BCRYPT_SHA256_ALGORITHM;
    case Cryptography::HashType::SHA384: return BCRYPT_SHA384_ALGORITHM;
    }
    return BCRYPT_SHA256_ALGORITHM;
}
#endif

#if SC_PLATFORM_LINUX
static void closeIfValid(int& fd)
{
    if (fd != -1)
    {
        ::close(fd);
        fd = -1;
    }
}

static Result openAlgorithmSocket(const char* algorithmType, const char* algorithmName, int& mainSocket)
{
    closeIfValid(mainSocket);

    mainSocket = ::socket(AF_ALG, SOCK_SEQPACKET, 0);
    SC_TRY_MSG(mainSocket != -1, "Cryptography - socket(AF_ALG) failed");

    sockaddr_alg sa;
    memset(&sa, 0, sizeof(sa));
    sa.salg_family = AF_ALG;
    strncpy(reinterpret_cast<char*>(sa.salg_type), algorithmType, sizeof(sa.salg_type) - 1);
    strncpy(reinterpret_cast<char*>(sa.salg_name), algorithmName, sizeof(sa.salg_name) - 1);

    if (::bind(mainSocket, reinterpret_cast<sockaddr*>(&sa), sizeof(sa)) == -1)
    {
        closeIfValid(mainSocket);
        return Result::Error("Cryptography - bind(AF_ALG) failed");
    }

    return Result(true);
}

static Result acceptOperationSocket(int mainSocket, int& opSocket)
{
    closeIfValid(opSocket);
    opSocket = ::accept(mainSocket, nullptr, 0);
    if (opSocket == -1)
    {
        return Result::Error("Cryptography - accept(AF_ALG) failed");
    }
    return Result(true);
}

static bool algorithmSupported(const char* algorithmType, const char* algorithmName)
{
    int  mainSocket = -1;
    int  opSocket   = -1;
    bool supported  = openAlgorithmSocket(algorithmType, algorithmName, mainSocket);
    if (supported)
        supported = acceptOperationSocket(mainSocket, opSocket);
    closeIfValid(opSocket);
    closeIfValid(mainSocket);
    return supported;
}

static Result configureKey(int mainSocket, Span<const uint8_t> key)
{
    SC_TRY_MSG(key.sizeInBytes() <= 0xffffffffu, "Cryptography - key is too large for the backend");
    SC_TRY_MSG(
        ::setsockopt(mainSocket, SOL_ALG, ALG_SET_KEY, key.data(), static_cast<unsigned int>(key.sizeInBytes())) == 0,
        "Cryptography - ALG_SET_KEY failed");
    return Result(true);
}

static bool aeadSupported(size_t requestedKeySize)
{
    uint8_t key[32]    = {0};
    int     mainSocket = -1;
    int     opSocket   = -1;

    bool supported = requestedKeySize <= sizeof(key) and openAlgorithmSocket("aead", "gcm(aes)", mainSocket);
    if (supported and not configureKey(mainSocket, Span<const uint8_t>(key, requestedKeySize)))
        supported = false;
    if (supported)
        supported = ::setsockopt(mainSocket, SOL_ALG, ALG_SET_AEAD_AUTHSIZE, nullptr, GCMTagSize) == 0;
    if (supported)
        supported = acceptOperationSocket(mainSocket, opSocket);

    closeIfValid(opSocket);
    closeIfValid(mainSocket);
    return supported;
}

static void configureAeadMessage(msghdr& message, char* control, size_t controlSize, uint32_t operation,
                                 Span<const uint8_t> nonce, size_t associatedDataSize)
{
    memset(&message, 0, sizeof(message));
    memset(control, 0, controlSize);
    message.msg_control    = control;
    message.msg_controllen = controlSize;

    cmsghdr* cmsg    = CMSG_FIRSTHDR(&message);
    cmsg->cmsg_level = SOL_ALG;
    cmsg->cmsg_type  = ALG_SET_OP;
    cmsg->cmsg_len   = CMSG_LEN(sizeof(operation));
    memcpy(CMSG_DATA(cmsg), &operation, sizeof(operation));

    cmsg             = CMSG_NXTHDR(&message, cmsg);
    cmsg->cmsg_level = SOL_ALG;
    cmsg->cmsg_type  = ALG_SET_IV;
    cmsg->cmsg_len   = CMSG_LEN(sizeof(af_alg_iv) + GCMNonceSize);
    af_alg_iv* iv    = reinterpret_cast<af_alg_iv*>(CMSG_DATA(cmsg));
    iv->ivlen        = GCMNonceSize;
    memcpy(iv->iv, nonce.data(), GCMNonceSize);

    const uint32_t associatedLength = static_cast<uint32_t>(associatedDataSize);
    cmsg                            = CMSG_NXTHDR(&message, cmsg);
    cmsg->cmsg_level                = SOL_ALG;
    cmsg->cmsg_type                 = ALG_SET_AEAD_ASSOCLEN;
    cmsg->cmsg_len                  = CMSG_LEN(sizeof(associatedLength));
    memcpy(CMSG_DATA(cmsg), &associatedLength, sizeof(associatedLength));
}
#endif
} // namespace
} // namespace SC

#if SC_PLATFORM_APPLE
struct SC::Cryptography::Aead::Internal
{
    static constexpr size_t CounterBatchBlocks = 16;

    CCCryptorRef aesEncryptor             = nullptr;
    uint8_t      hashSubkey[AESBlockSize] = {0};
    bool         initialized              = false;

    ~Internal() { close(); }

    void close()
    {
        if (aesEncryptor != nullptr)
        {
            CCCryptorRelease(aesEncryptor);
            aesEncryptor = nullptr;
        }
        secureClear(hashSubkey);
        initialized = false;
    }

    void reset() { close(); }

    Result encryptBlocks(Span<const uint8_t> input, Span<uint8_t> output)
    {
        size_t                written = 0;
        const CCCryptorStatus status  = CCCryptorUpdate(aesEncryptor, input.data(), input.sizeInBytes(), output.data(),
                                                        output.sizeInBytes(), &written);
        SC_TRY_MSG(status == kCCSuccess and written == input.sizeInBytes(),
                   "Cryptography::Aead - CommonCrypto AES encryption failed");
        return Result(true);
    }

    Result init(AeadType type, Span<const uint8_t> key)
    {
        close();
        SC_TRY(validateKeySize(key.sizeInBytes(), keySize(type), "Cryptography::Aead::init - invalid key size"));

        const CCCryptorStatus status = CCCryptorCreate(kCCEncrypt, kCCAlgorithmAES, kCCOptionECBMode, key.data(),
                                                       key.sizeInBytes(), nullptr, &aesEncryptor);
        if (status != kCCSuccess)
        {
            close();
            return Result::Error("Cryptography::Aead::init - CCCryptorCreate failed");
        }

        uint8_t zeroBlock[AESBlockSize] = {0};
        Result  result                  = encryptBlocks(zeroBlock, hashSubkey);
        secureClear(zeroBlock);
        if (not result)
        {
            close();
            return result;
        }
        initialized = true;
        return Result(true);
    }

    Result transform(Span<const uint8_t> nonce, Span<const uint8_t> input, Span<uint8_t> output)
    {
        uint8_t counter[AESBlockSize];
        uint8_t counterBlocks[CounterBatchBlocks * AESBlockSize];
        uint8_t keyStream[CounterBatchBlocks * AESBlockSize];
        gcmInitialCounter(nonce, counter);

        size_t offset = 0;
        while (offset < input.sizeInBytes())
        {
            const size_t remaining   = input.sizeInBytes() - offset;
            const size_t blockCount  = min(CounterBatchBlocks, (remaining + AESBlockSize - 1) / AESBlockSize);
            const size_t counterSize = blockCount * AESBlockSize;
            for (size_t blockIndex = 0; blockIndex < blockCount; ++blockIndex)
            {
                gcmIncrementCounter(counter);
                memcpy(counterBlocks + blockIndex * AESBlockSize, counter, AESBlockSize);
            }

            Result result =
                encryptBlocks(Span<const uint8_t>(counterBlocks, counterSize), Span<uint8_t>(keyStream, counterSize));
            if (not result)
            {
                secureClear(counter);
                secureClear(counterBlocks);
                secureClear(keyStream);
                return result;
            }

            const size_t bytesToProcess = min(remaining, counterSize);
            for (size_t idx = 0; idx < bytesToProcess; ++idx)
                output[offset + idx] = static_cast<uint8_t>(input[offset + idx] ^ keyStream[idx]);
            offset += bytesToProcess;
        }

        secureClear(counter);
        secureClear(counterBlocks);
        secureClear(keyStream);
        return Result(true);
    }

    Result calculateTag(Span<const uint8_t> nonce, Span<const uint8_t> aad, Span<const uint8_t> ciphertext,
                        uint8_t tag[AESBlockSize])
    {
        uint8_t counter[AESBlockSize];
        uint8_t encryptedCounter[AESBlockSize];
        gcmInitialCounter(nonce, counter);
        Result result = encryptBlocks(counter, encryptedCounter);
        if (result)
        {
            gcmHash(hashSubkey, aad, ciphertext, tag);
            for (size_t idx = 0; idx < AESBlockSize; ++idx)
                tag[idx] ^= encryptedCounter[idx];
        }
        secureClear(counter);
        secureClear(encryptedCounter);
        return result;
    }

    Result seal(Span<const uint8_t> nonce, Span<const uint8_t> aad, Span<const uint8_t> plaintext,
                Span<uint8_t> ciphertext, Span<uint8_t> tag, size_t& bytesWritten)
    {
        SC_TRY_MSG(initialized, "Cryptography::Aead::seal - not initialized");
        SC_TRY(validateAeadSealArguments(nonce, aad, plaintext, ciphertext, tag));

        Result result = transform(nonce, plaintext, ciphertext);
        if (result)
        {
            result =
                calculateTag(nonce, aad, Span<const uint8_t>(ciphertext.data(), plaintext.sizeInBytes()), tag.data());
        }
        if (not result)
        {
            secureClear(ciphertext);
            secureClear(tag);
            return result;
        }
        bytesWritten = plaintext.sizeInBytes();
        return Result(true);
    }

    Result open(Span<const uint8_t> nonce, Span<const uint8_t> aad, Span<const uint8_t> ciphertext,
                Span<const uint8_t> tag, Span<uint8_t> plaintext, size_t& bytesWritten)
    {
        SC_TRY_MSG(initialized, "Cryptography::Aead::open - not initialized");
        SC_TRY(validateAeadOpenArguments(nonce, aad, ciphertext, tag, plaintext));

        uint8_t expectedTag[AESBlockSize];
        Result  result = calculateTag(nonce, aad, ciphertext, expectedTag);
        if (not result or not gcmTagsEqual(tag, expectedTag))
        {
            secureClear(expectedTag);
            secureClear(plaintext);
            return result ? Result::Error("Cryptography::Aead::open - authentication failed") : result;
        }
        secureClear(expectedTag);

        result = transform(nonce, ciphertext, plaintext);
        if (not result)
        {
            secureClear(plaintext);
            return result;
        }
        bytesWritten = ciphertext.sizeInBytes();
        return Result(true);
    }
};

constexpr size_t SC::Cryptography::Aead::Internal::CounterBatchBlocks;

struct SC::Cryptography::Cipher::Internal
{
    CCCryptorRef      cryptor = nullptr;
    CipherStreamState stream;
    bool              initialized = false;

    ~Internal() { close(); }

    void close()
    {
        if (cryptor)
        {
            CCCryptorRelease(cryptor);
            cryptor = nullptr;
        }
        stream.reset();
        initialized = false;
    }

    void reset() { close(); }

    Result start(CipherType type, Operation operation, Span<const uint8_t> key, Span<const uint8_t> iv)
    {
        close();
        SC_TRY(validateKeySize(key.sizeInBytes(), keySize(type), "Cryptography::Cipher::start - invalid key size"));
        SC_TRY_MSG(iv.sizeInBytes() == AESBlockSize, "Cryptography::Cipher::start - invalid IV size");

        CCCryptorStatus status =
            CCCryptorCreate(operation == Operation::Encrypt ? kCCEncrypt : kCCDecrypt, kCCAlgorithmAES, 0, key.data(),
                            key.sizeInBytes(), iv.data(), &cryptor);
        SC_TRY_MSG(status == kCCSuccess, "Cryptography::Cipher::start - CCCryptorCreate failed");

        stream.operation = operation;
        initialized      = true;
        return Result(true);
    }

    Result processBlocks(Span<const uint8_t> input, Span<uint8_t> output, size_t& bytesWritten)
    {
        bytesWritten = 0;
        SC_TRY_MSG(input.sizeInBytes() % AESBlockSize == 0, "Cryptography::Cipher - block input is not aligned");
        SC_TRY_MSG(output.sizeInBytes() >= input.sizeInBytes(), "Cryptography::Cipher - insufficient output buffer");

        CCCryptorStatus status = CCCryptorUpdate(cryptor, input.data(), input.sizeInBytes(), output.data(),
                                                 output.sizeInBytes(), &bytesWritten);
        SC_TRY_MSG(status == kCCSuccess and bytesWritten == input.sizeInBytes(),
                   "Cryptography::Cipher - CCCryptorUpdate failed");
        return Result(true);
    }

    Result update(Span<const uint8_t> input, Span<uint8_t> output, size_t& bytesWritten)
    {
        return cipherUpdate(*this, input, output, bytesWritten);
    }

    Result finish(Span<uint8_t> output, size_t& bytesWritten) { return cipherFinish(*this, output, bytesWritten); }
};

struct SC::Cryptography::Hmac::Internal
{
    HashType      type = HashType::SHA256;
    CCHmacContext context;
    bool          initialized = false;

    ~Internal() { secureClear(Span<uint8_t>(reinterpret_cast<uint8_t*>(&context), sizeof(context))); }

    void reset()
    {
        secureClear(Span<uint8_t>(reinterpret_cast<uint8_t*>(&context), sizeof(context)));
        initialized = false;
    }

    Result setType(HashType newType)
    {
        secureClear(Span<uint8_t>(reinterpret_cast<uint8_t*>(&context), sizeof(context)));
        type        = newType;
        initialized = false;
        return Result(true);
    }

    Result setKey(Span<const uint8_t> key)
    {
        secureClear(Span<uint8_t>(reinterpret_cast<uint8_t*>(&context), sizeof(context)));
        CCHmacAlgorithm algorithm = type == HashType::SHA256 ? kCCHmacAlgSHA256 : kCCHmacAlgSHA384;
        CCHmacInit(&context, algorithm, key.data(), key.sizeInBytes());
        initialized = true;
        return Result(true);
    }

    Result add(Span<const uint8_t> data)
    {
        SC_TRY_MSG(initialized, "Cryptography::Hmac::add - key not set");
        CCHmacUpdate(&context, data.data(), data.sizeInBytes());
        return Result(true);
    }

    Result getMac(MacResult& result)
    {
        SC_TRY_MSG(initialized, "Cryptography::Hmac::getMac - key not set");
        result.size = digestSize(type);
        CCHmacFinal(&context, result.bytes);
        secureClear(Span<uint8_t>(reinterpret_cast<uint8_t*>(&context), sizeof(context)));
        initialized = false;
        return Result(true);
    }
};

#elif SC_PLATFORM_WINDOWS
struct SC::Cryptography::Aead::Internal
{
    BCRYPT_ALG_HANDLE algorithm       = nullptr;
    BCRYPT_KEY_HANDLE key             = nullptr;
    ULONG             keyObjectLength = 0;
    UCHAR             keyObject[4096] = {0};
    bool              initialized     = false;

    ~Internal() { close(); }

    void close()
    {
        if (key)
        {
            BCryptDestroyKey(key);
            key = nullptr;
        }
        if (algorithm)
        {
            BCryptCloseAlgorithmProvider(algorithm, 0);
            algorithm = nullptr;
        }
        secureClear(keyObject);
        keyObjectLength = 0;
        initialized     = false;
    }

    void reset() { close(); }

    Result init(AeadType type, Span<const uint8_t> keyBytes)
    {
        close();
        SC_TRY(validateKeySize(keyBytes.sizeInBytes(), keySize(type), "Cryptography::Aead::init - invalid key size"));

        NTSTATUS status = BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_AES_ALGORITHM, nullptr, 0);
        SC_TRY_MSG(bcryptSuccess(status), "Cryptography::Aead::init - BCryptOpenAlgorithmProvider failed");

        status = BCryptSetProperty(algorithm, BCRYPT_CHAINING_MODE,
                                   reinterpret_cast<PUCHAR>(const_cast<wchar_t*>(BCRYPT_CHAIN_MODE_GCM)),
                                   sizeof(BCRYPT_CHAIN_MODE_GCM), 0);
        if (not bcryptSuccess(status))
        {
            close();
            return Result::Error("Cryptography::Aead::init - BCRYPT_CHAIN_MODE_GCM failed");
        }

        ULONG bytesCopied = 0;
        status = BCryptGetProperty(algorithm, BCRYPT_OBJECT_LENGTH, reinterpret_cast<PUCHAR>(&keyObjectLength),
                                   sizeof(keyObjectLength), &bytesCopied, 0);
        if (not bcryptSuccess(status))
        {
            close();
            return Result::Error("Cryptography::Aead::init - BCRYPT_OBJECT_LENGTH failed");
        }

        if (keyObjectLength > sizeof(keyObject))
        {
            close();
            return Result::Error("Cryptography::Aead::init - fixed key object buffer too small");
        }

        status = BCryptGenerateSymmetricKey(algorithm, &key, keyObject, keyObjectLength,
                                            reinterpret_cast<PUCHAR>(const_cast<uint8_t*>(keyBytes.data())),
                                            static_cast<ULONG>(keyBytes.sizeInBytes()), 0);
        if (not bcryptSuccess(status))
        {
            close();
            return Result::Error("Cryptography::Aead::init - BCryptGenerateSymmetricKey failed");
        }

        initialized = true;
        return Result(true);
    }

    Result seal(Span<const uint8_t> nonce, Span<const uint8_t> aad, Span<const uint8_t> plaintext,
                Span<uint8_t> ciphertext, Span<uint8_t> tag, size_t& bytesWritten)
    {
        bytesWritten = 0;
        SC_TRY_MSG(initialized, "Cryptography::Aead::seal - not initialized");
        SC_TRY(validateAeadSealArguments(nonce, aad, plaintext, ciphertext, tag));

        BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO authInfo;
        BCRYPT_INIT_AUTH_MODE_INFO(authInfo);
        authInfo.pbNonce    = reinterpret_cast<PUCHAR>(const_cast<uint8_t*>(nonce.data()));
        authInfo.cbNonce    = static_cast<ULONG>(nonce.sizeInBytes());
        authInfo.pbAuthData = reinterpret_cast<PUCHAR>(const_cast<uint8_t*>(aad.data()));
        authInfo.cbAuthData = static_cast<ULONG>(aad.sizeInBytes());
        authInfo.pbTag      = reinterpret_cast<PUCHAR>(tag.data());
        authInfo.cbTag      = static_cast<ULONG>(tag.sizeInBytes());

        ULONG    written = 0;
        NTSTATUS status  = BCryptEncrypt(key, reinterpret_cast<PUCHAR>(const_cast<uint8_t*>(plaintext.data())),
                                         static_cast<ULONG>(plaintext.sizeInBytes()), &authInfo, nullptr, 0,
                                         reinterpret_cast<PUCHAR>(ciphertext.data()),
                                         static_cast<ULONG>(ciphertext.sizeInBytes()), &written, 0);
        SC_TRY_MSG(bcryptSuccess(status), "Cryptography::Aead::seal - BCryptEncrypt failed");

        bytesWritten = written;
        return Result(true);
    }

    Result open(Span<const uint8_t> nonce, Span<const uint8_t> aad, Span<const uint8_t> ciphertext,
                Span<const uint8_t> tag, Span<uint8_t> plaintext, size_t& bytesWritten)
    {
        bytesWritten = 0;
        SC_TRY_MSG(initialized, "Cryptography::Aead::open - not initialized");
        SC_TRY(validateAeadOpenArguments(nonce, aad, ciphertext, tag, plaintext));

        BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO authInfo;
        BCRYPT_INIT_AUTH_MODE_INFO(authInfo);
        authInfo.pbNonce    = reinterpret_cast<PUCHAR>(const_cast<uint8_t*>(nonce.data()));
        authInfo.cbNonce    = static_cast<ULONG>(nonce.sizeInBytes());
        authInfo.pbAuthData = reinterpret_cast<PUCHAR>(const_cast<uint8_t*>(aad.data()));
        authInfo.cbAuthData = static_cast<ULONG>(aad.sizeInBytes());
        authInfo.pbTag      = reinterpret_cast<PUCHAR>(const_cast<uint8_t*>(tag.data()));
        authInfo.cbTag      = static_cast<ULONG>(tag.sizeInBytes());

        ULONG    written = 0;
        NTSTATUS status  = BCryptDecrypt(key, reinterpret_cast<PUCHAR>(const_cast<uint8_t*>(ciphertext.data())),
                                         static_cast<ULONG>(ciphertext.sizeInBytes()), &authInfo, nullptr, 0,
                                         reinterpret_cast<PUCHAR>(plaintext.data()),
                                         static_cast<ULONG>(plaintext.sizeInBytes()), &written, 0);
        if (not bcryptSuccess(status))
        {
            secureClear(plaintext);
            return Result::Error("Cryptography::Aead::open - authentication failed");
        }

        bytesWritten = written;
        return Result(true);
    }
};

struct SC::Cryptography::Cipher::Internal
{
    BCRYPT_ALG_HANDLE algorithm       = nullptr;
    BCRYPT_KEY_HANDLE key             = nullptr;
    ULONG             keyObjectLength = 0;
    UCHAR             keyObject[4096] = {0};
    uint8_t           currentIV[16]   = {0};
    CipherStreamState stream;
    bool              initialized = false;

    ~Internal() { close(); }

    void close()
    {
        if (key)
        {
            BCryptDestroyKey(key);
            key = nullptr;
        }
        if (algorithm)
        {
            BCryptCloseAlgorithmProvider(algorithm, 0);
            algorithm = nullptr;
        }
        secureClear(keyObject);
        secureClear(currentIV);
        stream.reset();
        keyObjectLength = 0;
        initialized     = false;
    }

    void reset() { close(); }

    Result start(CipherType type, Operation newOperation, Span<const uint8_t> keyBytes, Span<const uint8_t> iv)
    {
        close();
        SC_TRY(
            validateKeySize(keyBytes.sizeInBytes(), keySize(type), "Cryptography::Cipher::start - invalid key size"));
        SC_TRY_MSG(iv.sizeInBytes() == AESBlockSize, "Cryptography::Cipher::start - invalid IV size");

        NTSTATUS status = BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_AES_ALGORITHM, nullptr, 0);
        SC_TRY_MSG(bcryptSuccess(status), "Cryptography::Cipher::start - BCryptOpenAlgorithmProvider failed");

        status = BCryptSetProperty(algorithm, BCRYPT_CHAINING_MODE,
                                   reinterpret_cast<PUCHAR>(const_cast<wchar_t*>(BCRYPT_CHAIN_MODE_CBC)),
                                   sizeof(BCRYPT_CHAIN_MODE_CBC), 0);
        if (not bcryptSuccess(status))
        {
            close();
            return Result::Error("Cryptography::Cipher::start - BCRYPT_CHAIN_MODE_CBC failed");
        }

        ULONG bytesCopied = 0;
        status = BCryptGetProperty(algorithm, BCRYPT_OBJECT_LENGTH, reinterpret_cast<PUCHAR>(&keyObjectLength),
                                   sizeof(keyObjectLength), &bytesCopied, 0);
        if (not bcryptSuccess(status))
        {
            close();
            return Result::Error("Cryptography::Cipher::start - BCRYPT_OBJECT_LENGTH failed");
        }

        if (keyObjectLength > sizeof(keyObject))
        {
            close();
            return Result::Error("Cryptography::Cipher::start - fixed key object buffer too small");
        }

        status = BCryptGenerateSymmetricKey(algorithm, &key, keyObject, keyObjectLength,
                                            reinterpret_cast<PUCHAR>(const_cast<uint8_t*>(keyBytes.data())),
                                            static_cast<ULONG>(keyBytes.sizeInBytes()), 0);
        if (not bcryptSuccess(status))
        {
            close();
            return Result::Error("Cryptography::Cipher::start - BCryptGenerateSymmetricKey failed");
        }

        memcpy(currentIV, iv.data(), sizeof(currentIV));
        stream.operation = newOperation;
        initialized      = true;
        return Result(true);
    }

    Result processBlocks(Span<const uint8_t> input, Span<uint8_t> output, size_t& bytesWritten)
    {
        bytesWritten = 0;
        if (input.empty())
            return Result(true);

        SC_TRY_MSG(input.sizeInBytes() % AESBlockSize == 0, "Cryptography::Cipher - block input is not aligned");
        SC_TRY_MSG(input.sizeInBytes() <= BcryptMaxInputSize,
                   "Cryptography::Cipher - message is too large for the backend");
        SC_TRY_MSG(output.sizeInBytes() >= input.sizeInBytes(), "Cryptography::Cipher - insufficient output buffer");

        ULONG    written = 0;
        NTSTATUS status;
        if (stream.operation == Operation::Encrypt)
        {
            status = BCryptEncrypt(key, reinterpret_cast<PUCHAR>(const_cast<uint8_t*>(input.data())),
                                   static_cast<ULONG>(input.sizeInBytes()), nullptr, currentIV, sizeof(currentIV),
                                   reinterpret_cast<PUCHAR>(output.data()), static_cast<ULONG>(input.sizeInBytes()),
                                   &written, 0);
        }
        else
        {
            status = BCryptDecrypt(key, reinterpret_cast<PUCHAR>(const_cast<uint8_t*>(input.data())),
                                   static_cast<ULONG>(input.sizeInBytes()), nullptr, currentIV, sizeof(currentIV),
                                   reinterpret_cast<PUCHAR>(output.data()), static_cast<ULONG>(input.sizeInBytes()),
                                   &written, 0);
        }
        SC_TRY_MSG(bcryptSuccess(status), "Cryptography::Cipher - BCryptEncrypt/Decrypt failed");
        bytesWritten = written;

        if (input.sizeInBytes() >= AESBlockSize)
        {
            if (stream.operation == Operation::Encrypt)
                memcpy(currentIV, output.data() + bytesWritten - AESBlockSize, AESBlockSize);
            else
                memcpy(currentIV, input.data() + input.sizeInBytes() - AESBlockSize, AESBlockSize);
        }
        return Result(true);
    }

    Result update(Span<const uint8_t> input, Span<uint8_t> output, size_t& bytesWritten)
    {
        return cipherUpdate(*this, input, output, bytesWritten);
    }

    Result finish(Span<uint8_t> output, size_t& bytesWritten) { return cipherFinish(*this, output, bytesWritten); }
};

struct SC::Cryptography::Hmac::Internal
{
    HashType           type               = HashType::SHA256;
    BCRYPT_ALG_HANDLE  algorithm          = nullptr;
    BCRYPT_HASH_HANDLE hash               = nullptr;
    ULONG              objectLength       = 0;
    UCHAR              objectBuffer[2048] = {0};
    bool               initialized        = false;

    ~Internal() { close(); }

    void close()
    {
        if (hash)
        {
            BCryptDestroyHash(hash);
            hash = nullptr;
        }
        if (algorithm)
        {
            BCryptCloseAlgorithmProvider(algorithm, 0);
            algorithm = nullptr;
        }
        secureClear(objectBuffer);
        objectLength = 0;
        initialized  = false;
    }

    void reset() { close(); }

    Result setType(HashType newType)
    {
        close();
        type = newType;
        return Result(true);
    }

    Result setKey(Span<const uint8_t> key)
    {
        close();
        SC_TRY_MSG(key.sizeInBytes() <= BcryptMaxInputSize,
                   "Cryptography::Hmac::setKey - key is too large for the backend");

        NTSTATUS status =
            BCryptOpenAlgorithmProvider(&algorithm, bcryptHashName(type), nullptr, BCRYPT_ALG_HANDLE_HMAC_FLAG);
        SC_TRY_MSG(bcryptSuccess(status), "Cryptography::Hmac::setKey - BCryptOpenAlgorithmProvider failed");

        ULONG bytesCopied = 0;
        status            = BCryptGetProperty(algorithm, BCRYPT_OBJECT_LENGTH, reinterpret_cast<PUCHAR>(&objectLength),
                                              sizeof(objectLength), &bytesCopied, 0);
        if (not bcryptSuccess(status))
        {
            close();
            return Result::Error("Cryptography::Hmac::setKey - BCRYPT_OBJECT_LENGTH failed");
        }

        if (objectLength > sizeof(objectBuffer))
        {
            close();
            return Result::Error("Cryptography::Hmac::setKey - fixed hash object buffer too small");
        }

        status = BCryptCreateHash(algorithm, &hash, objectBuffer, objectLength,
                                  reinterpret_cast<PUCHAR>(const_cast<uint8_t*>(key.data())),
                                  static_cast<ULONG>(key.sizeInBytes()), 0);
        if (not bcryptSuccess(status))
        {
            close();
            return Result::Error("Cryptography::Hmac::setKey - BCryptCreateHash failed");
        }

        initialized = true;
        return Result(true);
    }

    Result add(Span<const uint8_t> data)
    {
        SC_TRY_MSG(initialized, "Cryptography::Hmac::add - key not set");
        size_t offset = 0;
        while (offset < data.sizeInBytes())
        {
            const size_t chunkSize = min(BcryptMaxInputSize, data.sizeInBytes() - offset);
            NTSTATUS status = BCryptHashData(hash, reinterpret_cast<PUCHAR>(const_cast<uint8_t*>(data.data() + offset)),
                                             static_cast<ULONG>(chunkSize), 0);
            SC_TRY_MSG(bcryptSuccess(status), "Cryptography::Hmac::add - BCryptHashData failed");
            offset += chunkSize;
        }
        return Result(true);
    }

    Result getMac(MacResult& result)
    {
        SC_TRY_MSG(initialized, "Cryptography::Hmac::getMac - key not set");
        result.size     = digestSize(type);
        NTSTATUS status = BCryptFinishHash(hash, result.bytes, static_cast<ULONG>(result.size), 0);
        close();
        if (not bcryptSuccess(status))
        {
            secureClear(result.bytes);
            result.size = 0;
            return Result::Error("Cryptography::Hmac::getMac - BCryptFinishHash failed");
        }
        return Result(true);
    }
};

#elif SC_PLATFORM_LINUX
struct SC::Cryptography::Aead::Internal
{
    int  mainSocket  = -1;
    int  opSocket    = -1;
    bool initialized = false;

    ~Internal() { close(); }

    void close()
    {
        closeIfValid(opSocket);
        closeIfValid(mainSocket);
        initialized = false;
    }

    void reset() { close(); }

    Result init(AeadType type, Span<const uint8_t> key)
    {
        close();
        SC_TRY(validateKeySize(key.sizeInBytes(), keySize(type), "Cryptography::Aead::init - invalid key size"));
        SC_TRY(openAlgorithmSocket("aead", "gcm(aes)", mainSocket));
        auto deferClose = MakeDeferred([&] { close(); });
        SC_TRY(configureKey(mainSocket, key));
        SC_TRY_MSG(::setsockopt(mainSocket, SOL_ALG, ALG_SET_AEAD_AUTHSIZE, nullptr, GCMTagSize) == 0,
                   "Cryptography::Aead::init - ALG_SET_AEAD_AUTHSIZE failed");
        initialized = true;
        deferClose.disarm();
        return Result(true);
    }

    Result seal(Span<const uint8_t> nonce, Span<const uint8_t> aad, Span<const uint8_t> plaintext,
                Span<uint8_t> ciphertext, Span<uint8_t> tag, size_t& bytesWritten)
    {
        bytesWritten = 0;
        SC_TRY_MSG(initialized, "Cryptography::Aead::seal - not initialized");
        SC_TRY(validateAeadSealArguments(nonce, aad, plaintext, ciphertext, tag));
        SC_TRY_MSG(aad.sizeInBytes() <= AeadMaxAssociatedDataSize,
                   "Cryptography::Aead - associated data is too large for the Linux backend");
        SC_TRY_MSG(plaintext.sizeInBytes() <= AeadMaxInputSize - GCMTagSize and
                       aad.sizeInBytes() <= AeadMaxInputSize - plaintext.sizeInBytes(),
                   "Cryptography::Aead - message is too large for the backend");
        SC_TRY(acceptOperationSocket(mainSocket, opSocket));
        auto deferClose = MakeDeferred([&] { closeIfValid(opSocket); });

        iovec inputIov[2];
        inputIov[0].iov_base = const_cast<uint8_t*>(aad.data());
        inputIov[0].iov_len  = aad.sizeInBytes();
        inputIov[1].iov_base = const_cast<uint8_t*>(plaintext.data());
        inputIov[1].iov_len  = plaintext.sizeInBytes();

        char   control[CMSG_SPACE(sizeof(uint32_t)) + CMSG_SPACE(sizeof(af_alg_iv) + GCMNonceSize) +
                     CMSG_SPACE(sizeof(uint32_t))];
        msghdr inputMessage;
        configureAeadMessage(inputMessage, control, sizeof(control), ALG_OP_ENCRYPT, nonce, aad.sizeInBytes());
        inputMessage.msg_iov    = inputIov;
        inputMessage.msg_iovlen = 2;

        const size_t  inputSize = aad.sizeInBytes() + plaintext.sizeInBytes();
        const ssize_t sent      = ::sendmsg(opSocket, &inputMessage, 0);
        SC_TRY_MSG(sent == static_cast<ssize_t>(inputSize), "Cryptography::Aead::seal - sendmsg failed");

        uint8_t associatedDataOutput[AeadMaxAssociatedDataSize];
        iovec   outputIov[3];
        outputIov[0].iov_base = associatedDataOutput;
        outputIov[0].iov_len  = aad.sizeInBytes();
        outputIov[1].iov_base = ciphertext.data();
        outputIov[1].iov_len  = plaintext.sizeInBytes();
        outputIov[2].iov_base = tag.data();
        outputIov[2].iov_len  = tag.sizeInBytes();

        msghdr outputMessage;
        memset(&outputMessage, 0, sizeof(outputMessage));
        outputMessage.msg_iov    = outputIov;
        outputMessage.msg_iovlen = 3;

        const size_t  outputSize = aad.sizeInBytes() + plaintext.sizeInBytes() + GCMTagSize;
        const ssize_t received   = ::recvmsg(opSocket, &outputMessage, 0);
        if (received != static_cast<ssize_t>(outputSize))
        {
            secureClear(tag);
            return Result::Error("Cryptography::Aead::seal - recvmsg failed");
        }

        bytesWritten = plaintext.sizeInBytes();
        return Result(true);
    }

    Result open(Span<const uint8_t> nonce, Span<const uint8_t> aad, Span<const uint8_t> ciphertext,
                Span<const uint8_t> tag, Span<uint8_t> plaintext, size_t& bytesWritten)
    {
        bytesWritten = 0;
        SC_TRY_MSG(initialized, "Cryptography::Aead::open - not initialized");
        SC_TRY(validateAeadOpenArguments(nonce, aad, ciphertext, tag, plaintext));
        SC_TRY_MSG(aad.sizeInBytes() <= AeadMaxAssociatedDataSize,
                   "Cryptography::Aead - associated data is too large for the Linux backend");
        SC_TRY_MSG(ciphertext.sizeInBytes() <= AeadMaxInputSize - GCMTagSize and
                       aad.sizeInBytes() <= AeadMaxInputSize - ciphertext.sizeInBytes() - GCMTagSize,
                   "Cryptography::Aead - message is too large for the backend");
        SC_TRY(acceptOperationSocket(mainSocket, opSocket));
        auto deferClose = MakeDeferred([&] { closeIfValid(opSocket); });

        iovec inputIov[3];
        inputIov[0].iov_base = const_cast<uint8_t*>(aad.data());
        inputIov[0].iov_len  = aad.sizeInBytes();
        inputIov[1].iov_base = const_cast<uint8_t*>(ciphertext.data());
        inputIov[1].iov_len  = ciphertext.sizeInBytes();
        inputIov[2].iov_base = const_cast<uint8_t*>(tag.data());
        inputIov[2].iov_len  = tag.sizeInBytes();

        char   control[CMSG_SPACE(sizeof(uint32_t)) + CMSG_SPACE(sizeof(af_alg_iv) + GCMNonceSize) +
                     CMSG_SPACE(sizeof(uint32_t))];
        msghdr inputMessage;
        configureAeadMessage(inputMessage, control, sizeof(control), ALG_OP_DECRYPT, nonce, aad.sizeInBytes());
        inputMessage.msg_iov    = inputIov;
        inputMessage.msg_iovlen = 3;

        const size_t  inputSize = aad.sizeInBytes() + ciphertext.sizeInBytes() + GCMTagSize;
        const ssize_t sent      = ::sendmsg(opSocket, &inputMessage, 0);
        if (sent != static_cast<ssize_t>(inputSize))
        {
            secureClear(plaintext);
            return Result::Error("Cryptography::Aead::open - sendmsg failed");
        }

        uint8_t associatedDataOutput[AeadMaxAssociatedDataSize];
        uint8_t emptyPlaintextOutput = 0;
        iovec   outputIov[2];
        outputIov[0].iov_base = associatedDataOutput;
        outputIov[0].iov_len  = aad.sizeInBytes();
        outputIov[1].iov_base = ciphertext.sizeInBytes() == 0 ? &emptyPlaintextOutput : plaintext.data();
        // A zero-capacity recvmsg returns before AF_ALG authenticates an empty message. One scratch byte makes the
        // kernel execute the operation; valid empty plaintext still returns zero bytes and an invalid tag returns
        // EBADMSG.
        outputIov[1].iov_len = ciphertext.sizeInBytes() == 0 and aad.sizeInBytes() == 0 ? 1 : ciphertext.sizeInBytes();

        msghdr outputMessage;
        memset(&outputMessage, 0, sizeof(outputMessage));
        outputMessage.msg_iov    = outputIov;
        outputMessage.msg_iovlen = 2;

        const ssize_t received = ::recvmsg(opSocket, &outputMessage, 0);
        if (received != static_cast<ssize_t>(aad.sizeInBytes() + ciphertext.sizeInBytes()))
        {
            secureClear(plaintext);
            if (received == -1 and errno == EBADMSG)
                return Result::Error("Cryptography::Aead::open - authentication failed");
            return Result::Error("Cryptography::Aead::open - recvmsg failed");
        }

        bytesWritten = ciphertext.sizeInBytes();
        return Result(true);
    }
};

struct SC::Cryptography::Cipher::Internal
{
    int               mainSocket    = -1;
    int               opSocket      = -1;
    uint8_t           currentIV[16] = {0};
    CipherStreamState stream;
    bool              initialized = false;

    ~Internal() { close(); }

    void close()
    {
        closeIfValid(opSocket);
        closeIfValid(mainSocket);
        secureClear(currentIV);
        stream.reset();
        initialized = false;
    }

    void reset() { close(); }

    Result start(CipherType type, Operation newOperation, Span<const uint8_t> key, Span<const uint8_t> iv)
    {
        close();
        SC_TRY(validateKeySize(key.sizeInBytes(), keySize(type), "Cryptography::Cipher::start - invalid key size"));
        SC_TRY_MSG(iv.sizeInBytes() == AESBlockSize, "Cryptography::Cipher::start - invalid IV size");
        SC_TRY(openAlgorithmSocket("skcipher", "cbc(aes)", mainSocket));
        auto deferClose = MakeDeferred([&] { close(); });
        SC_TRY(configureKey(mainSocket, key));
        SC_TRY(acceptOperationSocket(mainSocket, opSocket));
        memcpy(currentIV, iv.data(), sizeof(currentIV));
        stream.operation = newOperation;
        initialized      = true;
        deferClose.disarm();
        return Result(true);
    }

    Result processBlocks(Span<const uint8_t> input, Span<uint8_t> output, size_t& bytesWritten)
    {
        bytesWritten = 0;
        if (input.empty())
            return Result(true);

        SC_TRY_MSG(input.sizeInBytes() % AESBlockSize == 0, "Cryptography::Cipher - block input is not aligned");
        SC_TRY_MSG(output.sizeInBytes() >= input.sizeInBytes(), "Cryptography::Cipher - insufficient output buffer");

        char control[CMSG_SPACE(sizeof(uint32_t)) + CMSG_SPACE(sizeof(af_alg_iv) + 16)];
        memset(control, 0, sizeof(control));

        iovec iov;
        iov.iov_base = const_cast<uint8_t*>(input.data());
        iov.iov_len  = input.sizeInBytes();

        msghdr message;
        memset(&message, 0, sizeof(message));
        message.msg_iov        = &iov;
        message.msg_iovlen     = 1;
        message.msg_control    = control;
        message.msg_controllen = sizeof(control);

        cmsghdr* cmsg    = CMSG_FIRSTHDR(&message);
        cmsg->cmsg_level = SOL_ALG;
        cmsg->cmsg_type  = ALG_SET_OP;
        cmsg->cmsg_len   = CMSG_LEN(sizeof(uint32_t));
        *reinterpret_cast<uint32_t*>(CMSG_DATA(cmsg)) =
            stream.operation == Operation::Encrypt ? ALG_OP_ENCRYPT : ALG_OP_DECRYPT;

        cmsg             = CMSG_NXTHDR(&message, cmsg);
        cmsg->cmsg_level = SOL_ALG;
        cmsg->cmsg_type  = ALG_SET_IV;
        cmsg->cmsg_len   = CMSG_LEN(sizeof(af_alg_iv) + sizeof(currentIV));
        af_alg_iv* iv    = reinterpret_cast<af_alg_iv*>(CMSG_DATA(cmsg));
        iv->ivlen        = sizeof(currentIV);
        memcpy(iv->iv, currentIV, sizeof(currentIV));

        ssize_t sent = ::sendmsg(opSocket, &message, 0);
        SC_TRY_MSG(sent == static_cast<ssize_t>(input.sizeInBytes()), "Cryptography::Cipher - sendmsg failed");

        ssize_t received = ::recv(opSocket, output.data(), input.sizeInBytes(), 0);
        SC_TRY_MSG(received == static_cast<ssize_t>(input.sizeInBytes()), "Cryptography::Cipher - recv failed");
        bytesWritten = input.sizeInBytes();

        if (stream.operation == Operation::Encrypt)
            memcpy(currentIV, output.data() + bytesWritten - AESBlockSize, AESBlockSize);
        else
            memcpy(currentIV, input.data() + input.sizeInBytes() - AESBlockSize, AESBlockSize);
        return Result(true);
    }

    Result update(Span<const uint8_t> input, Span<uint8_t> output, size_t& bytesWritten)
    {
        return cipherUpdate(*this, input, output, bytesWritten);
    }

    Result finish(Span<uint8_t> output, size_t& bytesWritten) { return cipherFinish(*this, output, bytesWritten); }
};

struct SC::Cryptography::Hmac::Internal
{
    HashType type        = HashType::SHA256;
    int      mainSocket  = -1;
    int      opSocket    = -1;
    bool     initialized = false;

    ~Internal() { close(); }

    void close()
    {
        closeIfValid(opSocket);
        closeIfValid(mainSocket);
        initialized = false;
    }

    void reset() { close(); }

    const char* algorithmName() const
    {
        switch (type)
        {
        case HashType::SHA256: return "hmac(sha256)";
        case HashType::SHA384: return "hmac(sha384)";
        }
        return "hmac(sha256)";
    }

    Result setType(HashType newType)
    {
        close();
        type = newType;
        return Result(true);
    }

    Result setKey(Span<const uint8_t> key)
    {
        close();
        SC_TRY(openAlgorithmSocket("hash", algorithmName(), mainSocket));
        auto deferClose = MakeDeferred([&] { close(); });
        SC_TRY(configureKey(mainSocket, key));
        SC_TRY(acceptOperationSocket(mainSocket, opSocket));
        initialized = true;
        deferClose.disarm();
        return Result(true);
    }

    Result add(Span<const uint8_t> data)
    {
        SC_TRY_MSG(initialized, "Cryptography::Hmac::add - key not set");
        size_t offset = 0;
        while (offset < data.sizeInBytes())
        {
            const size_t chunkSize = min(static_cast<size_t>(0x7fffffffu), data.sizeInBytes() - offset);
            ssize_t      sent      = ::send(opSocket, data.data() + offset, chunkSize, MSG_MORE);
            SC_TRY_MSG(sent > 0, "Cryptography::Hmac::add - send failed");
            offset += static_cast<size_t>(sent);
        }
        return Result(true);
    }

    Result getMac(MacResult& result)
    {
        SC_TRY_MSG(initialized, "Cryptography::Hmac::getMac - key not set");
        result.size      = digestSize(type);
        ssize_t received = ::recv(opSocket, result.bytes, result.size, 0);
        close();
        if (received != static_cast<ssize_t>(result.size))
        {
            secureClear(result.bytes);
            result.size = 0;
            return Result::Error("Cryptography::Hmac::getMac - recv failed");
        }
        return Result(true);
    }
};
#else
struct SC::Cryptography::Aead::Internal
{
    void reset() {}

    Result init(AeadType, Span<const uint8_t>) { return Result::Error("Cryptography::Aead - unsupported platform"); }
    Result seal(Span<const uint8_t>, Span<const uint8_t>, Span<const uint8_t>, Span<uint8_t>, Span<uint8_t>, size_t&)
    {
        return Result::Error("Cryptography::Aead - unsupported platform");
    }
    Result open(Span<const uint8_t>, Span<const uint8_t>, Span<const uint8_t>, Span<const uint8_t>, Span<uint8_t>,
                size_t&)
    {
        return Result::Error("Cryptography::Aead - unsupported platform");
    }
};

struct SC::Cryptography::Cipher::Internal
{
    Result start(CipherType, Operation, Span<const uint8_t>, Span<const uint8_t>)
    {
        return Result::Error("Cryptography::Cipher - unsupported platform");
    }
    Result update(Span<const uint8_t>, Span<uint8_t>, size_t&)
    {
        return Result::Error("Cryptography::Cipher - unsupported platform");
    }
    Result finish(Span<uint8_t>, size_t&) { return Result::Error("Cryptography::Cipher - unsupported platform"); }
    void   reset() {}
};

struct SC::Cryptography::Hmac::Internal
{
    Result setType(HashType) { return Result::Error("Cryptography::Hmac - unsupported platform"); }
    Result setKey(Span<const uint8_t>) { return Result::Error("Cryptography::Hmac - unsupported platform"); }
    Result add(Span<const uint8_t>) { return Result::Error("Cryptography::Hmac - unsupported platform"); }
    Result getMac(MacResult&) { return Result::Error("Cryptography::Hmac - unsupported platform"); }
    void   reset() {}
};
#endif

SC::Result SC::Cryptography::queryFeatures(Features& outFeatures)
{
    outFeatures = {};

#if SC_PLATFORM_APPLE
    outFeatures.secureRandom   = true;
    outFeatures.aes128Gcm      = true;
    outFeatures.aes256Gcm      = true;
    outFeatures.aes128CbcPkcs7 = true;
    outFeatures.aes256CbcPkcs7 = true;
    outFeatures.hmacSha256     = true;
    outFeatures.hmacSha384     = true;
    outFeatures.hkdfSha256     = true;
    outFeatures.hkdfSha384     = true;
#elif SC_PLATFORM_WINDOWS
    outFeatures.secureRandom   = true;
    outFeatures.aes128Gcm      = true;
    outFeatures.aes256Gcm      = true;
    outFeatures.aes128CbcPkcs7 = true;
    outFeatures.aes256CbcPkcs7 = true;
    outFeatures.hmacSha256     = true;
    outFeatures.hmacSha384     = true;
    outFeatures.hkdfSha256     = true;
    outFeatures.hkdfSha384     = true;
#elif SC_PLATFORM_LINUX
    outFeatures.secureRandom   = true;
    outFeatures.aes128Gcm      = aeadSupported(16);
    outFeatures.aes256Gcm      = aeadSupported(32);
    outFeatures.aes128CbcPkcs7 = algorithmSupported("skcipher", "cbc(aes)");
    outFeatures.aes256CbcPkcs7 = outFeatures.aes128CbcPkcs7;
    outFeatures.hmacSha256     = algorithmSupported("hash", "hmac(sha256)");
    outFeatures.hmacSha384     = algorithmSupported("hash", "hmac(sha384)");
    outFeatures.hkdfSha256     = outFeatures.hmacSha256;
    outFeatures.hkdfSha384     = outFeatures.hmacSha384;
#endif
    return Result(true);
}

SC::Result SC::Cryptography::Random::fill(Span<uint8_t> output)
{
    if (output.empty())
        return Result(true);

#if SC_PLATFORM_APPLE
    SC_TRY_MSG(CCRandomGenerateBytes(output.data(), output.sizeInBytes()) == kCCSuccess,
               "Cryptography::Random::fill - CCRandomGenerateBytes failed");
    return Result(true);
#elif SC_PLATFORM_WINDOWS
    size_t offset = 0;
    while (offset < output.sizeInBytes())
    {
        const size_t chunkSize = min(BcryptMaxInputSize, output.sizeInBytes() - offset);
        NTSTATUS     status    = BCryptGenRandom(nullptr, output.data() + offset, static_cast<ULONG>(chunkSize),
                                                 BCRYPT_USE_SYSTEM_PREFERRED_RNG);
        SC_TRY_MSG(bcryptSuccess(status), "Cryptography::Random::fill - BCryptGenRandom failed");
        offset += chunkSize;
    }
    return Result(true);
#elif SC_PLATFORM_LINUX
    size_t total = 0;
    while (total < output.sizeInBytes())
    {
        ssize_t res = ::getrandom(output.data() + total, output.sizeInBytes() - total, 0);
        if (res == -1 and errno == EINTR)
            continue;
        SC_TRY_MSG(res > 0, "Cryptography::Random::fill - getrandom failed");
        total += static_cast<size_t>(res);
    }
    return Result(true);
#else
    (void)output;
    return Result::Error("Cryptography::Random::fill - unsupported platform");
#endif
}

SC::Result SC::Cryptography::Aead::init(AeadType type, Span<const uint8_t> key)
{
    internal.get().reset();
    SC_TRY_MSG(isValid(type), "Cryptography::Aead::init - invalid AEAD type");
    return internal.get().init(type, key);
}

SC::Result SC::Cryptography::Aead::seal(Span<const uint8_t> nonce, Span<const uint8_t> aad,
                                        Span<const uint8_t> plaintext, Span<uint8_t> ciphertext, Span<uint8_t> tag,
                                        size_t& bytesWritten)
{
    bytesWritten = 0;
    return internal.get().seal(nonce, aad, plaintext, ciphertext, tag, bytesWritten);
}

SC::Result SC::Cryptography::Aead::open(Span<const uint8_t> nonce, Span<const uint8_t> aad,
                                        Span<const uint8_t> ciphertext, Span<const uint8_t> tag,
                                        Span<uint8_t> plaintext, size_t& bytesWritten)
{
    bytesWritten = 0;
    return internal.get().open(nonce, aad, ciphertext, tag, plaintext, bytesWritten);
}

SC::Result SC::Cryptography::Cipher::start(CipherType type, Operation operation, Span<const uint8_t> key,
                                           Span<const uint8_t> iv)
{
    reset();
    SC_TRY_MSG(isValid(type), "Cryptography::Cipher::start - invalid cipher type");
    SC_TRY_MSG(isValid(operation), "Cryptography::Cipher::start - invalid operation");
    return internal.get().start(type, operation, key, iv);
}

SC::Result SC::Cryptography::Cipher::update(Span<const uint8_t> input, Span<uint8_t> output, size_t& bytesWritten)
{
    bytesWritten = 0;
    return internal.get().update(input, output, bytesWritten);
}

SC::Result SC::Cryptography::Cipher::finish(Span<uint8_t> output, size_t& bytesWritten)
{
    bytesWritten = 0;
    return internal.get().finish(output, bytesWritten);
}

void SC::Cryptography::Cipher::reset() { internal.get().reset(); }

SC::Result SC::Cryptography::Hmac::setType(HashType type)
{
    reset();
    SC_TRY_MSG(isValid(type), "Cryptography::Hmac::setType - invalid hash type");
    return internal.get().setType(type);
}

SC::Result SC::Cryptography::Hmac::setKey(Span<const uint8_t> key) { return internal.get().setKey(key); }

SC::Result SC::Cryptography::Hmac::add(Span<const uint8_t> data) { return internal.get().add(data); }

SC::Result SC::Cryptography::Hmac::getMac(MacResult& result)
{
    result.size = 0;
    return internal.get().getMac(result);
}

void SC::Cryptography::Hmac::reset() { internal.get().reset(); }

SC::Result SC::Cryptography::Hkdf::derive(HashType type, Span<const uint8_t> salt, Span<const uint8_t> ikm,
                                          Span<const uint8_t> info, Span<uint8_t> output)
{
    size_t hashLen = digestSize(type);
    SC_TRY_MSG(hashLen > 0, "Cryptography::Hkdf::derive - unsupported hash type");
    SC_TRY_MSG(output.sizeInBytes() <= hashLen * 255, "Cryptography::Hkdf::derive - output too large");
    auto clearOutputOnFailure = MakeDeferred([&] { secureClear(output); });

    uint8_t zeroSalt[48] = {0};
    if (salt.empty())
        salt = Span<const uint8_t>(zeroSalt, hashLen);

    Hmac extract;
    SC_TRY(extract.setType(type));
    SC_TRY(extract.setKey(salt));
    SC_TRY(extract.add(ikm));

    MacResult prk;
    SC_TRY(extract.getMac(prk));

    uint8_t previous[48] = {0};
    auto    clearSecrets = MakeDeferred(
        [&]
        {
            secureClear(prk.bytes);
            secureClear(previous);
        });
    size_t  previousSize = 0;
    size_t  produced     = 0;
    uint8_t counter      = 1;

    while (produced < output.sizeInBytes())
    {
        Hmac expand;
        SC_TRY(expand.setType(type));
        SC_TRY(expand.setKey(prk.toBytesSpan()));
        if (previousSize > 0)
            SC_TRY(expand.add(Span<const uint8_t>(previous, previousSize)));
        SC_TRY(expand.add(info));
        SC_TRY(expand.add(Span<const uint8_t>(&counter, 1)));

        MacResult block;
        auto      clearBlock = MakeDeferred([&] { secureClear(block.bytes); });
        SC_TRY(expand.getMac(block));

        size_t bytesToCopy = min(block.size, output.sizeInBytes() - produced);
        memcpy(output.data() + produced, block.bytes, bytesToCopy);
        memcpy(previous, block.bytes, block.size);
        previousSize = block.size;
        produced += bytesToCopy;
        counter += 1;
    }
    clearOutputOnFailure.disarm();
    return Result(true);
}

template <>
void SC::Cryptography::Aead::InternalOpaque::construct(Handle& buffer)
{
    placementNew(buffer.reinterpret_as<Object>());
}

template <>
void SC::Cryptography::Aead::InternalOpaque::destruct(Object& obj)
{
    obj.~Object();
}

template <>
void SC::Cryptography::Cipher::InternalOpaque::construct(Handle& buffer)
{
    placementNew(buffer.reinterpret_as<Object>());
}

template <>
void SC::Cryptography::Cipher::InternalOpaque::destruct(Object& obj)
{
    obj.~Object();
}

template <>
void SC::Cryptography::Hmac::InternalOpaque::construct(Handle& buffer)
{
    placementNew(buffer.reinterpret_as<Object>());
}

template <>
void SC::Cryptography::Hmac::InternalOpaque::destruct(Object& obj)
{
    obj.~Object();
}

SC::Cryptography::Aead::Aead()  = default;
SC::Cryptography::Aead::~Aead() = default;

SC::Cryptography::Cipher::Cipher()  = default;
SC::Cryptography::Cipher::~Cipher() = default;

SC::Cryptography::Hmac::Hmac()  = default;
SC::Cryptography::Hmac::~Hmac() = default;
