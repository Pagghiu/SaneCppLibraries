// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once

#include "../../Foundation/Assert.h"
#include "ZLibStream.h"

#include "ZLibAPI.h"
#include "ZLibAPI.inl" // IWYU pragma: keep

static SC::ZLibAPI zlib;

struct SC::ZLibStream::Internal
{
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
        SC_TRY_MSG(inSliceOk and outSliceOk, "compress sliceStart");
        switch (result)
        {
        case ZLibAPI::OK: // All good
            return Result(true);
        case ZLibAPI::BUF_ERROR: return Result::Error("BUF_ERROR");
        case ZLibAPI::STREAM_END: return Result::Error("STREAM_END");
        case ZLibAPI::NEED_DICT: return Result::Error("NEED_DICT");
        case ZLibAPI::ERRNO: return Result::Error("ERRNO");
        case ZLibAPI::STREAM_ERROR: return Result::Error("STREAM_ERROR");
        case ZLibAPI::DATA_ERROR: return Result::Error("DATA_ERROR");
        case ZLibAPI::MEM_ERROR: return Result::Error("MEM_ERROR");
        case ZLibAPI::VERSION_ERROR: return Result::Error("VERSION_ERROR");
        }
        return Result::Error("UNKNOWN");
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
        SC_TRY_MSG(slicesOk, "compressFinalize sliceStart");
        streamEnded = result == ZLibAPI::Error::STREAM_END;
        switch (result)
        {
        case ZLibAPI::OK:         // All good
        case ZLibAPI::BUF_ERROR:  // Returned when output space is insufficient
        case ZLibAPI::STREAM_END: // Stream Ended
            return Result(true);
        case ZLibAPI::NEED_DICT: return Result::Error("NEED_DICT");
        case ZLibAPI::ERRNO: return Result::Error("ERRNO");
        case ZLibAPI::STREAM_ERROR: return Result::Error("STREAM_ERROR");
        case ZLibAPI::DATA_ERROR: return Result::Error("DATA_ERROR");
        case ZLibAPI::MEM_ERROR: return Result::Error("MEM_ERROR");
        case ZLibAPI::VERSION_ERROR: return Result::Error("VERSION_ERROR");
        }
        return Result::Error("UNKNOWN");
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

        SC_TRY_MSG(inSliceOk and outSliceOk, "decompress sliceStart");
        switch (result)
        {
        case ZLibAPI::OK:         // All good
        case ZLibAPI::STREAM_END: // Stream ended
            return Result(true);
        case ZLibAPI::BUF_ERROR: return Result::Error("BUF_ERROR");
        case ZLibAPI::NEED_DICT: return Result::Error("NEED_DICT");
        case ZLibAPI::ERRNO: return Result::Error("ERRNO");
        case ZLibAPI::STREAM_ERROR: return Result::Error("STREAM_ERROR");
        case ZLibAPI::DATA_ERROR: return Result::Error("DATA_ERROR");
        case ZLibAPI::MEM_ERROR: return Result::Error("MEM_ERROR");
        case ZLibAPI::VERSION_ERROR: return Result::Error("VERSION_ERROR");
        }
        return Result::Error("UNKNOWN");
    }

    static Result decompressFinalize(ZLibAPI::Stream& stream, Span<char>& output, bool& streamEnded)
    {
        // Intentionally not resetting next_in / avail_in, that can contain leftover data to process
        stream.next_out  = reinterpret_cast<uint8_t*>(output.data());
        stream.avail_out = static_cast<unsigned int>(output.sizeInBytes());

        const auto result    = zlib.inflate(stream, ZLibAPI::Flush::FINISH);
        const auto offsetOut = output.sizeInBytes() - stream.avail_out;
        const bool slicesOk  = output.sliceStart(offsetOut, output);
        SC_TRY_MSG(slicesOk, "decompressFinalize sliceStart");
        streamEnded = result == ZLibAPI::Error::STREAM_END;
        switch (result)
        {
        case ZLibAPI::OK:         // All good
        case ZLibAPI::BUF_ERROR:  // Returned when output space is insufficient
        case ZLibAPI::STREAM_END: // Stream Ended
            return Result(true);
        case ZLibAPI::NEED_DICT: return Result::Error("NEED_DICT");
        case ZLibAPI::ERRNO: return Result::Error("ERRNO");
        case ZLibAPI::STREAM_ERROR: return Result::Error("STREAM_ERROR");
        case ZLibAPI::DATA_ERROR: return Result::Error("DATA_ERROR");
        case ZLibAPI::MEM_ERROR: return Result::Error("MEM_ERROR");
        case ZLibAPI::VERSION_ERROR: return Result::Error("VERSION_ERROR");
        }
        return Result::Error("UNKNOWN");
    }
};

SC::ZLibStream::ZLibStream() {}

SC::ZLibStream::~ZLibStream()
{
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
    SC_TRY_MSG(state == State::Constructed, "Init can be called only in State::Constructed");

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
    return Result::Error("ZLibStream::Init failed");
}

SC::Result SC::ZLibStream::process(Span<const char>& input, Span<char>& output)
{
    SC_TRY_MSG(not output.empty(), "ZLibStream::process empty output is not allowed");
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
    Assert::unreachable();
}

SC::Result SC::ZLibStream::finalize(Span<char>& output, bool& streamEnded)
{
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
    Assert::unreachable();
}
