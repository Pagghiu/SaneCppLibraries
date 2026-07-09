// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "SaneCppHttp.h"

namespace
{
auto* percentDecodePointer          = &SC::HttpPercentDecode;
auto* formUrlDecodePointer          = &SC::HttpFormUrlDecode;
auto* jsonContentTypePointer        = &SC::HttpContentTypeApplicationJson;
auto* parseBearerTokenPointer       = &SC::HttpParseBearerToken;
auto* writeBearerAuthPointer        = &SC::HttpWriteBearerAuthorization;
auto* websocketCreateKeyPointer     = &SC::HttpWebSocketHandshake::createClientKey;
auto* websocketComputeAcceptPointer = &SC::HttpWebSocketHandshake::computeAccept;

struct ConsumePublicHttpHeaderSymbols
{
    ConsumePublicHttpHeaderSymbols()
    {
        SC::HttpParser                          parser;
        SC::HttpURLParser                       url;
        SC::HttpRequestTargetView               target;
        SC::HttpURLQueryItem                    queryItem;
        SC::HttpURLQueryIterator                queryIterator(SC::StringSpan{});
        SC::HttpFormUrlEncodedIterator          formIterator(SC::StringSpan{});
        SC::HttpCookieIterator                  cookieIterator(SC::StringSpan{});
        SC::HttpSetCookieView                   cookie;
        SC::HttpSetCookieBuilder                cookieBuilder;
        SC::HttpCacheControlBuilder             cacheControl;
        SC::HttpAuthorizationView               authorization;
        SC::HttpRoute                           route;
        SC::HttpRouteMatch                      routeMatch;
        SC::HttpRouter                          router;
        SC::HttpAsyncClient::Header             clientHeader;
        SC::HttpAsyncClient::RequestOptions     request;
        SC::HttpAsyncFileServerOptions          fileServerOptions;
        SC::HttpMultipartContentDispositionView multipartDisposition;
        SC::HttpMultipartPartHeadersView        multipartHeaders;
        SC::HttpMultipartParser                 multipartParser;
        SC::HttpWebSocketFrameHeaderView        frame;
        SC::HttpWebSocketFrameReader            frameReader;
        SC::HttpWebSocketFrameWriter            frameWriter;
        SC::HttpWebSocketEndpoint               endpoint;
        SC::HttpWebSocketConnectionPump         pump;
        SC::HttpWebSocketSmallHub               hub;

        parser.type  = SC::HttpParser::Type::Request;
        route.method = SC::HttpParser::Method::HttpGET;
        frame.opcode = SC::HttpWebSocketOpcode::Text;

        request.setRequest(SC::HttpParser::Method::HttpPOST, SC::StringSpan("/upload"), true)
            .setHeaders({&clientHeader, 1})
            .setKeepAlive(false)
            .setBody(SC::StringSpan("body"))
            .clearBody();

        frameReader.reset(SC::HttpWebSocketEndpointRole::Server);
        frameWriter.reset(SC::HttpWebSocketEndpointRole::Client);
        endpoint.reset(SC::HttpWebSocketEndpointRole::Server);

        (void)percentDecodePointer;
        (void)formUrlDecodePointer;
        (void)jsonContentTypePointer;
        (void)parseBearerTokenPointer;
        (void)writeBearerAuthPointer;
        (void)websocketCreateKeyPointer;
        (void)websocketComputeAcceptPointer;
        (void)url;
        (void)target;
        (void)queryItem;
        (void)queryIterator;
        (void)formIterator;
        (void)cookieIterator;
        (void)cookie;
        (void)cookieBuilder;
        (void)cacheControl;
        (void)authorization;
        (void)routeMatch;
        (void)router;
        (void)clientHeader;
        (void)fileServerOptions;
        (void)multipartDisposition;
        (void)multipartHeaders;
        (void)multipartParser;
        (void)pump;
        (void)hub;
    }
} consumePublicHttpHeaderSymbols;
} // namespace
