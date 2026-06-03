// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../Common/Result.h"
#include "../Common/Span.h"
#include "../Common/StringSpan.h"
#include "HttpExport.h"
#include "HttpParser.h"

namespace SC
{
//! @addtogroup group_http
//! @{

struct SC_HTTP_EXPORT HttpRouteParam
{
    StringSpan name;
    StringSpan value;
};

struct SC_HTTP_EXPORT HttpRoute
{
    HttpParser::Method method      = HttpParser::Method::HttpGET;
    StringSpan         pathPattern = {};
};

enum class HttpRouteMatchStatus
{
    Matched,
    MethodNotAllowed,
    NotFound,
    TooManyParams,
};

struct SC_HTTP_EXPORT HttpRouteMatch
{
    HttpRouteMatchStatus status    = HttpRouteMatchStatus::NotFound;
    const HttpRoute*     route     = nullptr;
    size_t               numParams = 0;
};

/// @brief Tiny allocation-free method/path router over caller-owned routes.
struct SC_HTTP_EXPORT HttpRouter
{
    Result init(Span<const HttpRoute> routeStorage);

    Result match(HttpParser::Method method, StringSpan requestTarget, Span<HttpRouteParam> params,
                 HttpRouteMatch& match) const;

    Result formatAllowHeader(StringSpan requestTarget, Span<char> storage, StringSpan& allow) const;

  private:
    Span<const HttpRoute> routes;
};

//! @}
} // namespace SC
