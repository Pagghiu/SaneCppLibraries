// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "HttpWebSocket.h"
#include "HttpAsyncClient.h"
#include "HttpConnection.h"
#include "Internal/HttpStringIterator.h"

#if SC_PLATFORM_APPLE
#include <CommonCrypto/CommonDigest.h>
#elif SC_PLATFORM_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <bcrypt.h>
#elif SC_PLATFORM_LINUX
#include <dlfcn.h>
#if defined(__has_include)
#if __has_include(<linux/if_alg.h>)
#include <linux/if_alg.h>
using SCHttpWebSocketSockAddrAlg = sockaddr_alg;
#else
#include <stdint.h>
#ifndef AF_ALG
#define AF_ALG 38
#endif
struct SCHttpWebSocketSockAddrAlg
{
    uint16_t salg_family;
    char     salg_type[14];
    uint32_t salg_feat;
    uint32_t salg_mask;
    char     salg_name[64];
};
#endif
#else
#include <linux/if_alg.h>
using SCHttpWebSocketSockAddrAlg = sockaddr_alg;
#endif
#include <sys/socket.h>
#include <unistd.h>
#endif

#include <string.h>

namespace
{
using size_t   = SC::size_t;
using uint8_t  = SC::uint8_t;
using uint32_t = SC::uint32_t;
using uint64_t = SC::uint64_t;

static constexpr const char SC_HTTP_WEBSOCKET_GUID[] = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
static constexpr const char SC_HTTP_WEBSOCKET_BASE64[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static bool scHttpWebSocketIsSupportedOpcode(SC::HttpWebSocketOpcode opcode)
{
    switch (opcode)
    {
    case SC::HttpWebSocketOpcode::Continuation:
    case SC::HttpWebSocketOpcode::Text:
    case SC::HttpWebSocketOpcode::Binary:
    case SC::HttpWebSocketOpcode::Close:
    case SC::HttpWebSocketOpcode::Ping:
    case SC::HttpWebSocketOpcode::Pong: return true;
    }
    return false;
}

static bool scHttpWebSocketRequiresIncomingMask(SC::HttpWebSocketEndpointRole endpointRole)
{
    return endpointRole == SC::HttpWebSocketEndpointRole::Server;
}

static bool scHttpWebSocketRequiresOutgoingMask(SC::HttpWebSocketEndpointRole endpointRole)
{
    return endpointRole == SC::HttpWebSocketEndpointRole::Client;
}

static void scHttpWebSocketApplyMask(SC::Span<char> payload, const uint8_t maskKey[4], uint64_t payloadOffset)
{
    for (size_t idx = 0; idx < payload.sizeInBytes(); ++idx)
    {
        payload[idx] = static_cast<char>(static_cast<uint8_t>(payload[idx]) ^ maskKey[(payloadOffset + idx) & 3]);
    }
}

static bool scHttpWebSocketEqualsIgnoreCase(SC::StringSpan lhs, SC::StringSpan rhs)
{
    return SC::HttpStringIterator::equalsIgnoreCase(lhs, rhs);
}

static bool scHttpWebSocketIsSpace(char value) { return value == ' ' or value == '\t'; }

static SC::StringSpan scHttpWebSocketTrim(SC::StringSpan value)
{
    const char* start = value.bytesWithoutTerminator();
    const char* end   = start + value.sizeInBytes();
    while (start < end and scHttpWebSocketIsSpace(*start))
    {
        start++;
    }
    while (end > start and scHttpWebSocketIsSpace(*(end - 1)))
    {
        end--;
    }
    return {{start, static_cast<size_t>(end - start)}, false, value.getEncoding()};
}

static SC::Result scHttpWebSocketBase64Encode(SC::Span<const uint8_t> data, SC::Span<char> storage,
                                              SC::StringSpan& output)
{
    const size_t encodedLength = ((data.sizeInBytes() + 2) / 3) * 4;
    SC_TRY_MSG(storage.sizeInBytes() >= encodedLength, "HttpWebSocketHandshake base64 output too small");

    size_t inputOffset  = 0;
    size_t outputOffset = 0;
    while (inputOffset < data.sizeInBytes())
    {
        const uint32_t a = data[inputOffset++];
        const uint32_t b = inputOffset < data.sizeInBytes() ? data[inputOffset++] : 0;
        const uint32_t c = inputOffset < data.sizeInBytes() ? data[inputOffset++] : 0;
        const uint32_t n = (a << 16) | (b << 8) | c;

        storage[outputOffset++] = SC_HTTP_WEBSOCKET_BASE64[(n >> 18) & 0x3F];
        storage[outputOffset++] = SC_HTTP_WEBSOCKET_BASE64[(n >> 12) & 0x3F];
        storage[outputOffset++] =
            inputOffset - 1 <= data.sizeInBytes() ? SC_HTTP_WEBSOCKET_BASE64[(n >> 6) & 0x3F] : '=';
        storage[outputOffset++] = inputOffset <= data.sizeInBytes() ? SC_HTTP_WEBSOCKET_BASE64[n & 0x3F] : '=';
    }

    const size_t remainder = data.sizeInBytes() % 3;
    if (remainder == 1)
    {
        storage[encodedLength - 2] = '=';
        storage[encodedLength - 1] = '=';
    }
    else if (remainder == 2)
    {
        storage[encodedLength - 1] = '=';
    }

    output = {{storage.data(), encodedLength}, false, SC::StringEncoding::Ascii};
    return SC::Result(true);
}

static int scHttpWebSocketBase64Value(char value)
{
    if (value >= 'A' and value <= 'Z')
    {
        return value - 'A';
    }
    if (value >= 'a' and value <= 'z')
    {
        return 26 + value - 'a';
    }
    if (value >= '0' and value <= '9')
    {
        return 52 + value - '0';
    }
    if (value == '+')
    {
        return 62;
    }
    if (value == '/')
    {
        return 63;
    }
    return -1;
}

static SC::Result scHttpWebSocketBase64Decode(SC::StringSpan value, SC::Span<uint8_t> output, size_t& decodedBytes)
{
    decodedBytes = 0;
    SC_TRY_MSG((value.sizeInBytes() % 4) == 0, "HttpWebSocketHandshake malformed base64 length");

    for (size_t offset = 0; offset < value.sizeInBytes(); offset += 4)
    {
        int sextets[4] = {0, 0, 0, 0};
        int padding    = 0;
        for (size_t idx = 0; idx < 4; ++idx)
        {
            const char current = value.bytesWithoutTerminator()[offset + idx];
            if (current == '=')
            {
                SC_TRY_MSG(offset + idx >= value.sizeInBytes() - 2, "HttpWebSocketHandshake malformed base64 padding");
                sextets[idx] = 0;
                padding++;
            }
            else
            {
                SC_TRY_MSG(padding == 0, "HttpWebSocketHandshake malformed base64 padding");
                sextets[idx] = scHttpWebSocketBase64Value(current);
                SC_TRY_MSG(sextets[idx] >= 0, "HttpWebSocketHandshake malformed base64 character");
            }
        }

        const uint32_t combined = (static_cast<uint32_t>(sextets[0]) << 18) |
                                  (static_cast<uint32_t>(sextets[1]) << 12) | (static_cast<uint32_t>(sextets[2]) << 6) |
                                  static_cast<uint32_t>(sextets[3]);
        const size_t bytesThisBlock = 3 - static_cast<size_t>(padding);
        SC_TRY_MSG(decodedBytes + bytesThisBlock <= output.sizeInBytes(),
                   "HttpWebSocketHandshake base64 decode output too small");
        if (bytesThisBlock >= 1)
        {
            output[decodedBytes++] = static_cast<uint8_t>((combined >> 16) & 0xFF);
        }
        if (bytesThisBlock >= 2)
        {
            output[decodedBytes++] = static_cast<uint8_t>((combined >> 8) & 0xFF);
        }
        if (bytesThisBlock >= 3)
        {
            output[decodedBytes++] = static_cast<uint8_t>(combined & 0xFF);
        }
    }
    return SC::Result(true);
}

static bool scHttpWebSocketForcePlatformSha1Unavailable  = false;
static bool scHttpWebSocketForceLibCryptoSha1Unavailable = false;

static uint32_t scHttpWebSocketRotateLeft(uint32_t value, uint32_t bits)
{
    return (value << bits) | (value >> (32 - bits));
}

struct HttpWebSocketSelfContainedSha1
{
    uint32_t state[5]   = {0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0};
    uint64_t totalBytes = 0;

    uint8_t block[64] = {0};
    size_t  blockSize = 0;

    void transform(const uint8_t* input)
    {
        uint32_t words[80];
        for (size_t idx = 0; idx < 16; ++idx)
        {
            const size_t offset = idx * 4;
            words[idx]          = (static_cast<uint32_t>(input[offset]) << 24) |
                         (static_cast<uint32_t>(input[offset + 1]) << 16) |
                         (static_cast<uint32_t>(input[offset + 2]) << 8) | static_cast<uint32_t>(input[offset + 3]);
        }
        for (size_t idx = 16; idx < 80; ++idx)
        {
            words[idx] =
                scHttpWebSocketRotateLeft(words[idx - 3] ^ words[idx - 8] ^ words[idx - 14] ^ words[idx - 16], 1);
        }

        uint32_t a = state[0];
        uint32_t b = state[1];
        uint32_t c = state[2];
        uint32_t d = state[3];
        uint32_t e = state[4];
        for (size_t idx = 0; idx < 80; ++idx)
        {
            uint32_t function;
            uint32_t constant;
            if (idx < 20)
            {
                function = (b & c) | ((~b) & d);
                constant = 0x5A827999;
            }
            else if (idx < 40)
            {
                function = b ^ c ^ d;
                constant = 0x6ED9EBA1;
            }
            else if (idx < 60)
            {
                function = (b & c) | (b & d) | (c & d);
                constant = 0x8F1BBCDC;
            }
            else
            {
                function = b ^ c ^ d;
                constant = 0xCA62C1D6;
            }
            const uint32_t temporary = scHttpWebSocketRotateLeft(a, 5) + function + e + constant + words[idx];
            e                        = d;
            d                        = c;
            c                        = scHttpWebSocketRotateLeft(b, 30);
            b                        = a;
            a                        = temporary;
        }
        state[0] += a;
        state[1] += b;
        state[2] += c;
        state[3] += d;
        state[4] += e;
    }

    void add(SC::Span<const uint8_t> data)
    {
        totalBytes += data.sizeInBytes();
        size_t offset = 0;
        while (offset < data.sizeInBytes())
        {
            const size_t available = sizeof(block) - blockSize;
            const size_t remaining = data.sizeInBytes() - offset;
            const size_t toCopy    = remaining < available ? remaining : available;
            ::memcpy(block + blockSize, data.data() + offset, toCopy);
            blockSize += toCopy;
            offset += toCopy;
            if (blockSize == sizeof(block))
            {
                transform(block);
                blockSize = 0;
            }
        }
    }

    void finish(uint8_t digest[20])
    {
        const uint64_t bitLength    = totalBytes * 8;
        uint8_t        padding[128] = {0x80};
        const size_t   paddingSize  = blockSize < 56 ? 56 - blockSize : 64 + 56 - blockSize;
        add({padding, paddingSize});

        uint8_t encodedLength[8];
        for (size_t idx = 0; idx < sizeof(encodedLength); ++idx)
        {
            encodedLength[sizeof(encodedLength) - idx - 1] = static_cast<uint8_t>(bitLength >> (idx * 8));
        }
        add(encodedLength);

        for (size_t idx = 0; idx < 5; ++idx)
        {
            digest[idx * 4]     = static_cast<uint8_t>(state[idx] >> 24);
            digest[idx * 4 + 1] = static_cast<uint8_t>(state[idx] >> 16);
            digest[idx * 4 + 2] = static_cast<uint8_t>(state[idx] >> 8);
            digest[idx * 4 + 3] = static_cast<uint8_t>(state[idx]);
        }
    }
};

static void scHttpWebSocketSelfContainedSha1(SC::Span<const uint8_t> data, uint8_t digest[20], size_t chunkSize)
{
    HttpWebSocketSelfContainedSha1 sha1;
    if (chunkSize == 0)
    {
        chunkSize = data.sizeInBytes();
    }
    size_t offset = 0;
    while (offset < data.sizeInBytes())
    {
        const size_t remaining = data.sizeInBytes() - offset;
        const size_t current   = remaining < chunkSize ? remaining : chunkSize;
        sha1.add({data.data() + offset, current});
        offset += current;
    }
    sha1.finish(digest);
}

#if SC_PLATFORM_APPLE
#if SC_COMPILER_CLANG
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#endif
static bool scHttpWebSocketPlatformSha1(SC::Span<const uint8_t> data, uint8_t digest[20])
{
    if (scHttpWebSocketForcePlatformSha1Unavailable)
    {
        return false;
    }
    const CC_LONG dataSize = static_cast<CC_LONG>(data.sizeInBytes());
    return static_cast<size_t>(dataSize) == data.sizeInBytes() and CC_SHA1(data.data(), dataSize, digest) != nullptr;
}
#if SC_COMPILER_CLANG
#pragma clang diagnostic pop
#endif
#elif SC_PLATFORM_WINDOWS
using HttpWebSocketBCryptOpenAlgorithmProvider = NTSTATUS(WINAPI*)(BCRYPT_ALG_HANDLE*, LPCWSTR, LPCWSTR, ULONG);
using HttpWebSocketBCryptHash = NTSTATUS(WINAPI*)(BCRYPT_ALG_HANDLE, PUCHAR, ULONG, PUCHAR, ULONG, PUCHAR, ULONG);
using HttpWebSocketBCryptCloseAlgorithmProvider = NTSTATUS(WINAPI*)(BCRYPT_ALG_HANDLE, ULONG);

static bool scHttpWebSocketPlatformSha1(SC::Span<const uint8_t> data, uint8_t digest[20])
{
    if (scHttpWebSocketForcePlatformSha1Unavailable)
    {
        return false;
    }
    const ULONG dataSize = static_cast<ULONG>(data.sizeInBytes());
    if (static_cast<size_t>(dataSize) != data.sizeInBytes())
    {
        return false;
    }

    HMODULE library = LoadLibraryW(L"bcrypt.dll");
    if (library == nullptr)
    {
        return false;
    }
    const auto openAlgorithm = reinterpret_cast<HttpWebSocketBCryptOpenAlgorithmProvider>(
        GetProcAddress(library, "BCryptOpenAlgorithmProvider"));
    const auto hash           = reinterpret_cast<HttpWebSocketBCryptHash>(GetProcAddress(library, "BCryptHash"));
    const auto closeAlgorithm = reinterpret_cast<HttpWebSocketBCryptCloseAlgorithmProvider>(
        GetProcAddress(library, "BCryptCloseAlgorithmProvider"));

    bool success = false;
    if (openAlgorithm != nullptr and hash != nullptr and closeAlgorithm != nullptr)
    {
        BCRYPT_ALG_HANDLE algorithm = nullptr;
        if (openAlgorithm(&algorithm, BCRYPT_SHA1_ALGORITHM, nullptr, 0) >= 0)
        {
            success = hash(algorithm, nullptr, 0, const_cast<PUCHAR>(data.data()), dataSize, digest, 20) >= 0;
            (void)closeAlgorithm(algorithm, 0);
        }
    }
    FreeLibrary(library);
    return success;
}
#elif SC_PLATFORM_LINUX
using HttpWebSocketEVPSha1Function   = const void* (*)();
using HttpWebSocketEVPDigestFunction = int (*)(const void*, size_t, unsigned char*, unsigned int*, const void*, void*);

template <typename Function>
static Function scHttpWebSocketLoadFunction(void* library, const char* name)
{
    Function function = nullptr;
    void*    symbol   = ::dlsym(library, name);
    static_assert(sizeof(function) == sizeof(symbol), "Function pointer size must match data pointer size");
    ::memcpy(&function, &symbol, sizeof(function));
    return function;
}

static bool scHttpWebSocketLibCryptoSha1(SC::Span<const uint8_t> data, uint8_t digest[20])
{
    if (scHttpWebSocketForceLibCryptoSha1Unavailable)
    {
        return false;
    }
    static constexpr const char* LibraryNames[] = {"libcrypto.so.3", "libcrypto.so.1.1"};
    for (const char* libraryName : LibraryNames)
    {
        void* library = ::dlopen(libraryName, RTLD_LAZY | RTLD_LOCAL);
        if (library == nullptr)
        {
            continue;
        }
        const auto evpSha1   = scHttpWebSocketLoadFunction<HttpWebSocketEVPSha1Function>(library, "EVP_sha1");
        const auto evpDigest = scHttpWebSocketLoadFunction<HttpWebSocketEVPDigestFunction>(library, "EVP_Digest");

        bool success = false;
        if (evpSha1 != nullptr and evpDigest != nullptr)
        {
            unsigned int digestSize = 0;
            const void*  digestType = evpSha1();
            success                 = digestType != nullptr and
                      evpDigest(data.data(), data.sizeInBytes(), digest, &digestSize, digestType, nullptr) == 1 and
                      digestSize == 20;
        }
        ::dlclose(library);
        if (success)
        {
            return true;
        }
    }
    return false;
}

static bool scHttpWebSocketAFAlgSha1(SC::Span<const uint8_t> data, uint8_t digest[20])
{
    SCHttpWebSocketSockAddrAlg sa = {};
    sa.salg_family                = AF_ALG;
    ::memcpy(sa.salg_type, "hash", sizeof("hash"));
    ::memcpy(sa.salg_name, "sha1", sizeof("sha1"));

    int mainSocket = ::socket(AF_ALG, SOCK_SEQPACKET, 0);
    if (mainSocket == -1)
    {
        return false;
    }
    if (::bind(mainSocket, reinterpret_cast<struct sockaddr*>(&sa), sizeof(sa)) == -1)
    {
        ::close(mainSocket);
        return false;
    }
    const int hashSocket = ::accept(mainSocket, nullptr, nullptr);
    if (hashSocket == -1)
    {
        ::close(mainSocket);
        return false;
    }

    const ssize_t written = ::send(hashSocket, data.data(), data.sizeInBytes(), 0);
    const ssize_t readBytes =
        written == static_cast<ssize_t>(data.sizeInBytes()) ? ::recv(hashSocket, digest, 20, 0) : -1;
    ::close(hashSocket);
    ::close(mainSocket);
    return readBytes == 20;
}

static bool scHttpWebSocketPlatformSha1(SC::Span<const uint8_t> data, uint8_t digest[20])
{
    if (scHttpWebSocketForcePlatformSha1Unavailable)
    {
        return false;
    }
    return scHttpWebSocketLibCryptoSha1(data, digest) or scHttpWebSocketAFAlgSha1(data, digest);
}
#else
static bool scHttpWebSocketPlatformSha1(SC::Span<const uint8_t>, uint8_t[20]) { return false; }
#endif

static SC::Result scHttpWebSocketSha1(SC::HttpWebSocketSha1Mode mode, SC::Span<const uint8_t> data, uint8_t digest[20],
                                      size_t selfContainedChunkSize = 0)
{
    switch (mode)
    {
    case SC::HttpWebSocketSha1Mode::Platform:
        SC_TRY_MSG(scHttpWebSocketPlatformSha1(data, digest),
                   "HttpWebSocketHandshake platform SHA1 provider unavailable");
        return SC::Result(true);
    case SC::HttpWebSocketSha1Mode::SelfContained:
        scHttpWebSocketSelfContainedSha1(data, digest, selfContainedChunkSize);
        return SC::Result(true);
    }
    return SC::Result::Error("HttpWebSocketHandshake SHA1 mode invalid");
}

static void scHttpWebSocketSelfContainedSha1RepeatedByte(uint8_t value, size_t count, uint8_t digest[20])
{
    HttpWebSocketSelfContainedSha1 sha1;
    uint8_t                        input[1000];
    ::memset(input, value, sizeof(input));
    while (count > 0)
    {
        const size_t current = count < sizeof(input) ? count : sizeof(input);
        sha1.add({input, current});
        count -= current;
    }
    sha1.finish(digest);
}
} // namespace

namespace SC
{
void httpWebSocketTestForceSha1ProvidersUnavailable(bool platformUnavailable, bool libCryptoUnavailable)
{
    scHttpWebSocketForcePlatformSha1Unavailable  = platformUnavailable;
    scHttpWebSocketForceLibCryptoSha1Unavailable = libCryptoUnavailable;
}

Result httpWebSocketTestSha1(HttpWebSocketSha1Mode mode, Span<const uint8_t> data, Span<uint8_t> digest,
                             size_t selfContainedChunkSize)
{
    SC_TRY_MSG(digest.sizeInBytes() == 20, "HttpWebSocketHandshakeTest SHA1 digest size invalid");
    return scHttpWebSocketSha1(mode, data, digest.data(), selfContainedChunkSize);
}

Result httpWebSocketTestSelfContainedSha1RepeatedByte(uint8_t value, size_t count, Span<uint8_t> digest)
{
    SC_TRY_MSG(digest.sizeInBytes() == 20, "HttpWebSocketHandshakeTest SHA1 digest size invalid");
    scHttpWebSocketSelfContainedSha1RepeatedByte(value, count, digest.data());
    return Result(true);
}

int HttpWebSocketHandshakeResult::httpStatusCode() const
{
    switch (status)
    {
    case Status::Accepted: return 101;
    case Status::BadRequest: return 400;
    case Status::UnsupportedVersion: return 426;
    }
    return 400;
}

bool HttpWebSocketFrameHeaderView::isControlFrame() const
{
    return static_cast<uint8_t>(opcode) >= static_cast<uint8_t>(HttpWebSocketOpcode::Close);
}

void HttpWebSocketTransportView::reset()
{
    readableStream = nullptr;
    writableStream = nullptr;
    buffersPool    = nullptr;
}

Result HttpWebSocketHandshake::createClientKey(Span<const uint8_t> nonce, Span<char> storage, StringSpan& key)
{
    SC_TRY_MSG(nonce.sizeInBytes() == NonceLength, "HttpWebSocketHandshake nonce must be 16 bytes");
    return scHttpWebSocketBase64Encode(nonce, storage, key);
}

Result HttpWebSocketHandshake::validateClientKey(StringSpan key)
{
    SC_TRY_MSG(key.sizeInBytes() == ClientKeyLength, "HttpWebSocketHandshake Sec-WebSocket-Key length invalid");

    uint8_t decoded[NonceLength] = {0};
    size_t  decodedBytes         = 0;
    SC_TRY(scHttpWebSocketBase64Decode(key, decoded, decodedBytes));
    SC_TRY_MSG(decodedBytes == NonceLength, "HttpWebSocketHandshake Sec-WebSocket-Key decoded length invalid");
    return Result(true);
}

Result HttpWebSocketHandshake::computeAccept(StringSpan clientKey, Span<char> storage, StringSpan& accept,
                                             HttpWebSocketSha1Mode sha1Mode)
{
    SC_TRY(validateClientKey(clientKey));

    uint8_t input[ClientKeyLength + sizeof(SC_HTTP_WEBSOCKET_GUID) - 1];
    ::memcpy(input, clientKey.bytesWithoutTerminator(), clientKey.sizeInBytes());
    ::memcpy(input + clientKey.sizeInBytes(), SC_HTTP_WEBSOCKET_GUID, sizeof(SC_HTTP_WEBSOCKET_GUID) - 1);

    uint8_t digest[20] = {0};
    SC_TRY(scHttpWebSocketSha1(sha1Mode, input, digest));
    return scHttpWebSocketBase64Encode({digest, sizeof(digest)}, storage, accept);
}

bool HttpWebSocketHandshake::headerContainsToken(StringSpan headerValue, StringSpan token)
{
    const char* start      = headerValue.bytesWithoutTerminator();
    const char* end        = start + headerValue.sizeInBytes();
    const char* tokenStart = start;
    for (const char* it = start; it <= end; ++it)
    {
        if (it == end or *it == ',')
        {
            StringSpan part = scHttpWebSocketTrim(
                {{tokenStart, static_cast<size_t>(it - tokenStart)}, false, headerValue.getEncoding()});
            if (scHttpWebSocketEqualsIgnoreCase(part, token))
            {
                return true;
            }
            tokenStart = it + 1;
        }
    }
    return false;
}

HttpWebSocketHandshakeResult HttpWebSocketHandshake::validateServerRequest(
    const HttpWebSocketServerHandshakeRequestView& request)
{
    HttpWebSocketHandshakeResult result;

    if (request.method != HttpParser::Method::HttpGET or
        not scHttpWebSocketEqualsIgnoreCase(request.version, "HTTP/1.1"))
    {
        result.status = HttpWebSocketHandshakeResult::Status::BadRequest;
        return result;
    }
    if (not scHttpWebSocketEqualsIgnoreCase(scHttpWebSocketTrim(request.upgrade), "websocket"))
    {
        result.status = HttpWebSocketHandshakeResult::Status::BadRequest;
        return result;
    }
    if (not headerContainsToken(request.connection, "Upgrade"))
    {
        result.status = HttpWebSocketHandshakeResult::Status::BadRequest;
        return result;
    }
    if (not validateClientKey(scHttpWebSocketTrim(request.secWebSocketKey)))
    {
        result.status = HttpWebSocketHandshakeResult::Status::BadRequest;
        return result;
    }
    if (request.secWebSocketVersion.isEmpty())
    {
        result.status = HttpWebSocketHandshakeResult::Status::BadRequest;
        return result;
    }
    if (not scHttpWebSocketEqualsIgnoreCase(scHttpWebSocketTrim(request.secWebSocketVersion), "13"))
    {
        result.status = HttpWebSocketHandshakeResult::Status::UnsupportedVersion;
        return result;
    }

    result.status = HttpWebSocketHandshakeResult::Status::Accepted;
    return result;
}

HttpWebSocketHandshakeResult HttpWebSocketHandshake::validateServerRequest(
    const HttpRequest& request, HttpWebSocketServerHandshakeRequestView* view)
{
    HttpWebSocketServerHandshakeRequestView local;
    local.method  = request.getParser().method;
    local.version = request.getVersion();
    (void)request.getHeader("Upgrade", local.upgrade);
    (void)request.getHeader("Connection", local.connection);
    (void)request.getHeader("Sec-WebSocket-Key", local.secWebSocketKey);
    (void)request.getHeader("Sec-WebSocket-Version", local.secWebSocketVersion);

    if (view != nullptr)
    {
        *view = local;
    }
    return validateServerRequest(local);
}

Result HttpWebSocketHandshake::validateClientResponse(const HttpWebSocketClientHandshakeResponseView& response,
                                                      StringSpan expectedClientKey, HttpWebSocketSha1Mode sha1Mode)
{
    SC_TRY_MSG(response.statusCode == 101, "HttpWebSocketHandshake expected 101 Switching Protocols");
    SC_TRY_MSG(scHttpWebSocketEqualsIgnoreCase(scHttpWebSocketTrim(response.upgrade), "websocket"),
               "HttpWebSocketHandshake response Upgrade header invalid");
    SC_TRY_MSG(headerContainsToken(response.connection, "Upgrade"),
               "HttpWebSocketHandshake response Connection header invalid");

    char       acceptStorage[AcceptKeyLength] = {0};
    StringSpan expectedAccept;
    SC_TRY(computeAccept(expectedClientKey, acceptStorage, expectedAccept, sha1Mode));
    SC_TRY_MSG(scHttpWebSocketEqualsIgnoreCase(scHttpWebSocketTrim(response.secWebSocketAccept), expectedAccept),
               "HttpWebSocketHandshake response Sec-WebSocket-Accept invalid");
    return Result(true);
}

Result HttpWebSocketHandshake::validateClientResponse(const HttpAsyncClientResponse& response,
                                                      StringSpan expectedClientKey, HttpWebSocketSha1Mode sha1Mode)
{
    HttpWebSocketClientHandshakeResponseView view;
    view.statusCode = response.getParser().statusCode;
    (void)response.getHeader("Upgrade", view.upgrade);
    (void)response.getHeader("Connection", view.connection);
    (void)response.getHeader("Sec-WebSocket-Accept", view.secWebSocketAccept);
    return validateClientResponse(view, expectedClientKey, sha1Mode);
}

Result HttpWebSocketHandshake::prepareClientRequest(HttpAsyncClientRequest& request, StringSpan clientKey)
{
    SC_TRY(validateClientKey(clientKey));
    SC_TRY(request.addHeader("Upgrade", "websocket"));
    SC_TRY(request.addHeader("Connection", "Upgrade"));
    SC_TRY(request.addHeader("Sec-WebSocket-Key", clientKey));
    SC_TRY(request.addHeader("Sec-WebSocket-Version", "13"));
    return request.sendHeaders();
}

Result HttpWebSocketHandshake::writeServerAccept(HttpResponse& response, StringSpan clientKey, Span<char> acceptStorage,
                                                 StringSpan& accept, HttpWebSocketSha1Mode sha1Mode)
{
    SC_TRY(computeAccept(clientKey, acceptStorage, accept, sha1Mode));
    SC_TRY(response.startResponse(101));
    SC_TRY(response.addHeader("Upgrade", "websocket"));
    SC_TRY(response.addHeader("Connection", "Upgrade"));
    SC_TRY(response.addHeader("Sec-WebSocket-Accept", accept));
    return response.sendHeaders();
}

Result HttpWebSocketHandshake::acceptServerConnection(HttpConnection& connection, HttpWebSocketTransportView& transport,
                                                      Span<char> acceptStorage, HttpWebSocketSha1Mode sha1Mode)
{
    HttpWebSocketServerHandshakeRequestView request;
    const HttpWebSocketHandshakeResult      validation = validateServerRequest(connection.request, &request);
    SC_TRY_MSG(validation.accepted(), "HttpWebSocketHandshake server request is not acceptable");

    StringSpan accept;
    SC_TRY(computeAccept(request.secWebSocketKey, acceptStorage, accept, sha1Mode));
    SC_TRY(connection.response.startResponse(101));
    SC_TRY(connection.response.addHeader("Upgrade", "websocket"));
    SC_TRY(connection.response.addHeader("Connection", "Upgrade"));
    SC_TRY(connection.response.addHeader("Sec-WebSocket-Accept", accept));

    transport.readableStream = &connection.getReadableTransportStream();
    transport.writableStream = &connection.getWritableTransportStream();
    transport.buffersPool    = &connection.buffersPool;

    SC_TRY(connection.response.sendHeaders(
        {[&connection](AsyncBufferView::ID) { connection.getReadableTransportStream().resumeReading(); }}));
    connection.markWebSocketUpgraded();
    return Result(true);
}

Result HttpWebSocketHandshake::rejectServerConnection(HttpResponse&                       response,
                                                      const HttpWebSocketHandshakeResult& result)
{
    const int statusCode = result.httpStatusCode();
    SC_TRY_MSG(statusCode != 101, "HttpWebSocketHandshake cannot reject an accepted request");
    SC_TRY(response.startResponse(statusCode));
    if (statusCode == 426)
    {
        SC_TRY(response.addHeader("Sec-WebSocket-Version", "13"));
    }
    SC_TRY(response.addContentLength(0));
    SC_TRY(response.addHeader("Connection", "close"));
    SC_TRY(response.sendHeaders());
    return response.end();
}

Result HttpWebSocketClientHandshake::connect(HttpAsyncClient& newClient, AsyncEventLoop& loop, StringSpan url,
                                             StringSpan newClientKey, HttpWebSocketTransportView& newTransport,
                                             HttpWebSocketSha1Mode newSha1Mode)
{
    SC_TRY(HttpWebSocketHandshake::validateClientKey(newClientKey));
    client    = &newClient;
    transport = &newTransport;
    clientKey = newClientKey;
    sha1Mode  = newSha1Mode;
    transport->reset();

    client->onPrepareRequest.bind<HttpWebSocketClientHandshake, &HttpWebSocketClientHandshake::onPrepareRequest>(*this);
    client->onResponse.bind<HttpWebSocketClientHandshake, &HttpWebSocketClientHandshake::onResponse>(*this);
    client->onError.bind<HttpWebSocketClientHandshake, &HttpWebSocketClientHandshake::onClientError>(*this);
    return client->start(loop, HttpParser::Method::HttpGET, url, false);
}

void HttpWebSocketClientHandshake::onPrepareRequest(HttpAsyncClientRequest& request)
{
    const Result result = HttpWebSocketHandshake::prepareClientRequest(request, clientKey);
    if (not result)
    {
        fail(result);
    }
}

void HttpWebSocketClientHandshake::onResponse(HttpAsyncClientResponse& response)
{
    SC_HTTP_ASSERT_RELEASE(client != nullptr);
    SC_HTTP_ASSERT_RELEASE(transport != nullptr);

    Result result = HttpWebSocketHandshake::validateClientResponse(response, clientKey, sha1Mode);
    if (result)
    {
        result = client->detachWebSocketTransport(*transport);
    }
    if (not result)
    {
        fail(result);
        return;
    }
    if (onConnected.isValid())
    {
        onConnected(*transport);
    }
}

void HttpWebSocketClientHandshake::onClientError(Result result) { fail(result); }

void HttpWebSocketClientHandshake::fail(Result result)
{
    if (onError.isValid())
    {
        onError(result);
    }
}

void HttpWebSocketFrameReader::reset(HttpWebSocketEndpointRole role)
{
    endpointRole = role;
    currentFrame = {};

    state = State::HeaderByte0;

    fragmentedMessageInProgress = false;

    headerByte0 = 0;

    extendedLengthBytesExpected = 0;
    extendedLengthBytesRead     = 0;
    maskBytesRead               = 0;

    extendedLengthAccumulator = 0;
    payloadBytesRemaining     = 0;
    payloadBytesConsumed      = 0;
}

Result HttpWebSocketFrameReader::parse(Span<char> data, size_t& consumedBytes)
{
    consumedBytes = 0;
    while (consumedBytes < data.sizeInBytes())
    {
        const uint8_t current = static_cast<uint8_t>(data[consumedBytes]);
        switch (state)
        {
        case State::HeaderByte0:
            headerByte0 = current;
            consumedBytes++;
            state = State::HeaderByte1;
            break;

        case State::HeaderByte1: {
            const uint8_t opcodeValue = headerByte0 & 0x0F;
            SC_TRY_MSG((headerByte0 & 0x70) == 0, "HttpWebSocketFrameReader RSV bits are not supported");
            currentFrame        = {};
            currentFrame.fin    = (headerByte0 & 0x80) != 0;
            currentFrame.opcode = static_cast<HttpWebSocketOpcode>(opcodeValue);
            SC_TRY_MSG(scHttpWebSocketIsSupportedOpcode(currentFrame.opcode),
                       "HttpWebSocketFrameReader unsupported opcode");

            currentFrame.masked              = (current & 0x80) != 0;
            const uint8_t payloadLengthField = current & 0x7F;

            extendedLengthAccumulator = 0;
            extendedLengthBytesRead   = 0;
            maskBytesRead             = 0;

            consumedBytes++;

            if (payloadLengthField < 126)
            {
                currentFrame.payloadLength = payloadLengthField;
                if (currentFrame.masked)
                {
                    state = State::MaskKey;
                }
                else
                {
                    SC_TRY(onHeaderReady());
                }
            }
            else
            {
                extendedLengthBytesExpected = payloadLengthField == 126 ? 2 : 8;
                state                       = State::ExtendedLength;
            }
            break;
        }

        case State::ExtendedLength:
            SC_TRY_MSG(not(extendedLengthBytesExpected == 8 and extendedLengthBytesRead == 0 and (current & 0x80) != 0),
                       "HttpWebSocketFrameReader invalid 64-bit payload length");
            extendedLengthAccumulator = (extendedLengthAccumulator << 8) | current;
            extendedLengthBytesRead++;
            consumedBytes++;
            if (extendedLengthBytesRead == extendedLengthBytesExpected)
            {
                currentFrame.payloadLength = extendedLengthAccumulator;
                if (currentFrame.masked)
                {
                    state = State::MaskKey;
                }
                else
                {
                    SC_TRY(onHeaderReady());
                }
            }
            break;

        case State::MaskKey:
            currentFrame.maskKey[maskBytesRead++] = current;
            consumedBytes++;
            if (maskBytesRead == 4)
            {
                SC_TRY(onHeaderReady());
            }
            break;

        case State::Payload: {
            const size_t availableBytes = data.sizeInBytes() - consumedBytes;
            const size_t toConsume =
                availableBytes < payloadBytesRemaining ? availableBytes : static_cast<size_t>(payloadBytesRemaining);

            SC_TRY_MSG(toConsume > 0, "HttpWebSocketFrameReader invalid payload progress");
            Span<char> payload = {data.data() + consumedBytes, toConsume};
            if (currentFrame.masked)
            {
                scHttpWebSocketApplyMask(payload, currentFrame.maskKey, payloadBytesConsumed);
            }

            payloadBytesRemaining -= toConsume;
            const bool frameFinished = payloadBytesRemaining == 0;
            if (onFramePayload.isValid())
            {
                SC_TRY(onFramePayload(payload, frameFinished));
            }
            payloadBytesConsumed += toConsume;
            consumedBytes += toConsume;

            if (frameFinished)
            {
                SC_TRY(finishCurrentFrame());
            }
            break;
        }
        }
    }
    return Result(true);
}

Result HttpWebSocketFrameReader::onHeaderReady()
{
    SC_TRY_MSG(currentFrame.masked == scHttpWebSocketRequiresIncomingMask(endpointRole),
               "HttpWebSocketFrameReader invalid frame masking for endpoint role");

    if (currentFrame.isControlFrame())
    {
        SC_TRY_MSG(currentFrame.fin, "HttpWebSocketFrameReader control frames must not be fragmented");
        SC_TRY_MSG(currentFrame.payloadLength <= 125, "HttpWebSocketFrameReader control frame payload too large");
    }
    else
    {
        if (currentFrame.opcode == HttpWebSocketOpcode::Continuation)
        {
            SC_TRY_MSG(fragmentedMessageInProgress, "HttpWebSocketFrameReader unexpected continuation frame");
        }
        else
        {
            SC_TRY_MSG(not fragmentedMessageInProgress, "HttpWebSocketFrameReader expected continuation frame");
        }
    }

    if (onFrameHeader.isValid())
    {
        SC_TRY(onFrameHeader(currentFrame));
    }

    payloadBytesRemaining = currentFrame.payloadLength;
    payloadBytesConsumed  = 0;

    if (payloadBytesRemaining == 0)
    {
        return finishCurrentFrame();
    }

    state = State::Payload;
    return Result(true);
}

Result HttpWebSocketFrameReader::finishCurrentFrame()
{
    if (not currentFrame.isControlFrame())
    {
        if (currentFrame.opcode == HttpWebSocketOpcode::Continuation)
        {
            fragmentedMessageInProgress = not currentFrame.fin;
        }
        else
        {
            fragmentedMessageInProgress = not currentFrame.fin;
        }
    }

    state                       = State::HeaderByte0;
    extendedLengthBytesExpected = 0;
    extendedLengthBytesRead     = 0;
    maskBytesRead               = 0;
    payloadBytesRemaining       = 0;
    payloadBytesConsumed        = 0;
    extendedLengthAccumulator   = 0;
    return Result(true);
}

void HttpWebSocketFrameWriter::reset(HttpWebSocketEndpointRole role)
{
    endpointRole                = role;
    currentFrame                = {};
    frameInProgress             = false;
    fragmentedMessageInProgress = false;
    payloadBytesRemaining       = 0;
    payloadBytesWritten         = 0;
}

Result HttpWebSocketFrameWriter::beginFrame(const HttpWebSocketFrameHeaderView& frame, Span<char> storage,
                                            Span<const char>& encodedHeader)
{
    SC_TRY_MSG(not frameInProgress, "HttpWebSocketFrameWriter frame already in progress");
    SC_TRY_MSG(scHttpWebSocketIsSupportedOpcode(frame.opcode), "HttpWebSocketFrameWriter unsupported opcode");

    if (frame.isControlFrame())
    {
        SC_TRY_MSG(frame.fin, "HttpWebSocketFrameWriter control frames must not be fragmented");
        SC_TRY_MSG(frame.payloadLength <= 125, "HttpWebSocketFrameWriter control frame payload too large");
    }
    else
    {
        if (frame.opcode == HttpWebSocketOpcode::Continuation)
        {
            SC_TRY_MSG(fragmentedMessageInProgress, "HttpWebSocketFrameWriter unexpected continuation frame");
        }
        else
        {
            SC_TRY_MSG(not fragmentedMessageInProgress, "HttpWebSocketFrameWriter expected continuation frame");
        }
    }

    const bool requiresMask = scHttpWebSocketRequiresOutgoingMask(endpointRole);
    SC_TRY_MSG(frame.masked == requiresMask, "HttpWebSocketFrameWriter invalid frame masking for endpoint role");

    const size_t encodedLength =
        2 + (frame.payloadLength < 126 ? 0 : (frame.payloadLength <= 0xFFFF ? 2 : 8)) + (frame.masked ? 4 : 0);
    SC_TRY_MSG(storage.sizeInBytes() >= encodedLength, "HttpWebSocketFrameWriter header buffer too small");

    currentFrame = frame;

    uint8_t* output = reinterpret_cast<uint8_t*>(storage.data());
    output[0]       = static_cast<uint8_t>((frame.fin ? 0x80 : 0) | static_cast<uint8_t>(frame.opcode));

    size_t headerOffset = 1;
    if (frame.payloadLength < 126)
    {
        output[headerOffset++] = static_cast<uint8_t>((frame.masked ? 0x80 : 0) | frame.payloadLength);
    }
    else if (frame.payloadLength <= 0xFFFF)
    {
        output[headerOffset++] = static_cast<uint8_t>((frame.masked ? 0x80 : 0) | 126);
        output[headerOffset++] = static_cast<uint8_t>((frame.payloadLength >> 8) & 0xFF);
        output[headerOffset++] = static_cast<uint8_t>(frame.payloadLength & 0xFF);
    }
    else
    {
        output[headerOffset++] = static_cast<uint8_t>((frame.masked ? 0x80 : 0) | 127);
        for (int shift = 56; shift >= 0; shift -= 8)
        {
            output[headerOffset++] = static_cast<uint8_t>((frame.payloadLength >> shift) & 0xFF);
        }
    }

    if (frame.masked)
    {
        for (size_t idx = 0; idx < 4; ++idx)
        {
            output[headerOffset++] = frame.maskKey[idx];
        }
    }

    encodedHeader         = {storage.data(), headerOffset};
    frameInProgress       = true;
    payloadBytesRemaining = frame.payloadLength;
    payloadBytesWritten   = 0;
    return Result(true);
}

Result HttpWebSocketFrameWriter::writePayload(Span<char> payload)
{
    SC_TRY_MSG(frameInProgress, "HttpWebSocketFrameWriter no frame in progress");
    SC_TRY_MSG(payload.sizeInBytes() <= payloadBytesRemaining,
               "HttpWebSocketFrameWriter payload exceeds declared frame length");

    if (currentFrame.masked and not payload.empty())
    {
        scHttpWebSocketApplyMask(payload, currentFrame.maskKey, payloadBytesWritten);
    }

    payloadBytesWritten += payload.sizeInBytes();
    payloadBytesRemaining -= payload.sizeInBytes();
    return Result(true);
}

Result HttpWebSocketFrameWriter::finishFrame()
{
    SC_TRY_MSG(frameInProgress, "HttpWebSocketFrameWriter no frame in progress");
    SC_TRY_MSG(payloadBytesRemaining == 0, "HttpWebSocketFrameWriter frame payload incomplete");

    if (not currentFrame.isControlFrame())
    {
        fragmentedMessageInProgress = not currentFrame.fin;
    }

    currentFrame        = {};
    frameInProgress     = false;
    payloadBytesWritten = 0;
    return Result(true);
}

void HttpWebSocketMessageAssembler::reset(Span<char> storage)
{
    messageStorage    = storage;
    currentFrame      = {};
    messageOpcode     = HttpWebSocketOpcode::Text;
    messageSize       = 0;
    assemblingMessage = false;
    ignoringFrame     = false;
}

Result HttpWebSocketMessageAssembler::onFrameHeader(const HttpWebSocketFrameHeaderView& header)
{
    currentFrame  = header;
    ignoringFrame = header.isControlFrame();
    if (ignoringFrame)
    {
        return Result(true);
    }

    if (header.opcode == HttpWebSocketOpcode::Continuation)
    {
        SC_TRY_MSG(assemblingMessage, "HttpWebSocketMessageAssembler unexpected continuation");
    }
    else
    {
        SC_TRY_MSG(not assemblingMessage, "HttpWebSocketMessageAssembler new message before final continuation");
        messageOpcode     = header.opcode;
        messageSize       = 0;
        assemblingMessage = true;
    }

    if (header.payloadLength == 0 and header.fin)
    {
        if (onMessage.isValid())
        {
            SC_TRY(onMessage(messageOpcode, {messageStorage.data(), messageSize}));
        }
        assemblingMessage = false;
        messageSize       = 0;
    }
    return Result(true);
}

Result HttpWebSocketMessageAssembler::onFramePayload(Span<char> payload, bool frameFinished)
{
    if (ignoringFrame)
    {
        return Result(true);
    }

    SC_TRY_MSG(assemblingMessage, "HttpWebSocketMessageAssembler payload without message");
    SC_TRY_MSG(messageSize + payload.sizeInBytes() <= messageStorage.sizeInBytes(),
               "HttpWebSocketMessageAssembler message storage too small");
    if (payload.sizeInBytes() > 0)
    {
        ::memcpy(messageStorage.data() + messageSize, payload.data(), payload.sizeInBytes());
        messageSize += payload.sizeInBytes();
    }

    if (frameFinished and currentFrame.fin)
    {
        if (onMessage.isValid())
        {
            SC_TRY(onMessage(messageOpcode, {messageStorage.data(), messageSize}));
        }
        assemblingMessage = false;
        messageSize       = 0;
    }
    return Result(true);
}

void HttpWebSocketEndpoint::reset(HttpWebSocketEndpointRole role)
{
    endpointRole = role;
    reader.reset(role);
    writer.reset(role);
    reader.onFrameHeader.bind<HttpWebSocketEndpoint, &HttpWebSocketEndpoint::onReaderFrameHeader>(*this);
    reader.onFramePayload.bind<HttpWebSocketEndpoint, &HttpWebSocketEndpoint::onReaderPayload>(*this);

    currentFrame        = {};
    controlPayloadSize  = 0;
    pendingControlFrame = {};
    automaticMaskKeySet = false;
    closeSent           = false;
    closeReceived       = false;
}

void HttpWebSocketEndpoint::setAutomaticMaskKey(const uint8_t maskKey[4])
{
    for (size_t idx = 0; idx < 4; ++idx)
    {
        automaticMaskKey[idx] = maskKey[idx];
    }
    automaticMaskKeySet = true;
}

Result HttpWebSocketEndpoint::receive(Span<char> data, size_t& consumedBytes)
{
    return reader.parse(data, consumedBytes);
}

Result HttpWebSocketEndpoint::applyOutgoingMask(HttpWebSocketFrameHeaderView& header, const uint8_t* maskKey) const
{
    const bool requiresMask = scHttpWebSocketRequiresOutgoingMask(endpointRole);
    header.masked           = requiresMask;
    if (requiresMask)
    {
        SC_TRY_MSG(maskKey != nullptr, "HttpWebSocketEndpoint mask key required for client frames");
        for (size_t idx = 0; idx < 4; ++idx)
        {
            header.maskKey[idx] = maskKey[idx];
        }
    }
    else
    {
        for (size_t idx = 0; idx < 4; ++idx)
        {
            header.maskKey[idx] = 0;
        }
    }
    return Result(true);
}

Result HttpWebSocketEndpoint::sendFrame(const HttpWebSocketFrameHeaderView& header, Span<const char> payload,
                                        Span<char> storage, Span<const char>& encodedFrame)
{
    SC_TRY_MSG(header.payloadLength == payload.sizeInBytes(),
               "HttpWebSocketEndpoint payload does not match frame length");

    Span<const char> encodedHeader;
    SC_TRY(writer.beginFrame(header, storage, encodedHeader));
    SC_TRY_MSG(storage.sizeInBytes() >= encodedHeader.sizeInBytes() + payload.sizeInBytes(),
               "HttpWebSocketEndpoint frame storage too small");

    if (payload.sizeInBytes() > 0)
    {
        Span<char> destinationPayload = {storage.data() + encodedHeader.sizeInBytes(), payload.sizeInBytes()};
        ::memcpy(destinationPayload.data(), payload.data(), payload.sizeInBytes());
        SC_TRY(writer.writePayload(destinationPayload));
    }
    SC_TRY(writer.finishFrame());

    encodedFrame = {storage.data(), encodedHeader.sizeInBytes() + payload.sizeInBytes()};
    return Result(true);
}

Result HttpWebSocketEndpoint::sendData(HttpWebSocketOpcode opcode, Span<const char> payload, bool fin,
                                       const uint8_t* maskKey, Span<char> storage, Span<const char>& encodedFrame)
{
    SC_TRY_MSG(opcode == HttpWebSocketOpcode::Text or opcode == HttpWebSocketOpcode::Binary or
                   opcode == HttpWebSocketOpcode::Continuation,
               "HttpWebSocketEndpoint sendData requires a data opcode");
    HttpWebSocketFrameHeaderView header;
    header.opcode        = opcode;
    header.fin           = fin;
    header.payloadLength = payload.sizeInBytes();
    SC_TRY(applyOutgoingMask(header, maskKey));
    return sendFrame(header, payload, storage, encodedFrame);
}

Result HttpWebSocketEndpoint::sendPing(Span<const char> payload, const uint8_t* maskKey, Span<char> storage,
                                       Span<const char>& encodedFrame)
{
    SC_TRY_MSG(payload.sizeInBytes() <= sizeof(controlPayload), "HttpWebSocketEndpoint ping payload too large");
    HttpWebSocketFrameHeaderView header;
    header.opcode        = HttpWebSocketOpcode::Ping;
    header.fin           = true;
    header.payloadLength = payload.sizeInBytes();
    SC_TRY(applyOutgoingMask(header, maskKey));
    return sendFrame(header, payload, storage, encodedFrame);
}

Result HttpWebSocketEndpoint::sendPong(Span<const char> payload, const uint8_t* maskKey, Span<char> storage,
                                       Span<const char>& encodedFrame)
{
    SC_TRY_MSG(payload.sizeInBytes() <= sizeof(controlPayload), "HttpWebSocketEndpoint pong payload too large");
    HttpWebSocketFrameHeaderView header;
    header.opcode        = HttpWebSocketOpcode::Pong;
    header.fin           = true;
    header.payloadLength = payload.sizeInBytes();
    SC_TRY(applyOutgoingMask(header, maskKey));
    return sendFrame(header, payload, storage, encodedFrame);
}

Result HttpWebSocketEndpoint::sendClose(uint16_t statusCode, Span<const char> reason, const uint8_t* maskKey,
                                        Span<char> storage, Span<const char>& encodedFrame)
{
    SC_TRY_MSG(statusCode != 0 or reason.empty(), "HttpWebSocketEndpoint close reason requires a status code");
    SC_TRY_MSG(reason.sizeInBytes() + (statusCode == 0 ? 0 : 2) <= sizeof(controlPayload),
               "HttpWebSocketEndpoint close payload too large");

    char   closePayload[125] = {0};
    size_t closePayloadSize  = 0;
    if (statusCode != 0)
    {
        closePayload[closePayloadSize++] = static_cast<char>((statusCode >> 8) & 0xFF);
        closePayload[closePayloadSize++] = static_cast<char>(statusCode & 0xFF);
    }
    if (reason.sizeInBytes() > 0)
    {
        ::memcpy(closePayload + closePayloadSize, reason.data(), reason.sizeInBytes());
        closePayloadSize += reason.sizeInBytes();
    }

    HttpWebSocketFrameHeaderView header;
    header.opcode        = HttpWebSocketOpcode::Close;
    header.fin           = true;
    header.payloadLength = closePayloadSize;
    SC_TRY(applyOutgoingMask(header, maskKey));
    SC_TRY(sendFrame(header, {closePayload, closePayloadSize}, storage, encodedFrame));
    closeSent = true;
    return Result(true);
}

Result HttpWebSocketEndpoint::getPendingControlFrame(Span<const char>& frame) const
{
    SC_TRY_MSG(hasPendingControlFrame(), "HttpWebSocketEndpoint no pending control frame");
    frame = pendingControlFrame;
    return Result(true);
}

void HttpWebSocketEndpoint::clearPendingControlFrame() { pendingControlFrame = {}; }

Result HttpWebSocketEndpoint::onReaderFrameHeader(const HttpWebSocketFrameHeaderView& header)
{
    currentFrame = header;
    if (header.isControlFrame())
    {
        controlPayloadSize = 0;
    }
    if (onFrameHeader.isValid())
    {
        SC_TRY(onFrameHeader(header));
    }
    if (header.isControlFrame() and header.payloadLength == 0)
    {
        SC_TRY(handleControlFrame({controlPayload, 0}));
    }
    return Result(true);
}

Result HttpWebSocketEndpoint::onReaderPayload(Span<char> payload, bool frameFinished)
{
    if (currentFrame.isControlFrame())
    {
        SC_TRY_MSG(controlPayloadSize + payload.sizeInBytes() <= sizeof(controlPayload),
                   "HttpWebSocketEndpoint control payload too large");
        if (payload.sizeInBytes() > 0)
        {
            ::memcpy(controlPayload + controlPayloadSize, payload.data(), payload.sizeInBytes());
            controlPayloadSize += payload.sizeInBytes();
        }
        if (frameFinished)
        {
            SC_TRY(handleControlFrame({controlPayload, controlPayloadSize}));
        }
        return Result(true);
    }

    if (onDataFramePayload.isValid())
    {
        SC_TRY(onDataFramePayload(currentFrame.opcode, payload, frameFinished));
    }
    return Result(true);
}

Result HttpWebSocketEndpoint::handleControlFrame(Span<char> payload)
{
    switch (currentFrame.opcode)
    {
    case HttpWebSocketOpcode::Ping:
        if (onPing.isValid())
        {
            SC_TRY(onPing(payload));
        }
        return queueAutomaticControl(HttpWebSocketOpcode::Pong, payload);
    case HttpWebSocketOpcode::Pong:
        if (onPong.isValid())
        {
            SC_TRY(onPong(payload));
        }
        return Result(true);
    case HttpWebSocketOpcode::Close: {
        SC_TRY_MSG(payload.sizeInBytes() != 1, "HttpWebSocketEndpoint malformed close payload");
        uint16_t   statusCode = 0;
        Span<char> reason;
        if (payload.sizeInBytes() >= 2)
        {
            statusCode =
                static_cast<uint16_t>((static_cast<uint8_t>(payload[0]) << 8) | static_cast<uint8_t>(payload[1]));
            (void)payload.sliceStart(2, reason);
        }
        closeReceived = true;
        if (onClose.isValid())
        {
            SC_TRY(onClose(statusCode, reason));
        }
        if (not closeSent)
        {
            SC_TRY(queueAutomaticControl(HttpWebSocketOpcode::Close, payload));
            closeSent = true;
        }
        return Result(true);
    }
    default: break;
    }
    return Result(true);
}

Result HttpWebSocketEndpoint::queueAutomaticControl(HttpWebSocketOpcode opcode, Span<const char> payload)
{
    SC_TRY_MSG(not hasPendingControlFrame(), "HttpWebSocketEndpoint pending control frame backpressure");

    HttpWebSocketFrameHeaderView header;
    header.opcode        = opcode;
    header.fin           = true;
    header.payloadLength = payload.sizeInBytes();
    SC_TRY(applyOutgoingMask(header, automaticMaskKeySet ? automaticMaskKey : nullptr));

    HttpWebSocketFrameWriter controlWriter;
    controlWriter.reset(endpointRole);

    Span<const char> encodedHeader;
    SC_TRY(controlWriter.beginFrame(header, automaticControlStorage, encodedHeader));
    SC_TRY_MSG(sizeof(automaticControlStorage) >= encodedHeader.sizeInBytes() + payload.sizeInBytes(),
               "HttpWebSocketEndpoint automatic control storage too small");
    if (payload.sizeInBytes() > 0)
    {
        Span<char> destinationPayload = {automaticControlStorage + encodedHeader.sizeInBytes(), payload.sizeInBytes()};
        ::memcpy(destinationPayload.data(), payload.data(), payload.sizeInBytes());
        SC_TRY(controlWriter.writePayload(destinationPayload));
    }
    SC_TRY(controlWriter.finishFrame());

    pendingControlFrame = {automaticControlStorage, encodedHeader.sizeInBytes() + payload.sizeInBytes()};
    return Result(true);
}

Result HttpWebSocketConnectionPump::attach(const HttpWebSocketTransportView& newTransport,
                                           HttpWebSocketEndpointRole         endpointRole)
{
    SC_TRY_MSG(newTransport.isValid(), "HttpWebSocketConnectionPump transport is invalid");
    detach();

    transport = newTransport;
    endpoint.reset(endpointRole);
    endpoint.onDataFramePayload.bind<HttpWebSocketConnectionPump, &HttpWebSocketConnectionPump::onEndpointDataFrame>(
        *this);

    dataListenerAdded = transport.readableStream->eventData
                            .addListener<HttpWebSocketConnectionPump, &HttpWebSocketConnectionPump::onData>(*this);
    if (not dataListenerAdded)
    {
        detach();
        return Result::Error("HttpWebSocketConnectionPump data listener limit reached");
    }

    endListenerAdded = transport.readableStream->eventEnd
                           .addListener<HttpWebSocketConnectionPump, &HttpWebSocketConnectionPump::onStreamEnd>(*this);
    if (not endListenerAdded)
    {
        detach();
        return Result::Error("HttpWebSocketConnectionPump end listener limit reached");
    }

    closeListenerAdded =
        transport.readableStream->eventClose
            .addListener<HttpWebSocketConnectionPump, &HttpWebSocketConnectionPump::onStreamEnd>(*this);
    if (not closeListenerAdded)
    {
        detach();
        return Result::Error("HttpWebSocketConnectionPump close listener limit reached");
    }
    return Result(true);
}

void HttpWebSocketConnectionPump::detach()
{
    if (transport.readableStream != nullptr)
    {
        if (dataListenerAdded)
        {
            (void)transport.readableStream->eventData
                .removeListener<HttpWebSocketConnectionPump, &HttpWebSocketConnectionPump::onData>(*this);
        }
        if (endListenerAdded)
        {
            (void)transport.readableStream->eventEnd
                .removeListener<HttpWebSocketConnectionPump, &HttpWebSocketConnectionPump::onStreamEnd>(*this);
        }
        if (closeListenerAdded)
        {
            (void)transport.readableStream->eventClose
                .removeListener<HttpWebSocketConnectionPump, &HttpWebSocketConnectionPump::onStreamEnd>(*this);
        }
    }
    dataListenerAdded  = false;
    endListenerAdded   = false;
    closeListenerAdded = false;
    transport.reset();
}

Result HttpWebSocketConnectionPump::writeFrame(Span<const char> frame)
{
    SC_TRY_MSG(transport.isValid(), "HttpWebSocketConnectionPump is not attached");
    SC_TRY_MSG(not frame.empty(), "HttpWebSocketConnectionPump cannot write empty frame");

    AsyncBufferView::ID bufferID;
    Span<char>          writableData;
    SC_TRY(transport.buffersPool->requestNewBuffer(frame.sizeInBytes(), bufferID, writableData));
    ::memcpy(writableData.data(), frame.data(), frame.sizeInBytes());
    transport.buffersPool->setNewBufferSize(bufferID, frame.sizeInBytes());
    const Result writeResult = transport.writableStream->write(bufferID);
    transport.buffersPool->unrefBuffer(bufferID);
    return writeResult;
}

Result HttpWebSocketConnectionPump::flushPendingControlFrame()
{
    if (not endpoint.hasPendingControlFrame())
    {
        return Result(true);
    }

    Span<const char> controlFrame;
    SC_TRY(endpoint.getPendingControlFrame(controlFrame));
    SC_TRY(writeFrame(controlFrame));
    endpoint.clearPendingControlFrame();
    return Result(true);
}

void HttpWebSocketConnectionPump::onData(AsyncBufferView::ID bufferID)
{
    if (not transport.isValid())
    {
        return;
    }

    Span<char> data;
    Result     dataResult = transport.buffersPool->getWritableData(bufferID, data);
    if (not dataResult)
    {
        fail(dataResult);
        return;
    }

    size_t consumed = 0;
    Result received = endpoint.receive(data, consumed);
    if (received)
    {
        received = flushPendingControlFrame();
    }
    if (not received)
    {
        fail(received);
    }
}

void HttpWebSocketConnectionPump::onStreamEnd()
{
    detach();
    if (onEnd.isValid())
    {
        onEnd();
    }
}

Result HttpWebSocketConnectionPump::onEndpointDataFrame(HttpWebSocketOpcode opcode, Span<char> payload,
                                                        bool frameFinished)
{
    if (onDataFramePayload.isValid())
    {
        SC_TRY(onDataFramePayload(opcode, payload, frameFinished));
    }
    return Result(true);
}

void HttpWebSocketConnectionPump::fail(Result result)
{
    if (onError.isValid())
    {
        onError(result);
    }
    if (transport.readableStream != nullptr)
    {
        transport.readableStream->destroy();
    }
}

void HttpWebSocketHubClient::reset()
{
    transport.reset();
    active = false;
}

Result HttpWebSocketSmallHub::init(Span<HttpWebSocketHubClient> clientStorage)
{
    clients    = clientStorage;
    numClients = 0;
    for (HttpWebSocketHubClient& client : clients)
    {
        client.reset();
    }
    return Result(true);
}

Result HttpWebSocketSmallHub::join(const HttpWebSocketTransportView& transport, size_t& clientIndex)
{
    SC_TRY_MSG(transport.isValid(), "HttpWebSocketSmallHub transport is invalid");
    for (size_t idx = 0; idx < clients.sizeInElements(); ++idx)
    {
        HttpWebSocketHubClient& client = clients[idx];
        if (not client.active)
        {
            client.transport = transport;
            client.active    = true;
            clientIndex      = idx;
            numClients++;
            return Result(true);
        }
    }
    return Result::Error("HttpWebSocketSmallHub is full");
}

Result HttpWebSocketSmallHub::leave(size_t clientIndex)
{
    SC_TRY_MSG(clientIndex < clients.sizeInElements(), "HttpWebSocketSmallHub client index out of range");
    HttpWebSocketHubClient& client = clients[clientIndex];
    if (client.active)
    {
        client.reset();
        numClients--;
    }
    return Result(true);
}

bool HttpWebSocketSmallHub::isClientActive(size_t clientIndex) const
{
    return clientIndex < clients.sizeInElements() and clients[clientIndex].active;
}

Result HttpWebSocketSmallHub::broadcastFrame(Span<const char> encodedFrame)
{
    SC_TRY_MSG(not encodedFrame.empty(), "HttpWebSocketSmallHub cannot broadcast an empty frame");
    for (size_t idx = 0; idx < clients.sizeInElements(); ++idx)
    {
        HttpWebSocketHubClient& client = clients[idx];
        if (not client.active)
        {
            continue;
        }
        if (onBroadcastFrame.isValid())
        {
            SC_TRY(onBroadcastFrame(idx, encodedFrame));
            continue;
        }

        AsyncBufferView::ID bufferID;
        Span<char>          writableData;
        SC_TRY(client.transport.buffersPool->requestNewBuffer(encodedFrame.sizeInBytes(), bufferID, writableData));
        ::memcpy(writableData.data(), encodedFrame.data(), encodedFrame.sizeInBytes());
        client.transport.buffersPool->setNewBufferSize(bufferID, encodedFrame.sizeInBytes());
        const Result writeResult = client.transport.writableStream->write(bufferID);
        client.transport.buffersPool->unrefBuffer(bufferID);
        SC_TRY(writeResult);
    }
    return Result(true);
}

Result HttpWebSocketSmallHub::broadcastText(Span<const char> payload, Span<char> frameStorage)
{
    HttpWebSocketEndpoint endpoint;
    endpoint.reset(HttpWebSocketEndpointRole::Server);

    Span<const char> encodedFrame;
    SC_TRY(endpoint.sendData(HttpWebSocketOpcode::Text, payload, true, nullptr, frameStorage, encodedFrame));
    return broadcastFrame(encodedFrame);
}
} // namespace SC
