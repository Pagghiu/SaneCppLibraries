// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "HttpAsyncClient.h"
#include "HttpURLParser.h"
#include "Internal/HttpStringIterator.h"

#include "../Foundation/Assert.h"

#include <string.h>

namespace SC
{
Result HttpAsyncClient::init(HttpConnectionBase& storage)
{
    connection = &storage;
    connection->readableSocketStream.setAutoDestroy(false);
    connection->writableSocketStream.setAutoDestroy(false);
    connectAsync.callback.bind<HttpAsyncClient, &HttpAsyncClient::onConnected>(*this);
    return Result(true);
}

Result HttpAsyncClient::close()
{
    response.failBodyStream(Result::Error("HttpAsyncClient closed"));
    response.abortBodyStream();
    closeConnection();
    state          = State::Idle;
    currentRequest = nullptr;
    request.reset();
    return Result(true);
}

Result HttpAsyncClient::start(AsyncEventLoop& loop, HttpParser::Method method, StringSpan url, bool keepAlive)
{
    RequestPreset preset;
    preset.method    = method;
    preset.url       = url;
    preset.keepAlive = keepAlive;
    preset.autoSend  = false;
    return startRequest(loop, preset);
}

Result HttpAsyncClient::get(AsyncEventLoop& loop, StringSpan url, bool keepAlive)
{
    RequestPreset preset;
    preset.method    = HttpParser::Method::HttpGET;
    preset.url       = url;
    preset.keepAlive = keepAlive;
    preset.autoSend  = true;
    return startRequest(loop, preset);
}

Result HttpAsyncClient::put(AsyncEventLoop& loop, StringSpan url, Span<const char> body, bool keepAlive)
{
    RequestPreset preset;
    preset.method    = HttpParser::Method::HttpPUT;
    preset.url       = url;
    preset.keepAlive = keepAlive;
    preset.autoSend  = true;
    preset.bodyMode  = RequestPreset::BodyMode::Span;
    preset.bodySpan  = body;
    return startRequest(loop, preset);
}

Result HttpAsyncClient::post(AsyncEventLoop& loop, StringSpan url, Span<const char> body, bool keepAlive)
{
    RequestPreset preset;
    preset.method    = HttpParser::Method::HttpPOST;
    preset.url       = url;
    preset.keepAlive = keepAlive;
    preset.autoSend  = true;
    preset.bodyMode  = RequestPreset::BodyMode::Span;
    preset.bodySpan  = body;
    return startRequest(loop, preset);
}

Result HttpAsyncClient::postMultipart(AsyncEventLoop& loop, StringSpan url, HttpMultipartWriter& writer, bool keepAlive)
{
    RequestPreset preset;
    preset.method          = HttpParser::Method::HttpPOST;
    preset.url             = url;
    preset.keepAlive       = keepAlive;
    preset.autoSend        = true;
    preset.bodyMode        = RequestPreset::BodyMode::Multipart;
    preset.multipartWriter = &writer;
    return startRequest(loop, preset);
}

Result HttpAsyncClient::startRequest(AsyncEventLoop& loop, const RequestPreset& preset)
{
    SC_TRY_MSG(connection != nullptr, "HttpAsyncClient::start init not called");

    if (state != State::Idle and response.isBodyComplete() and not responseFinalized)
    {
        finalizeResponse(false);
    }

    SC_TRY_MSG(state == State::Idle, "HttpAsyncClient::start another request is in progress");
    eventLoop = &loop;
    return startPreparedRequest(preset);
}

Result HttpAsyncClient::startPreparedRequest(const RequestPreset& preset)
{
    currentPreset     = preset;
    currentRequest    = &request;
    responseDelivered = false;
    responseFinalized = false;

    SC_TRY(prepareRequest(currentPreset));
    SC_TRY(ensureConnected());
    return Result(true);
}

Result HttpAsyncClient::prepareRequest(const RequestPreset& preset)
{
    response.reset(connection->getHeaderMemory());
    SC_TRY(
        response.initBodyStream(connection->buffersPool, {[this]() -> Result { return onResponseBodyStreamRead(); }}));

    SC_TRY(currentURL.parse(preset.url));
    SC_TRY_MSG(currentURL.protocol == "http", "HttpAsyncClient only supports http URLs");
    SC_TRY_MSG(currentURL.username.isEmpty() and currentURL.password.isEmpty(),
               "HttpAsyncClient userinfo not supported");
    return Result(true);
}

bool HttpAsyncClient::canReuseConnectionFor(StringSpan host, uint16_t port) const
{
    return hasOpenConnection and currentPort == port and currentHost == host;
}

Result HttpAsyncClient::ensureConnected()
{
    if (canReuseConnectionFor(currentURL.host, currentURL.port))
    {
        SC_TRY(connection->writableSocketStream.init(connection->buffersPool, *eventLoop, connection->socket));
        SC_TRY(beginResponseRead());
        return beginRequestSend();
    }

    closeConnection();
    return beginSocketConnection();
}

Result HttpAsyncClient::beginSocketConnection()
{
    char       addressBuffer[256];
    Span<char> ipAddress = {addressBuffer};
    SC_TRY(SocketDNS::resolveDNS(currentURL.hostname, ipAddress));

    SocketIPAddress remoteAddress;
    SC_TRY(remoteAddress.fromAddressPort({ipAddress, true, StringEncoding::Ascii}, currentURL.port));
    SC_TRY(eventLoop->createAsyncTCPSocket(remoteAddress.getAddressFamily(), connection->socket));

    state = State::Connecting;
    return connectAsync.start(*eventLoop, connection->socket, remoteAddress);
}

void HttpAsyncClient::onConnected(AsyncSocketConnect::Result& result)
{
    if (not result.isValid())
    {
        fail(result.isValid());
        return;
    }

    Result readInit = connection->readableSocketStream.init(connection->buffersPool, *eventLoop, connection->socket);
    if (not readInit)
    {
        fail(readInit);
        return;
    }
    Result writeInit = connection->writableSocketStream.init(connection->buffersPool, *eventLoop, connection->socket);
    if (not writeInit)
    {
        fail(writeInit);
        return;
    }

    const bool addedReadableError =
        connection->readableSocketStream.eventError.addListener<HttpAsyncClient, &HttpAsyncClient::onReadableError>(
            *this);
    const bool addedWritableError =
        connection->writableSocketStream.eventError.addListener<HttpAsyncClient, &HttpAsyncClient::onWritableError>(
            *this);
    const bool addedReadableEnd =
        connection->readableSocketStream.eventEnd.addListener<HttpAsyncClient, &HttpAsyncClient::onReadableEnd>(*this);
    const bool addedPipelineError =
        connection->pipeline.eventError.addListener<HttpAsyncClient, &HttpAsyncClient::onPipelineError>(*this);
    SC_ASSERT_RELEASE(addedReadableError);
    SC_ASSERT_RELEASE(addedWritableError);
    SC_ASSERT_RELEASE(addedReadableEnd);
    SC_ASSERT_RELEASE(addedPipelineError);

    const size_t hostLen = currentURL.host.sizeInBytes();
    if (hostLen >= sizeof(currentHostStorage))
    {
        fail(Result::Error("HttpAsyncClient host too long"));
        return;
    }
    ::memset(currentHostStorage, 0, sizeof(currentHostStorage));
    ::memcpy(currentHostStorage, currentURL.host.bytesWithoutTerminator(), hostLen);
    currentHost = StringSpan::fromNullTerminated(currentHostStorage, StringEncoding::Ascii);
    currentPort = currentURL.port;

    hasOpenConnection = true;

    Result responseStart = beginResponseRead();
    if (not responseStart)
    {
        fail(responseStart);
        return;
    }
    Result sendResult = beginRequestSend();
    if (not sendResult)
    {
        fail(sendResult);
    }
}

Result HttpAsyncClient::beginResponseRead()
{
    const bool addedResponseData =
        connection->readableSocketStream.eventData.addListener<HttpAsyncClient, &HttpAsyncClient::onResponseData>(
            *this);
    SC_TRY_MSG(addedResponseData, "HttpAsyncClient failed to register response listener");
    if (requestCount == 0)
    {
        SC_TRY(connection->readableSocketStream.start());
    }
    else
    {
        connection->readableSocketStream.resumeReading();
    }
    state = State::Sending;
    return Result(true);
}

Result HttpAsyncClient::beginRequestSend()
{
    request.setWritableStream(connection->writableSocketStream);
    request.setHeaderMemory(connection->getHeaderMemory());
    request.reset();
    request.setKeepAlive(currentPreset.keepAlive);
    request.setDefaultHost(currentURL.host);
    request.internalHeadersSentCallback = {[this](AsyncBufferView::ID bufferID) { onHeadersBufferWritten(bufferID); }};

    SC_TRY(request.startRequest(currentPreset.method, currentURL.path));

    switch (currentPreset.bodyMode)
    {
    case RequestPreset::BodyMode::None: break;
    case RequestPreset::BodyMode::Span: SC_TRY(request.setBody(currentPreset.bodySpan)); break;
    case RequestPreset::BodyMode::Stream:
        SC_TRY_MSG(currentPreset.bodyStream != nullptr, "HttpAsyncClient body stream missing");
        request.setBody(*currentPreset.bodyStream, currentPreset.contentLength);
        break;
    case RequestPreset::BodyMode::Multipart:
        SC_TRY_MSG(currentPreset.multipartWriter != nullptr, "HttpAsyncClient multipart writer missing");
        request.setMultipart(*currentPreset.multipartWriter);
        break;
    }

    if (onPrepareRequest.isValid())
    {
        onPrepareRequest(request);
    }

    SC_TRY(validateActiveRequest());

    if (currentPreset.autoSend)
    {
        if (not request.hasSentHeaders())
        {
            SC_TRY(request.sendHeaders());
        }
        if (request.getBodyType() == HttpAsyncClientRequest::BodyType::None and not request.hasEnded())
        {
            SC_TRY(request.end());
        }
    }
    else
    {
        SC_TRY_MSG(request.hasSentHeaders(), "HttpAsyncClient request headers not sent");
    }

    state = State::WaitingResponse;
    return Result(true);
}

Result HttpAsyncClient::validateActiveRequest() const
{
    if (request.hasTransferEncodingHeader() and not request.usesChunkedTransferEncoding())
    {
        return Result::Error("HttpAsyncClient does not support Transfer-Encoding request headers");
    }

    if (request.getBodyType() == HttpAsyncClientRequest::BodyType::Stream)
    {
        SC_TRY_MSG(request.getBodyStream() != nullptr, "HttpAsyncClient body stream missing");
        SC_TRY_MSG(&request.getBodyStream()->getBuffersPool() == &connection->buffersPool,
                   "HttpAsyncClient body stream must use the client buffers pool");
    }
    if (request.getBodyType() == HttpAsyncClientRequest::BodyType::Multipart)
    {
        SC_TRY_MSG(request.getMultipartWriter() != nullptr, "HttpAsyncClient multipart writer missing");
        SC_TRY_MSG(request.getMultipartWriter()->getBoundary().sizeInBytes() > 0,
                   "HttpAsyncClient multipart boundary missing");
    }
    return Result(true);
}

Result HttpAsyncClient::onResponseBodyStreamRead()
{
    if (state == State::StreamingResponse and not response.isBodyComplete())
    {
        connection->readableSocketStream.resumeReading();
    }
    return Result(true);
}

void HttpAsyncClient::onHeadersBufferWritten(AsyncBufferView::ID)
{
    if (currentRequest == nullptr)
    {
        return;
    }
    if (currentRequest->getBodyType() == HttpAsyncClientRequest::BodyType::Stream)
    {
        connection->pipeline.source   = currentRequest->getBodyStream();
        connection->pipeline.sinks[0] = &currentRequest->getWritableStream();

        Result res = connection->pipeline.pipe();
        if (res)
        {
            res = connection->pipeline.start();
        }
        if (not res)
        {
            fail(res);
        }
    }
    else if (currentRequest->getBodyType() == HttpAsyncClientRequest::BodyType::Span)
    {
        Result writeResult = connection->writableSocketStream.write(
            AsyncBufferView(currentRequest->getBodySpan()),
            {[this](AsyncBufferView::ID) { connection->writableSocketStream.end(); }});
        if (not writeResult)
        {
            fail(writeResult);
        }
    }
    else if (currentRequest->getBodyType() == HttpAsyncClientRequest::BodyType::Multipart)
    {
        static constexpr const char* HeaderSpaceFinished = "HttpAsyncClient header space is finished";

        const HttpMultipartWriter& writer = *currentRequest->getMultipartWriter();
        HttpFixedBufferWriter      bodyWriter;
        bodyWriter.reset(connection->getHeaderMemory());
        const auto appendMultipartBody = [&]() -> Result
        {
            for (size_t idx = 0; idx < writer.getNumParts(); ++idx)
            {
                const HttpMultipartWriter::Part& part = writer.getPart(idx);
                SC_TRY(bodyWriter.appendLiteral("--", HeaderSpaceFinished));
                SC_TRY(bodyWriter.append(writer.getBoundary(), HeaderSpaceFinished));
                SC_TRY(bodyWriter.appendLiteral("\r\nContent-Disposition: form-data; name=\"", HeaderSpaceFinished));
                SC_TRY(bodyWriter.append(part.partName, HeaderSpaceFinished));
                SC_TRY(bodyWriter.appendLiteral("\"", HeaderSpaceFinished));
                if (part.fileName.sizeInBytes() > 0)
                {
                    SC_TRY(bodyWriter.appendLiteral("; filename=\"", HeaderSpaceFinished));
                    SC_TRY(bodyWriter.append(part.fileName, HeaderSpaceFinished));
                    SC_TRY(bodyWriter.appendLiteral("\"", HeaderSpaceFinished));
                }
                SC_TRY(bodyWriter.appendLiteral("\r\n", HeaderSpaceFinished));
                if (part.contentType.sizeInBytes() > 0)
                {
                    SC_TRY(bodyWriter.appendLiteral("Content-Type: ", HeaderSpaceFinished));
                    SC_TRY(bodyWriter.append(part.contentType, HeaderSpaceFinished));
                    SC_TRY(bodyWriter.appendLiteral("\r\n", HeaderSpaceFinished));
                }
                SC_TRY(bodyWriter.appendLiteral("\r\n", HeaderSpaceFinished));
                SC_TRY(bodyWriter.append(part.body, HeaderSpaceFinished));
                SC_TRY(bodyWriter.appendLiteral("\r\n", HeaderSpaceFinished));
            }
            SC_TRY(bodyWriter.appendLiteral("--", HeaderSpaceFinished));
            SC_TRY(bodyWriter.append(writer.getBoundary(), HeaderSpaceFinished));
            SC_TRY(bodyWriter.appendLiteral("--\r\n", HeaderSpaceFinished));
            return Result(true);
        };

        const Result res = appendMultipartBody();
        if (not res)
        {
            fail(res);
            return;
        }

        Result writeResult = connection->writableSocketStream.write(
            AsyncBufferView(bodyWriter.written()),
            {[this](AsyncBufferView::ID) { connection->writableSocketStream.end(); }});
        if (not writeResult)
        {
            fail(writeResult);
        }
    }
}

bool HttpAsyncClient::responseMustNotHaveBody() const
{
    if (response.getParser().statusCode == 204 or response.getParser().statusCode == 304)
    {
        return true;
    }
    return response.getParser().statusCode >= 100 and response.getParser().statusCode < 200;
}

bool HttpAsyncClient::responseHasKnownLength() const
{
    StringSpan contentLength;
    return response.getHeader("Content-Length", contentLength);
}

void HttpAsyncClient::onResponseData(AsyncBufferView::ID bufferID)
{
    Span<const char> readData;
    Result           readable = connection->buffersPool.getReadableData(bufferID, readData);
    if (not readable)
    {
        fail(readable);
        return;
    }

    Result parseRes = response.writeHeaders(static_cast<uint32_t>(connection->getHeaderMemory().sizeInBytes()),
                                            readData, connection->readableSocketStream, bufferID);
    if (not parseRes)
    {
        fail(parseRes);
        return;
    }
    if (not response.hasReceivedHeaders())
    {
        return;
    }

    const bool removed =
        connection->readableSocketStream.eventData.removeListener<HttpAsyncClient, &HttpAsyncClient::onResponseData>(
            *this);
    SC_COMPILER_UNUSED(removed);

    if (response.getParser().statusCode < 200 and not responseMustNotHaveBody())
    {
        fail(Result::Error("HttpAsyncClient does not support informational responses"));
        return;
    }

    Result prepareBody(true);
    if (responseMustNotHaveBody())
    {
        response.setBodyBytesRemaining(0);
        response.setBodyFramingKind(HttpBodyFramingKind::None);
        response.setBodyComplete(true);
        prepareBody = response.initBodyStream(connection->buffersPool,
                                              {[this]() -> Result { return onResponseBodyStreamRead(); }});
    }
    else
    {
        prepareBody = response.prepareBodyStream(connection->buffersPool,
                                                 {[this]() -> Result { return onResponseBodyStreamRead(); }}, true);
    }
    if (not prepareBody)
    {
        fail(prepareBody);
        return;
    }

    const size_t bufferedBodyBytes = readData.sizeInBytes() - response.getHeadersLength();
    SC_COMPILER_UNUSED(bufferedBodyBytes);

    if (not responseDelivered and onResponse.isValid())
    {
        responseDelivered = true;
        onResponse(response);
    }

    Result startBodyStream = response.startBodyStream();
    if (not startBodyStream)
    {
        fail(startBodyStream);
        return;
    }

    if (responseMustNotHaveBody() and bufferedBodyBytes > 0)
    {
        fail(Result::Error("HttpAsyncClient received an unexpected response body"));
        return;
    }

    if (bufferedBodyBytes > 0 and not responseMustNotHaveBody())
    {
        AsyncBufferView::ID bufferedBodyID;
        Result              childView = connection->buffersPool.createChildView(bufferID, response.getHeadersLength(),
                                                                                bufferedBodyBytes, bufferedBodyID);
        if (not childView)
        {
            fail(childView);
            return;
        }
        Result processBuffered =
            response.processBodyData(connection->readableSocketStream, bufferedBodyID,
                                     {readData.data() + response.getHeadersLength(), bufferedBodyBytes}, false);
        connection->buffersPool.unrefBuffer(bufferedBodyID);
        if (not processBuffered)
        {
            fail(processBuffered);
            return;
        }
    }

    if (response.isBodyComplete())
    {
        finishResponse();
        return;
    }

    const bool addedBodyData =
        connection->readableSocketStream.eventData.addListener<HttpAsyncClient, &HttpAsyncClient::onResponseBodyData>(
            *this);
    SC_ASSERT_RELEASE(addedBodyData);
    state = State::StreamingResponse;
}

void HttpAsyncClient::onResponseBodyData(AsyncBufferView::ID bufferID)
{
    Span<const char> readData;
    Result           readable = connection->buffersPool.getReadableData(bufferID, readData);
    if (not readable)
    {
        fail(readable);
        return;
    }

    Result process = response.processBodyData(connection->readableSocketStream, bufferID, readData, false);
    if (not process)
    {
        fail(process);
        return;
    }
    if (response.isBodyComplete())
    {
        const bool removed = connection->readableSocketStream.eventData
                                 .removeListener<HttpAsyncClient, &HttpAsyncClient::onResponseBodyData>(*this);
        SC_COMPILER_UNUSED(removed);
        finishResponse();
    }
}

void HttpAsyncClient::finishResponse() { finalizeResponse(true); }

void HttpAsyncClient::finalizeResponse(bool shouldFinishBodyStream)
{
    if (responseFinalized)
    {
        return;
    }
    responseFinalized = true;

    requestCount++;

    const bool keepAlive = currentRequest != nullptr and currentRequest->getKeepAlive() and response.getKeepAlive() and
                           response.getBodyFramingKind() != HttpBodyFramingKind::CloseDelimited;

    if (keepAlive)
    {
        connection->readableSocketStream.pause();
    }
    else
    {
        hasOpenConnection = false;
        currentHost       = {};
        currentPort       = 0;
    }

    state          = State::Idle;
    currentRequest = nullptr;

    if (shouldFinishBodyStream)
    {
        response.finishBodyStream();
    }

    if (not keepAlive and state == State::Idle and currentRequest == nullptr)
    {
        closeConnection();
    }
}

void HttpAsyncClient::closeConnection()
{
    if (connection == nullptr)
    {
        return;
    }
    (void)connection->readableSocketStream.eventData.removeListener<HttpAsyncClient, &HttpAsyncClient::onResponseData>(
        *this);
    (void)connection->readableSocketStream.eventData
        .removeListener<HttpAsyncClient, &HttpAsyncClient::onResponseBodyData>(*this);
    (void)connection->readableSocketStream.eventError
        .removeListener<HttpAsyncClient, &HttpAsyncClient::onReadableError>(*this);
    (void)connection->writableSocketStream.eventError
        .removeListener<HttpAsyncClient, &HttpAsyncClient::onWritableError>(*this);
    (void)connection->readableSocketStream.eventEnd.removeListener<HttpAsyncClient, &HttpAsyncClient::onReadableEnd>(
        *this);
    (void)connection->pipeline.eventError.removeListener<HttpAsyncClient, &HttpAsyncClient::onPipelineError>(*this);
    (void)connection->pipeline.unpipe();
    connection->readableSocketStream.destroy();
    connection->writableSocketStream.destroy();
    if (connection->socket.isValid())
    {
        (void)connection->socket.close();
    }
    hasOpenConnection = false;
    currentHost       = {};
    currentPort       = 0;
}

void HttpAsyncClient::fail(Result error)
{
    response.failBodyStream(error);
    response.abortBodyStream();
    closeConnection();
    state          = State::Idle;
    currentRequest = nullptr;
    if (onError.isValid())
    {
        onError(error);
    }
}

void HttpAsyncClient::onReadableError(Result result) { fail(result); }
void HttpAsyncClient::onWritableError(Result result) { fail(result); }
void HttpAsyncClient::onPipelineError(Result result) { fail(result); }

void HttpAsyncClient::onReadableEnd()
{
    if (state == State::Idle)
    {
        closeConnection();
        return;
    }
    if (state == State::StreamingResponse and response.getBodyFramingKind() == HttpBodyFramingKind::CloseDelimited and
        response.hasReceivedHeaders())
    {
        response.finishBodyStream();
        finishResponse();
    }
    else if (response.isBodyComplete() and response.hasReceivedHeaders())
    {
        finishResponse();
    }
    else
    {
        fail(Result::Error("HttpAsyncClient disconnected before response completed"));
    }
}

} // namespace SC
