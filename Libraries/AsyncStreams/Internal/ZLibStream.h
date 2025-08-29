// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../../Foundation/AlignedStorage.h"
#include "../../Foundation/Result.h"
#include "../../Foundation/Span.h"
namespace SC
{
//! @addtogroup group_async_streams
//! @{

/// @brief Compresses or decompresses byte streams using gzip, zlib or deflate.
/// @n
/// Data can be added until needed with SC::ZLibStream::process call.
/// SC::ZLibStream::finalize will compute any end-of-stream data if needed.
struct ZLibStream
{
    enum Algorithm
    {
        CompressZLib,     ///< Use ZLIB algorithm to compress
        DecompressZLib,   ///< Use ZLIB algorithm to decompress
        CompressGZip,     ///< Use GZIP algorithm to compress
        DecompressGZip,   ///< Use GZIP algorithm to decompress
        CompressDeflate,  ///< Use DEFLATE algorithm to compress
        DecompressDeflate ///< Use DEFLATE algorithm to decompress
    };
    /// @brief Initializes a ZLibStream struct
    ZLibStream();

    /// @brief Destroys an ZLibStream struct
    ~ZLibStream();

    ZLibStream(const ZLibStream&)            = delete;
    ZLibStream(ZLibStream&&)                 = delete;
    ZLibStream& operator=(const ZLibStream&) = delete;
    ZLibStream& operator=(ZLibStream&&)      = delete;

    /// @brief Inits the compressor / decompressor with the required algorithm
    /// @param wantedAlgorithm Wanted algorithm (ZLIB, GZIP or DEFLATE with compression or decompression)
    /// @return Valid Result if the algorithm has been inited successfully
    Result init(Algorithm wantedAlgorithm);

    /// @brief Add data to be processed. Can be called multiple times before ZLibStream::finalize.
    /// @param input Span containing data to be processed, that will be modified pointing to data not (yet)
    /// processed due to insufficient output space.
    /// @param output Writable memory receiving processed data. It will then point to unused memory.
    /// @return Valid Result if data has been processed successfully
    Result process(Span<const char>& input, Span<char>& output);

    /// @brief Finalize stream by computing CRC or similar footers if needed (depending on the choosen Algorithm)
    /// @param output Writable memory receiving processed data. It will then point to unused memory.
    /// @param streamEnded Will be set to `true` if the stream has ended.
    /// @return Valid Result if no error has happened during finalization
    Result finalize(Span<char>& output, bool& streamEnded);

  private:
    struct Internal;
    AlignedStorage<112> buffer;

    enum class State
    {
        Constructed,
        Inited,
    };
    State     state     = State::Constructed;
    Algorithm algorithm = Algorithm::CompressZLib;
};

//! @}
} // namespace SC
