// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "HttpWebSocket.h"

namespace
{
using size_t   = SC::size_t;
using uint8_t  = SC::uint8_t;
using uint64_t = SC::uint64_t;

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
} // namespace

namespace SC
{
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
} // namespace SC
