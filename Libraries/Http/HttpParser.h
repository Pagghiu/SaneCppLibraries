// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../Foundation/Result.h"
#include "../Foundation/Span.h"

namespace SC
{
struct HttpParser;
} // namespace SC

//! @addtogroup group_http
//! @{

/// @brief Incremental HTTP request or response parser
struct SC::HttpParser
{
    /// @brief Method of the current request / response
    enum class Method
    {
        HttpGET,  ///< `GET` method
        HttpPUT,  ///< `PUT` method
        HttpPOST, ///< `POST` method
    };
    Method method = Method::HttpGET; ///< Http method

    size_t   tokenStart    = 0; ///< Offset in bytes to start of parsed token
    size_t   tokenLength   = 0; ///< Lenth in bytes of parsed token
    uint32_t statusCode    = 0; ///< Parsed http status code
    uint64_t contentLength = 0; ///< Content-Length of http request

    /// @brief State of the parser
    enum class State
    {
        Parsing,  ///< Parser is parsing
        Result,   ///< Parser is reporting a result
        Finished, ///< Parser has finished
    };

    /// @brief One possible Result reported by the parser
    enum class Result
    {
        Method,       ///< Http method has been found
        Url,          ///< Http url has been found
        Version,      ///< Http version number has been found
        HeaderName,   ///< Name of an http header has been found
        HeaderValue,  ///< Value of an http header has been found
        HeadersEnd,   ///< Lash http header has been found
        StatusCode,   ///< Http status code has been found
        StatusString, ///< Http status string has been found
        Body          ///< Start of http body has been found
    };

    Result result = Result::HeadersEnd; ///< Last found result
    State  state  = State::Parsing;     ///< Current state of the parser

    /// @brief Type of the stream to be parsed (Request or Response)
    enum class Type
    {
        Request, ///< Stream to be parsed is an Http request from a client
        Response ///< Stream to be parsed is an http response from a server
    };
    Type type = Type::Request; ///< Type of http stream (request or response)

    /// @brief Parse an incoming chunk of bytes, returning actually parsed span
    /// @param data Incoming chunk of bytes to be parsed
    /// @param readBytes Number of bytes actually read
    /// @param parsedData A sub-span of `data` pointing at the actually parsed data
    /// @return Valid result if parse didn't encounter any error
    [[nodiscard]] SC::Result parse(Span<const char> data, size_t& readBytes, Span<const char>& parsedData);

    /// @brief Header types
    enum class HeaderType
    {
        ContentLength = 0 ///< Content-Length header
    };

    /// @brief Check if current result matches this HeaderType
    [[nodiscard]] bool matchesHeader(HeaderType headerName) const;

  private:
    size_t globalStart           = 0;
    size_t globalLength          = 0;
    int    topLevelCoroutine     = 0;
    int    nestedParserCoroutine = 0;
    bool   parsedcontentLength   = false;
    size_t matchIndex            = 0;

    static constexpr size_t numMatches = 1;

    size_t   matchingHeader[numMatches]      = {0};
    bool     matchingHeaderValid[numMatches] = {false};
    uint64_t number                          = 0;

    [[nodiscard]] bool parseHeaderName(char currentChar);
    [[nodiscard]] bool parseHeaderValue(char currentChar);
    [[nodiscard]] bool parseStatusCode(char currentChar);
    [[nodiscard]] bool parseNumberValue(char currentChar);
    [[nodiscard]] bool parseHeadersEnd(char currentChar);
    [[nodiscard]] bool parseMethod(char currentChar);
    [[nodiscard]] bool parseUrl(char currentChar);

    template <bool spaces>
    [[nodiscard]] bool parseVersion(char currentChar);

    template <bool (HttpParser::*Func)(char), Result currentResult>
    [[nodiscard]] SC::Result process(Span<const char>& data, size_t& readBytes, Span<const char>& parsedData);
};

//! @}
