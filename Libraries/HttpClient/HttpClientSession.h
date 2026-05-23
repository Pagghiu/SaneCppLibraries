// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once

#include "HttpClient.h"

namespace SC
{
//! @addtogroup group_http_client
//! @{

/// @brief Cookie state owned by HttpClientSession caller-provided memory.
struct SC_HTTP_CLIENT_EXPORT HttpClientSessionCookie
{
    enum Flag : uint8_t
    {
        Secure       = 1 << 0,
        HttpOnly     = 1 << 1,
        DomainCookie = 1 << 2,
    };

    StringSpan name;
    StringSpan value;
    StringSpan domain;
    StringSpan path;
    uint8_t    flags = 0;

    [[nodiscard]] bool isInUse() const { return not name.isEmpty(); }
};

/// @brief Caller-managed authorization cache entry for one exact request origin.
struct SC_HTTP_CLIENT_EXPORT HttpClientSessionAuthCacheEntry
{
    StringSpan origin;              ///< `scheme://host[:port]`
    StringSpan authorizationHeader; ///< Full Authorization header value, e.g. `Basic ...`

    [[nodiscard]] bool isInUse() const { return not origin.isEmpty(); }
};

/// @brief Authentication challenge parsed from response headers without owning memory.
struct SC_HTTP_CLIENT_EXPORT HttpClientSessionAuthChallenge
{
    enum Target : uint8_t
    {
        Origin,
        Proxy,
    };

    enum Scheme : uint8_t
    {
        Unsupported,
        Basic,
    };

    Target     target = Origin;
    Scheme     scheme = Unsupported;
    StringSpan realm;

    [[nodiscard]] const char*        getTargetName() const { return getTargetName(target); }
    [[nodiscard]] const char*        getSchemeName() const { return getSchemeName(scheme); }
    [[nodiscard]] static const char* getTargetName(Target target);
    [[nodiscard]] static const char* getSchemeName(Scheme scheme);
};

/// @brief Caller-owned memory for the optional HttpClientSession layer.
struct SC_HTTP_CLIENT_EXPORT HttpClientSessionMemory
{
    Span<HttpClientHeader>                requestHeaders; ///< Header workspace for prepared requests
    Span<HttpClientSessionCookie>         cookies;        ///< Durable cookie slots
    Span<HttpClientSessionAuthCacheEntry> authEntries;    ///< Durable auth cache slots
    Span<char>                            headerScratch;  ///< Per-prepare header value scratch
    Span<char>                            stateScratch;   ///< Append-only durable state string scratch
};

/// @brief Retry policy used by HttpClientSessionRetryState.
struct SC_HTTP_CLIENT_EXPORT HttpClientSessionRetryPolicy
{
    uint8_t maxAttempts = 1;

    bool retryTransportErrors             = true;
    bool retryHttpStatusCodes             = true;
    bool retryNonIdempotentReplayableBody = false;
};

/// @brief Caller-owned retry bookkeeping for one logical request.
struct SC_HTTP_CLIENT_EXPORT HttpClientSessionRetryState
{
    HttpClientRequest::Method    method = HttpClientRequest::HttpGET;
    HttpClientSessionRetryPolicy policy;
    uint8_t                      attemptsStarted       = 0;
    bool                         requestBodyReplayable = true;

    [[nodiscard]] bool    isStarted() const { return attemptsStarted > 0; }
    [[nodiscard]] bool    hasAttemptsRemaining() const { return attemptsStarted < policy.maxAttempts; }
    [[nodiscard]] uint8_t getRemainingAttempts() const
    {
        return attemptsStarted < policy.maxAttempts ? static_cast<uint8_t>(policy.maxAttempts - attemptsStarted) : 0;
    }
};

/// @brief Optional caller-owned state layer above the stateless HttpClient core.
///
/// This helper never owns memory. `prepareRequest()` returns views into the original request plus
/// `HttpClientSessionMemory::requestHeaders` / `headerScratch`; start the operation before reusing
/// the same session memory for another prepared request.
struct SC_HTTP_CLIENT_EXPORT HttpClientSession
{
    [[nodiscard]] Result init(const HttpClientSessionMemory& memory);
    void                 clear();
    void                 clearCookies();
    void                 clearAuthorizations();

    [[nodiscard]] Result addAuthorization(StringSpan origin, StringSpan authorizationHeader);
    [[nodiscard]] bool   findAuthorization(StringSpan origin, StringSpan& authorizationHeader) const;
    [[nodiscard]] bool   hasAuthorization(StringSpan origin) const;
    [[nodiscard]] bool   findCookie(StringSpan name, StringSpan domain, StringSpan path,
                                    HttpClientSessionCookie& cookie) const;
    [[nodiscard]] bool   hasCookie(StringSpan name, StringSpan domain, StringSpan path) const;
    [[nodiscard]] Result prepareRequest(const HttpClientRequest& source, HttpClientRequest& prepared);
    [[nodiscard]] Result captureResponse(const HttpClientRequest& request, const HttpClientResponse& response);

    [[nodiscard]] static Result makeBasicAuthorization(StringSpan username, StringSpan password, Span<char> destination,
                                                       StringSpan& authorizationHeader);
    [[nodiscard]] static bool   findBasicAuthChallenge(const HttpClientResponse&              response,
                                                       HttpClientSessionAuthChallenge::Target target,
                                                       HttpClientSessionAuthChallenge&        challenge);
    [[nodiscard]] static Result makeBasicAuthorizationForChallenge(const HttpClientResponse&              response,
                                                                   HttpClientSessionAuthChallenge::Target target,
                                                                   StringSpan username, StringSpan password,
                                                                   Span<char>  destination,
                                                                   StringSpan& authorizationHeader);

    [[nodiscard]] Result beginRetry(HttpClientSessionRetryState& state, const HttpClientRequest& request,
                                    HttpClientSessionRetryPolicy policy) const;
    [[nodiscard]] bool   shouldRetry(HttpClientSessionRetryState& state, Result transportResult,
                                     const HttpClientResponse* response) const;

    [[nodiscard]] static bool isIdempotentMethod(HttpClientRequest::Method method);
    [[nodiscard]] static bool isRetryableStatusCode(int statusCode);

    [[nodiscard]] bool   isInitialized() const { return initialized; }
    [[nodiscard]] size_t getNumCookies() const;
    [[nodiscard]] size_t getNumAuthorizations() const;

  private:
    [[nodiscard]] Result copyStateString(StringSpan source, StringSpan& destination);
    [[nodiscard]] Result appendPreparedHeader(StringSpan name, StringSpan value, size_t& numHeaders);
    [[nodiscard]] Result appendScratch(StringSpan text, bool addSeparator);
    [[nodiscard]] Result appendMatchingCookies(StringSpan url, size_t& numHeaders);
    [[nodiscard]] Result captureSetCookie(StringSpan requestUrl, StringSpan setCookie);

    HttpClientSessionMemory sessionMemory;
    size_t                  stateScratchUsed  = 0;
    size_t                  headerScratchUsed = 0;
    bool                    initialized       = false;
};

//! @}
} // namespace SC
