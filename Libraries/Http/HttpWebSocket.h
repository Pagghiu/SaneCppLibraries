// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../AsyncStreams/AsyncStreams.h"
#include "../Foundation/Function.h"
#include "../Foundation/Result.h"
#include "../Foundation/Span.h"
#include "HttpExport.h"

namespace SC
{
//! @addtogroup group_http
//! @{

/// @brief WebSocket frame opcode as defined by RFC 6455
enum class HttpWebSocketOpcode : uint8_t
{
    Continuation = 0x0,

    Text   = 0x1,
    Binary = 0x2,
    Close  = 0x8,
    Ping   = 0x9,
    Pong   = 0xA,
};

/// @brief Identifies the local endpoint role to validate masking rules
enum class HttpWebSocketEndpointRole : uint8_t
{
    Client,
    Server,
};

/// @brief Parsed or to-be-written WebSocket frame header
struct SC_HTTP_EXPORT HttpWebSocketFrameHeaderView
{
    HttpWebSocketOpcode opcode = HttpWebSocketOpcode::Text;

    bool     fin           = true;
    bool     masked        = false;
    uint64_t payloadLength = 0;
    uint8_t  maskKey[4]    = {0, 0, 0, 0};

    [[nodiscard]] bool isControlFrame() const;
};

/// @brief Minimal transport handoff shape for later HTTP upgrade integration
struct SC_HTTP_EXPORT HttpWebSocketTransportView
{
    AsyncReadableStream* readableStream = nullptr;
    AsyncWritableStream* writableStream = nullptr;
    AsyncBuffersPool*    buffersPool    = nullptr;

    void reset();

    [[nodiscard]] bool isValid() const
    {
        return readableStream != nullptr and writableStream != nullptr and buffersPool != nullptr;
    }
};

/// @brief Incremental WebSocket frame reader operating on caller-owned mutable byte slices
struct SC_HTTP_EXPORT HttpWebSocketFrameReader
{
    Function<Result(const HttpWebSocketFrameHeaderView&)> onFrameHeader;
    Function<Result(Span<char>, bool)>                    onFramePayload;

    void   reset(HttpWebSocketEndpointRole endpointRole);
    Result parse(Span<char> data, size_t& consumedBytes);

  private:
    enum class State : uint8_t
    {
        HeaderByte0,
        HeaderByte1,
        ExtendedLength,
        MaskKey,
        Payload,
    };

    Result onHeaderReady();
    Result finishCurrentFrame();

    HttpWebSocketEndpointRole    endpointRole = HttpWebSocketEndpointRole::Client;
    HttpWebSocketFrameHeaderView currentFrame;

    State state = State::HeaderByte0;

    bool fragmentedMessageInProgress = false;

    uint8_t headerByte0 = 0;

    uint8_t extendedLengthBytesExpected = 0;
    uint8_t extendedLengthBytesRead     = 0;
    uint8_t maskBytesRead               = 0;

    uint64_t extendedLengthAccumulator = 0;
    uint64_t payloadBytesRemaining     = 0;
    uint64_t payloadBytesConsumed      = 0;
};

/// @brief Incremental WebSocket frame writer operating on caller-owned header storage and payload buffers
struct SC_HTTP_EXPORT HttpWebSocketFrameWriter
{
    void reset(HttpWebSocketEndpointRole endpointRole);

    Result beginFrame(const HttpWebSocketFrameHeaderView& frame, Span<char> storage, Span<const char>& encodedHeader);
    Result writePayload(Span<char> payload);
    Result finishFrame();

  private:
    HttpWebSocketEndpointRole    endpointRole = HttpWebSocketEndpointRole::Client;
    HttpWebSocketFrameHeaderView currentFrame;

    bool frameInProgress             = false;
    bool fragmentedMessageInProgress = false;

    uint64_t payloadBytesRemaining = 0;
    uint64_t payloadBytesWritten   = 0;
};

//! @}
} // namespace SC
