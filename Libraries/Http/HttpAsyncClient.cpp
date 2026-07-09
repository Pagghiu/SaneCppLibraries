// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "HttpAsyncClient.h"
#include "HttpURLParser.h"
#include "HttpWebSocket.h"
#include "Internal/HttpStringIterator.h"

#include "../AsyncStreams/ZLibTransformStreams.h"

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
    detachResponseDecompression();
    response.failBodyStream(Result::Error("HttpAsyncClient closed"));
    response.abortBodyStream();
    closeConnection();
    state             = State::Idle;
    currentRequest    = nullptr;
    webSocketUpgraded = false;
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

Result HttpAsyncClient::sendRequest(AsyncEventLoop& loop, const RequestOptions& options)
{
    RequestPreset preset;
    preset.method    = options.method;
    preset.url       = options.url;
    preset.keepAlive = options.keepAlive;
    preset.autoSend  = true;
    preset.headers   = options.headers;

    switch (options.bodyMode)
    {
    case RequestOptions::BodyMode::None: break;
    case RequestOptions::BodyMode::Span:
        preset.bodyMode = RequestPreset::BodyMode::Span;
        preset.bodySpan = options.body;
        break;
    case RequestOptions::BodyMode::Stream:
        SC_TRY_MSG(options.bodyStream != nullptr, "HttpAsyncClient RequestOptions body stream missing");
        preset.bodyMode      = RequestPreset::BodyMode::Stream;
        preset.bodyStream    = options.bodyStream;
        preset.contentLength = options.bodyLength;
        break;
    case RequestOptions::BodyMode::Multipart:
        SC_TRY_MSG(options.multipartWriter != nullptr, "HttpAsyncClient RequestOptions multipart writer missing");
        preset.bodyMode        = RequestPreset::BodyMode::Multipart;
        preset.multipartWriter = options.multipartWriter;
        break;
    }

    return startRequest(loop, preset);
}

Result HttpAsyncClient::get(AsyncEventLoop& loop, StringSpan url, bool keepAlive)
{
    RequestOptions options;
    options.method    = HttpParser::Method::HttpGET;
    options.url       = url;
    options.keepAlive = keepAlive;
    return sendRequest(loop, options);
}

Result HttpAsyncClient::head(AsyncEventLoop& loop, StringSpan url, bool keepAlive)
{
    RequestOptions options;
    options.method    = HttpParser::Method::HttpHEAD;
    options.url       = url;
    options.keepAlive = keepAlive;
    return sendRequest(loop, options);
}

Result HttpAsyncClient::options(AsyncEventLoop& loop, StringSpan url, bool keepAlive)
{
    RequestOptions options;
    options.method    = HttpParser::Method::HttpOPTIONS;
    options.url       = url;
    options.keepAlive = keepAlive;
    return sendRequest(loop, options);
}

Result HttpAsyncClient::deleteRequest(AsyncEventLoop& loop, StringSpan url, bool keepAlive)
{
    RequestOptions options;
    options.method    = HttpParser::Method::HttpDELETE;
    options.url       = url;
    options.keepAlive = keepAlive;
    return sendRequest(loop, options);
}

Result HttpAsyncClient::put(AsyncEventLoop& loop, StringSpan url, Span<const char> body, bool keepAlive)
{
    RequestOptions options;
    options.method    = HttpParser::Method::HttpPUT;
    options.url       = url;
    options.keepAlive = keepAlive;
    options.bodyMode  = RequestOptions::BodyMode::Span;
    options.body      = body;
    return sendRequest(loop, options);
}

Result HttpAsyncClient::post(AsyncEventLoop& loop, StringSpan url, Span<const char> body, bool keepAlive)
{
    RequestOptions options;
    options.method    = HttpParser::Method::HttpPOST;
    options.url       = url;
    options.keepAlive = keepAlive;
    options.bodyMode  = RequestOptions::BodyMode::Span;
    options.body      = body;
    return sendRequest(loop, options);
}

Result HttpAsyncClient::patch(AsyncEventLoop& loop, StringSpan url, Span<const char> body, bool keepAlive)
{
    RequestOptions options;
    options.method    = HttpParser::Method::HttpPATCH;
    options.url       = url;
    options.keepAlive = keepAlive;
    options.bodyMode  = RequestOptions::BodyMode::Span;
    options.body      = body;
    return sendRequest(loop, options);
}

Result HttpAsyncClient::postMultipart(AsyncEventLoop& loop, StringSpan url, HttpMultipartWriter& writer, bool keepAlive)
{
    RequestOptions options;
    options.method          = HttpParser::Method::HttpPOST;
    options.url             = url;
    options.keepAlive       = keepAlive;
    options.bodyMode        = RequestOptions::BodyMode::Multipart;
    options.multipartWriter = &writer;
    return sendRequest(loop, options);
}

Result HttpAsyncClient::startRequest(AsyncEventLoop& loop, const RequestPreset& preset)
{
    SC_TRY_MSG(connection != nullptr, "HttpAsyncClient::start init not called");
    SC_TRY_MSG(not webSocketUpgraded, "HttpAsyncClient connection is upgraded to WebSocket");

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

Result HttpAsyncClient::detachWebSocketTransport(HttpWebSocketTransportView& transport)
{
    SC_TRY_MSG(connection != nullptr, "HttpAsyncClient::detachWebSocketTransport init not called");
    SC_TRY_MSG(response.hasReceivedHeaders(), "HttpAsyncClient::detachWebSocketTransport response headers missing");
    SC_TRY_MSG(response.getParser().statusCode == 101, "HttpAsyncClient::detachWebSocketTransport expected 101");

    transport.readableStream = &connection->getReadableTransportStream();
    transport.writableStream = &connection->getWritableTransportStream();
    transport.buffersPool    = &connection->buffersPool;

    (void)connection->getReadableTransportStream()
        .eventData.removeListener<HttpAsyncClient, &HttpAsyncClient::onResponseData>(*this);
    (void)connection->getReadableTransportStream()
        .eventData.removeListener<HttpAsyncClient, &HttpAsyncClient::onResponseBodyData>(*this);
    (void)connection->getReadableTransportStream()
        .eventError.removeListener<HttpAsyncClient, &HttpAsyncClient::onReadableError>(*this);
    (void)connection->getWritableTransportStream()
        .eventError.removeListener<HttpAsyncClient, &HttpAsyncClient::onWritableError>(*this);
    (void)connection->getReadableTransportStream()
        .eventEnd.removeListener<HttpAsyncClient, &HttpAsyncClient::onReadableEnd>(*this);
    (void)connection->pipeline.eventError.removeListener<HttpAsyncClient, &HttpAsyncClient::onPipelineError>(*this);
    (void)connection->pipeline.unpipe();

    hasOpenConnection = false;
    currentProtocol   = {};
    currentHost       = {};
    currentPort       = 0;
    webSocketUpgraded = true;
    return Result(true);
}

Result HttpAsyncClient::prepareRequest(const RequestPreset& preset)
{
    response.reset(connection->getHeaderMemory());
    SC_TRY(
        response.initBodyStream(connection->buffersPool, {[this]() -> Result { return onResponseBodyStreamRead(); }}));

    SC_TRY(currentURL.parse(preset.url));
    const bool isHttp  = HttpStringIterator::equalsIgnoreCase(currentURL.protocol, "http");
    const bool isHttps = HttpStringIterator::equalsIgnoreCase(currentURL.protocol, "https");
    SC_TRY_MSG(isHttp or isHttps, "HttpAsyncClient only supports http and https URLs");
    SC_TRY_MSG(currentURL.username.isEmpty() and currentURL.password.isEmpty(),
               "HttpAsyncClient userinfo not supported");
    return Result(true);
}

bool HttpAsyncClient::canReuseConnectionFor(StringSpan protocol, StringSpan host, uint16_t port) const
{
    return hasOpenConnection and currentPort == port and currentProtocol == protocol and currentHost == host;
}

Result HttpAsyncClient::ensureConnected()
{
    if (canReuseConnectionFor(currentURL.protocol, currentURL.host, currentURL.port))
    {
        if (&connection->getWritableTransportStream() == &connection->writableSocketStream)
        {
            SC_TRY(connection->writableSocketStream.init(connection->buffersPool, *eventLoop, connection->socket));
        }
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
    connection->resetTransportStreams();

    if (transportSetup.isValid())
    {
        HttpAsyncClientTransportSetup setup;
        setup.connection = connection;
        setup.eventLoop  = eventLoop;
        setup.url        = &currentURL;
        setup.complete   = {[this](Result setupResult) { completeTransportSetup(setupResult); }};

        Result nativeSocket =
            connection->socket.get(setup.nativeSocket, Result::Error("HttpAsyncClient invalid socket"));
        if (not nativeSocket)
        {
            fail(nativeSocket);
            return;
        }

        Result setupResult = transportSetup(setup);
        if (not setupResult)
        {
            fail(setupResult);
        }
        return;
    }

    if (HttpStringIterator::equalsIgnoreCase(currentURL.protocol, "https"))
    {
        fail(Result::Error("HttpAsyncClient HTTPS transport not configured"));
        return;
    }

    completeTransportSetup(Result(true));
}

Result HttpAsyncClient::rememberConnectedOrigin()
{
    const size_t protocolLen = currentURL.protocol.sizeInBytes();
    SC_TRY_MSG(protocolLen < sizeof(currentProtocolStorage), "HttpAsyncClient protocol too long");

    const size_t hostLen = currentURL.host.sizeInBytes();
    SC_TRY_MSG(hostLen < sizeof(currentHostStorage), "HttpAsyncClient host too long");

    ::memset(currentProtocolStorage, 0, sizeof(currentProtocolStorage));
    ::memcpy(currentProtocolStorage, currentURL.protocol.bytesWithoutTerminator(), protocolLen);
    currentProtocol = StringSpan::fromNullTerminated(currentProtocolStorage, StringEncoding::Ascii);

    ::memset(currentHostStorage, 0, sizeof(currentHostStorage));
    ::memcpy(currentHostStorage, currentURL.host.bytesWithoutTerminator(), hostLen);
    currentHost = StringSpan::fromNullTerminated(currentHostStorage, StringEncoding::Ascii);
    currentPort = currentURL.port;
    return Result(true);
}

void HttpAsyncClient::completeTransportSetup(Result result)
{
    if (state != State::Connecting or connection == nullptr)
    {
        return;
    }
    if (not result)
    {
        fail(result);
        return;
    }

    const bool addedReadableError =
        connection->getReadableTransportStream()
            .eventError.addListener<HttpAsyncClient, &HttpAsyncClient::onReadableError>(*this);
    const bool addedWritableError =
        connection->getWritableTransportStream()
            .eventError.addListener<HttpAsyncClient, &HttpAsyncClient::onWritableError>(*this);
    const bool addedReadableEnd =
        connection->getReadableTransportStream().eventEnd.addListener<HttpAsyncClient, &HttpAsyncClient::onReadableEnd>(
            *this);
    const bool addedPipelineError =
        connection->pipeline.eventError.addListener<HttpAsyncClient, &HttpAsyncClient::onPipelineError>(*this);
    SC_HTTP_ASSERT_RELEASE(addedReadableError);
    SC_HTTP_ASSERT_RELEASE(addedWritableError);
    SC_HTTP_ASSERT_RELEASE(addedReadableEnd);
    SC_HTTP_ASSERT_RELEASE(addedPipelineError);

    Result origin = rememberConnectedOrigin();
    if (not origin)
    {
        fail(origin);
        return;
    }

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
    const bool addedResponseData = connection->getReadableTransportStream()
                                       .eventData.addListener<HttpAsyncClient, &HttpAsyncClient::onResponseData>(*this);
    SC_TRY_MSG(addedResponseData, "HttpAsyncClient failed to register response listener");
    if (connection->getReadableTransportStream().canStart())
    {
        SC_TRY(connection->getReadableTransportStream().start());
    }
    else
    {
        connection->getReadableTransportStream().resumeReading();
    }
    state = State::Sending;
    return Result(true);
}

Result HttpAsyncClient::beginRequestSend()
{
    request.setWritableStream(connection->getWritableTransportStream());
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

    for (const Header& header : currentPreset.headers)
    {
        SC_TRY(request.addHeader(header.name, header.value));
    }

    if (responseDecoder != nullptr and not request.hasAcceptEncodingHeader())
    {
        SC_TRY(request.addHeader("Accept-Encoding", "gzip, deflate"));
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
        if (request.getBodyTransform() != nullptr)
        {
            SC_TRY_MSG(&request.getBodyTransform()->AsyncReadableStream::getBuffersPool() == &connection->buffersPool,
                       "HttpAsyncClient body transform readable side must use the client buffers pool");
            SC_TRY_MSG(&request.getBodyTransform()->AsyncWritableStream::getBuffersPool() == &connection->buffersPool,
                       "HttpAsyncClient body transform writable side must use the client buffers pool");
        }
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
        connection->getReadableTransportStream().resumeReading();
    }
    return Result(true);
}

Result HttpAsyncClient::prepareResponseDecompression()
{
    responseDecoderActive = false;
    if (responseDecoder == nullptr or response.getBodyFramingKind() == HttpBodyFramingKind::None)
    {
        return Result(true);
    }

    StringSpan contentEncodingHeader;
    if (not response.getHeader("Content-Encoding", contentEncodingHeader))
    {
        return Result(true);
    }

    HttpContentEncoding encoding = HttpContentEncoding::Identity;
    SC_TRY(httpContentEncodingFromHeader(contentEncodingHeader, encoding));
    if (encoding == HttpContentEncoding::Identity)
    {
        return Result(true);
    }

    const ZLibStream::Algorithm algorithm =
        encoding == HttpContentEncoding::GZip ? ZLibStream::DecompressGZip : ZLibStream::DecompressDeflate;
    SC_TRY(responseDecoder->stream.init(algorithm));

    HttpIncomingMessage::BodyStream& rawBody = response.rawBodyStream();
    SC_TRY_MSG((rawBody.eventData.addListener<HttpAsyncClient, &HttpAsyncClient::onCompressedResponseBodyData>(*this)),
               "HttpAsyncClient response decoder data listener limit reached");
    SC_TRY_MSG((rawBody.eventEnd.addListener<HttpAsyncClient, &HttpAsyncClient::onCompressedResponseBodyEnd>(*this)),
               "HttpAsyncClient response decoder end listener limit reached");
    SC_TRY_MSG((rawBody.eventError.addListener<HttpAsyncClient, &HttpAsyncClient::onCompressedResponseError>(*this)),
               "HttpAsyncClient response decoder raw error listener limit reached");
    SC_TRY_MSG((responseDecoder->AsyncReadableStream::eventError
                    .addListener<HttpAsyncClient, &HttpAsyncClient::onCompressedResponseError>(*this)),
               "HttpAsyncClient response decoder readable error listener limit reached");
    SC_TRY_MSG((responseDecoder->AsyncWritableStream::eventError
                    .addListener<HttpAsyncClient, &HttpAsyncClient::onCompressedResponseError>(*this)),
               "HttpAsyncClient response decoder writable error listener limit reached");

    response.attachReadableStream(*responseDecoder);
    responseDecoderActive = true;
    return Result(true);
}

Result HttpAsyncClient::startResponseStreams()
{
    if (responseDecoderActive)
    {
        if (responseDecoder->AsyncReadableStream::canStart())
        {
            SC_TRY(responseDecoder->AsyncReadableStream::start());
        }
        else
        {
            responseDecoder->AsyncReadableStream::resumeReading();
        }
    }
    return response.startBodyStream();
}

void HttpAsyncClient::detachResponseDecompression()
{
    if (not responseDecoderActive)
    {
        return;
    }
    HttpIncomingMessage::BodyStream& rawBody = response.rawBodyStream();
    (void)rawBody.eventData.removeListener<HttpAsyncClient, &HttpAsyncClient::onCompressedResponseBodyData>(*this);
    (void)rawBody.eventEnd.removeListener<HttpAsyncClient, &HttpAsyncClient::onCompressedResponseBodyEnd>(*this);
    (void)rawBody.eventError.removeListener<HttpAsyncClient, &HttpAsyncClient::onCompressedResponseError>(*this);
    if (responseDecoder != nullptr)
    {
        (void)responseDecoder->AsyncReadableStream::eventError
            .removeListener<HttpAsyncClient, &HttpAsyncClient::onCompressedResponseError>(*this);
        (void)responseDecoder->AsyncWritableStream::eventError
            .removeListener<HttpAsyncClient, &HttpAsyncClient::onCompressedResponseError>(*this);
    }
    responseDecoderActive = false;
}

void HttpAsyncClient::onCompressedResponseBodyData(AsyncBufferView::ID bufferID)
{
    SC_HTTP_ASSERT_RELEASE(responseDecoder != nullptr);
    Result writeResult = responseDecoder->AsyncWritableStream::write(
        bufferID, {[this](AsyncBufferView::ID writtenBufferID) { onCompressedResponseBodyWritten(writtenBufferID); }});
    if (not writeResult)
    {
        fail(writeResult);
    }
}

void HttpAsyncClient::onCompressedResponseBodyWritten(AsyncBufferView::ID) {}

void HttpAsyncClient::onCompressedResponseBodyEnd()
{
    SC_HTTP_ASSERT_RELEASE(responseDecoder != nullptr);
    detachResponseDecompression();
    responseDecoder->AsyncWritableStream::end();
}

void HttpAsyncClient::onCompressedResponseError(Result result) { fail(result); }

void HttpAsyncClient::onHeadersBufferWritten(AsyncBufferView::ID)
{
    if (currentRequest == nullptr)
    {
        return;
    }
    if (currentRequest->getBodyType() == HttpAsyncClientRequest::BodyType::Stream)
    {
        connection->pipeline.source        = currentRequest->getBodyStream();
        connection->pipeline.transforms[0] = currentRequest->getBodyTransform();
        connection->pipeline.transforms[1] = nullptr;
        connection->pipeline.sinks[0]      = &currentRequest->getWritableStream();
        connection->pipeline.sinks[1]      = nullptr;

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
        Result writeResult = connection->getWritableTransportStream().write(
            AsyncBufferView(currentRequest->getBodySpan()),
            {[this](AsyncBufferView::ID) { connection->getWritableTransportStream().end(); }});
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

        Result writeResult = connection->getWritableTransportStream().write(
            AsyncBufferView(bodyWriter.written()),
            {[this](AsyncBufferView::ID) { connection->getWritableTransportStream().end(); }});
        if (not writeResult)
        {
            fail(writeResult);
        }
    }
}

bool HttpAsyncClient::responseMustNotHaveBody() const
{
    if (currentRequest != nullptr and currentRequest->getMethod() == HttpParser::Method::HttpHEAD)
    {
        return true;
    }
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
                                            readData, connection->getReadableTransportStream(), bufferID);
    if (not parseRes)
    {
        fail(parseRes);
        return;
    }
    if (not response.hasReceivedHeaders())
    {
        return;
    }

    const bool removed = connection->getReadableTransportStream()
                             .eventData.removeListener<HttpAsyncClient, &HttpAsyncClient::onResponseData>(*this);
    (void)(removed);

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

    Result prepareDecompression = prepareResponseDecompression();
    if (not prepareDecompression)
    {
        fail(prepareDecompression);
        return;
    }

    const size_t bufferedBodyBytes = readData.sizeInBytes() - response.getHeadersLength();
    (void)(bufferedBodyBytes);

    if (not responseDelivered and onResponse.isValid())
    {
        responseDelivered = true;
        onResponse(response);
    }

    Result startBodyStream = startResponseStreams();
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
            response.processBodyData(connection->getReadableTransportStream(), bufferedBodyID,
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

    const bool addedBodyData = connection->getReadableTransportStream()
                                   .eventData.addListener<HttpAsyncClient, &HttpAsyncClient::onResponseBodyData>(*this);
    SC_HTTP_ASSERT_RELEASE(addedBodyData);
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

    Result process = response.processBodyData(connection->getReadableTransportStream(), bufferID, readData, false);
    if (not process)
    {
        fail(process);
        return;
    }
    if (response.isBodyComplete())
    {
        const bool removed =
            connection->getReadableTransportStream()
                .eventData.removeListener<HttpAsyncClient, &HttpAsyncClient::onResponseBodyData>(*this);
        (void)(removed);
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

    if (webSocketUpgraded)
    {
        state          = State::Idle;
        currentRequest = nullptr;
        if (shouldFinishBodyStream)
        {
            response.finishBodyStream();
        }
        return;
    }

    const bool keepAlive = currentRequest != nullptr and currentRequest->getKeepAlive() and response.getKeepAlive() and
                           response.getBodyFramingKind() != HttpBodyFramingKind::CloseDelimited;

    if (keepAlive)
    {
        connection->getReadableTransportStream().pause();
    }
    else
    {
        hasOpenConnection = false;
        currentProtocol   = {};
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
    (void)connection->getReadableTransportStream()
        .eventData.removeListener<HttpAsyncClient, &HttpAsyncClient::onResponseData>(*this);
    (void)connection->getReadableTransportStream()
        .eventData.removeListener<HttpAsyncClient, &HttpAsyncClient::onResponseBodyData>(*this);
    (void)connection->getReadableTransportStream()
        .eventError.removeListener<HttpAsyncClient, &HttpAsyncClient::onReadableError>(*this);
    (void)connection->getWritableTransportStream()
        .eventError.removeListener<HttpAsyncClient, &HttpAsyncClient::onWritableError>(*this);
    (void)connection->getReadableTransportStream()
        .eventEnd.removeListener<HttpAsyncClient, &HttpAsyncClient::onReadableEnd>(*this);
    (void)connection->pipeline.eventError.removeListener<HttpAsyncClient, &HttpAsyncClient::onPipelineError>(*this);
    (void)connection->pipeline.unpipe();
    if (transportClose.isValid())
    {
        transportClose();
    }
    connection->readableSocketStream.destroy();
    connection->writableSocketStream.destroy();
    connection->resetTransportStreams();
    if (connection->socket.isValid())
    {
        (void)connection->socket.close();
    }
    hasOpenConnection = false;
    currentProtocol   = {};
    currentHost       = {};
    currentPort       = 0;
    webSocketUpgraded = false;
}

void HttpAsyncClient::fail(Result error)
{
    detachResponseDecompression();
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
