// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once

#include "ZLibStream.h"

#include "ZLibAPI.h"
#include "ZLibAPI.inl" // IWYU pragma: keep

static SC::ZLibAPI zlib;

struct SC::ZLibStream::Internal
{
    static Result error(ZLibAPI::Error status)
    {
        switch (status)
        {
        case ZLibAPI::BUF_ERROR:
            return Result::Error(AsyncStreamsResultCategory, AsyncStreamsError::CompressionNoProgress);
        case ZLibAPI::STREAM_END:
            return Result::Error(AsyncStreamsResultCategory, AsyncStreamsError::CompressionUnexpectedEnd);
        case ZLibAPI::NEED_DICT:
            return Result::Error(AsyncStreamsResultCategory, AsyncStreamsError::CompressionDictionaryRequired);
        case ZLibAPI::ERRNO: return Result::Error(AsyncStreamsResultCategory, AsyncStreamsError::CompressionIOFailure);
        case ZLibAPI::STREAM_ERROR:
            return Result::Error(AsyncStreamsResultCategory, AsyncStreamsError::CompressionStreamInvalid);
        case ZLibAPI::DATA_ERROR:
            return Result::Error(AsyncStreamsResultCategory, AsyncStreamsError::CompressionDataInvalid);
        case ZLibAPI::MEM_ERROR:
            return Result::Error(AsyncStreamsResultCategory, AsyncStreamsError::CompressionMemoryUnavailable);
        case ZLibAPI::VERSION_ERROR:
            return Result::Error(AsyncStreamsResultCategory, AsyncStreamsError::CompressionVersionMismatch);
        default: return Result::Error(AsyncStreamsResultCategory, AsyncStreamsError::CompressionFailed);
        }
    }

    static Result compress(ZLibAPI::Stream& stream, Span<const char>& input, Span<char>& output)
    {
        stream.next_in   = reinterpret_cast<const uint8_t*>(input.data());
        stream.avail_in  = static_cast<unsigned int>(input.sizeInBytes());
        stream.next_out  = reinterpret_cast<uint8_t*>(output.data());
        stream.avail_out = static_cast<unsigned int>(output.sizeInBytes());

        const auto result     = zlib.deflate(stream, ZLibAPI::Flush::NO_FLUSH);
        const auto offsetOut  = output.sizeInBytes() - stream.avail_out;
        const auto offsetIn   = input.sizeInBytes() - stream.avail_in;
        const bool outSliceOk = output.sliceStart(offsetOut, output);
        const bool inSliceOk  = input.sliceStart(offsetIn, input);
        if (not inSliceOk or not outSliceOk)
            return Result::Error(AsyncStreamsResultCategory, AsyncStreamsError::CompressionSpanInvalid);
        switch (result)
        {
        case ZLibAPI::OK: // All good
            return Result(true);
        default: return error(result);
        }
    }

    static Result compressFinalize(ZLibAPI::Stream& stream, Span<char>& output, bool& streamEnded)
    {
        stream.next_in   = nullptr;
        stream.avail_in  = 0;
        stream.next_out  = reinterpret_cast<uint8_t*>(output.data());
        stream.avail_out = static_cast<unsigned int>(output.sizeInBytes());

        const auto result    = zlib.deflate(stream, ZLibAPI::Flush::FINISH);
        const auto offsetOut = output.sizeInBytes() - stream.avail_out;
        const bool slicesOk  = output.sliceStart(offsetOut, output);
        if (not slicesOk)
            return Result::Error(AsyncStreamsResultCategory, AsyncStreamsError::CompressionSpanInvalid);
        streamEnded = result == ZLibAPI::Error::STREAM_END;
        switch (result)
        {
        case ZLibAPI::OK:         // All good
        case ZLibAPI::BUF_ERROR:  // Returned when output space is insufficient
        case ZLibAPI::STREAM_END: // Stream Ended
            return Result(true);
        default: return error(result);
        }
    }

    static Result decompress(ZLibAPI::Stream& stream, Span<const char>& input, Span<char>& output)
    {
        stream.next_in   = reinterpret_cast<const uint8_t*>(input.data());
        stream.avail_in  = static_cast<unsigned int>(input.sizeInBytes());
        stream.next_out  = reinterpret_cast<uint8_t*>(output.data());
        stream.avail_out = static_cast<unsigned int>(output.sizeInBytes());

        const auto result     = zlib.inflate(stream, ZLibAPI::Flush::NO_FLUSH);
        const auto offsetOut  = output.sizeInBytes() - stream.avail_out;
        const auto offsetIn   = input.sizeInBytes() - stream.avail_in;
        const bool outSliceOk = output.sliceStart(offsetOut, output);
        const bool inSliceOk  = input.sliceStart(offsetIn, input);

        if (not inSliceOk or not outSliceOk)
            return Result::Error(AsyncStreamsResultCategory, AsyncStreamsError::CompressionSpanInvalid);
        switch (result)
        {
        case ZLibAPI::OK:         // All good
        case ZLibAPI::STREAM_END: // Stream ended
            return Result(true);
        default: return error(result);
        }
    }

    static Result decompressFinalize(ZLibAPI::Stream& stream, Span<char>& output, bool& streamEnded)
    {
        // Intentionally not resetting next_in / avail_in, that can contain leftover data to process
        stream.next_out  = reinterpret_cast<uint8_t*>(output.data());
        stream.avail_out = static_cast<unsigned int>(output.sizeInBytes());

        const auto result    = zlib.inflate(stream, ZLibAPI::Flush::FINISH);
        const auto offsetOut = output.sizeInBytes() - stream.avail_out;
        const bool slicesOk  = output.sliceStart(offsetOut, output);
        if (not slicesOk)
            return Result::Error(AsyncStreamsResultCategory, AsyncStreamsError::CompressionSpanInvalid);
        streamEnded = result == ZLibAPI::Error::STREAM_END;
        switch (result)
        {
        case ZLibAPI::OK:         // All good
        case ZLibAPI::BUF_ERROR:  // Returned when output space is insufficient
        case ZLibAPI::STREAM_END: // Stream Ended
            return Result(true);
        default: return error(result);
        }
    }
};

SC::ZLibStream::ZLibStream() {}

SC::ZLibStream::~ZLibStream()
{
    if (state != State::Inited)
        return;
    ZLibAPI::Stream& stream = buffer.reinterpret_as<ZLibAPI::Stream>();
    switch (algorithm)
    {
    case Algorithm::CompressZLib:
    case Algorithm::CompressGZip:
    case Algorithm::CompressDeflate:
        // Cleanup compression data
        zlib.deflateEnd(stream);
        break;
    case Algorithm::DecompressZLib:
    case Algorithm::DecompressGZip:
    case Algorithm::DecompressDeflate:
        // Cleanup decompression data
        zlib.inflateEnd(stream);
        break;
    }
    zlib.unload();
}

SC::Result SC::ZLibStream::init(Algorithm wantedAlgorithm)
{
    ZLibAPI::Stream& stream = buffer.reinterpret_as<ZLibAPI::Stream>();
    if (state != State::Constructed)
        return Result::Error(AsyncStreamsResultCategory, AsyncStreamsError::CompressionAlreadyInitialized);

    SC_TRY(zlib.load());

    algorithm = wantedAlgorithm;

    // Initialize the stream
    stream.zalloc = nullptr;
    stream.zfree  = nullptr;
    stream.opaque = nullptr;

    ZLibAPI::Error ret = ZLibAPI::Error::STREAM_ERROR;
    switch (wantedAlgorithm)
    {
    case Algorithm::CompressZLib:
        // Zlib requires deflateInit2 and ZLibAPI::MaxBits
        ret = zlib.deflateInit2(stream, ZLibAPI::DEFAULT_COMPRESSION, ZLibAPI::DEFLATED, ZLibAPI::MaxBits, 8,
                                ZLibAPI::DEFAULT_STRATEGY);
        break;
    case Algorithm::CompressGZip:
        // GZip requires deflateInit2 and 16 + ZLibAPI::MaxBits
        ret = zlib.deflateInit2(stream, ZLibAPI::DEFAULT_COMPRESSION, ZLibAPI::DEFLATED, 16 + ZLibAPI::MaxBits, 8,
                                ZLibAPI::DEFAULT_STRATEGY);
        break;
    case Algorithm::CompressDeflate:
        // Deflate requires deflateInit2 and 16 + ZLibAPI::MaxBits
        ret = zlib.deflateInit2(stream, ZLibAPI::DEFAULT_COMPRESSION, ZLibAPI::DEFLATED, -ZLibAPI::MaxBits, 8,
                                ZLibAPI::DEFAULT_STRATEGY);
        break;
    case Algorithm::DecompressZLib:
        // Zlib requires inflateInit2 and ZLibAPI::MaxBits
        ret = zlib.inflateInit2(stream, ZLibAPI::MaxBits);
        break;
    case Algorithm::DecompressGZip:
        // GZip requires inflateInit2 and 16 + ZLibAPI::MaxBits
        ret = zlib.inflateInit2(stream, 16 + ZLibAPI::MaxBits);
        break;
    case Algorithm::DecompressDeflate:
        // Deflate requires inflateInit2 and 16 + ZLibAPI::MaxBits
        ret = zlib.inflateInit2(stream, -ZLibAPI::MaxBits);
        break;
    }
    if (ret == ZLibAPI::Error::OK)
    {
        state = State::Inited;
        return Result(true);
    }
    zlib.unload();
    return Internal::error(ret);
}

SC::Result SC::ZLibStream::process(Span<const char>& input, Span<char>& output)
{
    if (state != State::Inited)
        return Result::Error(AsyncStreamsResultCategory, AsyncStreamsError::CompressionNotInitialized);
    if (output.empty())
        return Result::Error(AsyncStreamsResultCategory, AsyncStreamsError::CompressionOutputBufferEmpty);
    ZLibAPI::Stream& stream = buffer.reinterpret_as<ZLibAPI::Stream>();
    switch (algorithm)
    {
    case Algorithm::CompressZLib:
    case Algorithm::CompressGZip:
    case Algorithm::CompressDeflate:
        // Compression
        return Internal::compress(stream, input, output);
    case Algorithm::DecompressZLib:
    case Algorithm::DecompressGZip:
    case Algorithm::DecompressDeflate:
        // Decompression
        return Internal::decompress(stream, input, output);
    }
    AsyncStreamsAssert::unreachable();
}

SC::Result SC::ZLibStream::finalize(Span<char>& output, bool& streamEnded)
{
    if (state != State::Inited)
        return Result::Error(AsyncStreamsResultCategory, AsyncStreamsError::CompressionNotInitialized);
    ZLibAPI::Stream& stream = buffer.reinterpret_as<ZLibAPI::Stream>();
    switch (algorithm)
    {
    case Algorithm::CompressZLib:
    case Algorithm::CompressGZip:
    case Algorithm::CompressDeflate:
        // Compression finalization
        return Internal::compressFinalize(stream, output, streamEnded);
    case Algorithm::DecompressZLib:
    case Algorithm::DecompressGZip:
    case Algorithm::DecompressDeflate:
        // Decompression finalization
        return Internal::decompressFinalize(stream, output, streamEnded);
    }
    AsyncStreamsAssert::unreachable();
}
