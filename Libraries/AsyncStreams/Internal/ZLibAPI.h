// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../../Foundation/Result.h"

namespace SC
{
struct ZLibAPI;
}
struct SC::ZLibAPI
{
    struct Stream
    {
        using alloc_func = void* (*)(void* opaque, unsigned int items, unsigned int size);
        using free_func  = void (*)(void* opaque, void* address);

        const unsigned char* next_in;  // Next input byte
        unsigned int         avail_in; // Number of bytes available at next_in
        unsigned long        total_in; // Total number of input bytes read so far

        unsigned char* next_out;  // Next output byte
        unsigned int   avail_out; // Number of bytes available at next_out
        unsigned long  total_out; // Total number of output bytes written so far

        const char* msg; // Last error message, NULL if no error

        void* state; // Internal state, not used directly by applications

        alloc_func* zalloc; // Custom memory allocation function (or NULL)
        free_func*  zfree;  // Custom memory deallocation function (or NULL)
        void*       opaque; // Opaque data passed to zalloc and zfree

        int           data_type; // Type of data (e.g., binary, text)
        unsigned long adler;     // Adler-32 checksum of the uncompressed data
        unsigned long reserved;  // Reserved for future use
    };
    static constexpr int         MaxBits = 15;
    static constexpr const char* Version = "1.2.12";

    enum Flush : int
    {
        NO_FLUSH      = 0,
        PARTIAL_FLUSH = 1,
        SYNC_FLUSH    = 2,
        FULL_FLUSH    = 3,
        FINISH        = 4,
        BLOCK         = 5,
        TREES         = 6,
    };

    enum Error : int
    {
        OK            = 0,
        STREAM_END    = 1,
        NEED_DICT     = 2,
        ERRNO         = -1,
        STREAM_ERROR  = -2,
        DATA_ERROR    = -3,
        MEM_ERROR     = -4,
        BUF_ERROR     = -5,
        VERSION_ERROR = -6,
    };

    enum Compression : int
    {
        NO_COMPRESSION      = 0,
        BEST_SPEED          = 1,
        BEST_COMPRESSION    = 9,
        DEFAULT_COMPRESSION = -1,
    };

    enum Strategy : int
    {
        FILTERED         = 1,
        HUFFMAN_ONLY     = 2,
        RLE              = 3,
        FIXED            = 4,
        DEFAULT_STRATEGY = 0,
    };

    enum Method : int
    {
        DEFLATED = 8,
    };

    Result load(const char* libPath = nullptr);

    void unload();

    Error deflate(Stream& strm, Flush flag) { return pDeflate(&strm, flag); }
    Error inflate(Stream& strm, Flush flag) { return pInflate(&strm, flag); }
    Error inflateEnd(Stream& strm) { return pInflateEnd(&strm); }
    Error deflateEnd(Stream& strm) { return pDeflateEnd(&strm); }

    Error deflateInit2(Stream& strm, Compression level, Method method, int windowBits, int memLevel, Strategy strategy)
    {
        return pDeflateInit2(&strm, level, method, windowBits, memLevel, strategy, Version,
                             static_cast<int>(sizeof(Stream)));
    }

    Error inflateInit2(Stream& strm, int windowBits)
    {
        return pInflateInit2(&strm, windowBits, Version, static_cast<int>(sizeof(Stream)));
    }

  private:
    // Function pointers for zlib functions
    Error (*pDeflate)(void* strm, Flush flush)                                               = nullptr;
    Error (*pDeflateEnd)(void* strm)                                                         = nullptr;
    Error (*pInflate)(void* strm, Flush flush)                                               = nullptr;
    Error (*pInflateEnd)(void* strm)                                                         = nullptr;
    Error (*pDeflateInit2)(void* strm, Compression level, Method method, int windowBits, int memLevel,
                           Strategy strategy, const char* version, int stream_size)          = nullptr;
    Error (*pInflateInit2)(void* strm, int windowBits, const char* version, int stream_size) = nullptr;

    // Handle for dynamic library
    void* library = nullptr;

    struct Internal;
};
