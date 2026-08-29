// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../Common/CompilerMove.h"
#include "AsyncStreams.h"

//! @addtogroup group_async_streams
//! @{
namespace SC
{
/// @brief Adapts an allocation-free streaming cipher session to AsyncTransformStream.
/// @tparam T_Cipher Session type providing update(), finish(), and reset() with the same span-based shape as
/// Cryptography::Cipher. The concrete cryptography library is only required where this template is instantiated.
/// @warning The current Cryptography::Cipher implementation is AES-CBC with PKCS#7 padding. It does not authenticate
/// ciphertext and must only be used inside an existing authenticated protocol.
/// @warning Decryption can emit plaintext before final padding validation. Do not release it before the surrounding
/// protocol has authenticated the ciphertext.
/// @note Writable output buffers must be at least 16 bytes. Input and output buffers never overlap.
template <typename T_Cipher>
struct AsyncCipherTransformStreamT : public AsyncTransformStream
{
    T_Cipher cipher;

    AsyncCipherTransformStreamT() = default;

    template <typename T_Argument>
    explicit AsyncCipherTransformStreamT(T_Argument&& argument) : cipher(forward<T_Argument>(argument))
    {}

  private:
    static constexpr size_t BlockSize             = 16;
    static constexpr size_t MaximumUpdateOverhead = BlockSize - 1;

    virtual Result onProcess(Span<const char> input, Span<char> output) override
    {
        if (output.sizeInBytes() < BlockSize)
        {
            cipher.reset();
            AsyncWritableStream::emitError(
                Result::Error("AsyncCipherTransformStreamT - output buffers must be at least 16 bytes"));
            AsyncTransformStream::afterProcess({}, output);
            return Result(true);
        }

        const size_t     inputSize = input.sizeInBytes() < output.sizeInBytes() - MaximumUpdateOverhead
                                         ? input.sizeInBytes()
                                         : output.sizeInBytes() - MaximumUpdateOverhead;
        Span<const char> inputChunk;
        SC_TRY(input.sliceStartLength(0, inputSize, inputChunk));

        size_t bytesWritten = 0;
        Result result       = cipher.update(inputChunk.reinterpret_as_span_of<const uint8_t>(),
                                            output.reinterpret_as_span_of<uint8_t>(), bytesWritten);
        if (not result or bytesWritten > output.sizeInBytes())
        {
            cipher.reset();
            AsyncWritableStream::emitError(result ? Result::Error("AsyncCipherTransformStreamT - invalid output size")
                                                  : result);
            AsyncTransformStream::afterProcess({}, output);
            return Result(true);
        }

        Span<const char> inputAfter;
        Span<char>       outputAfter;
        SC_TRY(input.sliceStart(inputSize, inputAfter));
        SC_TRY(output.sliceStart(bytesWritten, outputAfter));
        AsyncTransformStream::afterProcess(inputAfter, outputAfter);
        return Result(true);
    }

    virtual Result onFinalize(Span<char> output) override
    {
        if (output.sizeInBytes() < BlockSize)
        {
            cipher.reset();
            AsyncWritableStream::emitError(
                Result::Error("AsyncCipherTransformStreamT - output buffers must be at least 16 bytes"));
            AsyncTransformStream::afterFinalize(output, true);
            return Result(true);
        }

        size_t bytesWritten = 0;
        Result result       = cipher.finish(output.reinterpret_as_span_of<uint8_t>(), bytesWritten);
        if (not result or bytesWritten > output.sizeInBytes())
        {
            cipher.reset();
            AsyncWritableStream::emitError(result ? Result::Error("AsyncCipherTransformStreamT - invalid output size")
                                                  : result);
            AsyncTransformStream::afterFinalize(output, true);
            return Result(true);
        }

        Span<char> outputAfter;
        SC_TRY(output.sliceStart(bytesWritten, outputAfter));
        AsyncTransformStream::afterFinalize(outputAfter, true);
        return Result(true);
    }

    virtual Result asyncDestroyReadable() override
    {
        cipher.reset();
        return AsyncReadableStream::finishedDestroyingReadable();
    }

    virtual Result asyncDestroyWritable() override
    {
        cipher.reset();
        AsyncWritableStream::finishedDestroyingWritable();
        return Result(true);
    }
};

/// @brief Adapts an incremental message-authentication session to AsyncWritableStream.
/// @tparam T_Hmac Session type providing add() and reset() with the same span-based shape as Cryptography::Hmac.
/// @note Retrieve the completed MAC from hmac inside eventFinish. Automatic stream destruction resets the session
/// immediately after that event returns.
template <typename T_Hmac>
struct AsyncHmacWritableStreamT : public AsyncWritableStream
{
    T_Hmac hmac;

    AsyncHmacWritableStreamT() = default;

    template <typename T_Argument>
    explicit AsyncHmacWritableStreamT(T_Argument&& argument) : hmac(forward<T_Argument>(argument))
    {}

  private:
    virtual Result asyncWrite(AsyncBufferView::ID bufferID, Function<void(AsyncBufferView::ID)> callback) override
    {
        Span<const char> data;
        SC_TRY(getBuffersPool().getReadableData(bufferID, data));
        Result result = hmac.add(data.reinterpret_as_span_of<const uint8_t>());
        if (not result)
            hmac.reset();
        finishedWriting(bufferID, move(callback), result);
        return Result(true);
    }

    virtual Result asyncDestroyWritable() override
    {
        hmac.reset();
        finishedDestroyingWritable();
        return Result(true);
    }
};
} // namespace SC
//! @}
