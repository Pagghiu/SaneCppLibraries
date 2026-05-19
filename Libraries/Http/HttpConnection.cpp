// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "HttpConnection.h"
#include "Internal/HttpStringIterator.h"

#include "../AsyncStreams/ZLibTransformStreams.h"
#include "../Foundation/Assert.h"
#include "Internal/HttpFixedBufferWriter.inl"
#include "Internal/HttpParsedHeaders.inl"

namespace
{
static bool scHttpHexValue(char current, uint8_t& value)
{
    if (current >= '0' and current <= '9')
    {
        value = static_cast<uint8_t>(current - '0');
        return true;
    }
    if (current >= 'a' and current <= 'f')
    {
        value = static_cast<uint8_t>(10 + current - 'a');
        return true;
    }
    if (current >= 'A' and current <= 'F')
    {
        value = static_cast<uint8_t>(10 + current - 'A');
        return true;
    }
    return false;
}

static bool scHttpTransferEncodingIsChunked(SC::StringSpan value)
{
    SC::HttpStringIterator it(value);
    while (it.match(' ') or it.match('\t'))
    {
        if (not it.stepForward())
        {
            break;
        }
    }
    const auto start = it;
    while (not it.isAtEnd() and not it.match(' ') and not it.match('\t'))
    {
        if (not it.stepForward())
        {
            break;
        }
    }
    SC::StringSpan token = SC::HttpStringIterator::fromIterators(start, it, value.getEncoding());
    if (not SC::HttpStringIterator::equalsIgnoreCase(token, "chunked"))
    {
        return false;
    }
    while (it.match(' ') or it.match('\t'))
    {
        if (not it.stepForward())
        {
            break;
        }
    }
    return it.isAtEnd();
}

static SC::Result scHttpFormatChunkHeader(uint64_t size, SC::Span<char> storage, SC::StringSpan& header)
{
    static constexpr const char HexDigits[] = "0123456789ABCDEF";

    SC_TRY_MSG(storage.sizeInBytes() >= 4, "HttpOutgoingMessage chunk header buffer too small");

    char   reversed[16];
    size_t numDigits = 0;
    do
    {
        reversed[numDigits++] = HexDigits[size & 0xF];
        size >>= 4;
    } while (size > 0 and numDigits < sizeof(reversed));

    SC_TRY_MSG(size == 0, "HttpOutgoingMessage chunk size overflow");
    SC_TRY_MSG(numDigits + 2 <= storage.sizeInBytes(), "HttpOutgoingMessage chunk header buffer too small");

    for (size_t idx = 0; idx < numDigits; ++idx)
    {
        storage[idx] = reversed[numDigits - idx - 1];
    }
    storage[numDigits + 0] = '\r';
    storage[numDigits + 1] = '\n';
    header                 = SC::StringSpan({storage.data(), numDigits + 2}, false, SC::StringEncoding::Ascii);
    return SC::Result(true);
}
} // namespace

namespace SC
{
StringSpan httpContentEncodingName(HttpContentEncoding encoding)
{
    switch (encoding)
    {
    case HttpContentEncoding::Identity: return "identity";
    case HttpContentEncoding::GZip: return "gzip";
    case HttpContentEncoding::Deflate: return "deflate";
    }
    return {};
}

Result httpContentEncodingFromHeader(StringSpan headerValue, HttpContentEncoding& encoding)
{
    HttpStringIterator it(headerValue);
    while (it.match(' ') or it.match('\t'))
    {
        if (not it.stepForward())
        {
            break;
        }
    }
    const auto start = it;
    while (not it.isAtEnd() and not it.match(' ') and not it.match('\t') and not it.match(','))
    {
        if (not it.stepForward())
        {
            break;
        }
    }
    StringSpan token = HttpStringIterator::fromIterators(start, it, headerValue.getEncoding());
    if (token.isEmpty() or HttpStringIterator::equalsIgnoreCase(token, "identity"))
    {
        encoding = HttpContentEncoding::Identity;
        return Result(true);
    }
    if (HttpStringIterator::equalsIgnoreCase(token, "gzip"))
    {
        encoding = HttpContentEncoding::GZip;
        return Result(true);
    }
    if (HttpStringIterator::equalsIgnoreCase(token, "deflate"))
    {
        encoding = HttpContentEncoding::Deflate;
        return Result(true);
    }
    return Result::Error("HttpContentEncoding unsupported content encoding");
}

//-------------------------------------------------------------------------------------------------------
// HttpMultipartWriter
//-------------------------------------------------------------------------------------------------------
void HttpMultipartWriter::reset()
{
    numParts = 0;
    boundary = {};
    ::memset(boundaryStorage, 0, sizeof(boundaryStorage));
}

Result HttpMultipartWriter::setBoundary(StringSpan boundaryValue)
{
    reset();
    SC_TRY_MSG(boundaryValue.sizeInBytes() > 0, "HttpMultipartWriter::setBoundary empty boundary");
    SC_TRY_MSG(boundaryValue.sizeInBytes() < sizeof(boundaryStorage), "HttpMultipartWriter::setBoundary too long");
    ::memcpy(boundaryStorage, boundaryValue.bytesWithoutTerminator(), boundaryValue.sizeInBytes());
    boundary = StringSpan::fromNullTerminated(boundaryStorage, StringEncoding::Ascii);
    return Result(true);
}

Result HttpMultipartWriter::addField(StringSpan fieldName, StringSpan value)
{
    SC_TRY_MSG(boundary.sizeInBytes() > 0, "HttpMultipartWriter::addField boundary not set");
    SC_TRY_MSG(numParts < MaxParts, "HttpMultipartWriter::addField too many parts");
    Part& part       = parts[numParts++];
    part.partName    = fieldName;
    part.fileName    = {};
    part.contentType = {};
    part.body        = value.toCharSpan();
    return Result(true);
}

Result HttpMultipartWriter::addFile(StringSpan fieldName, StringSpan fileName, Span<const char> body,
                                    StringSpan contentType)
{
    SC_TRY_MSG(boundary.sizeInBytes() > 0, "HttpMultipartWriter::addFile boundary not set");
    SC_TRY_MSG(numParts < MaxParts, "HttpMultipartWriter::addFile too many parts");
    Part& part       = parts[numParts++];
    part.partName    = fieldName;
    part.fileName    = fileName;
    part.contentType = contentType;
    part.body        = body;
    return Result(true);
}

size_t HttpMultipartWriter::getContentLength() const
{
    size_t total = 0;
    for (size_t idx = 0; idx < numParts; ++idx)
    {
        const Part& part = parts[idx];
        total += 2 + boundary.sizeInBytes() + 2; // --boundary\r\n
        total += sizeof("Content-Disposition: form-data; name=\"") - 1;
        total += part.partName.sizeInBytes();
        total += 1;
        if (part.fileName.sizeInBytes() > 0)
        {
            total += sizeof("; filename=\"") - 1;
            total += part.fileName.sizeInBytes();
            total += 1;
        }
        total += 2;

        if (part.contentType.sizeInBytes() > 0)
        {
            total += sizeof("Content-Type: ") - 1;
            total += part.contentType.sizeInBytes();
            total += 2;
        }
        total += 2;
        total += part.body.sizeInBytes();
        total += 2;
    }
    total += 2 + boundary.sizeInBytes() + 4; // --boundary--\r\n
    return total;
}
//-------------------------------------------------------------------------------------------------------
// HttpConnection
//-------------------------------------------------------------------------------------------------------
HttpConnection::HttpConnection()
{
    // This is needed on Linux so that ReadableSocketStream constructor don't need to be exported
    // in order to be used across plugin boundaries.
}

void HttpConnection::reset()
{
    request.reset();
    response.reset();
    state             = State::Inactive;
    webSocketUpgraded = false;
}

void HttpConnectionBase::reset()
{
    (void)pipeline.unpipe();
    readableSocketStream.destroy();
    writableSocketStream.destroy();
    if (socket.isValid())
    {
        (void)socket.close();
    }
}

//-------------------------------------------------------------------------------------------------------
// HttpIncomingMessage
//-------------------------------------------------------------------------------------------------------
void HttpIncomingMessage::resetIncoming(HttpParser::Type type, Span<char> memory)
{
    bodyStream.destroy();
    headerMemory          = memory;
    readableStream        = &bodyStream;
    bodyBytesRemaining    = 0;
    bodyFramingKind       = HttpBodyFramingKind::None;
    bodyComplete          = true;
    bodyStreamStarted     = false;
    chunkedState          = ChunkedState::Size;
    chunkedChunkSize      = 0;
    chunkedBytesRemaining = 0;
    chunkedSizeHasDigits  = false;
    parsedHeaders.reset(type, memory);
}

HttpIncomingMessage::BodyStream::BodyStream()
{
    setReadQueue(queue);
    setAutoDestroy(false);
}

Result HttpIncomingMessage::BodyStream::begin(AsyncBuffersPool& buffersPool, Function<Result()>&& callback)
{
    onReadRequest = move(callback);
    return init(buffersPool);
}

bool HttpIncomingMessage::BodyStream::pushBodyData(AsyncBufferView::ID bufferID, size_t sizeInBytes)
{
    return push(bufferID, sizeInBytes);
}

void HttpIncomingMessage::BodyStream::finishBody() { pushEnd(); }

void HttpIncomingMessage::BodyStream::failBody(Result result) { emitError(result); }

Result HttpIncomingMessage::BodyStream::asyncRead()
{
    if (onReadRequest.isValid())
    {
        return onReadRequest();
    }
    return Result(true);
}

bool HttpIncomingMessage::findParserToken(HttpParser::Token token, StringSpan& res) const
{
    return parsedHeaders.findParserToken(token, res);
}

bool HttpIncomingMessage::getHeader(StringSpan headerName, StringSpan& value) const
{
    return parsedHeaders.getHeader(headerName, value);
}

StringSpan HttpIncomingMessage::getVersion() const
{
    StringSpan version;
    (void)findParserToken(HttpParser::Token::Version, version);
    return version;
}

AsyncReadableStream& HttpIncomingMessage::getReadableStream()
{
    SC_ASSERT_RELEASE(readableStream != nullptr);
    return *readableStream;
}

const AsyncReadableStream& HttpIncomingMessage::getReadableStream() const
{
    SC_ASSERT_RELEASE(readableStream != nullptr);
    return *readableStream;
}

Result HttpIncomingMessage::consumeBodyBytes(size_t bytes)
{
    if (bodyFramingKind != HttpBodyFramingKind::ContentLength)
    {
        return Result(true);
    }
    SC_TRY_MSG(bytes <= bodyBytesRemaining, "HttpIncomingMessage body exceeds Content-Length");
    bodyBytesRemaining -= bytes;
    return Result(true);
}

Result HttpIncomingMessage::initBodyStream(AsyncBuffersPool& buffersPool, Function<Result()>&& onReadRequest)
{
    bodyStream.destroy();
    attachReadableStream(bodyStream);
    bodyStreamStarted = false;
    return bodyStream.begin(buffersPool, move(onReadRequest));
}

Result HttpIncomingMessage::startBodyStream()
{
    if (bodyStream.canStart())
    {
        SC_TRY(bodyStream.start());
    }
    else
    {
        bodyStream.resumeReading();
    }
    bodyStreamStarted = true;
    return Result(true);
}

bool HttpIncomingMessage::pushBodyData(AsyncBufferView::ID bufferID, size_t sizeInBytes)
{
    return bodyStream.pushBodyData(bufferID, sizeInBytes);
}

void HttpIncomingMessage::finishBodyStream()
{
    bodyComplete = true;
    bodyStream.finishBody();
}

void HttpIncomingMessage::failBodyStream(Result result)
{
    bodyComplete = false;
    bodyStream.failBody(result);
}

void HttpIncomingMessage::abortBodyStream() { bodyStream.destroy(); }

Result HttpIncomingMessage::prepareBodyStream(AsyncBuffersPool& buffersPool, Function<Result()>&& onReadRequest,
                                              bool allowCloseDelimited)
{
    StringSpan transferEncoding;
    StringSpan contentLengthValue;
    const bool hasTransferEncoding = getHeader("Transfer-Encoding", transferEncoding);
    if (hasTransferEncoding and getHeader("Content-Length", contentLengthValue))
    {
        return Result::Error("HttpIncomingMessage conflicting Content-Length and Transfer-Encoding");
    }

    if (hasTransferEncoding)
    {
        if (not scHttpTransferEncodingIsChunked(transferEncoding))
        {
            return Result::Error("HttpIncomingMessage unsupported Transfer-Encoding");
        }
        bodyFramingKind       = HttpBodyFramingKind::Chunked;
        bodyBytesRemaining    = 0;
        bodyComplete          = false;
        chunkedState          = ChunkedState::Size;
        chunkedChunkSize      = 0;
        chunkedBytesRemaining = 0;
        chunkedSizeHasDigits  = false;
    }
    else if (getHeader("Content-Length", contentLengthValue))
    {
        bodyFramingKind    = HttpBodyFramingKind::ContentLength;
        bodyBytesRemaining = getParser().contentLength;
        bodyComplete       = (bodyBytesRemaining == 0);
    }
    else if (allowCloseDelimited)
    {
        bodyFramingKind    = HttpBodyFramingKind::CloseDelimited;
        bodyBytesRemaining = 0;
        bodyComplete       = false;
    }
    else
    {
        bodyFramingKind    = HttpBodyFramingKind::None;
        bodyBytesRemaining = 0;
        bodyComplete       = true;
    }

    return initBodyStream(buffersPool, move(onReadRequest));
}

Result HttpIncomingMessage::processBodyData(AsyncReadableStream& sourceStream, AsyncBufferView::ID bufferID,
                                            Span<const char> readData, bool allowTrailingData)
{
    if (bodyFramingKind == HttpBodyFramingKind::None)
    {
        if (readData.sizeInBytes() > 0)
        {
            return Result::Error("HttpIncomingMessage unexpected body data");
        }
        return Result(true);
    }

    if (bodyFramingKind == HttpBodyFramingKind::ContentLength or bodyFramingKind == HttpBodyFramingKind::CloseDelimited)
    {
        if (bodyFramingKind == HttpBodyFramingKind::ContentLength and readData.sizeInBytes() > bodyBytesRemaining)
        {
            return Result::Error("HttpIncomingMessage received body beyond Content-Length");
        }

        const bool shouldContinue = pushBodyData(bufferID, readData.sizeInBytes());
        SC_TRY(consumeBodyBytes(readData.sizeInBytes()));
        if (bodyFramingKind == HttpBodyFramingKind::ContentLength and bodyBytesRemaining == 0)
        {
            finishBodyStream();
        }
        if (not shouldContinue)
        {
            sourceStream.pause();
        }
        return Result(true);
    }

    size_t rawOffset = 0;
    while (rawOffset < readData.sizeInBytes())
    {
        const char current = readData.data()[rawOffset];
        switch (chunkedState)
        {
        case ChunkedState::Size: {
            uint8_t hexValue = 0;
            if (scHttpHexValue(current, hexValue))
            {
                SC_TRY_MSG(chunkedChunkSize <= (UINT64_MAX - hexValue) / 16, "HttpIncomingMessage chunk size overflow");
                chunkedChunkSize     = chunkedChunkSize * 16 + hexValue;
                chunkedSizeHasDigits = true;
                rawOffset++;
                break;
            }
            if (current == ';')
            {
                SC_TRY_MSG(chunkedSizeHasDigits, "HttpIncomingMessage invalid chunk size");
                chunkedState = ChunkedState::SizeExtension;
                rawOffset++;
                break;
            }
            if (current == '\r')
            {
                SC_TRY_MSG(chunkedSizeHasDigits, "HttpIncomingMessage invalid chunk size");
                chunkedState = ChunkedState::SizeLF;
                rawOffset++;
                break;
            }
            return Result::Error("HttpIncomingMessage invalid chunk size");
        }
        case ChunkedState::SizeExtension:
            if (current == '\r')
            {
                chunkedState = ChunkedState::SizeLF;
            }
            rawOffset++;
            break;
        case ChunkedState::SizeLF:
            SC_TRY_MSG(current == '\n', "HttpIncomingMessage malformed chunk header");
            rawOffset++;
            if (chunkedChunkSize == 0)
            {
                chunkedState = ChunkedState::TrailerLineStart;
            }
            else
            {
                chunkedBytesRemaining = chunkedChunkSize;
                chunkedChunkSize      = 0;
                chunkedSizeHasDigits  = false;
                chunkedState          = ChunkedState::Data;
            }
            break;
        case ChunkedState::Data: {
            const size_t available = readData.sizeInBytes() - rawOffset;
            const size_t toEmit =
                available < chunkedBytesRemaining ? available : static_cast<size_t>(chunkedBytesRemaining);
            if (toEmit == 0)
            {
                chunkedState = ChunkedState::DataCR;
                break;
            }
            AsyncBufferView::ID childID;
            SC_TRY(sourceStream.getBuffersPool().createChildView(bufferID, rawOffset, toEmit, childID));
            const bool shouldContinue = pushBodyData(childID, toEmit);
            sourceStream.getBuffersPool().unrefBuffer(childID);
            chunkedBytesRemaining -= toEmit;
            rawOffset += toEmit;
            if (chunkedBytesRemaining == 0)
            {
                chunkedState = ChunkedState::DataCR;
            }
            if (not shouldContinue)
            {
                if (rawOffset < readData.sizeInBytes())
                {
                    AsyncBufferView::ID pendingID;
                    SC_TRY(sourceStream.getBuffersPool().createChildView(
                        bufferID, rawOffset, readData.sizeInBytes() - rawOffset, pendingID));
                    SC_TRY(sourceStream.unshift(pendingID));
                    sourceStream.getBuffersPool().unrefBuffer(pendingID);
                }
                sourceStream.pause();
                return Result(true);
            }
            break;
        }
        case ChunkedState::DataCR:
            SC_TRY_MSG(current == '\r', "HttpIncomingMessage malformed chunk terminator");
            chunkedState = ChunkedState::DataLF;
            rawOffset++;
            break;
        case ChunkedState::DataLF:
            SC_TRY_MSG(current == '\n', "HttpIncomingMessage malformed chunk terminator");
            chunkedChunkSize     = 0;
            chunkedSizeHasDigits = false;
            chunkedState         = ChunkedState::Size;
            rawOffset++;
            break;
        case ChunkedState::TrailerLineStart:
            if (current == '\r')
            {
                chunkedState = ChunkedState::TrailerEndLF;
                rawOffset++;
                break;
            }
            return Result::Error("HttpIncomingMessage non-empty trailers are not supported");
        case ChunkedState::TrailerEndLF:
            SC_TRY_MSG(current == '\n', "HttpIncomingMessage malformed trailer terminator");
            chunkedState = ChunkedState::Finished;
            bodyComplete = true;
            rawOffset++;
            if (rawOffset < readData.sizeInBytes())
            {
                if (allowTrailingData)
                {
                    AsyncBufferView::ID pendingID;
                    SC_TRY(sourceStream.getBuffersPool().createChildView(
                        bufferID, rawOffset, readData.sizeInBytes() - rawOffset, pendingID));
                    SC_TRY(sourceStream.unshift(pendingID));
                    sourceStream.getBuffersPool().unrefBuffer(pendingID);
                }
                else
                {
                    return Result::Error("HttpIncomingMessage does not support pipelined body data");
                }
            }
            finishBodyStream();
            return Result(true);
        case ChunkedState::Finished:
            if (allowTrailingData)
            {
                AsyncBufferView::ID pendingID;
                SC_TRY(sourceStream.getBuffersPool().createChildView(bufferID, rawOffset,
                                                                     readData.sizeInBytes() - rawOffset, pendingID));
                SC_TRY(sourceStream.unshift(pendingID));
                sourceStream.getBuffersPool().unrefBuffer(pendingID);
                return Result(true);
            }
            return Result::Error("HttpIncomingMessage does not support pipelined body data");
        }
    }
    return Result(true);
}

Result HttpIncomingMessage::writeHeaders(const uint32_t maxSize, Span<const char> readData, AsyncReadableStream& stream,
                                         AsyncBufferView::ID bufferID, const char* outOfSpaceError,
                                         const char* sizeExceededError, bool stopAtHeadersEnd,
                                         bool unshiftPendingBodyToStream)
{
    attachReadableStream(stream);
    const auto onParserResult = [&](const HttpParser&) -> Result { return Result(true); };
    SC_TRY(parsedHeaders.writeHeaders(maxSize, readData, stream, bufferID, outOfSpaceError, sizeExceededError,
                                      stopAtHeadersEnd, unshiftPendingBodyToStream, onParserResult));
    return Result(true);
}

size_t HttpIncomingMessage::getHeadersLength() const { return parsedHeaders.getHeadersLength(); }

bool HttpIncomingMessage::isMultipart() const
{
    StringSpan contentType;
    if (getHeader("Content-Type", contentType))
    {
        return HttpStringIterator::startsWithIgnoreCase(contentType, "multipart/form-data");
    }
    return false;
}

StringSpan HttpIncomingMessage::getBoundary() const
{
    StringSpan contentType;
    if (getHeader("Content-Type", contentType))
    {
        HttpStringIterator it(contentType);
        if (it.advanceUntilMatchesIgnoreCase("boundary="))
        {
            for (size_t i = 0; i < 9; ++i)
                (void)it.stepForward();

            if (it.advanceIfMatches('"'))
            {
                auto start = it;
                while (not it.isAtEnd() and not it.match('"'))
                    (void)it.stepForward();
                return HttpStringIterator::fromIterators(start, it, contentType.getEncoding());
            }

            auto start = it;
            char matched;
            (void)it.advanceUntilMatchesAny({';', ' ', '\r', '\0'}, matched);
            return HttpStringIterator::fromIterators(start, it, contentType.getEncoding());
        }
    }
    return {};
}

//-------------------------------------------------------------------------------------------------------
// HttpRequest
//-------------------------------------------------------------------------------------------------------
void HttpRequest::setHeaderMemory(Span<char> memory)
{
    resetIncoming(HttpParser::Type::Request, memory);
    url = {};
}

void HttpRequest::reset()
{
    resetIncoming(HttpParser::Type::Request, headerMemory);
    url = {};
}

Result HttpRequest::writeHeaders(const uint32_t maxSize, Span<const char> readData, AsyncReadableStream& stream,
                                 AsyncBufferView::ID bufferID)
{
    SC_TRY(HttpIncomingMessage::writeHeaders(maxSize, readData, stream, bufferID, "Header space is finished",
                                             "Header size exceeded limit", false));
    SC_TRY(findParserToken(HttpParser::Token::Url, url));
    setBodyBytesRemaining(getParser().contentLength);
    return Result(true);
}

//-------------------------------------------------------------------------------------------------------
// HttpAsyncClientResponse
//-------------------------------------------------------------------------------------------------------
void HttpAsyncClientResponse::reset(Span<char> memory) { resetIncoming(HttpParser::Type::Response, memory); }

Result HttpAsyncClientResponse::writeHeaders(uint32_t maxHeaderSize, Span<const char> readData,
                                             AsyncReadableStream& stream, AsyncBufferView::ID bufferID)
{
    SC_TRY(HttpIncomingMessage::writeHeaders(maxHeaderSize, readData, stream, bufferID,
                                             "HttpAsyncClientResponse header space is finished",
                                             "HttpAsyncClientResponse header size exceeded", true, false));
    return Result(true);
}

//-------------------------------------------------------------------------------------------------------
// HttpOutgoingMessage
//-------------------------------------------------------------------------------------------------------
HttpOutgoingMessage::ChunkedWritableStream::ChunkedWritableStream() { setWriteQueue(queue); }

Result HttpOutgoingMessage::ChunkedWritableStream::init(AsyncBuffersPool&    buffersPool,
                                                        AsyncWritableStream& newDestination)
{
    destination         = &newDestination;
    currentBodyBufferID = {};
    currentBodyCallback = {};
    finalChunkStarted   = false;
    finalChunkWritten   = false;
    return AsyncWritableStream::init(buffersPool);
}

Result HttpOutgoingMessage::ChunkedWritableStream::asyncWrite(AsyncBufferView::ID                 bufferID,
                                                              Function<void(AsyncBufferView::ID)> cb)
{
    SC_TRY_MSG(destination != nullptr, "HttpOutgoingMessage chunked destination missing");

    Span<const char> body;
    SC_TRY(getBuffersPool().getReadableData(bufferID, body));

    StringSpan header;
    SC_TRY(scHttpFormatChunkHeader(static_cast<uint64_t>(body.sizeInBytes()), headerStorage, header));

    currentBodyBufferID = bufferID;
    currentBodyCallback = move(cb);
    getBuffersPool().refBuffer(bufferID);
    return destination->write(AsyncBufferView(header.toCharSpan()),
                              {[this](AsyncBufferView::ID writtenBufferID) { onChunkHeaderWritten(writtenBufferID); }});
}

bool HttpOutgoingMessage::ChunkedWritableStream::canEndWritable()
{
    if (finalChunkWritten)
    {
        return true;
    }
    if (finalChunkStarted)
    {
        return false;
    }

    SC_ASSERT_RELEASE(destination != nullptr);
    finalChunkStarted = true;
    Result finalWrite = destination->write(AsyncBufferView("0\r\n\r\n"), {[this](AsyncBufferView::ID writtenBufferID)
                                                                          { onFinalChunkWritten(writtenBufferID); }});
    if (not finalWrite)
    {
        eventError.emit(finalWrite);
        finalChunkWritten = true;
        destination->end();
        return true;
    }
    return false;
}

void HttpOutgoingMessage::ChunkedWritableStream::onChunkHeaderWritten(AsyncBufferView::ID)
{
    SC_ASSERT_RELEASE(destination != nullptr);
    Result bodyWrite = destination->write(
        currentBodyBufferID, {[this](AsyncBufferView::ID writtenBufferID) { onChunkBodyWritten(writtenBufferID); }});
    if (not bodyWrite)
    {
        eventError.emit(bodyWrite);
    }
}

void HttpOutgoingMessage::ChunkedWritableStream::onChunkBodyWritten(AsyncBufferView::ID)
{
    SC_ASSERT_RELEASE(destination != nullptr);
    Result terminatorWrite =
        destination->write(AsyncBufferView("\r\n"), {[this](AsyncBufferView::ID writtenBufferID)
                                                     { onChunkTerminatorWritten(writtenBufferID); }});
    if (not terminatorWrite)
    {
        eventError.emit(terminatorWrite);
    }
}

void HttpOutgoingMessage::ChunkedWritableStream::onChunkTerminatorWritten(AsyncBufferView::ID)
{
    AsyncBufferView::ID                 savedBufferID = currentBodyBufferID;
    Function<void(AsyncBufferView::ID)> savedCallback = move(currentBodyCallback);
    currentBodyBufferID                               = {};
    currentBodyCallback                               = {};
    getBuffersPool().unrefBuffer(savedBufferID);
    finishedWriting(savedBufferID, move(savedCallback), Result(true));
}

void HttpOutgoingMessage::ChunkedWritableStream::onFinalChunkWritten(AsyncBufferView::ID)
{
    SC_ASSERT_RELEASE(destination != nullptr);
    finalChunkWritten = true;
    destination->end();
    resumeWriting();
}

void HttpOutgoingMessage::setHeaderMemory(Span<char> memory)
{
    headerMemory = memory;
    responseHeaders.reset(memory);
}

bool HttpOutgoingMessage::hasHeader(KnownHeader header) const
{
    switch (header)
    {
    case KnownHeader::Connection: return connectionHeaderAdded;
    case KnownHeader::Host: return hostHeaderAdded;
    case KnownHeader::UserAgent: return userAgentHeaderAdded;
    case KnownHeader::ContentLength: return contentLengthAdded;
    case KnownHeader::ContentType: return contentTypeAdded;
    case KnownHeader::TransferEncoding: return transferEncodingAdded;
    case KnownHeader::ContentEncoding: return contentEncodingAdded;
    case KnownHeader::AcceptEncoding: return acceptEncodingAdded;
    }
    return false;
}

Result HttpOutgoingMessage::addHeader(StringSpan headerName, StringSpan headerValue)
{
    SC_TRY_MSG(not headersSent, "Headers already sent");
    SC_TRY_MSG(responseHeaders.writtenBytes() != 0, "startResponse or startRequest must be the first call");

    if (HttpStringIterator::equalsIgnoreCase(headerName, StringSpan("Connection")))
    {
        if (HttpStringIterator::equalsIgnoreCase(headerValue, StringSpan("keep-alive")))
        {
            if (forceDisableKeepAlive)
            {
                return Result::Error("HttpOutgoingMessage::addHeader - keep-alive forcefully disabled");
            }
            keepAlive = true;
        }
        else if (HttpStringIterator::equalsIgnoreCase(headerValue, StringSpan("close")) ||
                 HttpStringIterator::equalsIgnoreCase(headerValue, StringSpan("Closed")))
        {
            keepAlive = false;
        }
        connectionHeaderAdded = true;
    }
    else if (HttpStringIterator::equalsIgnoreCase(headerName, StringSpan("Host")))
    {
        hostHeaderAdded = true;
    }
    else if (HttpStringIterator::equalsIgnoreCase(headerName, StringSpan("User-Agent")))
    {
        userAgentHeaderAdded = true;
    }
    else if (HttpStringIterator::equalsIgnoreCase(headerName, StringSpan("Content-Length")))
    {
        contentLengthAdded = true;
    }
    else if (HttpStringIterator::equalsIgnoreCase(headerName, StringSpan("Content-Type")))
    {
        contentTypeAdded = true;
    }
    else if (HttpStringIterator::equalsIgnoreCase(headerName, StringSpan("Transfer-Encoding")))
    {
        transferEncodingAdded = true;
    }
    else if (HttpStringIterator::equalsIgnoreCase(headerName, StringSpan("Content-Encoding")))
    {
        contentEncodingAdded = true;
    }
    else if (HttpStringIterator::equalsIgnoreCase(headerName, StringSpan("Accept-Encoding")))
    {
        acceptEncodingAdded = true;
    }

    SC_TRY(responseHeaders.appendHeader(headerName, headerValue,
                                        "HttpOutgoingMessage::appendAscii - header space is finished"));
    return Result(true);
}

Result HttpOutgoingMessage::setChunkedTransferEncoding()
{
    SC_TRY_MSG(not headersSent, "Headers already sent");
    if (chunkedTransferEncodingEnabled)
    {
        return Result(true);
    }
    chunkedTransferEncodingEnabled = true;
    if (not transferEncodingAdded)
    {
        SC_TRY(addHeader("Transfer-Encoding", "chunked"));
    }
    return Result(true);
}

Result HttpOutgoingMessage::sendHeaders(Function<void(AsyncBufferView::ID)> callback)
{
    SC_TRY_MSG(not headersSent, "Headers already sent");
    SC_TRY_MSG(responseHeaders.writtenBytes() != 0, "startResponse or startRequest must be the first call");
    SC_TRY_MSG(not(chunkedTransferEncodingEnabled and contentLengthAdded),
               "HttpOutgoingMessage does not support Content-Length with Transfer-Encoding");

    if (chunkedTransferEncodingEnabled)
    {
        SC_TRY_MSG(destinationStream != nullptr, "HttpOutgoingMessage missing destination stream");
        SC_TRY(chunkedWritableStream.init(destinationStream->getBuffersPool(), *destinationStream));
        writableStream = &chunkedWritableStream;
    }
    else
    {
        writableStream = destinationStream;
    }

    if (not connectionHeaderAdded)
    {
        if (forceDisableKeepAlive or not keepAlive)
        {
            SC_TRY(responseHeaders.appendLiteral("Connection: close\r\n",
                                                 "HttpOutgoingMessage::appendAscii - header space is finished"));
        }
        else
        {
            SC_TRY(responseHeaders.appendLiteral("Connection: keep-alive\r\n",
                                                 "HttpOutgoingMessage::appendAscii - header space is finished"));
        }
    }
    SC_TRY(responseHeaders.appendLiteral("\r\n", "HttpOutgoingMessage::appendAscii - header space is finished"));
    SC_TRY(destinationStream->write(responseHeaders.written(), move(callback)));
    headersSent = true;
    return Result(true);
}

void HttpOutgoingMessage::reset()
{
    chunkedWritableStream.destroy();
    headersSent                    = false;
    endCalled                      = false;
    keepAlive                      = true;
    connectionHeaderAdded          = false;
    hostHeaderAdded                = false;
    userAgentHeaderAdded           = false;
    contentLengthAdded             = false;
    contentTypeAdded               = false;
    transferEncodingAdded          = false;
    contentEncodingAdded           = false;
    acceptEncodingAdded            = false;
    chunkedTransferEncodingEnabled = false;
    forceDisableKeepAlive          = false;
    writableStream                 = destinationStream;
    responseHeaders.reset(headerMemory);
}

Result HttpOutgoingMessage::end()
{
    SC_TRY_MSG(headersSent, "Forgot to send headers");
    writableStream->end();
    endCalled = true;
    return Result(true);
}

void HttpOutgoingMessage::setKeepAlive(bool value) { keepAlive = value; }

//-------------------------------------------------------------------------------------------------------
// HttpResponse
//-------------------------------------------------------------------------------------------------------
Result HttpResponse::startResponse(int code)
{
    SC_TRY_MSG(not headersSent, "Headers already sent");
    SC_TRY_MSG(responseHeaders.writtenBytes() == 0, "startResponse must be the first call");
    SC_TRY(responseHeaders.appendLiteral("HTTP/1.1 ", "HttpResponse::appendAscii - header space is finished"));
    switch (code)
    {
    case 101:
        SC_TRY(responseHeaders.appendLiteral("101 Switching Protocols\r\n",
                                             "HttpResponse::appendAscii - header space is finished"));
        break;
    case 200:
        SC_TRY(responseHeaders.appendLiteral("200 OK\r\n", "HttpResponse::appendAscii - header space is finished"));
        break;
    case 201:
        SC_TRY(
            responseHeaders.appendLiteral("201 Created\r\n", "HttpResponse::appendAscii - header space is finished"));
        break;
    case 400:
        SC_TRY(responseHeaders.appendLiteral("400 Bad Request\r\n",
                                             "HttpResponse::appendAscii - header space is finished"));
        break;
    case 404:
        SC_TRY(
            responseHeaders.appendLiteral("404 Not Found\r\n", "HttpResponse::appendAscii - header space is finished"));
        break;
    case 405:
        SC_TRY(responseHeaders.appendLiteral("405 Method Not Allowed\r\n",
                                             "HttpResponse::appendAscii - header space is finished"));
        break;
    case 426:
        SC_TRY(responseHeaders.appendLiteral("426 Upgrade Required\r\n",
                                             "HttpResponse::appendAscii - header space is finished"));
        break;
    }
    return Result(true);
}

void HttpResponse::grabUnusedHeaderMemory(HttpRequest& request) { setHeaderMemory(request.getUnusedHeaderMemory()); }

//-------------------------------------------------------------------------------------------------------
// HttpAsyncClientRequest
//-------------------------------------------------------------------------------------------------------
void HttpAsyncClientRequest::reset()
{
    HttpOutgoingMessage::reset();
    method                      = HttpParser::Method::HttpGET;
    url                         = {};
    bodyStream                  = nullptr;
    bodyTransform               = nullptr;
    bodyType                    = BodyType::None;
    bodySpan                    = {};
    contentLength               = 0;
    contentEncoding             = HttpContentEncoding::Identity;
    multipartWriter             = nullptr;
    defaultHost                 = {};
    userHeadersSentCallback     = {};
    internalHeadersSentCallback = {};
}

Result HttpAsyncClientRequest::startRequest(HttpParser::Method value, StringSpan valueURL)
{
    static constexpr const char* HeaderSpaceFinished = "HttpAsyncClientRequest header space is finished";

    SC_TRY_MSG(not headersSent, "Headers already sent");
    SC_TRY_MSG(responseHeaders.writtenBytes() == 0, "startRequest must be the first call");

    method = value;
    url    = valueURL.sizeInBytes() > 0 ? valueURL : StringSpan("/");

    switch (method)
    {
    case HttpParser::Method::HttpGET: SC_TRY(responseHeaders.appendLiteral("GET ", HeaderSpaceFinished)); break;
    case HttpParser::Method::HttpHEAD: SC_TRY(responseHeaders.appendLiteral("HEAD ", HeaderSpaceFinished)); break;
    case HttpParser::Method::HttpPUT: SC_TRY(responseHeaders.appendLiteral("PUT ", HeaderSpaceFinished)); break;
    case HttpParser::Method::HttpPOST: SC_TRY(responseHeaders.appendLiteral("POST ", HeaderSpaceFinished)); break;
    }

    SC_TRY(responseHeaders.append(url, HeaderSpaceFinished));
    SC_TRY(responseHeaders.appendLiteral(" HTTP/1.1\r\n", HeaderSpaceFinished));
    return Result(true);
}

Result HttpAsyncClientRequest::setExpectedBodyLength(uint64_t value)
{
    bodyType                       = value > 0 ? BodyType::Manual : BodyType::None;
    bodySpan                       = {};
    bodyStream                     = nullptr;
    bodyTransform                  = nullptr;
    multipartWriter                = nullptr;
    contentLength                  = value;
    contentEncoding                = HttpContentEncoding::Identity;
    chunkedTransferEncodingEnabled = false;
    return Result(true);
}

Result HttpAsyncClientRequest::setBody(Span<const char> value)
{
    bodyType                       = BodyType::Span;
    bodySpan                       = value;
    bodyStream                     = nullptr;
    bodyTransform                  = nullptr;
    contentLength                  = value.sizeInBytes();
    contentEncoding                = HttpContentEncoding::Identity;
    multipartWriter                = nullptr;
    chunkedTransferEncodingEnabled = false;
    return Result(true);
}

Result HttpAsyncClientRequest::setBody(AsyncReadableStream& stream)
{
    bodyType        = BodyType::Stream;
    bodySpan        = {};
    bodyStream      = &stream;
    bodyTransform   = nullptr;
    contentLength   = 0;
    contentEncoding = HttpContentEncoding::Identity;
    multipartWriter = nullptr;
    return setChunkedTransferEncoding();
}

void HttpAsyncClientRequest::setBody(AsyncReadableStream& stream, uint64_t contentLengthValue)
{
    bodyType                       = BodyType::Stream;
    bodySpan                       = {};
    bodyStream                     = &stream;
    bodyTransform                  = nullptr;
    contentLength                  = contentLengthValue;
    contentEncoding                = HttpContentEncoding::Identity;
    multipartWriter                = nullptr;
    chunkedTransferEncodingEnabled = false;
}

Result HttpAsyncClientRequest::setCompressedBody(AsyncReadableStream& stream, SyncZLibTransformStream& compressor,
                                                 HttpContentEncoding encoding)
{
    SC_TRY_MSG(encoding == HttpContentEncoding::GZip or encoding == HttpContentEncoding::Deflate,
               "HttpAsyncClientRequest compressed body requires gzip or deflate");

    bodyType        = BodyType::Stream;
    bodySpan        = {};
    bodyStream      = &stream;
    bodyTransform   = &compressor;
    contentLength   = 0;
    contentEncoding = encoding;
    multipartWriter = nullptr;

    const ZLibStream::Algorithm algorithm =
        encoding == HttpContentEncoding::GZip ? ZLibStream::CompressGZip : ZLibStream::CompressDeflate;
    SC_TRY(compressor.stream.init(algorithm));
    SC_TRY(HttpOutgoingMessage::addHeader("Content-Encoding", httpContentEncodingName(encoding)));
    return setChunkedTransferEncoding();
}

void HttpAsyncClientRequest::setMultipart(HttpMultipartWriter& value)
{
    bodyType                       = BodyType::Multipart;
    bodySpan                       = {};
    bodyStream                     = nullptr;
    bodyTransform                  = nullptr;
    multipartWriter                = &value;
    contentLength                  = value.getContentLength();
    contentEncoding                = HttpContentEncoding::Identity;
    chunkedTransferEncodingEnabled = false;
}

Result HttpAsyncClientRequest::sendHeaders(Function<void(AsyncBufferView::ID)> callback)
{
    static constexpr const char* HeaderSpaceFinished = "HttpAsyncClientRequest header space is finished";

    if (not hasHeader(KnownHeader::UserAgent))
    {
        SC_TRY(HttpOutgoingMessage::addHeader("User-Agent", "SC"));
    }
    if (defaultHost.sizeInBytes() > 0 and not hasHeader(KnownHeader::Host))
    {
        SC_TRY(HttpOutgoingMessage::addHeader("Host", defaultHost));
    }
    if (bodyType == BodyType::Multipart and not hasHeader(KnownHeader::ContentType))
    {
        SC_TRY_MSG(multipartWriter != nullptr, "HttpAsyncClientRequest multipart writer missing");
        SC_TRY_MSG(multipartWriter->getBoundary().sizeInBytes() > 0,
                   "HttpAsyncClientRequest multipart boundary missing");
        SC_TRY(responseHeaders.appendLiteral("Content-Type: multipart/form-data; boundary=", HeaderSpaceFinished));
        SC_TRY(responseHeaders.append(multipartWriter->getBoundary(), HeaderSpaceFinished));
        SC_TRY(responseHeaders.appendLiteral("\r\n", HeaderSpaceFinished));
        contentTypeAdded = true;
    }

    const bool hasBody = bodyType != BodyType::None;
    if (hasBody and not chunkedTransferEncodingEnabled and not hasHeader(KnownHeader::ContentLength))
    {
        SC_TRY(responseHeaders.appendContentLength(contentLength, HeaderSpaceFinished,
                                                   "HttpAsyncClientRequest failed formatting Content-Length"));
        contentLengthAdded = true;
    }

    userHeadersSentCallback = move(callback);
    if (userHeadersSentCallback.isValid() or internalHeadersSentCallback.isValid())
    {
        return HttpOutgoingMessage::sendHeaders({[this](AsyncBufferView::ID bufferID)
                                                 {
                                                     if (userHeadersSentCallback.isValid())
                                                     {
                                                         userHeadersSentCallback(bufferID);
                                                     }
                                                     if (internalHeadersSentCallback.isValid())
                                                     {
                                                         internalHeadersSentCallback(bufferID);
                                                     }
                                                 }});
    }
    return HttpOutgoingMessage::sendHeaders();
}

//-------------------------------------------------------------------------------------------------------
// HttpConnectionsPool
//-------------------------------------------------------------------------------------------------------
Result HttpConnectionsPool::init(SpanWithStride<HttpConnection> connectionsStorage)
{
    SC_TRY_MSG(numConnections == 0, "HttpConnectionsPool::init - numConnections != 0");
    for (size_t idx = 0; idx < connectionsStorage.sizeInElements(); ++idx)
    {
        HttpConnection& connection = connectionsStorage[idx];
        if (connection.getHeaderMemory().sizeInBytes() == 0)
        {
            return Result::Error("HttpConnection::headerMemory is empty");
        }
    }

    connections = connectionsStorage;
    return Result(true);
}

Result HttpConnectionsPool::close()
{
    SC_TRY_MSG(numConnections == 0, "HttpConnectionsPool::close - numConnections != 0");
    connections = {};
    return Result(true);
}

bool HttpConnectionsPool::activateNew(HttpConnection::ID& connectionID)
{
    for (size_t idx = 0; idx < connections.sizeInElements(); ++idx)
    {
        HttpConnection& connection = connections[idx];
        if (connection.state == HttpConnection::State::Inactive)
        {
            connection.state        = HttpConnection::State::Active;
            connectionID.index      = idx;
            connection.connectionID = connectionID;

            if (idx > highestActiveConnection)
            {
                highestActiveConnection = idx;
            }
            connection.request.setHeaderMemory(connection.getHeaderMemory());
            numConnections++;
            if (numConnections == connections.sizeInElements())
            {
                // avoid deadlock by force disabling keep-alive if this is the last connection
                // TODO: Consider some criteria that will disable keep alive after a threshold of active connections
                connection.response.forceDisableKeepAlive = true;
            }
            return true;
        }
    }
    return false;
}

bool HttpConnectionsPool::deactivate(HttpConnection::ID connectionID)
{
    HttpConnection& connection = connections[connectionID.index];
    if (connection.state == HttpConnection::State::Inactive)
    {
        return false;
    }

    connection.reset();
    numConnections--;

    if (numConnections == 0)
    {
        highestActiveConnection = 0;
    }
    else if (connectionID.index == highestActiveConnection)
    {
        while (highestActiveConnection > 0 and
               connections[highestActiveConnection].state == HttpConnection::State::Inactive)
        {
            highestActiveConnection--;
        }
    }
    return true;
}

Result HttpConnectionsPool::Memory::assignTo(HttpConnectionsPool::Configuration conf,
                                             SpanWithStride<HttpConnection>     connectionsSpan)
{
    const size_t numClients = connectionsSpan.sizeInElements();
    SC_TRY_MSG(allReadQueue.sizeInElements() >= numClients * conf.readQueueSize, "Insufficient read queue");
    SC_TRY_MSG(allWriteQueue.sizeInElements() >= numClients * conf.writeQueueSize, "Insufficient write queue");
    SC_TRY_MSG(allBuffers.sizeInElements() >= numClients * conf.buffersQueueSize, "Insufficient buffers queue");
    SC_TRY_MSG(allHeaders.sizeInElements() >= numClients * conf.headerBytesLength, "Insufficient headers storage");
    SC_TRY_MSG(allStreams.sizeInElements() >= numClients * conf.streamBytesLength, "Insufficient streams storage");
    for (size_t idx = 0; idx < numClients; ++idx)
    {
        HttpConnection& connection = connectionsSpan[idx];

        const size_t NumSlices   = conf.readQueueSize;
        const size_t SliceLength = conf.streamBytesLength / NumSlices;

        Span<AsyncBufferView> buffers;
        SC_TRY(allBuffers.sliceStartLength(idx * conf.buffersQueueSize, conf.buffersQueueSize, buffers));
        Span<char> streamStorage;
        SC_TRY(allStreams.sliceStartLength(idx * conf.streamBytesLength, conf.streamBytesLength, streamStorage));
        for (size_t sliceIdx = 0; sliceIdx < NumSlices; ++sliceIdx)
        {
            Span<char> slice;
            SC_TRY(streamStorage.sliceStartLength(sliceIdx * SliceLength, SliceLength, slice));
            buffers[sliceIdx] = slice;
            buffers[sliceIdx].setReusable(true);
        }
        connection.buffersPool.setBuffers(buffers);
        Span<char> headerStorage;
        SC_TRY(allHeaders.sliceStartLength(idx * conf.headerBytesLength, conf.headerBytesLength, headerStorage));
        connection.setHeaderMemory(headerStorage);
        Span<AsyncReadableStream::Request> readQueue;
        SC_TRY(allReadQueue.sliceStartLength(idx * conf.readQueueSize, conf.readQueueSize, readQueue));
        Span<AsyncWritableStream::Request> writeQueue;
        SC_TRY(allWriteQueue.sliceStartLength(idx * conf.writeQueueSize, conf.writeQueueSize, writeQueue));
        connection.readableSocketStream.setReadQueue(readQueue);
        connection.writableSocketStream.setWriteQueue(writeQueue);
    }
    return Result(true);
}
} // namespace SC
