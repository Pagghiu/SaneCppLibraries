// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "HttpStringAppend.h"
#include "Libraries/Common/Assert.h"
#include "Libraries/Http/HttpAsyncClient.h"
#include "Libraries/Memory/Buffer.h"

namespace HttpTestHelpers
{
struct ResponseCollector
{
    SC::Buffer buffer;

    SC::AsyncReadableStream*     readable = nullptr;
    SC::HttpAsyncClientResponse* response = nullptr;

    SC::Function<void(SC::HttpAsyncClientResponse&)> onEnd;

    void reset() { buffer = {}; }

    SC::Result append(SC::Span<const char> data)
    {
        SC::GrowableBuffer<SC::Buffer> gb(buffer);

        SC::HttpStringAppend& sb = static_cast<SC::HttpStringAppend&>(static_cast<SC::IGrowableBuffer&>(gb));
        return SC::Result(sb.append(data, 0));
    }

    void attach(SC::HttpAsyncClientResponse&                       newResponse,
                SC::Function<void(SC::HttpAsyncClientResponse&)>&& endCallback = {})
    {
        detach();
        reset();
        response = &newResponse;
        readable = &newResponse.getReadableStream();
        onEnd    = SC::move(endCallback);

        const bool added = readable->eventData.addListener<ResponseCollector, &ResponseCollector::onData>(*this);
        SC_ASSERT_RELEASE(added);
        if (onEnd.isValid())
        {
            const bool addedEnd =
                readable->eventEnd.addListener<ResponseCollector, &ResponseCollector::onStreamEnd>(*this);
            SC_ASSERT_RELEASE(addedEnd);
        }
    }

    void detach()
    {
        if (readable)
        {
            (void)readable->eventData.removeListener<ResponseCollector, &ResponseCollector::onData>(*this);
            if (onEnd.isValid())
            {
                (void)readable->eventEnd.removeListener<ResponseCollector, &ResponseCollector::onStreamEnd>(*this);
            }
            readable = nullptr;
        }
        response = nullptr;
        onEnd    = {};
    }

    void onData(SC::AsyncBufferView::ID bufferID)
    {
        SC::Span<const char> data;
        SC_ASSERT_RELEASE(readable != nullptr);
        SC_ASSERT_RELEASE(readable->getBuffersPool().getReadableData(bufferID, data));
        SC_ASSERT_RELEASE(append(data));
    }

    void onStreamEnd()
    {
        SC_ASSERT_RELEASE(response != nullptr);
        if (onEnd.isValid())
        {
            onEnd(*response);
        }
    }

    [[nodiscard]] SC::StringSpan view() const { return {buffer.toSpanConst(), false, SC::StringEncoding::Ascii}; }
};

struct ChunkedBodyStream : public SC::AsyncReadableStream
{
    SC::AsyncReadableStream::Request readQueue[2];

    SC::AsyncBufferView::ID rootBufferID;
    SC::Span<const char>    sourceData;

    SC::size_t offset    = 0;
    SC::size_t chunkSize = 0;

    ChunkedBodyStream() { setReadQueue(readQueue); }

    SC::Result init(SC::AsyncBuffersPool& pool, SC::Span<const char> data, SC::size_t chunk)
    {
        sourceData   = data;
        offset       = 0;
        chunkSize    = chunk;
        rootBufferID = {};
        SC_TRY(SC::AsyncReadableStream::init(pool));
        SC_TRY(pool.pushBuffer(SC::AsyncBufferView(data), rootBufferID));
        pool.refBuffer(rootBufferID);
        return SC::Result(true);
    }

  private:
    virtual SC::Result asyncRead() override
    {
        if (offset >= sourceData.sizeInBytes())
        {
            pushEnd();
            return SC::Result(true);
        }

        const SC::size_t remaining = sourceData.sizeInBytes() - offset;
        const SC::size_t length    = chunkSize < remaining ? chunkSize : remaining;

        SC::AsyncBufferView::ID childID;
        SC_TRY(getBuffersPool().createChildView(rootBufferID, offset, length, childID));
        const bool shouldContinue = push(childID, length);
        getBuffersPool().unrefBuffer(childID);
        offset += length;
        reactivate(offset < sourceData.sizeInBytes() and shouldContinue);
        return SC::Result(true);
    }

    virtual SC::Result asyncDestroyReadable() override
    {
        if (rootBufferID.isValid())
        {
            getBuffersPool().unrefBuffer(rootBufferID);
            rootBufferID = {};
        }
        return finishedDestroyingReadable();
    }
};
} // namespace HttpTestHelpers
