// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "HttpRouter.h"
#include "HttpURLParser.h"

namespace SC
{
struct HttpRouterInternal
{
    static StringSpan trimLeadingSlash(StringSpan path)
    {
        if (path.sizeInBytes() > 0 and path.bytesWithoutTerminator()[0] == '/')
        {
            return {{path.bytesWithoutTerminator() + 1, path.sizeInBytes() - 1}, false, path.getEncoding()};
        }
        return path;
    }

    static bool nextSegment(StringSpan path, size_t& cursor, StringSpan& segment)
    {
        if (cursor > path.sizeInBytes())
        {
            return false;
        }
        if (cursor == path.sizeInBytes())
        {
            cursor++;
            return false;
        }
        const char*  data  = path.bytesWithoutTerminator();
        const size_t start = cursor;
        while (cursor < path.sizeInBytes() and data[cursor] != '/')
        {
            cursor++;
        }
        segment = {{data + start, cursor - start}, false, path.getEncoding()};
        if (cursor < path.sizeInBytes() and data[cursor] == '/')
        {
            cursor++;
        }
        return true;
    }

    static bool isParamSegment(StringSpan segment)
    {
        return segment.sizeInBytes() > 1 and segment.bytesWithoutTerminator()[0] == ':';
    }

    static StringSpan methodName(HttpParser::Method method)
    {
        switch (method)
        {
        case HttpParser::Method::HttpGET: return "GET";
        case HttpParser::Method::HttpPUT: return "PUT";
        case HttpParser::Method::HttpPOST: return "POST";
        case HttpParser::Method::HttpHEAD: return "HEAD";
        case HttpParser::Method::HttpPATCH: return "PATCH";
        case HttpParser::Method::HttpDELETE: return "DELETE";
        case HttpParser::Method::HttpOPTIONS: return "OPTIONS";
        }
        return {};
    }

    static bool append(Span<char> storage, size_t& offset, StringSpan value)
    {
        if (offset + value.sizeInBytes() > storage.sizeInBytes())
        {
            return false;
        }
        for (size_t idx = 0; idx < value.sizeInBytes(); ++idx)
        {
            storage.data()[offset + idx] = value.bytesWithoutTerminator()[idx];
        }
        offset += value.sizeInBytes();
        return true;
    }

    static bool containsMethod(Span<HttpParser::Method> methods, size_t count, HttpParser::Method method)
    {
        for (size_t idx = 0; idx < count; ++idx)
        {
            if (methods[idx] == method)
            {
                return true;
            }
        }
        return false;
    }

    static HttpRouteMatchStatus matchPath(StringSpan pattern, StringSpan requestPath, Span<HttpRouteParam> params,
                                          size_t& numParams)
    {
        numParams   = 0;
        pattern     = trimLeadingSlash(pattern);
        requestPath = trimLeadingSlash(requestPath);

        size_t patternCursor = 0;
        size_t pathCursor    = 0;
        while (true)
        {
            StringSpan patternSegment;
            StringSpan pathSegment;
            const bool hasPattern = nextSegment(pattern, patternCursor, patternSegment);
            const bool hasPath    = nextSegment(requestPath, pathCursor, pathSegment);
            if (not hasPattern and not hasPath)
            {
                return HttpRouteMatchStatus::Matched;
            }
            if (hasPattern != hasPath)
            {
                return HttpRouteMatchStatus::NotFound;
            }

            if (isParamSegment(patternSegment))
            {
                if (numParams >= params.sizeInElements())
                {
                    return HttpRouteMatchStatus::TooManyParams;
                }
                params[numParams].name = {
                    {patternSegment.bytesWithoutTerminator() + 1, patternSegment.sizeInBytes() - 1},
                    false,
                    patternSegment.getEncoding()};
                params[numParams].value = pathSegment;
                numParams++;
            }
            else if (patternSegment != pathSegment)
            {
                return HttpRouteMatchStatus::NotFound;
            }
        }
    }
};

Result HttpRouter::init(Span<const HttpRoute> routeStorage)
{
    routes = routeStorage;
    return Result(true);
}

Result HttpRouter::match(HttpParser::Method method, StringSpan requestTarget, Span<HttpRouteParam> params,
                         HttpRouteMatch& match) const
{
    match = {};

    HttpRequestTargetView target;
    SC_TRY(target.parse(requestTarget));
    const StringSpan requestPath = target.path;
    bool             pathMatched = false;
    for (const HttpRoute& route : routes)
    {
        size_t                     numParams = 0;
        const HttpRouteMatchStatus status =
            HttpRouterInternal::matchPath(route.pathPattern, requestPath, params, numParams);
        if (status == HttpRouteMatchStatus::TooManyParams)
        {
            match.status = status;
            return Result(true);
        }
        if (status != HttpRouteMatchStatus::Matched)
        {
            continue;
        }
        pathMatched = true;
        if (route.method == method)
        {
            match.status    = HttpRouteMatchStatus::Matched;
            match.route     = &route;
            match.numParams = numParams;
            return Result(true);
        }
    }

    match.status = pathMatched ? HttpRouteMatchStatus::MethodNotAllowed : HttpRouteMatchStatus::NotFound;
    return Result(true);
}

Result HttpRouter::formatAllowHeader(StringSpan requestTarget, Span<char> storage, StringSpan& allow) const
{
    allow = {};

    HttpParser::Method    methods[7];
    size_t                numMethods = 0;
    HttpRouteParam        params[8];
    HttpRequestTargetView target;
    SC_TRY(target.parse(requestTarget));
    const StringSpan requestPath = target.path;
    for (const HttpRoute& route : routes)
    {
        size_t                     numParams = 0;
        const HttpRouteMatchStatus status =
            HttpRouterInternal::matchPath(route.pathPattern, requestPath, params, numParams);
        if (status == HttpRouteMatchStatus::Matched and
            not HttpRouterInternal::containsMethod({methods, numMethods}, numMethods, route.method))
        {
            methods[numMethods++] = route.method;
        }
    }

    size_t offset = 0;
    for (size_t idx = 0; idx < numMethods; ++idx)
    {
        if (idx > 0)
        {
            SC_TRY_MSG(HttpRouterInternal::append(storage, offset, ", "), "Allow output buffer is too small");
        }
        SC_TRY_MSG(HttpRouterInternal::append(storage, offset, HttpRouterInternal::methodName(methods[idx])),
                   "Allow output buffer is too small");
    }
    allow = {{storage.data(), offset}, false, StringEncoding::Ascii};
    return Result(true);
}

} // namespace SC
