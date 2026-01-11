// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once

#include "../Foundation/AlignedStorage.h"
#include "../Foundation/Function.h"
#include "../Foundation/Internal/IGrowableBuffer.h"
#include "../Foundation/Result.h"
#include "../Foundation/Span.h"
#include "Internal/CircularQueue.h"
#include "Internal/Event.h"

//! @defgroup group_async_streams Async Streams
//! Read, transform and write data concurrently from async sources to destinations.
//!
/// Read, writes and transforms happen in parallel if sources and destinations are asynchronous.
/// This library does not allocate any memory, all buffers are supplied by the caller.
///
/// Async Streams are largely inspired by [node.js Streams](https://nodejs.org/api/stream.html), a very powerful tool to
/// process large amounts of data concurrently.
///
/// The basic idea about an async stream is to create a Source / Sink abstraction (also called Readable and Writable)
/// and process small buffers of data at time.
///
/// The state machine that coordinates this interaction handles data buffering and more importantly handles also
/// back-pressure, that means:
///
/// - **Pausing** the readable stream when a connected writable stream cannot process data fast enough
/// - **Resuming** the readable stream when a connected writable stream is finally able to receive more data
///
/// By implementing streams on top of async operations it's possible to run many of them concurrently very efficiently.
/// When properly implemented for example an async pipeline can concurrently read from disk, write to a socket while
/// compressing data.
///
///
/// Most notable differences with node.js streams are for now:
///
/// - No allocation (designed to work inside user-provided list of buffers)
/// - No object mode
/// - Fixed Layout to create data pipelines (AsyncPipeline)
/// - onData support only (no readable event)

//! @addtogroup group_async_streams
//! @{
namespace SC
{

/// @brief A Span of bytes memory to be read or written by async streams
struct SC_COMPILER_EXPORT AsyncBufferView
{
    struct SC_COMPILER_EXPORT ID
    {
        using NumericType = int32_t;
        NumericType                  identifier;
        static constexpr NumericType InvalidValue = -1;

        constexpr ID() : identifier(InvalidValue) {}
        explicit constexpr ID(int32_t value) : identifier(value) {}

        [[nodiscard]] constexpr bool operator==(ID other) const { return identifier == other.identifier; }
    };
    enum class Type : uint8_t
    {
        Empty,
        Writable,
        ReadOnly,
        Growable,
        Child,
    };

    AsyncBufferView() : writableData(), offset(0), length(0), refs(0), type(Type::Empty), reUse(false) {}
    AsyncBufferView(Span<char> data) : writableData(data)
    {
        type     = Type::Writable;
        offset   = 0;
        length   = data.sizeInBytes();
        parentID = ID();
    }
    AsyncBufferView(Span<const char> data) : readonlyData(data)
    {
        type     = Type::ReadOnly;
        offset   = 0;
        length   = data.sizeInBytes();
        parentID = ID();
    }

    /// @brief Tags this AsyncBufferView as reusable after its refCount goes to zero
    void setReusable(bool reusable) { reUse = reusable; }

    /// @brief Saves a copy (or a moved instance) of a String / Buffer (or anything that works with GrowableBuffer<T>)
    /// inside an AsyncBufferView in order to access its data later, as long as its size fits inside the inline storage.
    /// Destroying the AsyncBufferView will also destroy the copied / moved instance.
    template <typename T>
    AsyncBufferView(T&& t) // universal reference, it can capture both lvalue and rvalue
    {
        type     = Type::Growable;
        offset   = 0;
        length   = 0;
        parentID = ID();
        // Here we're type-erasing T in our own inline storage provided by a slightly oversized Function<>
        // that it will be able to construct (and destruct) the right GrowableBuffer<T> from just a piece of storage
        // and return a pointer to the corresponding IGrowableBuffer* interface
        getGrowableBuffer = [t = forward<T>(t)](GrowableStorage& storage, bool construct) mutable -> IGrowableBuffer*
        {
            using Type = typename TypeTraits::RemoveReference<T>::type;
            if (construct)
            {
                placementNew(storage.reinterpret_as<GrowableBuffer<Type>>(), t);
                return &storage.reinterpret_as<GrowableBuffer<Type>>();
            }
            else
            {
                dtor(storage.reinterpret_as<GrowableBuffer<Type>>());
                return nullptr;
            }
        };
    }

    template <int N>
    AsyncBufferView(const char (&literal)[N])
    {
        readonlyData = {literal, N - 1};
        type         = Type::ReadOnly;
        offset       = 0;
        length       = N - 1;
    }

    Type getType() const { return type; }

  private:
#if SC_PLATFORM_64_BIT
    static constexpr int TypeErasedCaptureSize = sizeof(void*) * 3; // This is enough to hold String / Buffer by copy
#else
    static constexpr int TypeErasedCaptureSize = sizeof(void*) * 6; // This is enough to hold String / Buffer by copy
#endif
    static constexpr int TypeErasedGrowableSize = sizeof(void*) * 6;

    using GrowableStorage = AlignedStorage<TypeErasedGrowableSize>;
    Function<IGrowableBuffer*(GrowableStorage&, bool), TypeErasedCaptureSize> getGrowableBuffer;

    union
    {
        Span<char>       writableData;
        Span<const char> readonlyData;
    };
    AsyncBufferView::ID parentID;

    friend struct AsyncBuffersPool;

    size_t  offset = 0;
    size_t  length = 0;
    int32_t refs   = 0;           // Counts AsyncReadable (single) or AsyncWritable (multiple) using it
    Type    type   = Type::Empty; // If it's Empty, Writable, ReadOnly, Growable or Child
    bool    reUse  = false;       // If it can be re-used after refs == 0
};

/// @brief Holds a Span of AsyncBufferView (allocated by user) holding available memory for the streams
/// @note User must fill the AsyncBuffersPool::buffers with a `Span` of AsyncBufferView
struct SC_COMPILER_EXPORT AsyncBuffersPool
{
    /// @brief Increments a buffer reference count
    void refBuffer(AsyncBufferView::ID bufferID);

    /// @brief Decrements a buffer reference count.
    /// When reference count becomes zero the buffer will be re-used
    void unrefBuffer(AsyncBufferView::ID bufferID);

    /// @brief Access data span owned by the buffer
    Result getReadableData(AsyncBufferView::ID bufferID, Span<const char>& data);

    /// @brief Access data span owned by the buffer
    Result getWritableData(AsyncBufferView::ID bufferID, Span<char>& data);

    /// @brief Access the raw AsyncBufferView (if any) at a given bufferID (or nullptr if invalid)
    AsyncBufferView* getBuffer(AsyncBufferView::ID bufferID);

    /// @brief Requests a new available buffer that is at least minimumSizeInBytes, incrementing its refcount
    Result requestNewBuffer(size_t minimumSizeInBytes, AsyncBufferView::ID& bufferID, Span<char>& data);

    /// @brief Sets the new size in bytes for the buffer
    void setNewBufferSize(AsyncBufferView::ID bufferID, size_t newSizeInBytes);

    /// @brief Adds a buffer to the pool in any empty slot (found by scanning from start to end)
    Result pushBuffer(AsyncBufferView&& buffer, AsyncBufferView::ID& bufferID);

    /// @brief Splits a span of memory in equally sized slices, assigning them to buffers and marking them as reusable
    static Result sliceInEqualParts(Span<AsyncBufferView> buffers, Span<char> memory, size_t numSlices);

    /// @brief Sets memory for the new buffers
    void setBuffers(Span<AsyncBufferView> newBuffers) { buffers = newBuffers; }

    /// @brief Gets size of buffers held by the pool
    [[nodiscard]] size_t getNumBuffers() const { return buffers.sizeInElements(); }

    /// @brief Creates a child view that references a slice of the parent buffer
    Result createChildView(AsyncBufferView::ID parentBufferID, size_t offset, size_t length,
                           AsyncBufferView::ID& outChildBufferID);

  private:
    /// @brief Span of buffers to be filled in by the user
    Span<AsyncBufferView> buffers;
};

/// @brief Async source abstraction emitting data events in caller provided byte buffers.
/// After AsyncReadableStream::start it will start emitting AsyncReadableStream::eventData with buffers.
/// User must provide a custom async red implementation in AsyncReadableStream::asyncRead.
/// The stream must be paused when the AsyncBuffersPool is full (use AsyncReadableStream::getBufferOrPause).
/// Once the stream is ended, it will emit AsyncReadableStream::eventEnd and it cannot be used further.
/// AsyncReadableStream::eventError will be emitted when an error occurs in any phase.
struct SC_COMPILER_EXPORT AsyncReadableStream
{
    struct Request
    {
        AsyncBufferView::ID bufferID;
    };
    /// @brief Function that every stream must define to implement its custom read operation
    Function<Result()> asyncRead;

    static constexpr int MaxListeners = 8;

    Event<MaxListeners, Result>              eventError; /// Emitted when an error occurs
    Event<MaxListeners, AsyncBufferView::ID> eventData;  /// Emitted when a new buffer has been read
    Event<MaxListeners>                      eventEnd;   /// Emitted when there is no more data
    Event<MaxListeners>                      eventClose; /// Emitted when the underlying resource has been closed

    /// @brief Inits the readable stream with an AsyncBuffersPool instance that will provide memory for it
    /// @param buffersPool An instance of AsyncBuffersPool providing read buffers
    /// @note Remember to call AsyncReadableStream::setReadQueue before calling init
    Result init(AsyncBuffersPool& buffersPool);

    /// @brief Starts the readable stream, that will emit eventData
    Result start();

    /// @brief Pauses the readable stream (that can be later resumed)
    void pause();

    /// @brief Resumes the readable stream paused by AsyncReadableStream::pause
    void resumeReading();

    /// @brief Forcefully destroys the readable stream before calling end event releasing all resources
    /// @note It's safe to call destroy in any state and also when already destroyed (it's idempotent)
    void destroy();

    /// @brief Returns true if the stream is ended (AsyncReadableStream::end has been called)
    [[nodiscard]] bool isEnded() const { return state == State::Ended; }

    /// @brief Obtains the AsyncBuffersPool to request more buffers
    AsyncBuffersPool& getBuffersPool();

    /// @brief Sets the read queue for this readable stream
    constexpr void setReadQueue(Span<Request> requests) { readQueue = requests; }

    /// @brief Returns the size of read queue
    [[nodiscard]] size_t getReadQueueSize() const { return readQueue.size(); }

    /// @brief Use push from inside AsyncReadableStream::asyncRead function to queue received data
    /// @return `true` if the caller can continue pushing
    [[nodiscard]] bool push(AsyncBufferView::ID bufferID, size_t newSize);

    /// @brief Use pushEnd from inside AsyncReadableStream::asyncRead to signal production end
    void pushEnd();

    /// @brief Use reactivate(true) from inside AsyncReadableStream::asyncRead function to ask the
    /// state machine to invoke asyncRead again.
    void reactivate(bool doReactivate);

    /// @brief Signals an async error received
    void emitError(Result error);

    /// @brief Returns an unused buffer from pool or pauses the stream if none is available
    [[nodiscard]] bool getBufferOrPause(size_t minumumSizeInBytes, AsyncBufferView::ID& bufferID, Span<char>& data);

  private:
    void emitOnData();
    void executeRead();

    enum class State
    {
        Stopped,      // Stream must be inited
        CanRead,      // Stream is ready to issue a read ( AsyncReadableStream::start / AsyncReadableStream::resume)
        Reading,      // A read is being issued (may be sync or async)
        SyncPushing,  // One or multiple AsyncReadableStream::push have been received (sync)
        SyncReadMore, // SyncPushing + AsyncReadableStream::reactivate(true)
        AsyncReading, // An async read is in flight
        AsyncPushing, // AsyncReading + AsyncReadableStream::push
        Pausing,      // Pause requested while read in flight
        Paused,       // Actually paused with no read in flight
        Ended,        // Emitted all data, no more data will be emitted
        Destroyed,    // Readable has been destroyed before emitting all data
        Errored,      // Error occurred
    };
    State state = State::Stopped;

    AsyncBuffersPool* buffers = nullptr;

    CircularQueue<Request> readQueue;
};

/// @brief Async destination abstraction where bytes can be written to.
/// When buffers are pushed faster than the stream can handle, they will get queued.
/// Queuing process happens with a linked list stored in the AsyncBufferView itself.
/// As AsyncBufferView contains a fixed (at init) number of buffers, the queue is bounded
/// by the fact that user will be unable to allocate buffers to write until at least one
/// will be made available again (i.e. a write finishes).
/// User can listen to AsyncWritableStream::eventWritten to know when a buffer is written
/// (and its refcount decreased) or AsyncWritableStream::eventDrain when the queue is empty.
struct SC_COMPILER_EXPORT AsyncWritableStream
{
    /// @brief Function that every stream must define to implement its custom write operation
    Function<Result(AsyncBufferView::ID, Function<void(AsyncBufferView::ID)>)> asyncWrite;

    struct Request
    {
        AsyncBufferView::ID bufferID;

        Function<void(AsyncBufferView::ID)> cb;
    };
    static constexpr int MaxListeners = 8;

    Event<MaxListeners, Result> eventError; /// Emitted when an error occurs

    Event<MaxListeners> eventDrain;  /// Emitted when write queue is empty
    Event<MaxListeners> eventFinish; /// Emitted when no more data can be written

    /// @brief Inits the writable stream
    /// @param buffersPool An instance of AsyncBuffersPool providing write buffers
    /// @note Remember to call AsyncWritableStream::setWriteQueue before calling init
    Result init(AsyncBuffersPool& buffersPool);

    /// @brief Sets the write queue for this writable stream
    constexpr void setWriteQueue(Span<Request> requests) { writeQueue = requests; }

    /// @brief Returns the size of write queue
    [[nodiscard]] size_t getWriteQueueSize() const { return writeQueue.size(); }

    /// @brief Writes a buffer (that must be allocated by the AsyncBuffersPool passed in AsyncWritableStream)
    /// When the buffer it will be actually written, AsyncWritableStream::eventWritten will be raised and
    /// its reference count will be decreased.
    /// @param bufferID Buffer allocated from the associated AsyncBuffersPool (AsyncWritableStream::getBuffersPool)
    /// @param cb Callback that will be invoked when the write is finished
    /// @return Invalid Result if write queue is full
    Result write(AsyncBufferView::ID bufferID, Function<void(AsyncBufferView::ID)> cb = {});

    /// @brief Push a new buffer view to the queue, registering it with the allocator
    /// @return Invalid Result if write queue is full or if there are no available empty buffers slots in the pool
    Result write(AsyncBufferView&& bufferView, Function<void(AsyncBufferView::ID)> cb = {});

    /// @brief Ends the writable stream, waiting for all in-flight and queued writes to finish.
    /// After this happens, AsyncWritableStream::eventFinished will be raised
    void end();

    /// @brief Forcefully destroys the writable stream before calling end event releasing all resources
    /// @note It's safe to call destroy in any state and also when already destroyed (it's idempotent)
    void destroy();

    /// @brief Obtains the buffers pool to access its data
    AsyncBuffersPool& getBuffersPool();

    /// @brief Signals that the given buffer (previously queued by write) has been fully written
    void finishedWriting(AsyncBufferView::ID bufferID, Function<void(AsyncBufferView::ID)>&& cb, Result res);

    /// @brief Resumes writing queued requests for this stream
    void resumeWriting();

    /// @brief Puts back a buffer at the top of the write queue
    Result unshift(AsyncBufferView::ID bufferID, Function<void(AsyncBufferView::ID)>&& cb);

    /// @brief Signals an async error received
    void emitError(Result error);

    /// @brief Allows keeping a writable in ENDING state until it has finished flushing all pending data.
    /// If a writable stream redefines this function it should return true to allow transitioning to ENDED
    /// state and return false to keep staying in ENDING state.
    Function<bool()> canEndWritable;

    /// @brief Will emit error if the passed in Result is false
    void tryAsync(Result potentialError);

    /// @brief Returns true if this stream is writing something
    [[nodiscard]] bool isStillWriting() const { return state == State::Writing or state == State::Ending; }

  protected:
    void stop() { state = State::Stopped; }

  private:
    enum class State
    {
        Stopped,
        Writing,
        Ending,
        Ended
    };
    State state = State::Stopped;

    AsyncBuffersPool* buffers = nullptr;

    CircularQueue<Request> writeQueue;
};

/// @brief A stream that can both produce and consume buffers
struct SC_COMPILER_EXPORT AsyncDuplexStream : public AsyncReadableStream, public AsyncWritableStream
{
    AsyncDuplexStream();

    Result init(AsyncBuffersPool& buffersPool, Span<AsyncReadableStream::Request> readableRequests,
                Span<AsyncWritableStream::Request> writableRequests);
};

/// @brief A duplex stream that produces new buffers transforming received buffers
struct SC_COMPILER_EXPORT AsyncTransformStream : public AsyncDuplexStream
{
    AsyncTransformStream();

    void afterProcess(Span<const char> inputAfter, Span<char> outputAfter);
    void afterFinalize(Span<char> outputAfter, bool streamEnded);

    Function<Result(Span<const char>, Span<char>)> onProcess;
    Function<Result(Span<char>)>                   onFinalize;

  private:
    Function<void(AsyncBufferView::ID)> inputCallback;

    Span<const char> inputData;
    Span<char>       outputData;

    AsyncBufferView::ID inputBufferID;
    AsyncBufferView::ID outputBufferID;

    Result transform(AsyncBufferView::ID bufferID, Function<void(AsyncBufferView::ID)> cb);
    Result prepare(AsyncBufferView::ID bufferID, Function<void(AsyncBufferView::ID)> cb);

    bool canEndTransform();
    void tryFinalize();

    enum class State
    {
        None,
        Paused,
        Processing,
        Finalizing,
        Finalized,
    };
    State state = State::None;
};

/// @brief Pipes read data from SC::AsyncReadableStream, forwarding them to SC::AsyncWritableStream.
/// When the source provides data at a faster rate than what the sink (writable) is able to process,
/// or when running out of buffers to read data into, AsyncPipeline will AsyncReadableStream::pause
/// the source. This is called "back-pressure" handling in the Async Streams terminology.
/// When a writable has finished writing, AsyncReadableStream::resume will be called to try un-pausing.
/// Caller needs to set AsyncPipeline::source field and AsyncPipeline::sinks with valid streams.
/// @note It's crucial to use the same AsyncBuffersPool for the AsyncReadableStream and all AsyncWritableStream
struct SC_COMPILER_EXPORT AsyncPipeline
{
    static constexpr int MaxListeners  = 8;
    static constexpr int MaxTransforms = 8;
    static constexpr int MaxSinks      = 8;

    AsyncPipeline()                                = default;
    AsyncPipeline(const AsyncPipeline&)            = delete;
    AsyncPipeline(AsyncPipeline&&)                 = delete;
    AsyncPipeline& operator=(const AsyncPipeline&) = delete;
    AsyncPipeline& operator=(AsyncPipeline&&)      = delete;
    ~AsyncPipeline();

    AsyncReadableStream* source                    = nullptr;   /// Provided source (must be != nullptr)
    AsyncDuplexStream*   transforms[MaxTransforms] = {nullptr}; /// Provided transforms (optional, can be all nullptrs)
    AsyncWritableStream* sinks[MaxSinks]           = {nullptr}; /// Provided sinks (at least one must be != nullptr)
    Event<MaxListeners, Result> eventError         = {};        /// Reports errors by source, transforms or sinks

    /// @brief Pipes source, transforms and sinks together
    /// @note Caller must have already setup source and sinks (and optionally transforms)
    Result pipe();

    /// @brief Unregisters all events from source, transforms and sinks
    [[nodiscard]] bool unpipe();

    /// @brief Starts the pipeline
    /// @note Both source and sinks must have been already setup by the caller
    Result start();

    // TODO: Add a pause and cancel/step
  private:
    void   emitError(Result res);
    Result checkBuffersPool();
    Result chainTransforms(AsyncReadableStream*& readable);
    Result validate();

    void asyncWriteWritable(AsyncBufferView::ID bufferID, AsyncWritableStream& writable);
    void dispatchToPipes(AsyncBufferView::ID bufferID);
    void endPipes();
    void afterSinkEnd();
    void afterWrite(AsyncBufferView::ID bufferID);
    bool listenToEventData(AsyncReadableStream& readable, AsyncDuplexStream& transform, bool listen);
};
} // namespace SC
//! @}
