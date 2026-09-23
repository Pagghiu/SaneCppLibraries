// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once

#include "../Common/ResultErrorFormatter.h"
#include "HttpError.h"

namespace SC
{
/// @brief Format an Http-owned error into caller-provided UTF-8 storage.
/// @details Include this optional header only where canonical English text is needed.
inline ResultErrorFormat formatHttpError(HttpError error, Span<char> output)
{
    ResultErrorFormatter formatter(output);
    switch (error)
    {
    case HttpError::MalformedPercentEscape: formatter.append("URL contains a malformed percent escape"); break;
    case HttpError::DecodedOutputTooSmall: formatter.append("Decoded URL output buffer is too small"); break;
    case HttpError::RequestTargetEmpty: formatter.append("HTTP request target is empty"); break;
    case HttpError::RequestTargetWhitespace: formatter.append("HTTP request target contains whitespace"); break;
    case HttpError::UnsupportedRequestTargetForm: formatter.append("HTTP request target form is unsupported"); break;
    case HttpError::MalformedURL: formatter.append("URL syntax is malformed"); break;
    case HttpError::UnsupportedProtocol: formatter.append("URL protocol is unsupported"); break;
    case HttpError::InvalidURLPath: formatter.append("URL path contains invalid whitespace"); break;
    case HttpError::InvalidURLHost: formatter.append("URL host is invalid"); break;
    case HttpError::InvalidURLUserInfo: formatter.append("URL user information is invalid"); break;
    case HttpError::InvalidIPv6Host: formatter.append("URL IPv6 host is invalid"); break;
    case HttpError::InvalidURLPort: formatter.append("URL port is invalid"); break;
    case HttpError::AllowHeaderOutputTooSmall: formatter.append("Allow header output buffer is too small"); break;
    case HttpError::MultipartDispositionEmpty: formatter.append("Multipart Content-Disposition is empty"); break;
    case HttpError::MultipartDispositionTypeEmpty: formatter.append("Multipart disposition type is empty"); break;
    case HttpError::MultipartBoundaryInvalid: formatter.append("Multipart boundary is empty"); break;
    case HttpError::MultipartBoundaryTooLong: formatter.append("Multipart boundary is too long"); break;
    case HttpError::MultipartBoundaryCandidateTooLong:
        formatter.append("Multipart boundary candidate is too long");
        break;
    case HttpError::MultipartMalformedSyntax: formatter.append("Multipart syntax is malformed"); break;
    case HttpError::MultipartSpanInvalid: formatter.append("Multipart parser span is invalid"); break;
    case HttpError::AuthorizationCredentialsMissing: formatter.append("Authorization credentials are missing"); break;
    case HttpError::AuthorizationNotBearer: formatter.append("Authorization scheme is not Bearer"); break;
    case HttpError::AuthorizationNotBasic: formatter.append("Authorization scheme is not Basic"); break;
    case HttpError::BasicBase64Incomplete: formatter.append("Basic credentials have incomplete base64"); break;
    case HttpError::BasicBase64Invalid: formatter.append("Basic credentials have invalid base64"); break;
    case HttpError::BasicBase64PaddingInvalid: formatter.append("Basic credentials have invalid base64 padding"); break;
    case HttpError::BasicBase64TrailingData: formatter.append("Basic credentials have trailing base64 data"); break;
    case HttpError::BasicDecodeOutputTooSmall: formatter.append("Basic credential output buffer is too small"); break;
    case HttpError::BasicPasswordSeparatorMissing:
        formatter.append("Basic credentials lack a password separator");
        break;
    case HttpError::BasicUsernameContainsColon: formatter.append("Basic username contains a colon"); break;
    case HttpError::AuthorizationOutputTooSmall: formatter.append("Authorization output buffer is too small"); break;
    case HttpError::BearerTokenEmpty: formatter.append("Bearer token is empty"); break;
    case HttpError::BasicCredentialsEmpty: formatter.append("Basic credentials are empty"); break;
    case HttpError::SetCookieHeaderEmpty: formatter.append("Set-Cookie header is empty"); break;
    case HttpError::SetCookieNameValueMissing: formatter.append("Set-Cookie name/value pair is missing"); break;
    case HttpError::SetCookieNameEmpty: formatter.append("Set-Cookie name is empty"); break;
    case HttpError::SetCookieOutputTooSmall: formatter.append("Set-Cookie output buffer is too small"); break;
    case HttpError::CacheControlConflictingVisibility:
        formatter.append("Cache-Control cannot be both public and private");
        break;
    case HttpError::CacheControlOutputTooSmall: formatter.append("Cache-Control output buffer is too small"); break;
    case HttpError::CacheControlNoDirectives: formatter.append("Cache-Control has no directives"); break;
    case HttpError::HeaderStorageExhausted: formatter.append("HTTP header storage is exhausted"); break;
    case HttpError::HeaderSizeLimitExceeded: formatter.append("HTTP header size limit is exceeded"); break;
    case HttpError::HeaderTokenLimitExceeded: formatter.append("HTTP header token limit is exceeded"); break;
    case HttpError::HeaderOutputTooSmall: formatter.append("HTTP header output buffer is too small"); break;
    case HttpError::MultipartBodyOutputTooSmall: formatter.append("Multipart body output buffer is too small"); break;
    case HttpError::ContentLengthFormattingFailed: formatter.append("Content-Length formatting failed"); break;
    case HttpError::MultipartWriterBoundaryEmpty: formatter.append("Multipart output boundary is empty"); break;
    case HttpError::MultipartWriterBoundaryUnsafe: formatter.append("Multipart output boundary is unsafe"); break;
    case HttpError::MultipartWriterBoundaryMissing: formatter.append("Multipart output boundary is not set"); break;
    case HttpError::MultipartWriterFieldNameEmpty: formatter.append("Multipart output field name is empty"); break;
    case HttpError::MultipartWriterFieldNameUnsafe: formatter.append("Multipart output field name is unsafe"); break;
    case HttpError::MultipartWriterFileNameUnsafe: formatter.append("Multipart output file name is unsafe"); break;
    case HttpError::MultipartWriterContentTypeUnsafe:
        formatter.append("Multipart output content type is unsafe");
        break;
    case HttpError::MultipartWriterPartLimitExceeded:
        formatter.append("Multipart output part limit is exceeded");
        break;
    case HttpError::BodyExceedsContentLength: formatter.append("HTTP body exceeds Content-Length"); break;
    case HttpError::BodyFramingHeadersConflict:
        formatter.append("Content-Length conflicts with Transfer-Encoding");
        break;
    case HttpError::TransferEncodingUnsupported: formatter.append("HTTP Transfer-Encoding is unsupported"); break;
    case HttpError::UnexpectedBodyData: formatter.append("Unexpected HTTP body data"); break;
    case HttpError::BodyStreamConsumptionMismatch:
        formatter.append("HTTP body stream consumed an unexpected byte count");
        break;
    case HttpError::ChunkSizeOverflow: formatter.append("HTTP chunk size overflows"); break;
    case HttpError::ChunkSizeInvalid: formatter.append("HTTP chunk size is invalid"); break;
    case HttpError::ChunkHeaderMalformed: formatter.append("HTTP chunk header is malformed"); break;
    case HttpError::ChunkTerminatorMalformed: formatter.append("HTTP chunk terminator is malformed"); break;
    case HttpError::ChunkTrailersUnsupported: formatter.append("HTTP chunk trailers are unsupported"); break;
    case HttpError::ChunkTrailerTerminatorMalformed:
        formatter.append("HTTP chunk trailer terminator is malformed");
        break;
    case HttpError::PipelinedBodyUnsupported: formatter.append("Pipelined HTTP body data is unsupported"); break;
    case HttpError::ChunkHeaderOutputTooSmall: formatter.append("HTTP chunk header output buffer is too small"); break;
    case HttpError::ChunkedDestinationMissing: formatter.append("Chunked HTTP output destination is missing"); break;
    case HttpError::HeadersAlreadySent: formatter.append("HTTP headers have already been sent"); break;
    case HttpError::HeaderStartMissing: formatter.append("HTTP message start must precede headers"); break;
    case HttpError::HeaderStartAlreadyWritten: formatter.append("HTTP message start has already been written"); break;
    case HttpError::KeepAliveDisabled: formatter.append("HTTP keep-alive is disabled for this connection"); break;
    case HttpError::ContentLengthTransferEncodingConflict:
        formatter.append("Content-Length conflicts with Transfer-Encoding");
        break;
    case HttpError::DestinationStreamMissing: formatter.append("HTTP output destination stream is missing"); break;
    case HttpError::HeadersNotSent: formatter.append("HTTP headers have not been sent"); break;
    case HttpError::ResponseStatusUnsupported: formatter.append("HTTP response status is unsupported"); break;
    case HttpError::ResponseStatusInvalid: formatter.append("HTTP response status must have three digits"); break;
    case HttpError::ResponseReasonPhraseEmpty: formatter.append("HTTP response reason phrase is empty"); break;
    case HttpError::ResponseReasonPhraseInvalid:
        formatter.append("HTTP response reason phrase contains a line break");
        break;
    case HttpError::ResponseStatusFormattingFailed: formatter.append("HTTP response status formatting failed"); break;
    case HttpError::RedirectStatusInvalid: formatter.append("HTTP redirect status must be 3xx"); break;
    case HttpError::RedirectLocationEmpty: formatter.append("HTTP redirect location is empty"); break;
    case HttpError::ContentEncodingUnsupported: formatter.append("HTTP Content-Encoding is unsupported"); break;
    case HttpError::RequestStartAlreadyWritten: formatter.append("HTTP request start has already been written"); break;
    case HttpError::CompressedBodyEncodingInvalid:
        formatter.append("Compressed HTTP request body requires gzip or deflate");
        break;
    case HttpError::MultipartWriterMissing: formatter.append("Multipart request writer is missing"); break;
    case HttpError::PoolActiveConnectionsRemain:
        formatter.append("HTTP connection pool still has active connections");
        break;
    case HttpError::PoolHeaderStorageEmpty: formatter.append("HTTP connection header storage is empty"); break;
    case HttpError::PoolReadQueueConfigurationInvalid:
        formatter.append("HTTP connection read queue configuration is invalid");
        break;
    case HttpError::PoolBufferQueueConfigurationInvalid:
        formatter.append("HTTP connection buffer queue is smaller than its read queue");
        break;
    case HttpError::PoolReadQueueStorageTooSmall: formatter.append("HTTP read queue storage is too small"); break;
    case HttpError::PoolWriteQueueStorageTooSmall: formatter.append("HTTP write queue storage is too small"); break;
    case HttpError::PoolBufferQueueStorageTooSmall: formatter.append("HTTP buffer queue storage is too small"); break;
    case HttpError::PoolHeaderStorageTooSmall: formatter.append("HTTP header storage is too small"); break;
    case HttpError::PoolStreamStorageTooSmall: formatter.append("HTTP stream storage is too small"); break;
    case HttpError::ParserMethodMalformed: formatter.append("HTTP request method is malformed"); break;
    case HttpError::ParserRequestTargetMalformed: formatter.append("HTTP request target is malformed"); break;
    case HttpError::ParserVersionMalformed: formatter.append("HTTP version is malformed"); break;
    case HttpError::ParserStatusCodeMalformed: formatter.append("HTTP response status code is malformed"); break;
    case HttpError::ParserStatusTextMalformed: formatter.append("HTTP response status text is malformed"); break;
    case HttpError::ParserHeaderNameMalformed: formatter.append("HTTP header name is malformed"); break;
    case HttpError::ParserHeaderValueMalformed: formatter.append("HTTP header value is malformed"); break;
    case HttpError::ParserContentLengthMalformed: formatter.append("HTTP Content-Length is malformed"); break;
    case HttpError::ParserConnectionHeaderMalformed: formatter.append("HTTP Connection header is malformed"); break;
    case HttpError::ParserHeaderTerminatorMalformed: formatter.append("HTTP header terminator is malformed"); break;
    case HttpError::ParserSpanInvalid: formatter.append("HTTP parser span is invalid"); break;
    case HttpError::HeaderBlockIncomplete: formatter.append("HTTP header block is incomplete"); break;
    case HttpError::ServerReadQueueEmpty: formatter.append("HTTP server connection read queue is empty"); break;
    case HttpError::ServerWriteQueueEmpty: formatter.append("HTTP server connection write queue is empty"); break;
    case HttpError::ServerBufferPoolEmpty: formatter.append("HTTP server connection buffer pool is empty"); break;
    case HttpError::ServerResizeAddressChanged:
        formatter.append("HTTP server connection storage address changed");
        break;
    case HttpError::ServerResizeActiveConnection:
        formatter.append("HTTP server resize would remove an active connection");
        break;
    case HttpError::ServerAlreadyStarted: formatter.append("HTTP server is already started or stopping"); break;
    case HttpError::ServerNotInitialized: formatter.append("HTTP server has no initialized connections"); break;
    case HttpError::ServerExternalListenerRequired:
        formatter.append("HTTP server has no active external listener");
        break;
    case HttpError::ServerConnectionSlotUnavailable:
        formatter.append("HTTP server connection slot is unavailable");
        break;
    case HttpError::ServerStopRequired: formatter.append("HTTP server must stop before closing"); break;
    case HttpError::ServerNotStarted: formatter.append("HTTP server is not started"); break;
    case HttpError::ServerNotStopping: formatter.append("HTTP server is not stopping"); break;
    case HttpError::ServerDataListenerUnavailable:
        formatter.append("HTTP server readable data listener is unavailable");
        break;
    default: return ResultErrorFormatter::failure(ResultErrorFormatStatus::UnknownError, output);
    }
    return formatter.finish();
}

inline ResultErrorFormat formatHttpError(Result result, Span<char> output)
{
    if (result)
        return ResultErrorFormatter::failure(ResultErrorFormatStatus::NotAnError, output);
    if (result.category() != HttpResultCategory)
        return ResultErrorFormatter::failure(ResultErrorFormatStatus::ForeignCategory, output);
    return formatHttpError(static_cast<HttpError>(result.errorValue()), output);
}
} // namespace SC
