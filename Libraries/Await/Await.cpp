// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "../Foundation/Compiler.h"

#if SC_INCLUDE_STD_CPP && SC_LANGUAGE_CPP_AT_LEAST_20

#include "../Foundation/Assert.h"
#include "../Foundation/PrimitiveTypes.h"
#include "Await.h"
#include <stdlib.h>
#if SC_PLATFORM_WINDOWS
#include <windows.h>
#ifdef CopyFile
#undef CopyFile
#endif
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace SC
{
namespace
{
static const char AwaitCancellationMessageStorage[]   = "AwaitTask cancelled";
static const char AwaitWrongEventLoopMessageStorage[] = "AwaitTask belongs to another AwaitEventLoop";

static constexpr size_t AwaitFrameAlignment =
#if defined(__STDCPP_DEFAULT_NEW_ALIGNMENT__)
    __STDCPP_DEFAULT_NEW_ALIGNMENT__;
#else
    alignof(void*);
#endif

static void* alignPointer(void* pointer, size_t alignment)
{
    const size_t address        = reinterpret_cast<size_t>(pointer);
    const size_t alignedAddress = (address + alignment - 1) & ~(alignment - 1);
    return reinterpret_cast<void*>(alignedAddress);
}

static size_t alignSize(size_t value, size_t alignment) { return (value + alignment - 1) & ~(alignment - 1); }

static bool isPowerOfTwo(size_t value) { return value != 0 and (value & (value - 1)) == 0; }

static size_t awaitAllocatorPageSize()
{
#if SC_PLATFORM_WINDOWS
    static const size_t pageSize = []()
    {
        SYSTEM_INFO sysInfo;
        GetSystemInfo(&sysInfo);
        return static_cast<size_t>(sysInfo.dwPageSize);
    }();
#else
    static const size_t pageSize = static_cast<size_t>(sysconf(_SC_PAGESIZE));
#endif
    return pageSize;
}

static size_t roundUpToAwaitAllocatorPageSize(size_t size)
{
    const size_t pageSize = awaitAllocatorPageSize();
    return (size + pageSize - 1) / pageSize * pageSize;
}

struct AwaitAllocationHeader
{
    AwaitAllocator* allocator          = nullptr;
    void*           rawAllocation      = nullptr;
    void*           blockHeader        = nullptr;
    size_t          requestedBytes     = 0;
    size_t          allocatedBytes     = 0;
    size_t          requestedAlignment = 0;
};

static AwaitAllocationHeader& allocationHeaderFromMemory(void* memory)
{
    return *(reinterpret_cast<AwaitAllocationHeader*>(memory) - 1);
}

static void clearCancellation(AwaitTask::Handle continuation, void* object)
{
    if (continuation == nullptr)
    {
        return;
    }
    AwaitTask::Promise& promise = continuation.promise();
    if (promise.cancellation.object == object)
    {
        promise.cancellation = {};
    }
}

static void setupCancellableAwait(AwaitTask::Handle& continuation, Function<void(AsyncResult&)>& stopCallback,
                                  AwaitTask::Handle newContinuation, void* object,
                                  Result (*cancel)(void* object, AwaitEventLoop& eventLoop))
{
    continuation                       = newContinuation;
    AwaitTask::Handle* continuationPtr = &continuation;
    stopCallback                       = [continuationPtr](AsyncResult&) { continuationPtr->resume(); };

    continuation.promise().cancellation = {object, cancel};
}

template <typename AsyncRequestType>
static Result cancelCancellableAwait(AsyncRequestType& request, Result& operationResult, AwaitEventLoop& eventLoop,
                                     Function<void(AsyncResult&)>& stopCallback)
{
    operationResult = AwaitCancelledResult();
    return request.stop(eventLoop.asyncEventLoop(), &stopCallback);
}
} // namespace

const char* AwaitCancellationMessage() { return AwaitCancellationMessageStorage; }

Result AwaitCancelledResult() { return Result::FromStableCharPointer(AwaitCancellationMessageStorage); }

bool AwaitIsCancelled(Result result) { return result.message == AwaitCancellationMessageStorage; }

const char* AwaitWrongEventLoopMessage() { return AwaitWrongEventLoopMessageStorage; }

Result AwaitWrongEventLoopResult() { return Result::FromStableCharPointer(AwaitWrongEventLoopMessageStorage); }

bool AwaitIsWrongEventLoop(Result result) { return result.message == AwaitWrongEventLoopMessageStorage; }

struct AwaitAllocator::BlockHeader
{
    AwaitAllocator* allocator  = nullptr;
    BlockHeader*    previous   = nullptr;
    BlockHeader*    next       = nullptr;
    size_t          blockBytes = 0;
    bool            free       = true;
};

AwaitAllocator::~AwaitAllocator()
{
    Result result = close();
    SC_ASSERT_RELEASE(result);
}

Result AwaitAllocator::createFixed(Span<char> storage)
{
    if (isOpen())
    {
        return Result::Error("AwaitAllocator is already open");
    }
    if (storage.empty())
    {
        return Result::Error("AwaitAllocator fixed storage is empty");
    }

    resetState();
    SC_TRY(initializeFixedStorage(storage));
    currentMode = AwaitAllocatorMode::Fixed;
    return Result(true);
}

Result AwaitAllocator::createVirtual(AwaitAllocatorVirtualOptions options)
{
    if (isOpen())
    {
        return Result::Error("AwaitAllocator is already open");
    }
    if (options.reserveBytes == 0)
    {
        return Result::Error("AwaitAllocator virtual reservation is empty");
    }

    resetState();
    virtualReservedBytes  = roundUpToAwaitAllocatorPageSize(options.reserveBytes);
    virtualCommittedBytes = 0;
#if SC_PLATFORM_WINDOWS
    virtualMemory = ::VirtualAlloc(nullptr, virtualReservedBytes, MEM_RESERVE, PAGE_NOACCESS);
#else
    virtualMemory = ::mmap(nullptr, virtualReservedBytes, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (virtualMemory == MAP_FAILED)
    {
        virtualMemory = nullptr;
    }
#endif
    if (virtualMemory == nullptr)
    {
        resetState();
        return Result::Error("AwaitAllocator virtual reservation failed");
    }

    currentMode = AwaitAllocatorMode::Virtual;
    if (options.initialCommitBytes > 0 and not ensureCommitted(options.initialCommitBytes))
    {
        releaseVirtualMemory();
        resetState();
        return Result::Error("AwaitAllocator virtual initial commit failed");
    }
    return Result(true);
}

Result AwaitAllocator::createMalloc()
{
    if (isOpen())
    {
        return Result::Error("AwaitAllocator is already open");
    }
    resetState();
    currentMode = AwaitAllocatorMode::Malloc;
    return Result(true);
}

Result AwaitAllocator::createPolymorphic(AwaitAllocatorInterface& customAllocatorInterface)
{
    if (isOpen())
    {
        return Result::Error("AwaitAllocator is already open");
    }
    resetState();
    allocatorInterface = &customAllocatorInterface;
    currentMode        = AwaitAllocatorMode::Polymorphic;
    return Result(true);
}

Result AwaitAllocator::close()
{
    if (not isOpen())
    {
        return Result(true);
    }
    if (currentStatistics.bytesInUse != 0 or currentStatistics.numAllocations != currentStatistics.numReleases)
    {
        SC_ASSERT_RELEASE(false);
        return Result::Error("AwaitAllocator closed with live allocations");
    }

    releaseVirtualMemory();
    currentMode           = AwaitAllocatorMode::None;
    fixedStorage          = {};
    firstBlock            = nullptr;
    allocatorInterface    = nullptr;
    virtualMemory         = nullptr;
    virtualReservedBytes  = 0;
    virtualCommittedBytes = 0;
    return Result(true);
}

void* AwaitAllocator::allocate(const void* owner, size_t numBytes, size_t alignment)
{
    if (not isOpen() or numBytes == 0 or not isPowerOfTwo(alignment))
    {
        recordAllocationFailure(numBytes);
        return nullptr;
    }

    if (currentMode == AwaitAllocatorMode::Fixed or currentMode == AwaitAllocatorMode::Virtual)
    {
        return allocateFromBlocks(owner, numBytes, alignment);
    }

    const size_t rawBytes = numBytes + sizeof(AwaitAllocationHeader) + alignment;
    void*        raw      = nullptr;
    if (currentMode == AwaitAllocatorMode::Malloc)
    {
        raw = ::malloc(rawBytes);
    }
    else if (currentMode == AwaitAllocatorMode::Polymorphic)
    {
        raw = allocatorInterface->allocateImpl(owner, rawBytes, alignof(AwaitAllocationHeader));
    }

    if (raw == nullptr)
    {
        recordAllocationFailure(numBytes);
        return nullptr;
    }

    void*                  memory = alignPointer(static_cast<char*>(raw) + sizeof(AwaitAllocationHeader), alignment);
    AwaitAllocationHeader& header = allocationHeaderFromMemory(memory);
    header.allocator              = this;
    header.rawAllocation          = raw;
    header.blockHeader            = nullptr;
    header.requestedBytes         = numBytes;
    header.allocatedBytes         = rawBytes;
    header.requestedAlignment     = alignment;

    currentStatistics.numAllocations++;
    currentStatistics.requestedBytesAllocated += numBytes;
    if (numBytes > currentStatistics.largestRequestedAllocationSize)
    {
        currentStatistics.largestRequestedAllocationSize = numBytes;
    }
    currentStatistics.bytesInUse += rawBytes;
    if (currentStatistics.bytesInUse > currentStatistics.peakBytesInUse)
    {
        currentStatistics.peakBytesInUse = currentStatistics.bytesInUse;
    }
    return memory;
}

void AwaitAllocator::release(void* memory)
{
    if (memory == nullptr)
    {
        return;
    }

    AwaitAllocationHeader& header = allocationHeaderFromMemory(memory);
    SC_ASSERT_RELEASE(header.allocator == this);
    if (header.allocator != this)
    {
        return;
    }

    currentStatistics.numReleases++;
    currentStatistics.requestedBytesReleased += header.requestedBytes;
    SC_ASSERT_RELEASE(currentStatistics.bytesInUse >= header.allocatedBytes);
    currentStatistics.bytesInUse -= header.allocatedBytes;

    if (currentMode == AwaitAllocatorMode::Fixed or currentMode == AwaitAllocatorMode::Virtual)
    {
        releaseBlock(*static_cast<BlockHeader*>(header.blockHeader));
    }
    else if (currentMode == AwaitAllocatorMode::Malloc)
    {
        ::free(header.rawAllocation);
    }
    else if (currentMode == AwaitAllocatorMode::Polymorphic)
    {
        allocatorInterface->releaseImpl(header.rawAllocation);
    }
}

void AwaitAllocator::releaseFromAnyAllocator(void* memory)
{
    if (memory == nullptr)
    {
        return;
    }
    AwaitAllocationHeader& header = allocationHeaderFromMemory(memory);
    SC_ASSERT_RELEASE(header.allocator != nullptr);
    if (header.allocator != nullptr)
    {
        header.allocator->release(memory);
    }
}

AwaitAllocatorMode AwaitAllocator::mode() const { return currentMode; }

AwaitAllocatorStatistics AwaitAllocator::statistics() const { return currentStatistics; }

bool AwaitAllocator::isOpen() const { return currentMode != AwaitAllocatorMode::None; }

size_t AwaitAllocator::used() const { return currentStatistics.bytesInUse; }

size_t AwaitAllocator::capacity() const
{
    if (currentMode == AwaitAllocatorMode::Fixed)
    {
        return fixedStorage.sizeInBytes();
    }
    if (currentMode == AwaitAllocatorMode::Virtual)
    {
        return virtualReservedBytes;
    }
    return 0;
}

size_t AwaitAllocator::peakUsed() const { return currentStatistics.peakBytesInUse; }

size_t AwaitAllocator::largestAllocationSize() const { return currentStatistics.largestRequestedAllocationSize; }

size_t AwaitAllocator::failedAllocationSize() const { return currentStatistics.lastFailedAllocationSize; }

size_t AwaitAllocator::reservedBytes() const
{
    return currentMode == AwaitAllocatorMode::Virtual ? virtualReservedBytes : 0;
}

size_t AwaitAllocator::committedBytes() const
{
    return currentMode == AwaitAllocatorMode::Virtual ? virtualCommittedBytes : 0;
}

Result AwaitAllocator::initializeFixedStorage(Span<char> storage)
{
    char* base        = storage.data();
    char* storageEnd  = base + storage.sizeInBytes();
    char* alignedBase = static_cast<char*>(alignPointer(base, alignof(BlockHeader)));
    if (alignedBase >= storageEnd or static_cast<size_t>(storageEnd - alignedBase) < sizeof(BlockHeader))
    {
        fixedStorage = storage;
        firstBlock   = nullptr;
        return Result(true);
    }

    fixedStorage           = storage;
    firstBlock             = reinterpret_cast<BlockHeader*>(alignedBase);
    firstBlock->allocator  = this;
    firstBlock->previous   = nullptr;
    firstBlock->next       = nullptr;
    firstBlock->blockBytes = static_cast<size_t>(storageEnd - alignedBase);
    firstBlock->free       = true;
    return Result(true);
}

void* AwaitAllocator::allocateFromBlocks(const void*, size_t numBytes, size_t alignment)
{
    for (;;)
    {
        for (BlockHeader* block = firstBlock; block != nullptr; block = block->next)
        {
            if (not block->free)
            {
                continue;
            }

            char* blockStart = reinterpret_cast<char*>(block);
            char* blockEnd   = blockStart + block->blockBytes;
            void* memory    = alignPointer(blockStart + sizeof(BlockHeader) + sizeof(AwaitAllocationHeader), alignment);
            char* headerPtr = static_cast<char*>(memory) - sizeof(AwaitAllocationHeader);
            if (headerPtr < blockStart + sizeof(BlockHeader))
            {
                continue;
            }

            size_t usedBytes = static_cast<size_t>(static_cast<char*>(memory) + numBytes - blockStart);
            usedBytes        = alignSize(usedBytes, alignof(BlockHeader));
            if (blockStart + usedBytes > blockEnd)
            {
                continue;
            }

            const size_t remainingBytes = block->blockBytes - usedBytes;
            if (remainingBytes > sizeof(BlockHeader) + sizeof(AwaitAllocationHeader) + alignof(BlockHeader))
            {
                BlockHeader* nextBlock = reinterpret_cast<BlockHeader*>(blockStart + usedBytes);
                nextBlock->allocator   = this;
                nextBlock->previous    = block;
                nextBlock->next        = block->next;
                nextBlock->blockBytes  = remainingBytes;
                nextBlock->free        = true;
                if (block->next != nullptr)
                {
                    block->next->previous = nextBlock;
                }
                block->next       = nextBlock;
                block->blockBytes = usedBytes;
            }

            block->free = false;

            AwaitAllocationHeader& header = allocationHeaderFromMemory(memory);
            header.allocator              = this;
            header.rawAllocation          = block;
            header.blockHeader            = block;
            header.requestedBytes         = numBytes;
            header.allocatedBytes         = block->blockBytes;
            header.requestedAlignment     = alignment;

            currentStatistics.numAllocations++;
            currentStatistics.requestedBytesAllocated += numBytes;
            if (numBytes > currentStatistics.largestRequestedAllocationSize)
            {
                currentStatistics.largestRequestedAllocationSize = numBytes;
            }
            currentStatistics.bytesInUse += block->blockBytes;
            if (currentStatistics.bytesInUse > currentStatistics.peakBytesInUse)
            {
                currentStatistics.peakBytesInUse = currentStatistics.bytesInUse;
            }
            return memory;
        }

        if (currentMode != AwaitAllocatorMode::Virtual)
        {
            recordAllocationFailure(numBytes);
            return nullptr;
        }

        const size_t previousCommittedBytes = virtualCommittedBytes;
        const size_t requestedBytes =
            previousCommittedBytes + sizeof(BlockHeader) + sizeof(AwaitAllocationHeader) + alignment + numBytes;
        if (not ensureCommitted(requestedBytes) or virtualCommittedBytes == previousCommittedBytes)
        {
            recordAllocationFailure(numBytes);
            return nullptr;
        }
    }
}

void AwaitAllocator::releaseBlock(BlockHeader& header)
{
    SC_ASSERT_RELEASE(header.allocator == this);
    header.free = true;

    if (header.next != nullptr and header.next->free)
    {
        BlockHeader* next = header.next;
        header.blockBytes += next->blockBytes;
        header.next = next->next;
        if (header.next != nullptr)
        {
            header.next->previous = &header;
        }
    }

    if (header.previous != nullptr and header.previous->free)
    {
        BlockHeader* previous = header.previous;
        previous->blockBytes += header.blockBytes;
        previous->next = header.next;
        if (previous->next != nullptr)
        {
            previous->next->previous = previous;
        }
    }
}

bool AwaitAllocator::ensureCommitted(size_t sizeInBytes)
{
    if (currentMode != AwaitAllocatorMode::Virtual or virtualMemory == nullptr)
    {
        return false;
    }
    if (sizeInBytes <= virtualCommittedBytes)
    {
        return true;
    }
    if (sizeInBytes > virtualReservedBytes)
    {
        return false;
    }

    const size_t previousCommittedBytes = virtualCommittedBytes;
    const size_t targetCommittedBytes   = roundUpToAwaitAllocatorPageSize(sizeInBytes);
    void*        commitAddress          = static_cast<char*>(virtualMemory) + previousCommittedBytes;
    const size_t bytesToCommit          = targetCommittedBytes - previousCommittedBytes;
#if SC_PLATFORM_WINDOWS
    if (::VirtualAlloc(commitAddress, bytesToCommit, MEM_COMMIT, PAGE_READWRITE) == nullptr)
    {
        return false;
    }
#else
    if (::mprotect(commitAddress, bytesToCommit, PROT_READ | PROT_WRITE) != 0)
    {
        return false;
    }
#endif
    virtualCommittedBytes = targetCommittedBytes;

    if (firstBlock == nullptr)
    {
        firstBlock             = static_cast<BlockHeader*>(virtualMemory);
        firstBlock->allocator  = this;
        firstBlock->previous   = nullptr;
        firstBlock->next       = nullptr;
        firstBlock->blockBytes = virtualCommittedBytes;
        firstBlock->free       = true;
        return true;
    }

    BlockHeader* lastBlock = firstBlock;
    while (lastBlock->next != nullptr)
    {
        lastBlock = lastBlock->next;
    }

    const size_t addedBytes = virtualCommittedBytes - previousCommittedBytes;
    if (lastBlock->free)
    {
        lastBlock->blockBytes += addedBytes;
    }
    else
    {
        BlockHeader* nextBlock =
            reinterpret_cast<BlockHeader*>(static_cast<char*>(virtualMemory) + previousCommittedBytes);
        nextBlock->allocator  = this;
        nextBlock->previous   = lastBlock;
        nextBlock->next       = nullptr;
        nextBlock->blockBytes = addedBytes;
        nextBlock->free       = true;
        lastBlock->next       = nextBlock;
    }
    return true;
}

void AwaitAllocator::releaseVirtualMemory()
{
    if (virtualMemory == nullptr)
    {
        return;
    }
#if SC_PLATFORM_WINDOWS
    const bool result = ::VirtualFree(virtualMemory, 0, MEM_RELEASE) == TRUE;
#else
    const bool result = ::munmap(virtualMemory, virtualReservedBytes) == 0;
#endif
    SC_ASSERT_RELEASE(result);
    virtualMemory         = nullptr;
    virtualReservedBytes  = 0;
    virtualCommittedBytes = 0;
}

void AwaitAllocator::recordAllocationFailure(size_t numBytes)
{
    currentStatistics.numAllocationFailures++;
    currentStatistics.lastFailedAllocationSize = numBytes;
    if (numBytes > currentStatistics.largestFailedAllocationSize)
    {
        currentStatistics.largestFailedAllocationSize = numBytes;
    }
}

void AwaitAllocator::resetState()
{
    currentMode           = AwaitAllocatorMode::None;
    currentStatistics     = {};
    fixedStorage          = {};
    firstBlock            = nullptr;
    allocatorInterface    = nullptr;
    virtualMemory         = nullptr;
    virtualReservedBytes  = 0;
    virtualCommittedBytes = 0;
}

AwaitTask::AwaitTask(Handle newHandle) : handle(newHandle) {}

AwaitTask::AwaitTask(AwaitTask&& other) noexcept : handle(other.handle) { other.handle = {}; }

AwaitTask& AwaitTask::operator=(AwaitTask&& other) noexcept
{
    if (this != &other)
    {
        SC_ASSERT_RELEASE(not isActive());
        destroy();
        handle       = other.handle;
        other.handle = {};
    }
    return *this;
}

AwaitTask::~AwaitTask()
{
    SC_ASSERT_RELEASE(not isActive());
    destroy();
}

bool AwaitTask::isValid() const { return handle != nullptr; }

bool AwaitTask::isStarted() const { return handle and handle.promise().started; }

bool AwaitTask::isCompleted() const { return handle and handle.promise().completed; }

bool AwaitTask::isActive() const { return isStarted() and not isCompleted(); }

bool AwaitTask::isCancellationRequested() const { return handle and handle.promise().cancellationRequested; }

Result AwaitTask::result() const
{
    if (not handle)
    {
        return Result::Error("AwaitTask is invalid");
    }
    if (not isCompleted())
    {
        return Result::Error("AwaitTask is not completed");
    }
    return handle.promise().taskResult;
}

Result AwaitTask::cancel(AwaitEventLoop& await)
{
    if (not handle)
    {
        return Result::Error("AwaitTask is invalid");
    }
    Promise& promise = handle.promise();
    if (promise.completed)
    {
        return Result(true);
    }
    if (promise.eventLoop != nullptr and promise.eventLoop != &await)
    {
        return AwaitWrongEventLoopResult();
    }
    if (not promise.started)
    {
        return Result::Error("AwaitTask is not started");
    }
    if (promise.cancellationRequested)
    {
        return Result(true);
    }
    if (promise.cancellation.cancel == nullptr)
    {
        return Result::Error("AwaitTask cannot be cancelled right now");
    }
    promise.cancellationRequested = true;
    return promise.cancellation.cancel(promise.cancellation.object, await);
}

bool AwaitTask::await_ready() const { return not isActive(); }

bool AwaitTask::await_suspend(Handle parent)
{
    if (not isActive())
    {
        return false;
    }

    Promise&        child           = handle.promise();
    AwaitEventLoop* childEventLoop  = child.eventLoop;
    AwaitEventLoop* parentEventLoop = parent.promise().eventLoop;
    if (childEventLoop != nullptr and parentEventLoop != nullptr and childEventLoop != parentEventLoop)
    {
        return false;
    }

    SC_ASSERT_RELEASE(child.continuation == nullptr);
    child.continuation = parent;

    parent.promise().cancellation = {this, [](void* object, AwaitEventLoop& await)
                                     { return static_cast<AwaitTask*>(object)->cancel(await); }};
    return true;
}

Result AwaitTask::await_resume() const { return result(); }

Result AwaitTask::start()
{
    if (not handle)
    {
        return Result::Error("AwaitTask is invalid");
    }
    if (handle.promise().started)
    {
        return Result::Error("AwaitTask already started");
    }
    handle.promise().started = true;
    return Result(true);
}

void AwaitTask::resume()
{
    SC_ASSERT_RELEASE(handle != nullptr);
    handle.resume();
}

void AwaitTask::destroy()
{
    if (handle)
    {
        AwaitTask::Promise& promise = handle.promise();
        if (promise.inCompletionCallback and promise.eventLoop != nullptr)
        {
            promise.eventLoop->deferDestroy(handle);
            handle = {};
            return;
        }
        handle.destroy();
        handle = {};
    }
}

AwaitTask::Promise::Promise()
    : taskResult(Result::Error("AwaitTask not completed")), eventLoop(nullptr), started(false), completed(false)
{
    deferredDestroyNext   = {};
    completionObject      = nullptr;
    completionCallback    = nullptr;
    cancellationRequested = false;
    inCompletionCallback  = false;
    destroyDeferred       = false;
}

AwaitEventLoop* AwaitTask::Promise::findEventLoop() { return nullptr; }

AwaitEventLoop* AwaitTask::Promise::findEventLoop(AwaitEventLoop& await) { return &await; }

void* AwaitTask::Promise::allocateFrame(size_t size, AwaitEventLoop* eventLoop) noexcept
{
    constexpr size_t alignment = AwaitFrameAlignment;
    if (eventLoop == nullptr)
    {
        return nullptr;
    }
    return eventLoop->allocator().allocate(nullptr, size, alignment);
}

void AwaitTask::Promise::deallocateFrame(void* frame) noexcept
{
    if (frame == nullptr)
    {
        return;
    }

    AwaitAllocator::releaseFromAnyAllocator(frame);
}

void* AwaitTask::Promise::operator new(size_t size) noexcept { return allocateFrame(size, nullptr); }

void AwaitTask::Promise::operator delete(void* frame, size_t) noexcept { deallocateFrame(frame); }

AwaitTask AwaitTask::Promise::get_return_object_on_allocation_failure() { return {}; }

AwaitTask AwaitTask::Promise::get_return_object() { return AwaitTask(AwaitTask::Handle::from_promise(*this)); }

AwaitSuspendAlways AwaitTask::Promise::initial_suspend() noexcept { return {}; }

bool AwaitTask::Promise::FinalSuspend::await_ready() noexcept { return false; }

void AwaitTask::Promise::FinalSuspend::await_suspend(AwaitTask::Handle handle) noexcept
{
    AwaitTask::Promise& promise  = handle.promise();
    promise.completed            = true;
    promise.cancellation         = {};
    promise.inCompletionCallback = true;
    if (promise.completionCallback != nullptr)
    {
        void (*callback)(void*)    = promise.completionCallback;
        void* callbackObject       = promise.completionObject;
        promise.completionObject   = nullptr;
        promise.completionCallback = nullptr;
        callback(callbackObject);
        promise.inCompletionCallback = false;
        return;
    }
    if (promise.continuation != nullptr)
    {
        AwaitTask::Handle parentContinuation      = promise.continuation;
        promise.continuation                      = {};
        parentContinuation.promise().cancellation = {};
        parentContinuation.resume();
    }
    promise.inCompletionCallback = false;
}

void AwaitTask::Promise::FinalSuspend::await_resume() noexcept {}

AwaitTask::Promise::FinalSuspend AwaitTask::Promise::final_suspend() noexcept { return {}; }

void AwaitTask::Promise::return_value(Result newResult) noexcept { taskResult = newResult; }

void AwaitTask::Promise::unhandled_exception() noexcept { taskResult = Result::Error("AwaitTask unhandled exception"); }

AwaitEventLoop::AwaitEventLoop(AsyncEventLoop& asyncEventLoop, AwaitAllocator& allocator)
    : eventLoop(asyncEventLoop), frameAllocator(&allocator), deferredDestroyList({})
{
    SC_ASSERT_RELEASE(allocator.isOpen());
}

AsyncEventLoop& AwaitEventLoop::asyncEventLoop() { return eventLoop; }

const AsyncEventLoop& AwaitEventLoop::asyncEventLoop() const { return eventLoop; }

AwaitAllocator& AwaitEventLoop::allocator() { return *frameAllocator; }

Result AwaitEventLoop::spawn(AwaitTask& task)
{
    if (not task.isValid())
    {
        return Result::Error("AwaitTask is invalid");
    }
    AwaitEventLoop* taskEventLoop = task.handle.promise().eventLoop;
    if (taskEventLoop != nullptr and taskEventLoop != this)
    {
        return AwaitWrongEventLoopResult();
    }
    if (taskEventLoop == nullptr)
    {
        task.handle.promise().eventLoop = this;
    }

    SC_TRY(task.start());
    task.resume();
    return Result(true);
}

void AwaitEventLoop::deferDestroy(AwaitTask::Handle handle)
{
    AwaitTask::Promise& promise = handle.promise();
    if (promise.destroyDeferred)
    {
        return;
    }
    promise.destroyDeferred     = true;
    promise.deferredDestroyNext = deferredDestroyList;
    deferredDestroyList         = handle;
}

void AwaitEventLoop::drainDeferredDestroys()
{
    while (deferredDestroyList != nullptr)
    {
        AwaitTask::Handle   handle  = deferredDestroyList;
        AwaitTask::Promise& promise = handle.promise();
        deferredDestroyList         = promise.deferredDestroyNext;
        promise.deferredDestroyNext = {};
        promise.destroyDeferred     = false;
        handle.destroy();
    }
}

Result AwaitEventLoop::run()
{
    Result result = eventLoop.run();
    drainDeferredDestroys();
    return result;
}

Result AwaitEventLoop::runOnce()
{
    Result result = eventLoop.runOnce();
    drainDeferredDestroys();
    return result;
}

Result AwaitEventLoop::runNoWait()
{
    Result result = eventLoop.runNoWait();
    drainDeferredDestroys();
    return result;
}

AwaitSleepAwaiter AwaitEventLoop::sleep(TimeMs duration) { return AwaitSleepAwaiter(*this, duration); }

AwaitSocketAcceptAwaiter AwaitEventLoop::accept(const SocketDescriptor& serverSocket, SocketDescriptor& outClient)
{
    return AwaitSocketAcceptAwaiter(*this, serverSocket, outClient);
}

AwaitSocketConnectAwaiter AwaitEventLoop::connect(const SocketDescriptor& socket, SocketIPAddress address)
{
    return AwaitSocketConnectAwaiter(*this, socket, address);
}

AwaitSocketSendAwaiter AwaitEventLoop::send(const SocketDescriptor& socket, Span<const char> data,
                                            AwaitSocketSendResult* outResult)
{
    return AwaitSocketSendAwaiter(*this, socket, data, outResult);
}

AwaitSocketSendAwaiter AwaitEventLoop::send(const SocketDescriptor& socket, Span<Span<const char>> data,
                                            AwaitSocketSendResult* outResult)
{
    return AwaitSocketSendAwaiter(*this, socket, data, outResult);
}

AwaitSocketSendToAwaiter AwaitEventLoop::sendTo(const SocketDescriptor& socket, SocketIPAddress address,
                                                Span<const char> data, AwaitSocketSendResult* outResult)
{
    return AwaitSocketSendToAwaiter(*this, socket, address, data, outResult);
}

AwaitSocketSendToAwaiter AwaitEventLoop::sendTo(const SocketDescriptor& socket, SocketIPAddress address,
                                                Span<Span<const char>> data, AwaitSocketSendResult* outResult)
{
    return AwaitSocketSendToAwaiter(*this, socket, address, data, outResult);
}

AwaitSocketSendAllAwaiter AwaitEventLoop::sendAll(const SocketDescriptor& socket, Span<const char> data,
                                                  AwaitSocketSendResult* outResult)
{
    return AwaitSocketSendAllAwaiter(*this, socket, data, outResult);
}

AwaitSocketSendAllBuffersAwaiter AwaitEventLoop::sendAll(const SocketDescriptor& socket, Span<Span<const char>> data,
                                                         AwaitSocketSendResult* outResult)
{
    return AwaitSocketSendAllBuffersAwaiter(*this, socket, data, outResult);
}

AwaitSocketReceiveAwaiter AwaitEventLoop::receive(const SocketDescriptor& socket, Span<char> buffer,
                                                  AwaitSocketReceiveResult& outResult)
{
    return AwaitSocketReceiveAwaiter(*this, socket, buffer, outResult);
}

AwaitSocketReceiveExactAwaiter AwaitEventLoop::receiveExact(const SocketDescriptor& socket, Span<char> buffer,
                                                            AwaitSocketReceiveResult* outResult)
{
    return AwaitSocketReceiveExactAwaiter(*this, socket, buffer, outResult);
}

AwaitSocketReceiveLineAwaiter AwaitEventLoop::receiveLine(const SocketDescriptor& socket, Span<char> buffer,
                                                          AwaitSocketReceiveLineResult& outResult)
{
    return AwaitSocketReceiveLineAwaiter(*this, socket, buffer, outResult);
}

AwaitSocketReceiveFromAwaiter AwaitEventLoop::receiveFrom(const SocketDescriptor& socket, Span<char> buffer,
                                                          AwaitSocketReceiveFromResult& outResult)
{
    return AwaitSocketReceiveFromAwaiter(*this, socket, buffer, outResult);
}

AwaitFileReadAwaiter AwaitEventLoop::fileRead(const FileDescriptor& file, Span<char> buffer,
                                              AwaitFileReadResult& outResult, AwaitFileReadOptions options)
{
    return AwaitFileReadAwaiter(*this, file, buffer, outResult, options);
}

AwaitFileReadUntilFullOrEOFAwaiter AwaitEventLoop::fileReadUntilFullOrEOF(const FileDescriptor& file, Span<char> buffer,
                                                                          AwaitFileReadResult& outResult,
                                                                          AwaitFileReadOptions options)
{
    return AwaitFileReadUntilFullOrEOFAwaiter(*this, file, buffer, outResult, options);
}

AwaitFileWriteAwaiter AwaitEventLoop::fileWrite(const FileDescriptor& file, Span<const char> data,
                                                AwaitFileWriteResult* outResult, AwaitFileWriteOptions options)
{
    return AwaitFileWriteAwaiter(*this, file, data, outResult, options);
}

AwaitFileWriteAwaiter AwaitEventLoop::fileWrite(const FileDescriptor& file, Span<Span<const char>> data,
                                                AwaitFileWriteResult* outResult, AwaitFileWriteOptions options)
{
    return AwaitFileWriteAwaiter(*this, file, data, outResult, options);
}

AwaitFileSendAwaiter AwaitEventLoop::fileSend(const FileDescriptor& file, const SocketDescriptor& socket,
                                              AwaitFileSendResult& outResult, AwaitFileSendOptions options)
{
    return AwaitFileSendAwaiter(*this, file, socket, outResult, options);
}

AwaitFilePollAwaiter AwaitEventLoop::filePoll(const FileDescriptor& file) { return AwaitFilePollAwaiter(*this, file); }

AwaitLoopWakeUpAwaiter AwaitEventLoop::wakeUp(AwaitLoopWakeUp& wakeUp, AwaitLoopWakeUpResult& outResult,
                                              AsyncLoopWakeUpOptions options)
{
    return AwaitLoopWakeUpAwaiter(*this, wakeUp, outResult, options);
}

AwaitFileSystemOperationAwaiter AwaitEventLoop::fsOpen(ThreadPool& threadPool, StringSpan path, FileOpen mode,
                                                       FileDescriptor& outFile)
{
    return AwaitFileSystemOperationAwaiter(*this, threadPool, AwaitFileSystemOperationType::Open, path, StringSpan(),
                                           mode, &outFile);
}

AwaitFileSystemOperationAwaiter AwaitEventLoop::fsClose(ThreadPool& threadPool, FileDescriptor& file)
{
    return AwaitFileSystemOperationAwaiter(*this, threadPool, AwaitFileSystemOperationType::Close, file);
}

AwaitFileSystemOperationAwaiter AwaitEventLoop::fsRead(ThreadPool& threadPool, FileDescriptor& file, Span<char> buffer,
                                                       AwaitFileReadResult& outResult, uint64_t offset)
{
    return AwaitFileSystemOperationAwaiter(*this, threadPool, AwaitFileSystemOperationType::Read, file, buffer,
                                           outResult, offset);
}

AwaitFileSystemOperationAwaiter AwaitEventLoop::fsWrite(ThreadPool& threadPool, FileDescriptor& file,
                                                        Span<const char> data, AwaitFileWriteResult* outResult,
                                                        uint64_t offset)
{
    return AwaitFileSystemOperationAwaiter(*this, threadPool, AwaitFileSystemOperationType::Write, file, data,
                                           outResult, offset);
}

AwaitFileSystemOperationAwaiter AwaitEventLoop::fsCopyFile(ThreadPool& threadPool, StringSpan path,
                                                           StringSpan destinationPath, FileSystemCopyFlags copyFlags)
{
    return AwaitFileSystemOperationAwaiter(*this, threadPool, AwaitFileSystemOperationType::CopyFile, path,
                                           destinationPath, FileOpen(), nullptr, copyFlags);
}

AwaitFileSystemOperationAwaiter AwaitEventLoop::fsCopyDirectory(ThreadPool& threadPool, StringSpan path,
                                                                StringSpan          destinationPath,
                                                                FileSystemCopyFlags copyFlags)
{
    return AwaitFileSystemOperationAwaiter(*this, threadPool, AwaitFileSystemOperationType::CopyDirectory, path,
                                           destinationPath, FileOpen(), nullptr, copyFlags);
}

AwaitFileSystemOperationAwaiter AwaitEventLoop::fsRename(ThreadPool& threadPool, StringSpan path, StringSpan newPath)
{
    return AwaitFileSystemOperationAwaiter(*this, threadPool, AwaitFileSystemOperationType::Rename, path, newPath);
}

AwaitFileSystemOperationAwaiter AwaitEventLoop::fsRemoveEmptyDirectory(ThreadPool& threadPool, StringSpan path)
{
    return AwaitFileSystemOperationAwaiter(*this, threadPool, AwaitFileSystemOperationType::RemoveEmptyDirectory, path);
}

AwaitFileSystemOperationAwaiter AwaitEventLoop::fsRemoveFile(ThreadPool& threadPool, StringSpan path)
{
    return AwaitFileSystemOperationAwaiter(*this, threadPool, AwaitFileSystemOperationType::RemoveFile, path);
}

AwaitProcessExitAwaiter AwaitEventLoop::processExit(FileDescriptor::Handle process, AwaitProcessExitResult& outResult)
{
    return AwaitProcessExitAwaiter(*this, process, outResult);
}

AwaitSignalAwaiter AwaitEventLoop::signal(int signalNumber, AwaitSignalResult& outResult)
{
    return AwaitSignalAwaiter(*this, signalNumber, outResult);
}

AwaitTaskSpawnAwaiter AwaitEventLoop::spawnAndWait(AwaitTask& task) { return AwaitTaskSpawnAwaiter(*this, task); }

AwaitTaskTimeoutAwaiter AwaitEventLoop::waitFor(AwaitTask& task, TimeMs timeout, AwaitTimeoutResult* outResult)
{
    return AwaitTaskTimeoutAwaiter(*this, task, timeout, outResult);
}

AwaitLoopWorkAwaiter AwaitEventLoop::loopWork(ThreadPool& threadPool, Function<Result()> work)
{
    return AwaitLoopWorkAwaiter(*this, threadPool, move(work));
}

Result AwaitLoopWakeUp::wakeUp(AwaitEventLoop& await) { return wakeUp(await.asyncEventLoop()); }

Result AwaitLoopWakeUp::wakeUp(AsyncEventLoop& eventLoop) { return request.wakeUp(eventLoop); }

bool AwaitLoopWakeUp::isActive() const { return request.isActive(); }

AwaitSleepAwaiter::AwaitSleepAwaiter(AwaitEventLoop& await, TimeMs duration) : await(await), duration(duration) {}

bool AwaitSleepAwaiter::await_ready() const { return false; }

bool AwaitSleepAwaiter::await_suspend(AwaitTask::Handle newContinuation)
{
    setupCancellableAwait(continuation, stopCallback, newContinuation, this, AwaitSleepAwaiter::cancel);

    request.callback = [this](AsyncLoopTimeout::Result& result)
    {
        operationResult = result.isValid();
        continuation.resume();
    };

    operationResult = request.start(await.asyncEventLoop(), duration);
    return operationResult;
}

Result AwaitSleepAwaiter::await_resume()
{
    clearCancellation(continuation, this);
    return operationResult;
}

Result AwaitSleepAwaiter::cancel(void* object, AwaitEventLoop& eventLoop)
{
    return static_cast<AwaitSleepAwaiter*>(object)->cancel(eventLoop);
}

Result AwaitSleepAwaiter::cancel(AwaitEventLoop& eventLoop)
{
    return cancelCancellableAwait(request, operationResult, eventLoop, stopCallback);
}

AwaitLoopWakeUpAwaiter::AwaitLoopWakeUpAwaiter(AwaitEventLoop& await, AwaitLoopWakeUp& wakeUp,
                                               AwaitLoopWakeUpResult& outResult, AsyncLoopWakeUpOptions options)
    : await(await), wakeUp(wakeUp), outResult(outResult), options(options)
{}

bool AwaitLoopWakeUpAwaiter::await_ready() const { return false; }

bool AwaitLoopWakeUpAwaiter::await_suspend(AwaitTask::Handle newContinuation)
{
    setupCancellableAwait(continuation, stopCallback, newContinuation, this, AwaitLoopWakeUpAwaiter::cancel);

    outResult               = {};
    wakeUp.request.callback = [this](AsyncLoopWakeUp::Result& result)
    {
        operationResult         = result.isValid();
        outResult.deliveryCount = result.completionData.deliveryCount;
        continuation.resume();
    };

    operationResult = wakeUp.request.start(await.asyncEventLoop(), options);
    return operationResult;
}

Result AwaitLoopWakeUpAwaiter::await_resume()
{
    clearCancellation(continuation, this);
    return operationResult;
}

Result AwaitLoopWakeUpAwaiter::cancel(void* object, AwaitEventLoop& eventLoop)
{
    return static_cast<AwaitLoopWakeUpAwaiter*>(object)->cancel(eventLoop);
}

Result AwaitLoopWakeUpAwaiter::cancel(AwaitEventLoop& eventLoop)
{
    return cancelCancellableAwait(wakeUp.request, operationResult, eventLoop, stopCallback);
}

AwaitSocketAcceptAwaiter::AwaitSocketAcceptAwaiter(AwaitEventLoop& await, const SocketDescriptor& serverSocket,
                                                   SocketDescriptor& outClient)
    : await(await), serverSocket(serverSocket), outClient(outClient)
{}

bool AwaitSocketAcceptAwaiter::await_ready() const { return false; }

bool AwaitSocketAcceptAwaiter::await_suspend(AwaitTask::Handle newContinuation)
{
    setupCancellableAwait(continuation, stopCallback, newContinuation, this, AwaitSocketAcceptAwaiter::cancel);

    request.callback = [this](AsyncSocketAccept::Result& result)
    {
        operationResult = result.moveTo(outClient);
        continuation.resume();
    };

    operationResult = request.start(await.asyncEventLoop(), serverSocket);
    return operationResult;
}

Result AwaitSocketAcceptAwaiter::await_resume()
{
    clearCancellation(continuation, this);
    return operationResult;
}

Result AwaitSocketAcceptAwaiter::cancel(void* object, AwaitEventLoop& eventLoop)
{
    return static_cast<AwaitSocketAcceptAwaiter*>(object)->cancel(eventLoop);
}

Result AwaitSocketAcceptAwaiter::cancel(AwaitEventLoop& eventLoop)
{
    return cancelCancellableAwait(request, operationResult, eventLoop, stopCallback);
}

AwaitSocketConnectAwaiter::AwaitSocketConnectAwaiter(AwaitEventLoop& await, const SocketDescriptor& socket,
                                                     SocketIPAddress address)
    : await(await), socket(socket), address(address)
{}

bool AwaitSocketConnectAwaiter::await_ready() const { return false; }

bool AwaitSocketConnectAwaiter::await_suspend(AwaitTask::Handle newContinuation)
{
    setupCancellableAwait(continuation, stopCallback, newContinuation, this, AwaitSocketConnectAwaiter::cancel);

    request.callback = [this](AsyncSocketConnect::Result& result)
    {
        operationResult = result.isValid();
        continuation.resume();
    };

    operationResult = request.start(await.asyncEventLoop(), socket, address);
    return operationResult;
}

Result AwaitSocketConnectAwaiter::await_resume()
{
    clearCancellation(continuation, this);
    return operationResult;
}

Result AwaitSocketConnectAwaiter::cancel(void* object, AwaitEventLoop& eventLoop)
{
    return static_cast<AwaitSocketConnectAwaiter*>(object)->cancel(eventLoop);
}

Result AwaitSocketConnectAwaiter::cancel(AwaitEventLoop& eventLoop)
{
    return cancelCancellableAwait(request, operationResult, eventLoop, stopCallback);
}

AwaitSocketSendAwaiter::AwaitSocketSendAwaiter(AwaitEventLoop& await, const SocketDescriptor& socket,
                                               Span<const char> data, AwaitSocketSendResult* outResult)
    : await(await), socket(socket), data(data), outResult(outResult)
{}

AwaitSocketSendAwaiter::AwaitSocketSendAwaiter(AwaitEventLoop& await, const SocketDescriptor& socket,
                                               Span<Span<const char>> data, AwaitSocketSendResult* outResult)
    : await(await), socket(socket), buffers(data), outResult(outResult), singleBuffer(false)
{}

bool AwaitSocketSendAwaiter::await_ready() const { return false; }

bool AwaitSocketSendAwaiter::await_suspend(AwaitTask::Handle newContinuation)
{
    setupCancellableAwait(continuation, stopCallback, newContinuation, this, AwaitSocketSendAwaiter::cancel);

    request.callback = [this](AsyncSocketSend::Result& result)
    {
        operationResult = result.isValid();
        if (outResult != nullptr)
        {
            outResult->numBytes = result.completionData.numBytes;
        }
        continuation.resume();
    };

    if (singleBuffer)
    {
        operationResult = request.start(await.asyncEventLoop(), socket, data);
    }
    else
    {
        operationResult = request.start(await.asyncEventLoop(), socket, buffers);
    }
    return operationResult;
}

Result AwaitSocketSendAwaiter::await_resume()
{
    clearCancellation(continuation, this);
    return operationResult;
}

Result AwaitSocketSendAwaiter::cancel(void* object, AwaitEventLoop& eventLoop)
{
    return static_cast<AwaitSocketSendAwaiter*>(object)->cancel(eventLoop);
}

Result AwaitSocketSendAwaiter::cancel(AwaitEventLoop& eventLoop)
{
    return cancelCancellableAwait(request, operationResult, eventLoop, stopCallback);
}

AwaitSocketSendToAwaiter::AwaitSocketSendToAwaiter(AwaitEventLoop& await, const SocketDescriptor& socket,
                                                   SocketIPAddress address, Span<const char> data,
                                                   AwaitSocketSendResult* outResult)
    : await(await), socket(socket), address(address), data(data), outResult(outResult)
{}

AwaitSocketSendToAwaiter::AwaitSocketSendToAwaiter(AwaitEventLoop& await, const SocketDescriptor& socket,
                                                   SocketIPAddress address, Span<Span<const char>> data,
                                                   AwaitSocketSendResult* outResult)
    : await(await), socket(socket), address(address), buffers(data), outResult(outResult), singleBuffer(false)
{}

bool AwaitSocketSendToAwaiter::await_ready() const { return false; }

bool AwaitSocketSendToAwaiter::await_suspend(AwaitTask::Handle newContinuation)
{
    setupCancellableAwait(continuation, stopCallback, newContinuation, this, AwaitSocketSendToAwaiter::cancel);

    request.callback = [this](AsyncSocketSendTo::Result& result)
    {
        operationResult = result.isValid();
        if (outResult != nullptr)
        {
            outResult->numBytes = result.completionData.numBytes;
        }
        continuation.resume();
    };

    if (singleBuffer)
    {
        operationResult = request.start(await.asyncEventLoop(), socket, address, data);
    }
    else
    {
        operationResult = request.start(await.asyncEventLoop(), socket, address, buffers);
    }
    return operationResult;
}

Result AwaitSocketSendToAwaiter::await_resume()
{
    clearCancellation(continuation, this);
    return operationResult;
}

Result AwaitSocketSendToAwaiter::cancel(void* object, AwaitEventLoop& eventLoop)
{
    return static_cast<AwaitSocketSendToAwaiter*>(object)->cancel(eventLoop);
}

Result AwaitSocketSendToAwaiter::cancel(AwaitEventLoop& eventLoop)
{
    return cancelCancellableAwait(request, operationResult, eventLoop, stopCallback);
}

AwaitSocketSendAllAwaiter::AwaitSocketSendAllAwaiter(AwaitEventLoop& await, const SocketDescriptor& socket,
                                                     Span<const char> data, AwaitSocketSendResult* outResult)
    : await(await), socket(socket), data(data), outResult(outResult)
{}

bool AwaitSocketSendAllAwaiter::await_ready() const { return data.empty(); }

bool AwaitSocketSendAllAwaiter::await_suspend(AwaitTask::Handle newContinuation)
{
    setupCancellableAwait(continuation, stopCallback, newContinuation, this, AwaitSocketSendAllAwaiter::cancel);

    request.callback = [this](AsyncSocketSend::Result& result)
    {
        operationResult = result.isValid();
        if (operationResult)
        {
            const size_t bytesSent = result.completionData.numBytes;
            if (bytesSent == 0)
            {
                operationResult = Result::Error("AwaitSocketSendAll made no progress");
                continuation.resume();
                return;
            }

            numBytesSent += bytesSent;
            if (outResult != nullptr)
            {
                outResult->numBytes = numBytesSent;
            }

            if (numBytesSent < data.sizeInBytes())
            {
                Span<const char> remaining;
                operationResult = Result(data.sliceStart(numBytesSent, remaining));
                if (operationResult)
                {
                    request.buffer = remaining;
                    result.reactivateRequest(true);
                    return;
                }
            }
        }
        continuation.resume();
    };

    operationResult = request.start(await.asyncEventLoop(), socket, data);
    return operationResult;
}

Result AwaitSocketSendAllAwaiter::await_resume()
{
    clearCancellation(continuation, this);
    if (outResult != nullptr and operationResult)
    {
        outResult->numBytes = numBytesSent;
    }
    return operationResult;
}

Result AwaitSocketSendAllAwaiter::cancel(void* object, AwaitEventLoop& eventLoop)
{
    return static_cast<AwaitSocketSendAllAwaiter*>(object)->cancel(eventLoop);
}

Result AwaitSocketSendAllAwaiter::cancel(AwaitEventLoop& eventLoop)
{
    return cancelCancellableAwait(request, operationResult, eventLoop, stopCallback);
}

AwaitSocketSendAllBuffersAwaiter::AwaitSocketSendAllBuffersAwaiter(AwaitEventLoop&         await,
                                                                   const SocketDescriptor& socket,
                                                                   Span<Span<const char>>  data,
                                                                   AwaitSocketSendResult*  outResult)
    : await(await), socket(socket), data(data), outResult(outResult)
{}

bool AwaitSocketSendAllBuffersAwaiter::await_ready() const
{
    for (size_t idx = 0; idx < data.sizeInElements(); ++idx)
    {
        if (not data[idx].empty())
        {
            return false;
        }
    }
    return true;
}

bool AwaitSocketSendAllBuffersAwaiter::await_suspend(AwaitTask::Handle newContinuation)
{
    setupCancellableAwait(continuation, stopCallback, newContinuation, this, AwaitSocketSendAllBuffersAwaiter::cancel);

    deferredStart.callback = [this](AsyncLoopTimeout::Result& result)
    {
        operationResult = result.isValid();
        if (operationResult)
        {
            operationResult = startCurrentBuffer();
        }
        if (not operationResult)
        {
            continuation.resume();
        }
    };

    request.callback = [this](AsyncSocketSend::Result& result)
    {
        operationResult = result.isValid();
        if (operationResult)
        {
            const size_t bytesSent = result.completionData.numBytes;
            if (bytesSent == 0)
            {
                operationResult = Result::Error("AwaitSocketSendAllBuffers made no progress");
                continuation.resume();
                return;
            }

            numBytesSent += bytesSent;
            bufferOffset += bytesSent;
            if (outResult != nullptr)
            {
                outResult->numBytes = numBytesSent;
            }

            if (findNextBuffer())
            {
                operationResult = deferredStart.start(await.asyncEventLoop(), TimeMs{0});
                if (not operationResult)
                {
                    continuation.resume();
                }
                return;
            }
        }
        continuation.resume();
    };

    if (not findNextBuffer())
    {
        return false;
    }
    operationResult = startCurrentBuffer();
    return operationResult;
}

Result AwaitSocketSendAllBuffersAwaiter::await_resume()
{
    clearCancellation(continuation, this);
    if (outResult != nullptr and operationResult)
    {
        outResult->numBytes = numBytesSent;
    }
    return operationResult;
}

Result AwaitSocketSendAllBuffersAwaiter::cancel(void* object, AwaitEventLoop& eventLoop)
{
    return static_cast<AwaitSocketSendAllBuffersAwaiter*>(object)->cancel(eventLoop);
}

Result AwaitSocketSendAllBuffersAwaiter::cancel(AwaitEventLoop& eventLoop)
{
    if (not request.isFree())
    {
        return cancelCancellableAwait(request, operationResult, eventLoop, stopCallback);
    }
    if (not deferredStart.isFree())
    {
        return cancelCancellableAwait(deferredStart, operationResult, eventLoop, stopCallback);
    }
    return cancelCancellableAwait(request, operationResult, eventLoop, stopCallback);
}

bool AwaitSocketSendAllBuffersAwaiter::findNextBuffer()
{
    while (bufferIndex < data.sizeInElements())
    {
        if (bufferOffset < data[bufferIndex].sizeInBytes())
        {
            return true;
        }
        bufferIndex++;
        bufferOffset = 0;
    }
    return false;
}

Result AwaitSocketSendAllBuffersAwaiter::startCurrentBuffer()
{
    SC_TRY(updateRequestBuffer());
    return request.start(await.asyncEventLoop(), socket, request.buffer);
}

Result AwaitSocketSendAllBuffersAwaiter::updateRequestBuffer()
{
    Span<const char> remaining;
    SC_TRY(data[bufferIndex].sliceStart(bufferOffset, remaining));
    request.buffer = remaining;
    return Result(true);
}

AwaitSocketReceiveAwaiter::AwaitSocketReceiveAwaiter(AwaitEventLoop& await, const SocketDescriptor& socket,
                                                     Span<char> buffer, AwaitSocketReceiveResult& outResult)
    : await(await), socket(socket), buffer(buffer), outResult(outResult)
{}

bool AwaitSocketReceiveAwaiter::await_ready() const { return false; }

bool AwaitSocketReceiveAwaiter::await_suspend(AwaitTask::Handle newContinuation)
{
    setupCancellableAwait(continuation, stopCallback, newContinuation, this, AwaitSocketReceiveAwaiter::cancel);

    outResult        = {};
    request.callback = [this](AsyncSocketReceive::Result& result)
    {
        operationResult        = result.get(outResult.data);
        outResult.disconnected = result.completionData.disconnected;
        continuation.resume();
    };

    operationResult = request.start(await.asyncEventLoop(), socket, buffer);
    return operationResult;
}

Result AwaitSocketReceiveAwaiter::await_resume()
{
    clearCancellation(continuation, this);
    return operationResult;
}

Result AwaitSocketReceiveAwaiter::cancel(void* object, AwaitEventLoop& eventLoop)
{
    return static_cast<AwaitSocketReceiveAwaiter*>(object)->cancel(eventLoop);
}

Result AwaitSocketReceiveAwaiter::cancel(AwaitEventLoop& eventLoop)
{
    return cancelCancellableAwait(request, operationResult, eventLoop, stopCallback);
}

AwaitSocketReceiveExactAwaiter::AwaitSocketReceiveExactAwaiter(AwaitEventLoop& await, const SocketDescriptor& socket,
                                                               Span<char> buffer, AwaitSocketReceiveResult* outResult)
    : await(await), socket(socket), buffer(buffer), outResult(outResult)
{}

bool AwaitSocketReceiveExactAwaiter::await_ready() const { return false; }

bool AwaitSocketReceiveExactAwaiter::await_suspend(AwaitTask::Handle newContinuation)
{
    setupCancellableAwait(continuation, stopCallback, newContinuation, this, AwaitSocketReceiveExactAwaiter::cancel);

    if (outResult != nullptr)
    {
        *outResult = {};
    }
    if (buffer.empty())
    {
        operationResult = updateOutResult(false);
        return false;
    }

    request.callback = [this](AsyncSocketReceive::Result& result)
    {
        Span<char> received;
        operationResult = result.get(received);
        if (operationResult)
        {
            const size_t bytesReceived = received.sizeInBytes();
            numBytesReceived += bytesReceived;
            operationResult = updateOutResult(result.completionData.disconnected);
            if (not operationResult)
            {
                continuation.resume();
                return;
            }

            if (numBytesReceived < buffer.sizeInBytes())
            {
                if (result.completionData.disconnected)
                {
                    operationResult = Result::Error("AwaitSocketReceiveExact disconnected before buffer was full");
                    continuation.resume();
                    return;
                }
                if (bytesReceived == 0)
                {
                    operationResult = Result::Error("AwaitSocketReceiveExact made no progress");
                    continuation.resume();
                    return;
                }

                operationResult = updateRequestBuffer();
                if (operationResult)
                {
                    result.reactivateRequest(true);
                    return;
                }
            }
        }
        continuation.resume();
    };

    operationResult = startRemainingReceive();
    return operationResult;
}

Result AwaitSocketReceiveExactAwaiter::await_resume()
{
    clearCancellation(continuation, this);
    return operationResult;
}

Result AwaitSocketReceiveExactAwaiter::cancel(void* object, AwaitEventLoop& eventLoop)
{
    return static_cast<AwaitSocketReceiveExactAwaiter*>(object)->cancel(eventLoop);
}

Result AwaitSocketReceiveExactAwaiter::cancel(AwaitEventLoop& eventLoop)
{
    return cancelCancellableAwait(request, operationResult, eventLoop, stopCallback);
}

Result AwaitSocketReceiveExactAwaiter::startRemainingReceive()
{
    SC_TRY(updateRequestBuffer());
    return request.start(await.asyncEventLoop(), socket, request.buffer);
}

Result AwaitSocketReceiveExactAwaiter::updateRequestBuffer()
{
    Span<char> remaining;
    SC_TRY(buffer.sliceStart(numBytesReceived, remaining));
    request.buffer = remaining;
    return Result(true);
}

Result AwaitSocketReceiveExactAwaiter::updateOutResult(bool disconnected)
{
    if (outResult != nullptr)
    {
        SC_TRY(buffer.sliceStartLength(0, numBytesReceived, outResult->data));
        outResult->disconnected = disconnected;
    }
    return Result(true);
}

AwaitSocketReceiveLineAwaiter::AwaitSocketReceiveLineAwaiter(AwaitEventLoop& await, const SocketDescriptor& socket,
                                                             Span<char> buffer, AwaitSocketReceiveLineResult& outResult)
    : await(await), socket(socket), buffer(buffer), outResult(outResult)
{}

bool AwaitSocketReceiveLineAwaiter::await_ready() const { return false; }

bool AwaitSocketReceiveLineAwaiter::await_suspend(AwaitTask::Handle newContinuation)
{
    setupCancellableAwait(continuation, stopCallback, newContinuation, this, AwaitSocketReceiveLineAwaiter::cancel);

    outResult = {};
    if (buffer.empty())
    {
        operationResult = Result::Error("AwaitSocketReceiveLine buffer is empty");
        return false;
    }

    request.callback = [this](AsyncSocketReceive::Result& result)
    {
        Span<char> received;
        operationResult = result.get(received);
        if (operationResult)
        {
            if (result.completionData.disconnected)
            {
                operationResult = updateOutResult(true);
                continuation.resume();
                return;
            }
            if (received.empty())
            {
                operationResult = Result::Error("AwaitSocketReceiveLine made no progress");
                continuation.resume();
                return;
            }

            if (currentByte == '\n')
            {
                lineComplete    = true;
                operationResult = updateOutResult(false);
                continuation.resume();
                return;
            }
            if (numBytesReceived >= buffer.sizeInBytes())
            {
                operationResult = Result::Error("AwaitSocketReceiveLine buffer exhausted before newline");
                (void)updateOutResult(false);
                continuation.resume();
                return;
            }

            buffer[numBytesReceived++] = currentByte;
            operationResult            = startNextByte();
            if (operationResult)
            {
                result.reactivateRequest(true);
                return;
            }
        }
        continuation.resume();
    };

    operationResult = startNextByte();
    if (not operationResult)
    {
        return false;
    }
    operationResult = request.start(await.asyncEventLoop(), socket, request.buffer);
    return operationResult;
}

Result AwaitSocketReceiveLineAwaiter::await_resume()
{
    clearCancellation(continuation, this);
    return operationResult;
}

Result AwaitSocketReceiveLineAwaiter::cancel(void* object, AwaitEventLoop& eventLoop)
{
    return static_cast<AwaitSocketReceiveLineAwaiter*>(object)->cancel(eventLoop);
}

Result AwaitSocketReceiveLineAwaiter::cancel(AwaitEventLoop& eventLoop)
{
    return cancelCancellableAwait(request, operationResult, eventLoop, stopCallback);
}

Result AwaitSocketReceiveLineAwaiter::startNextByte()
{
    request.buffer = {&currentByte, sizeof(currentByte)};
    return Result(true);
}

Result AwaitSocketReceiveLineAwaiter::updateOutResult(bool disconnected)
{
    size_t lineSize = numBytesReceived;
    if (lineComplete and lineSize > 0 and buffer[lineSize - 1] == '\r')
    {
        lineSize--;
    }
    SC_TRY(buffer.sliceStartLength(0, lineSize, outResult.line));
    outResult.disconnected = disconnected;
    outResult.lineComplete = lineComplete;
    return Result(true);
}

AwaitSocketReceiveFromAwaiter::AwaitSocketReceiveFromAwaiter(AwaitEventLoop& await, const SocketDescriptor& socket,
                                                             Span<char> buffer, AwaitSocketReceiveFromResult& outResult)
    : await(await), socket(socket), buffer(buffer), outResult(outResult)
{}

bool AwaitSocketReceiveFromAwaiter::await_ready() const { return false; }

bool AwaitSocketReceiveFromAwaiter::await_suspend(AwaitTask::Handle newContinuation)
{
    setupCancellableAwait(continuation, stopCallback, newContinuation, this, AwaitSocketReceiveFromAwaiter::cancel);

    outResult        = {};
    request.callback = [this](AsyncSocketReceiveFrom::Result& result)
    {
        operationResult         = result.get(outResult.data);
        outResult.sourceAddress = result.getSourceAddress();
        outResult.disconnected  = result.completionData.disconnected;
        continuation.resume();
    };

    operationResult = request.start(await.asyncEventLoop(), socket, buffer);
    return operationResult;
}

Result AwaitSocketReceiveFromAwaiter::await_resume()
{
    clearCancellation(continuation, this);
    return operationResult;
}

Result AwaitSocketReceiveFromAwaiter::cancel(void* object, AwaitEventLoop& eventLoop)
{
    return static_cast<AwaitSocketReceiveFromAwaiter*>(object)->cancel(eventLoop);
}

Result AwaitSocketReceiveFromAwaiter::cancel(AwaitEventLoop& eventLoop)
{
    return cancelCancellableAwait(request, operationResult, eventLoop, stopCallback);
}

AwaitFileReadAwaiter::AwaitFileReadAwaiter(AwaitEventLoop& await, const FileDescriptor& file, Span<char> buffer,
                                           AwaitFileReadResult& outResult, AwaitFileReadOptions options)
    : await(await), file(file), buffer(buffer), outResult(outResult), options(options)
{}

bool AwaitFileReadAwaiter::await_ready() const { return false; }

bool AwaitFileReadAwaiter::await_suspend(AwaitTask::Handle newContinuation)
{
    setupCancellableAwait(continuation, stopCallback, newContinuation, this, AwaitFileReadAwaiter::cancel);

    outResult        = {};
    request.callback = [this](AsyncFileRead::Result& result)
    {
        operationResult     = result.get(outResult.data);
        outResult.endOfFile = result.completionData.endOfFile;
        continuation.resume();
    };

    if (options.threadPool != nullptr)
    {
        operationResult = request.executeOn(taskSequence, *options.threadPool);
        if (not operationResult)
        {
            return false;
        }
    }

    if (options.useOffset)
    {
        request.setOffset(options.offset);
    }
    operationResult = request.start(await.asyncEventLoop(), file, buffer);
    return operationResult;
}

Result AwaitFileReadAwaiter::await_resume()
{
    clearCancellation(continuation, this);
    return operationResult;
}

Result AwaitFileReadAwaiter::cancel(void* object, AwaitEventLoop& eventLoop)
{
    return static_cast<AwaitFileReadAwaiter*>(object)->cancel(eventLoop);
}

Result AwaitFileReadAwaiter::cancel(AwaitEventLoop& eventLoop)
{
    return cancelCancellableAwait(request, operationResult, eventLoop, stopCallback);
}

AwaitFileReadUntilFullOrEOFAwaiter::AwaitFileReadUntilFullOrEOFAwaiter(AwaitEventLoop&       await,
                                                                       const FileDescriptor& file, Span<char> buffer,
                                                                       AwaitFileReadResult& outResult,
                                                                       AwaitFileReadOptions options)
    : await(await), file(file), buffer(buffer), outResult(outResult), options(options)
{}

bool AwaitFileReadUntilFullOrEOFAwaiter::await_ready() const { return false; }

bool AwaitFileReadUntilFullOrEOFAwaiter::await_suspend(AwaitTask::Handle newContinuation)
{
    setupCancellableAwait(continuation, stopCallback, newContinuation, this,
                          AwaitFileReadUntilFullOrEOFAwaiter::cancel);

    outResult = {};
    if (buffer.empty())
    {
        operationResult = updateOutResult(false);
        return false;
    }

    request.callback = [this](AsyncFileRead::Result& result)
    {
        Span<char> readData;
        operationResult = result.get(readData);
        if (operationResult)
        {
            const size_t bytesRead = readData.sizeInBytes();
            numBytesRead += bytesRead;
            operationResult = updateOutResult(result.completionData.endOfFile);
            if (not operationResult)
            {
                continuation.resume();
                return;
            }

            if (numBytesRead < buffer.sizeInBytes())
            {
                if (result.completionData.endOfFile)
                {
                    continuation.resume();
                    return;
                }
                if (bytesRead == 0)
                {
                    operationResult = Result::Error("AwaitFileReadUntilFullOrEOF made no progress");
                    continuation.resume();
                    return;
                }

                operationResult = updateRequestBufferAndOffset();
                if (operationResult)
                {
                    result.reactivateRequest(true);
                    return;
                }
            }
        }
        continuation.resume();
    };

    if (options.threadPool != nullptr)
    {
        operationResult = request.executeOn(taskSequence, *options.threadPool);
        if (not operationResult)
        {
            return false;
        }
    }

    operationResult = startRemainingRead();
    return operationResult;
}

Result AwaitFileReadUntilFullOrEOFAwaiter::await_resume()
{
    clearCancellation(continuation, this);
    return operationResult;
}

Result AwaitFileReadUntilFullOrEOFAwaiter::cancel(void* object, AwaitEventLoop& eventLoop)
{
    return static_cast<AwaitFileReadUntilFullOrEOFAwaiter*>(object)->cancel(eventLoop);
}

Result AwaitFileReadUntilFullOrEOFAwaiter::cancel(AwaitEventLoop& eventLoop)
{
    return cancelCancellableAwait(request, operationResult, eventLoop, stopCallback);
}

Result AwaitFileReadUntilFullOrEOFAwaiter::startRemainingRead()
{
    SC_TRY(updateRequestBufferAndOffset());
    return request.start(await.asyncEventLoop(), file, request.buffer);
}

Result AwaitFileReadUntilFullOrEOFAwaiter::updateRequestBufferAndOffset()
{
    Span<char> remaining;
    SC_TRY(buffer.sliceStart(numBytesRead, remaining));
    request.buffer = remaining;
    if (options.useOffset)
    {
        request.setOffset(options.offset + numBytesRead);
    }
    return Result(true);
}

Result AwaitFileReadUntilFullOrEOFAwaiter::updateOutResult(bool endOfFile)
{
    SC_TRY(buffer.sliceStartLength(0, numBytesRead, outResult.data));
    outResult.endOfFile = endOfFile;
    return Result(true);
}

AwaitFileWriteAwaiter::AwaitFileWriteAwaiter(AwaitEventLoop& await, const FileDescriptor& file, Span<const char> data,
                                             AwaitFileWriteResult* outResult, AwaitFileWriteOptions options)
    : await(await), file(file), data(data), outResult(outResult), options(options)
{}

AwaitFileWriteAwaiter::AwaitFileWriteAwaiter(AwaitEventLoop& await, const FileDescriptor& file,
                                             Span<Span<const char>> data, AwaitFileWriteResult* outResult,
                                             AwaitFileWriteOptions options)
    : await(await), file(file), buffers(data), outResult(outResult), options(options), singleBuffer(false)
{}

bool AwaitFileWriteAwaiter::await_ready() const { return false; }

bool AwaitFileWriteAwaiter::await_suspend(AwaitTask::Handle newContinuation)
{
    setupCancellableAwait(continuation, stopCallback, newContinuation, this, AwaitFileWriteAwaiter::cancel);

    request.callback = [this](AsyncFileWrite::Result& result)
    {
        operationResult = result.isValid();
        if (outResult != nullptr)
        {
            operationResult = result.get(outResult->numBytes);
        }
        continuation.resume();
    };

    if (options.threadPool != nullptr)
    {
        operationResult = request.executeOn(taskSequence, *options.threadPool);
        if (not operationResult)
        {
            return false;
        }
    }

    if (options.useOffset)
    {
        request.setOffset(options.offset);
    }

    if (singleBuffer)
    {
        operationResult = request.start(await.asyncEventLoop(), file, data);
    }
    else
    {
        operationResult = request.start(await.asyncEventLoop(), file, buffers);
    }
    return operationResult;
}

Result AwaitFileWriteAwaiter::await_resume()
{
    clearCancellation(continuation, this);
    return operationResult;
}

Result AwaitFileWriteAwaiter::cancel(void* object, AwaitEventLoop& eventLoop)
{
    return static_cast<AwaitFileWriteAwaiter*>(object)->cancel(eventLoop);
}

Result AwaitFileWriteAwaiter::cancel(AwaitEventLoop& eventLoop)
{
    return cancelCancellableAwait(request, operationResult, eventLoop, stopCallback);
}

AwaitFileSendAwaiter::AwaitFileSendAwaiter(AwaitEventLoop& await, const FileDescriptor& file,
                                           const SocketDescriptor& socket, AwaitFileSendResult& outResult,
                                           AwaitFileSendOptions options)
    : await(await), file(file), socket(socket), outResult(outResult), options(options)
{}

bool AwaitFileSendAwaiter::await_ready() const { return false; }

bool AwaitFileSendAwaiter::await_suspend(AwaitTask::Handle newContinuation)
{
    setupCancellableAwait(continuation, stopCallback, newContinuation, this, AwaitFileSendAwaiter::cancel);

    outResult        = {};
    request.callback = [this](AsyncFileSend::Result& result)
    {
        operationResult            = result.isValid();
        outResult.bytesTransferred = result.getBytesTransferred();
        outResult.usedZeroCopy     = result.usedZeroCopy();
        outResult.complete         = result.isComplete();
        continuation.resume();
    };

    if (options.threadPool != nullptr)
    {
        operationResult = request.executeOn(taskSequence, *options.threadPool);
        if (not operationResult)
        {
            return false;
        }
    }

    operationResult =
        request.start(await.asyncEventLoop(), file, socket, options.offset, options.length, options.pipeSize);
    return operationResult;
}

Result AwaitFileSendAwaiter::await_resume()
{
    clearCancellation(continuation, this);
    return operationResult;
}

Result AwaitFileSendAwaiter::cancel(void* object, AwaitEventLoop& eventLoop)
{
    return static_cast<AwaitFileSendAwaiter*>(object)->cancel(eventLoop);
}

Result AwaitFileSendAwaiter::cancel(AwaitEventLoop& eventLoop)
{
    return cancelCancellableAwait(request, operationResult, eventLoop, stopCallback);
}

AwaitFilePollAwaiter::AwaitFilePollAwaiter(AwaitEventLoop& await, const FileDescriptor& file) : await(await), file(file)
{}

bool AwaitFilePollAwaiter::await_ready() const { return false; }

bool AwaitFilePollAwaiter::await_suspend(AwaitTask::Handle newContinuation)
{
    setupCancellableAwait(continuation, stopCallback, newContinuation, this, AwaitFilePollAwaiter::cancel);

#if SC_PLATFORM_WINDOWS
    operationResult = Result::Error("Await filePoll is not supported on Windows");
    return false;
#else
    request.callback = [this](AsyncFilePoll::Result& result)
    {
        operationResult = result.isValid();
        continuation.resume();
    };

    FileDescriptor::Handle handle = FileDescriptor::Invalid;
    operationResult               = file.get(handle, Result::Error("Await filePoll invalid file"));
    if (not operationResult)
    {
        return false;
    }
    operationResult = request.start(await.asyncEventLoop(), handle);
    return operationResult;
#endif
}

Result AwaitFilePollAwaiter::await_resume()
{
    clearCancellation(continuation, this);
    return operationResult;
}

Result AwaitFilePollAwaiter::cancel(void* object, AwaitEventLoop& eventLoop)
{
    return static_cast<AwaitFilePollAwaiter*>(object)->cancel(eventLoop);
}

Result AwaitFilePollAwaiter::cancel(AwaitEventLoop& eventLoop)
{
    return cancelCancellableAwait(request, operationResult, eventLoop, stopCallback);
}

AwaitFileSystemOperationAwaiter::AwaitFileSystemOperationAwaiter(AwaitEventLoop& await, ThreadPool& threadPool,
                                                                 AwaitFileSystemOperationType operation,
                                                                 StringSpan path, StringSpan otherPath, FileOpen mode,
                                                                 FileDescriptor* outFile, FileSystemCopyFlags copyFlags)
    : await(await), threadPool(threadPool), operation(operation), path(path), otherPath(otherPath), mode(mode),
      outFile(outFile), copyFlags(copyFlags)
{}

AwaitFileSystemOperationAwaiter::AwaitFileSystemOperationAwaiter(AwaitEventLoop& await, ThreadPool& threadPool,
                                                                 AwaitFileSystemOperationType operation,
                                                                 FileDescriptor&              file)
    : await(await), threadPool(threadPool), operation(operation), fileToClose(&file)
{}

AwaitFileSystemOperationAwaiter::AwaitFileSystemOperationAwaiter(AwaitEventLoop& await, ThreadPool& threadPool,
                                                                 AwaitFileSystemOperationType operation,
                                                                 FileDescriptor& file, Span<char> buffer,
                                                                 AwaitFileReadResult& outResult, uint64_t offset)
    : await(await), threadPool(threadPool), operation(operation), fileToUse(&file), readBuffer(buffer),
      outReadResult(&outResult), offset(offset)
{}

AwaitFileSystemOperationAwaiter::AwaitFileSystemOperationAwaiter(AwaitEventLoop& await, ThreadPool& threadPool,
                                                                 AwaitFileSystemOperationType operation,
                                                                 FileDescriptor& file, Span<const char> data,
                                                                 AwaitFileWriteResult* outResult, uint64_t offset)
    : await(await), threadPool(threadPool), operation(operation), fileToUse(&file), writeBuffer(data),
      outWriteResult(outResult), offset(offset)
{}

bool AwaitFileSystemOperationAwaiter::await_ready() const { return false; }

bool AwaitFileSystemOperationAwaiter::await_suspend(AwaitTask::Handle newContinuation)
{
    setupCancellableAwait(continuation, stopCallback, newContinuation, this, AwaitFileSystemOperationAwaiter::cancel);

    request.callback = [this](AsyncFileSystemOperation::Result& result)
    {
        operationResult = result.isValid();
        if (operationResult and operation == AwaitFileSystemOperationType::Open)
        {
            FileDescriptor openedFile(result.completionData.handle);
            operationResult = outFile->assign(move(openedFile));
        }
        else if (operationResult and operation == AwaitFileSystemOperationType::Read)
        {
            operationResult =
                Result(readBuffer.sliceStartLength(0, result.completionData.numBytes, outReadResult->data));
            outReadResult->endOfFile = result.completionData.numBytes == 0;
        }
        else if (operationResult and operation == AwaitFileSystemOperationType::Write and outWriteResult != nullptr)
        {
            outWriteResult->numBytes = result.completionData.numBytes;
        }
        continuation.resume();
    };

    operationResult = request.setThreadPool(threadPool);
    if (not operationResult)
    {
        return false;
    }

    switch (operation)
    {
    case AwaitFileSystemOperationType::Open:
        if (outFile == nullptr)
        {
            operationResult = Result::Error("Await fsOpen missing output file");
            return false;
        }
        operationResult = request.open(await.asyncEventLoop(), path, mode);
        return operationResult;
    case AwaitFileSystemOperationType::Close: {
        if (fileToClose == nullptr)
        {
            operationResult = Result::Error("Await fsClose missing file");
            return false;
        }
        FileDescriptor::Handle handle = FileDescriptor::Invalid;
        operationResult               = fileToClose->get(handle, Result::Error("Await fsClose invalid file"));
        if (not operationResult)
        {
            return false;
        }
        operationResult = request.close(await.asyncEventLoop(), handle);
        if (operationResult)
        {
            fileToClose->detach();
        }
        return operationResult;
    }
    case AwaitFileSystemOperationType::Read: {
        if (fileToUse == nullptr or outReadResult == nullptr)
        {
            operationResult = Result::Error("Await fsRead missing file or result");
            return false;
        }
        FileDescriptor::Handle handle = FileDescriptor::Invalid;
        operationResult               = fileToUse->get(handle, Result::Error("Await fsRead invalid file"));
        if (not operationResult)
        {
            return false;
        }
        *outReadResult  = {};
        operationResult = request.read(await.asyncEventLoop(), handle, readBuffer, offset);
        return operationResult;
    }
    case AwaitFileSystemOperationType::Write: {
        if (fileToUse == nullptr)
        {
            operationResult = Result::Error("Await fsWrite missing file");
            return false;
        }
        FileDescriptor::Handle handle = FileDescriptor::Invalid;
        operationResult               = fileToUse->get(handle, Result::Error("Await fsWrite invalid file"));
        if (not operationResult)
        {
            return false;
        }
        if (outWriteResult != nullptr)
        {
            *outWriteResult = {};
        }
        operationResult = request.write(await.asyncEventLoop(), handle, writeBuffer, offset);
        return operationResult;
    }
    case AwaitFileSystemOperationType::CopyFile:
        operationResult = request.copyFile(await.asyncEventLoop(), path, otherPath, copyFlags);
        return operationResult;
    case AwaitFileSystemOperationType::CopyDirectory:
        operationResult = request.copyDirectory(await.asyncEventLoop(), path, otherPath, copyFlags);
        return operationResult;
    case AwaitFileSystemOperationType::Rename:
        operationResult = request.rename(await.asyncEventLoop(), path, otherPath);
        return operationResult;
    case AwaitFileSystemOperationType::RemoveEmptyDirectory:
        operationResult = request.removeEmptyDirectory(await.asyncEventLoop(), path);
        return operationResult;
    case AwaitFileSystemOperationType::RemoveFile:
        operationResult = request.removeFile(await.asyncEventLoop(), path);
        return operationResult;
    }

    operationResult = Result::Error("Await file system operation is invalid");
    return false;
}

Result AwaitFileSystemOperationAwaiter::await_resume()
{
    clearCancellation(continuation, this);
    return operationResult;
}

Result AwaitFileSystemOperationAwaiter::cancel(void* object, AwaitEventLoop& eventLoop)
{
    return static_cast<AwaitFileSystemOperationAwaiter*>(object)->cancel(eventLoop);
}

Result AwaitFileSystemOperationAwaiter::cancel(AwaitEventLoop& eventLoop)
{
    return cancelCancellableAwait(request, operationResult, eventLoop, stopCallback);
}

AwaitTaskGroup::AwaitTaskGroup(AwaitEventLoop& await, Span<AwaitTask*> taskStorage,
                               AwaitTaskGroupCancelPolicy cancelPolicy)
    : await(await), tasks(taskStorage), cancelPolicy(cancelPolicy)
{}

Result AwaitTaskGroup::spawn(AwaitTask& task)
{
    if (numTasks >= tasks.sizeInElements())
    {
        return Result::Error("AwaitTaskGroup storage is full");
    }
    SC_TRY(await.spawn(task));
    tasks[numTasks] = &task;
    numTasks++;
    return Result(true);
}

AwaitTaskGroupWaitAllAwaiter AwaitTaskGroup::waitAll() { return AwaitTaskGroupWaitAllAwaiter(*this); }

AwaitTaskGroupWaitAnyAwaiter AwaitTaskGroup::waitAny(AwaitTaskGroupWaitAnyResult& outResult,
                                                     AwaitTaskGroupWaitAnyPolicy  waitAnyPolicy)
{
    return AwaitTaskGroupWaitAnyAwaiter(*this, outResult, waitAnyPolicy);
}

Result AwaitTaskGroup::collectResults(Span<Result> outResults, AwaitTaskGroupResultSummary* outSummary) const
{
    if (outResults.sizeInElements() < numTasks)
    {
        return Result::Error("AwaitTaskGroup result storage is too small");
    }

    AwaitTaskGroupResultSummary summary;
    summary.numTasks = numTasks;

    Result aggregate(true);
    for (size_t idx = 0; idx < numTasks; ++idx)
    {
        AwaitTask* task = tasks[idx];
        if (task == nullptr)
        {
            return Result::Error("AwaitTaskGroup contains invalid task");
        }

        Result taskResult = task->result();
        outResults[idx]   = taskResult;
        if (task->isCompleted())
        {
            summary.numCompleted++;
        }
        if (taskResult)
        {
            summary.numSucceeded++;
        }
        else
        {
            summary.numFailed++;
            if (summary.firstFailureTask == nullptr)
            {
                summary.firstFailureIndex = idx;
                summary.firstFailureTask  = task;
                summary.firstFailure      = taskResult;
                aggregate                 = taskResult;
            }
        }
    }

    if (outSummary != nullptr)
    {
        *outSummary = summary;
    }
    return aggregate;
}

Result AwaitTaskGroup::summarizeResults(AwaitTaskGroupResultSummary& outSummary) const
{
    AwaitTaskGroupResultSummary summary;
    summary.numTasks = numTasks;

    Result aggregate(true);
    for (size_t idx = 0; idx < numTasks; ++idx)
    {
        AwaitTask* task = tasks[idx];
        if (task == nullptr)
        {
            return Result::Error("AwaitTaskGroup contains invalid task");
        }

        Result taskResult = task->result();
        if (task->isCompleted())
        {
            summary.numCompleted++;
        }
        if (taskResult)
        {
            summary.numSucceeded++;
        }
        else
        {
            summary.numFailed++;
            if (summary.firstFailureTask == nullptr)
            {
                summary.firstFailureIndex = idx;
                summary.firstFailureTask  = task;
                summary.firstFailure      = taskResult;
                aggregate                 = taskResult;
            }
        }
    }

    outSummary = summary;
    return aggregate;
}

size_t AwaitTaskGroup::size() const { return numTasks; }

size_t AwaitTaskGroup::capacity() const { return tasks.sizeInElements(); }

size_t AwaitTaskGroup::remainingCapacity() const { return capacity() - size(); }

bool AwaitTaskGroup::isEmpty() const { return size() == 0; }

bool AwaitTaskGroup::isFull() const { return size() == capacity(); }

AwaitTaskRegistry::AwaitTaskRegistry(AwaitEventLoop& await, Span<AwaitTask> taskStorage)
    : await(await), tasks(taskStorage)
{}

Result AwaitTaskRegistry::spawn(AwaitTask&& task, AwaitTaskRegistrySpawnResult* outResult)
{
    if (not task.isValid())
    {
        return Result::Error("AwaitTask is invalid");
    }
    if (task.isStarted())
    {
        return Result::Error("AwaitTaskRegistry can only spawn unstarted tasks");
    }

    for (size_t idx = 0; idx < tasks.sizeInElements(); ++idx)
    {
        AwaitTask& slot = tasks[idx];
        if (slot.isValid())
        {
            continue;
        }

        slot               = move(task);
        Result spawnResult = await.spawn(slot);
        if (not spawnResult)
        {
            slot = AwaitTask();
            return spawnResult;
        }

        if (outResult != nullptr)
        {
            outResult->index = idx;
            outResult->task  = &slot;
        }
        return Result(true);
    }
    return Result::Error("AwaitTaskRegistry storage is full");
}

Result AwaitTaskRegistry::cancelAll()
{
    Result result(true);
    for (size_t idx = 0; idx < tasks.sizeInElements(); ++idx)
    {
        AwaitTask& task = tasks[idx];
        if (task.isActive())
        {
            Result cancelResult = task.cancel(await);
            if (result and not cancelResult)
            {
                result = cancelResult;
            }
        }
    }
    return result;
}

AwaitTaskRegistryWaitAllAwaiter AwaitTaskRegistry::waitAll() { return AwaitTaskRegistryWaitAllAwaiter(*this); }

AwaitTaskRegistryWaitAnyAwaiter AwaitTaskRegistry::waitAny(AwaitTaskRegistryWaitAnyResult& outResult,
                                                           AwaitTaskRegistryWaitAnyPolicy  waitAnyPolicy)
{
    return AwaitTaskRegistryWaitAnyAwaiter(*this, outResult, waitAnyPolicy);
}

size_t AwaitTaskRegistry::clearCompleted(AwaitTaskGroupResultSummary* outSummary)
{
    AwaitTaskGroupResultSummary summary;
    size_t                      numCleared = 0;

    for (size_t idx = 0; idx < tasks.sizeInElements(); ++idx)
    {
        AwaitTask& task = tasks[idx];
        if (not task.isValid())
        {
            continue;
        }

        summary.numTasks++;
        if (not task.isCompleted())
        {
            continue;
        }

        summary.numCompleted++;
        Result taskResult = task.result();
        if (taskResult)
        {
            summary.numSucceeded++;
        }
        else
        {
            summary.numFailed++;
            if (summary.firstFailureTask == nullptr)
            {
                summary.firstFailureIndex = idx;
                summary.firstFailureTask  = &task;
                summary.firstFailure      = taskResult;
            }
        }

        task = AwaitTask();
        numCleared++;
    }

    if (outSummary != nullptr)
    {
        *outSummary = summary;
    }
    return numCleared;
}

AwaitTask* AwaitTaskRegistry::taskAt(size_t index) { return index < tasks.sizeInElements() ? &tasks[index] : nullptr; }

const AwaitTask* AwaitTaskRegistry::taskAt(size_t index) const
{
    return index < tasks.sizeInElements() ? &tasks[index] : nullptr;
}

size_t AwaitTaskRegistry::size() const
{
    size_t count = 0;
    for (size_t idx = 0; idx < tasks.sizeInElements(); ++idx)
    {
        if (tasks[idx].isValid())
        {
            count++;
        }
    }
    return count;
}

size_t AwaitTaskRegistry::activeCount() const
{
    size_t count = 0;
    for (size_t idx = 0; idx < tasks.sizeInElements(); ++idx)
    {
        if (tasks[idx].isActive())
        {
            count++;
        }
    }
    return count;
}

size_t AwaitTaskRegistry::completedCount() const
{
    size_t count = 0;
    for (size_t idx = 0; idx < tasks.sizeInElements(); ++idx)
    {
        if (tasks[idx].isCompleted())
        {
            count++;
        }
    }
    return count;
}

size_t AwaitTaskRegistry::capacity() const { return tasks.sizeInElements(); }

size_t AwaitTaskRegistry::remainingCapacity() const { return capacity() - size(); }

bool AwaitTaskRegistry::isEmpty() const { return size() == 0; }

bool AwaitTaskRegistry::isFull() const { return size() == capacity(); }

bool AwaitTaskRegistry::hasActiveTasks() const { return activeCount() != 0; }

bool AwaitTaskRegistry::hasCompletedTasks() const { return completedCount() != 0; }

AwaitTaskRegistryWaitAllAwaiter::AwaitTaskRegistryWaitAllAwaiter(AwaitTaskRegistry& registry) : registry(registry) {}

bool AwaitTaskRegistryWaitAllAwaiter::await_ready() const { return false; }

bool AwaitTaskRegistryWaitAllAwaiter::await_suspend(AwaitTask::Handle newContinuation)
{
    continuation = newContinuation;

    for (size_t idx = 0; idx < registry.tasks.sizeInElements(); ++idx)
    {
        AwaitTask& task = registry.tasks[idx];
        if (not task.isValid())
        {
            continue;
        }

        totalTasks++;
        if (task.isCompleted())
        {
            completedTasks++;
            continue;
        }
        if (not task.isActive())
        {
            operationResult = Result::Error("AwaitTaskRegistry contains inactive task");
            clearTaskCallbacks();
            return false;
        }

        AwaitTask::Promise& promise = task.handle.promise();
        if (promise.completionCallback != nullptr or promise.continuation != nullptr)
        {
            operationResult = Result::Error("AwaitTask is already being awaited");
            clearTaskCallbacks();
            return false;
        }

        promise.completionObject   = this;
        promise.completionCallback = AwaitTaskRegistryWaitAllAwaiter::onTaskCompleted;
    }

    if (completedTasks == totalTasks)
    {
        operationResult = collectResult();
        return false;
    }

    continuation.promise().cancellation = {this, AwaitTaskRegistryWaitAllAwaiter::cancel};
    return true;
}

Result AwaitTaskRegistryWaitAllAwaiter::await_resume()
{
    clearCancellation(continuation, this);
    return operationResult;
}

Result AwaitTaskRegistryWaitAllAwaiter::cancel(void* object, AwaitEventLoop& eventLoop)
{
    return static_cast<AwaitTaskRegistryWaitAllAwaiter*>(object)->cancel(eventLoop);
}

Result AwaitTaskRegistryWaitAllAwaiter::cancel(AwaitEventLoop&)
{
    operationResult     = AwaitCancelledResult();
    Result cancelResult = registry.cancelAll();
    if (not cancelResult)
    {
        return cancelResult;
    }
    if (registry.activeCount() == 0)
    {
        finish(operationResult);
    }
    return Result(true);
}

void AwaitTaskRegistryWaitAllAwaiter::onTaskCompleted(void* object)
{
    static_cast<AwaitTaskRegistryWaitAllAwaiter*>(object)->onTaskCompleted();
}

void AwaitTaskRegistryWaitAllAwaiter::onTaskCompleted()
{
    if (finished)
    {
        return;
    }

    completedTasks++;
    if (completedTasks == totalTasks)
    {
        finish(operationResult ? collectResult() : operationResult);
    }
}

void AwaitTaskRegistryWaitAllAwaiter::finish(Result result)
{
    if (finished)
    {
        return;
    }
    finished        = true;
    operationResult = result;
    clearTaskCallbacks();
    continuation.resume();
}

void AwaitTaskRegistryWaitAllAwaiter::clearTaskCallbacks()
{
    for (size_t idx = 0; idx < registry.tasks.sizeInElements(); ++idx)
    {
        AwaitTask& task = registry.tasks[idx];
        if (not task.isValid())
        {
            continue;
        }
        AwaitTask::Promise& promise = task.handle.promise();
        if (promise.completionObject == this)
        {
            promise.completionObject   = nullptr;
            promise.completionCallback = nullptr;
        }
    }
}

Result AwaitTaskRegistryWaitAllAwaiter::collectResult() const
{
    for (size_t idx = 0; idx < registry.tasks.sizeInElements(); ++idx)
    {
        const AwaitTask& task = registry.tasks[idx];
        if (task.isValid())
        {
            SC_TRY(task.result());
        }
    }
    return Result(true);
}

AwaitTaskRegistryWaitAnyAwaiter::AwaitTaskRegistryWaitAnyAwaiter(AwaitTaskRegistry&              registry,
                                                                 AwaitTaskRegistryWaitAnyResult& outResult,
                                                                 AwaitTaskRegistryWaitAnyPolicy  waitAnyPolicy)
    : registry(registry), outResult(outResult), waitAnyPolicy(waitAnyPolicy)
{}

bool AwaitTaskRegistryWaitAnyAwaiter::await_ready() const { return false; }

bool AwaitTaskRegistryWaitAnyAwaiter::await_suspend(AwaitTask::Handle newContinuation)
{
    continuation = newContinuation;
    outResult    = {};

    for (size_t idx = 0; idx < registry.tasks.sizeInElements(); ++idx)
    {
        AwaitTask& task = registry.tasks[idx];
        if (not task.isValid())
        {
            continue;
        }

        totalTasks++;
        if (task.isCompleted())
        {
            completedTasks++;
            if (winnerIndex == size_t(-1))
            {
                operationResult = setWinner(idx);
            }
            continue;
        }
        if (not task.isActive())
        {
            operationResult = Result::Error("AwaitTaskRegistry contains inactive task");
            clearTaskCallbacks();
            return false;
        }

        AwaitTask::Promise& promise = task.handle.promise();
        if (promise.completionCallback != nullptr or promise.continuation != nullptr)
        {
            operationResult = Result::Error("AwaitTask is already being awaited");
            clearTaskCallbacks();
            return false;
        }

        promise.completionObject   = this;
        promise.completionCallback = AwaitTaskRegistryWaitAnyAwaiter::onTaskCompleted;
    }

    if (totalTasks == 0)
    {
        operationResult = Result::Error("AwaitTaskRegistry is empty");
        return false;
    }

    if (winnerIndex == size_t(-1) and not operationResult)
    {
        clearTaskCallbacks();
        return false;
    }

    continuation.promise().cancellation = {this, AwaitTaskRegistryWaitAnyAwaiter::cancel};

    if (winnerIndex != size_t(-1))
    {
        if (waitAnyPolicy == AwaitTaskRegistryWaitAnyPolicy::LeaveRemainingRunning or completedTasks == totalTasks)
        {
            clearTaskCallbacks();
            return false;
        }

        Result cancelResult = cancelRemaining(registry.await);
        if (not cancelResult)
        {
            operationResult = cancelResult;
            clearTaskCallbacks();
            return false;
        }
        return completedTasks != totalTasks;
    }

    return true;
}

Result AwaitTaskRegistryWaitAnyAwaiter::await_resume()
{
    clearCancellation(continuation, this);
    return operationResult;
}

Result AwaitTaskRegistryWaitAnyAwaiter::cancel(void* object, AwaitEventLoop& eventLoop)
{
    return static_cast<AwaitTaskRegistryWaitAnyAwaiter*>(object)->cancel(eventLoop);
}

Result AwaitTaskRegistryWaitAnyAwaiter::cancel(AwaitEventLoop& eventLoop)
{
    operationResult = AwaitCancelledResult();
    if (waitAnyPolicy == AwaitTaskRegistryWaitAnyPolicy::LeaveRemainingRunning)
    {
        finish(operationResult);
        return Result(true);
    }

    cancelling = true;
    return cancelRemaining(eventLoop);
}

void AwaitTaskRegistryWaitAnyAwaiter::onTaskCompleted(void* object)
{
    static_cast<AwaitTaskRegistryWaitAnyAwaiter*>(object)->onTaskCompleted();
}

void AwaitTaskRegistryWaitAnyAwaiter::onTaskCompleted()
{
    if (finished)
    {
        return;
    }

    completedTasks++;
    if (winnerIndex == size_t(-1) and not cancelling)
    {
        for (size_t idx = 0; idx < registry.tasks.sizeInElements(); ++idx)
        {
            AwaitTask& task = registry.tasks[idx];
            if (task.isCompleted())
            {
                operationResult = setWinner(idx);
                break;
            }
        }

        if (waitAnyPolicy == AwaitTaskRegistryWaitAnyPolicy::LeaveRemainingRunning)
        {
            finish(operationResult);
            return;
        }

        Result cancelResult = cancelRemaining(registry.await);
        if (not cancelResult)
        {
            finish(cancelResult);
            return;
        }
    }

    if ((cancelling or waitAnyPolicy == AwaitTaskRegistryWaitAnyPolicy::CancelRemaining) and
        completedTasks == totalTasks)
    {
        finish(operationResult);
    }
}

void AwaitTaskRegistryWaitAnyAwaiter::finish(Result result)
{
    if (finished)
    {
        return;
    }
    finished        = true;
    operationResult = result;
    clearTaskCallbacks();
    continuation.resume();
}

void AwaitTaskRegistryWaitAnyAwaiter::clearTaskCallbacks()
{
    for (size_t idx = 0; idx < registry.tasks.sizeInElements(); ++idx)
    {
        AwaitTask& task = registry.tasks[idx];
        if (not task.isValid())
        {
            continue;
        }
        AwaitTask::Promise& promise = task.handle.promise();
        if (promise.completionObject == this)
        {
            promise.completionObject   = nullptr;
            promise.completionCallback = nullptr;
        }
    }
}

Result AwaitTaskRegistryWaitAnyAwaiter::setWinner(size_t index)
{
    AwaitTask& task = registry.tasks[index];
    if (not task.isValid())
    {
        return Result::Error("AwaitTaskRegistry contains invalid task");
    }

    winnerIndex     = index;
    outResult.index = index;
    outResult.task  = &task;
    return task.result();
}

Result AwaitTaskRegistryWaitAnyAwaiter::cancelRemaining(AwaitEventLoop& eventLoop)
{
    Result cancelResult(true);
    for (size_t idx = 0; idx < registry.tasks.sizeInElements(); ++idx)
    {
        AwaitTask& task = registry.tasks[idx];
        if (idx != winnerIndex and task.isActive())
        {
            Result taskCancelResult = task.cancel(eventLoop);
            if (cancelResult and not taskCancelResult)
            {
                cancelResult = taskCancelResult;
            }
        }
    }
    return cancelResult;
}

AwaitTaskGroupWaitAllAwaiter::AwaitTaskGroupWaitAllAwaiter(AwaitTaskGroup& group) : group(group) {}

bool AwaitTaskGroupWaitAllAwaiter::await_ready() const { return group.numTasks == 0; }

bool AwaitTaskGroupWaitAllAwaiter::await_suspend(AwaitTask::Handle newContinuation)
{
    continuation = newContinuation;

    for (size_t idx = 0; idx < group.numTasks; ++idx)
    {
        AwaitTask* task = group.tasks[idx];
        if (task == nullptr or not task->isValid())
        {
            operationResult = Result::Error("AwaitTaskGroup contains invalid task");
            clearChildCallbacks();
            return false;
        }
        if (task->isCompleted())
        {
            completedTasks++;
            continue;
        }
        if (not task->isActive())
        {
            operationResult = Result::Error("AwaitTaskGroup contains inactive task");
            clearChildCallbacks();
            return false;
        }

        AwaitTask::Promise& promise = task->handle.promise();
        if (promise.completionCallback != nullptr or promise.continuation != nullptr)
        {
            operationResult = Result::Error("AwaitTask is already being awaited");
            clearChildCallbacks();
            return false;
        }

        promise.completionObject   = this;
        promise.completionCallback = AwaitTaskGroupWaitAllAwaiter::onTaskCompleted;
    }

    if (completedTasks == group.numTasks)
    {
        operationResult = collectResult();
        return false;
    }

    continuation.promise().cancellation = {this, AwaitTaskGroupWaitAllAwaiter::cancel};
    return true;
}

Result AwaitTaskGroupWaitAllAwaiter::await_resume()
{
    clearCancellation(continuation, this);
    return operationResult;
}

Result AwaitTaskGroupWaitAllAwaiter::cancel(void* object, AwaitEventLoop& eventLoop)
{
    return static_cast<AwaitTaskGroupWaitAllAwaiter*>(object)->cancel(eventLoop);
}

Result AwaitTaskGroupWaitAllAwaiter::cancel(AwaitEventLoop& eventLoop)
{
    operationResult = AwaitCancelledResult();
    if (group.cancelPolicy == AwaitTaskGroupCancelPolicy::LeaveChildrenRunning)
    {
        finish(operationResult);
        return Result(true);
    }

    Result cancelResult(true);
    for (size_t idx = 0; idx < group.numTasks; ++idx)
    {
        AwaitTask* task = group.tasks[idx];
        if (task != nullptr and task->isActive())
        {
            Result taskCancelResult = task->cancel(eventLoop);
            if (cancelResult and not taskCancelResult)
            {
                cancelResult = taskCancelResult;
            }
        }
    }
    return cancelResult;
}

void AwaitTaskGroupWaitAllAwaiter::onTaskCompleted(void* object)
{
    static_cast<AwaitTaskGroupWaitAllAwaiter*>(object)->onTaskCompleted();
}

void AwaitTaskGroupWaitAllAwaiter::onTaskCompleted()
{
    if (finished)
    {
        return;
    }

    completedTasks++;
    if (completedTasks == group.numTasks)
    {
        finish(operationResult ? collectResult() : operationResult);
    }
}

void AwaitTaskGroupWaitAllAwaiter::finish(Result result)
{
    if (finished)
    {
        return;
    }
    finished        = true;
    operationResult = result;
    clearChildCallbacks();
    continuation.resume();
}

void AwaitTaskGroupWaitAllAwaiter::clearChildCallbacks()
{
    for (size_t idx = 0; idx < group.numTasks; ++idx)
    {
        AwaitTask* task = group.tasks[idx];
        if (task == nullptr or not task->isValid())
        {
            continue;
        }
        AwaitTask::Promise& promise = task->handle.promise();
        if (promise.completionObject == this)
        {
            promise.completionObject   = nullptr;
            promise.completionCallback = nullptr;
        }
    }
}

Result AwaitTaskGroupWaitAllAwaiter::collectResult() const
{
    for (size_t idx = 0; idx < group.numTasks; ++idx)
    {
        AwaitTask* task = group.tasks[idx];
        if (task == nullptr)
        {
            return Result::Error("AwaitTaskGroup contains invalid task");
        }
        SC_TRY(task->result());
    }
    return Result(true);
}

AwaitTaskGroupWaitAnyAwaiter::AwaitTaskGroupWaitAnyAwaiter(AwaitTaskGroup&              group,
                                                           AwaitTaskGroupWaitAnyResult& outResult,
                                                           AwaitTaskGroupWaitAnyPolicy  waitAnyPolicy)
    : group(group), outResult(outResult), waitAnyPolicy(waitAnyPolicy)
{}

bool AwaitTaskGroupWaitAnyAwaiter::await_ready() const { return false; }

bool AwaitTaskGroupWaitAnyAwaiter::await_suspend(AwaitTask::Handle newContinuation)
{
    continuation = newContinuation;
    outResult    = {};

    if (group.numTasks == 0)
    {
        operationResult = Result::Error("AwaitTaskGroup is empty");
        return false;
    }

    for (size_t idx = 0; idx < group.numTasks; ++idx)
    {
        AwaitTask* task = group.tasks[idx];
        if (task == nullptr or not task->isValid())
        {
            operationResult = Result::Error("AwaitTaskGroup contains invalid task");
            clearChildCallbacks();
            return false;
        }
        if (task->isCompleted())
        {
            completedTasks++;
            if (winnerIndex == size_t(-1))
            {
                operationResult = setWinner(idx);
            }
            continue;
        }
        if (not task->isActive())
        {
            operationResult = Result::Error("AwaitTaskGroup contains inactive task");
            clearChildCallbacks();
            return false;
        }

        AwaitTask::Promise& promise = task->handle.promise();
        if (promise.completionCallback != nullptr or promise.continuation != nullptr)
        {
            operationResult = Result::Error("AwaitTask is already being awaited");
            clearChildCallbacks();
            return false;
        }

        promise.completionObject   = this;
        promise.completionCallback = AwaitTaskGroupWaitAnyAwaiter::onTaskCompleted;
    }

    if (winnerIndex == size_t(-1) and not operationResult)
    {
        clearChildCallbacks();
        return false;
    }

    continuation.promise().cancellation = {this, AwaitTaskGroupWaitAnyAwaiter::cancel};

    if (winnerIndex != size_t(-1))
    {
        if (waitAnyPolicy == AwaitTaskGroupWaitAnyPolicy::LeaveRemainingRunning or completedTasks == group.numTasks)
        {
            clearChildCallbacks();
            return false;
        }

        Result cancelResult = cancelRemaining(group.await);
        if (not cancelResult)
        {
            operationResult = cancelResult;
            clearChildCallbacks();
            return false;
        }
        return completedTasks != group.numTasks;
    }

    return true;
}

Result AwaitTaskGroupWaitAnyAwaiter::await_resume()
{
    clearCancellation(continuation, this);
    return operationResult;
}

Result AwaitTaskGroupWaitAnyAwaiter::cancel(void* object, AwaitEventLoop& eventLoop)
{
    return static_cast<AwaitTaskGroupWaitAnyAwaiter*>(object)->cancel(eventLoop);
}

Result AwaitTaskGroupWaitAnyAwaiter::cancel(AwaitEventLoop& eventLoop)
{
    operationResult = AwaitCancelledResult();
    if (group.cancelPolicy == AwaitTaskGroupCancelPolicy::LeaveChildrenRunning)
    {
        finish(operationResult);
        return Result(true);
    }

    cancelling = true;
    return cancelRemaining(eventLoop);
}

void AwaitTaskGroupWaitAnyAwaiter::onTaskCompleted(void* object)
{
    static_cast<AwaitTaskGroupWaitAnyAwaiter*>(object)->onTaskCompleted();
}

void AwaitTaskGroupWaitAnyAwaiter::onTaskCompleted()
{
    if (finished)
    {
        return;
    }

    completedTasks++;
    if (winnerIndex == size_t(-1) and not cancelling)
    {
        for (size_t idx = 0; idx < group.numTasks; ++idx)
        {
            AwaitTask* task = group.tasks[idx];
            if (task != nullptr and task->isCompleted())
            {
                operationResult = setWinner(idx);
                break;
            }
        }

        if (waitAnyPolicy == AwaitTaskGroupWaitAnyPolicy::LeaveRemainingRunning)
        {
            finish(operationResult);
            return;
        }

        Result cancelResult = cancelRemaining(group.await);
        if (not cancelResult)
        {
            finish(cancelResult);
            return;
        }
    }

    if ((cancelling or waitAnyPolicy == AwaitTaskGroupWaitAnyPolicy::CancelRemaining) and
        completedTasks == group.numTasks)
    {
        finish(operationResult);
    }
}

void AwaitTaskGroupWaitAnyAwaiter::finish(Result result)
{
    if (finished)
    {
        return;
    }
    finished        = true;
    operationResult = result;
    clearChildCallbacks();
    continuation.resume();
}

void AwaitTaskGroupWaitAnyAwaiter::clearChildCallbacks()
{
    for (size_t idx = 0; idx < group.numTasks; ++idx)
    {
        AwaitTask* task = group.tasks[idx];
        if (task == nullptr or not task->isValid())
        {
            continue;
        }
        AwaitTask::Promise& promise = task->handle.promise();
        if (promise.completionObject == this)
        {
            promise.completionObject   = nullptr;
            promise.completionCallback = nullptr;
        }
    }
}

Result AwaitTaskGroupWaitAnyAwaiter::setWinner(size_t index)
{
    AwaitTask* task = group.tasks[index];
    if (task == nullptr)
    {
        return Result::Error("AwaitTaskGroup contains invalid task");
    }

    winnerIndex     = index;
    outResult.index = index;
    outResult.task  = task;
    return task->result();
}

Result AwaitTaskGroupWaitAnyAwaiter::cancelRemaining(AwaitEventLoop& eventLoop)
{
    Result cancelResult(true);
    for (size_t idx = 0; idx < group.numTasks; ++idx)
    {
        AwaitTask* task = group.tasks[idx];
        if (task != nullptr and idx != winnerIndex and task->isActive())
        {
            Result taskCancelResult = task->cancel(eventLoop);
            if (cancelResult and not taskCancelResult)
            {
                cancelResult = taskCancelResult;
            }
        }
    }
    return cancelResult;
}

AwaitProcessExitAwaiter::AwaitProcessExitAwaiter(AwaitEventLoop& await, FileDescriptor::Handle process,
                                                 AwaitProcessExitResult& outResult)
    : await(await), process(process), outResult(outResult)
{}

bool AwaitProcessExitAwaiter::await_ready() const { return false; }

bool AwaitProcessExitAwaiter::await_suspend(AwaitTask::Handle newContinuation)
{
    setupCancellableAwait(continuation, stopCallback, newContinuation, this, AwaitProcessExitAwaiter::cancel);

    outResult        = {};
    request.callback = [this](AsyncProcessExit::Result& result)
    {
        operationResult = result.get(outResult.exitStatus);
        continuation.resume();
    };

    operationResult = request.start(await.asyncEventLoop(), process);
    return operationResult;
}

Result AwaitProcessExitAwaiter::await_resume()
{
    clearCancellation(continuation, this);
    return operationResult;
}

Result AwaitProcessExitAwaiter::cancel(void* object, AwaitEventLoop& eventLoop)
{
    return static_cast<AwaitProcessExitAwaiter*>(object)->cancel(eventLoop);
}

Result AwaitProcessExitAwaiter::cancel(AwaitEventLoop& eventLoop)
{
    return cancelCancellableAwait(request, operationResult, eventLoop, stopCallback);
}

AwaitSignalAwaiter::AwaitSignalAwaiter(AwaitEventLoop& await, int signalNumber, AwaitSignalResult& outResult)
    : await(await), signalNumber(signalNumber), outResult(outResult)
{}

bool AwaitSignalAwaiter::await_ready() const { return false; }

bool AwaitSignalAwaiter::await_suspend(AwaitTask::Handle newContinuation)
{
    setupCancellableAwait(continuation, stopCallback, newContinuation, this, AwaitSignalAwaiter::cancel);

    outResult        = {};
    request.callback = [this](AsyncSignal::Result& result)
    {
        operationResult         = result.isValid();
        outResult.signalNumber  = result.completionData.signalNumber;
        outResult.deliveryCount = result.completionData.deliveryCount;
        continuation.resume();
    };

    AsyncSignalOptions options;
    options.mode    = AsyncSignalOptions::Mode::OneShot;
    operationResult = request.start(await.asyncEventLoop(), signalNumber, options);
    return operationResult;
}

Result AwaitSignalAwaiter::await_resume()
{
    clearCancellation(continuation, this);
    return operationResult;
}

Result AwaitSignalAwaiter::cancel(void* object, AwaitEventLoop& eventLoop)
{
    return static_cast<AwaitSignalAwaiter*>(object)->cancel(eventLoop);
}

Result AwaitSignalAwaiter::cancel(AwaitEventLoop& eventLoop)
{
    return cancelCancellableAwait(request, operationResult, eventLoop, stopCallback);
}

AwaitTaskSpawnAwaiter::AwaitTaskSpawnAwaiter(AwaitEventLoop& await, AwaitTask& task) : await(await), task(task) {}

bool AwaitTaskSpawnAwaiter::await_ready() const { return false; }

bool AwaitTaskSpawnAwaiter::await_suspend(AwaitTask::Handle newContinuation)
{
    continuation = newContinuation;

    if (not task.isValid())
    {
        operationResult = Result::Error("AwaitTask is invalid");
        return false;
    }

    AwaitEventLoop* taskEventLoop = task.handle.promise().eventLoop;
    if (taskEventLoop != nullptr and taskEventLoop != &await)
    {
        operationResult = AwaitWrongEventLoopResult();
        return false;
    }

    if (not task.isStarted())
    {
        operationResult = await.spawn(task);
        if (not operationResult)
        {
            return false;
        }
    }

    if (not task.isActive())
    {
        operationResult = task.result();
        return false;
    }

    AwaitTask::Promise& child = task.handle.promise();
    if (child.completionCallback != nullptr or child.continuation != nullptr)
    {
        operationResult = Result::Error("AwaitTask is already being awaited");
        return false;
    }

    child.continuation                  = continuation;
    continuation.promise().cancellation = {this, AwaitTaskSpawnAwaiter::cancel};
    return true;
}

Result AwaitTaskSpawnAwaiter::await_resume()
{
    clearCancellation(continuation, this);
    return operationResult;
}

Result AwaitTaskSpawnAwaiter::cancel(void* object, AwaitEventLoop& eventLoop)
{
    return static_cast<AwaitTaskSpawnAwaiter*>(object)->cancel(eventLoop);
}

Result AwaitTaskSpawnAwaiter::cancel(AwaitEventLoop& eventLoop)
{
    operationResult = AwaitCancelledResult();
    return task.cancel(eventLoop);
}

AwaitTaskTimeoutAwaiter::AwaitTaskTimeoutAwaiter(AwaitEventLoop& await, AwaitTask& task, TimeMs timeout,
                                                 AwaitTimeoutResult* outResult)
    : await(await), task(task), timeout(timeout), outResult(outResult)
{}

bool AwaitTaskTimeoutAwaiter::await_ready() const { return not task.isActive(); }

bool AwaitTaskTimeoutAwaiter::await_suspend(AwaitTask::Handle newContinuation)
{
    continuation = newContinuation;
    if (outResult != nullptr)
    {
        outResult->timedOut = false;
    }

    if (not task.isActive())
    {
        operationResult = task.result();
        return false;
    }

    AwaitTask::Promise& promise = task.handle.promise();
    if (promise.eventLoop != nullptr and promise.eventLoop != &await)
    {
        operationResult = AwaitWrongEventLoopResult();
        return false;
    }
    if (promise.completionCallback != nullptr or promise.continuation != nullptr)
    {
        operationResult = Result::Error("AwaitTask is already being awaited");
        return false;
    }

    promise.completionObject   = this;
    promise.completionCallback = AwaitTaskTimeoutAwaiter::onTaskCompleted;

    timeoutRequest.callback = [this](AsyncLoopTimeout::Result& result)
    {
        operationResult = result.isValid();
        if (not operationResult)
        {
            finish(operationResult);
            return;
        }

        timeoutFired = true;
        if (outResult != nullptr)
        {
            outResult->timedOut = true;
        }
        operationResult     = Result::Error("AwaitTask timed out");
        Result cancelResult = task.cancel(await);
        if (not cancelResult)
        {
            finish(cancelResult);
        }
    };

    operationResult = timeoutRequest.start(await.asyncEventLoop(), timeout);
    if (not operationResult)
    {
        promise.completionObject   = nullptr;
        promise.completionCallback = nullptr;
    }
    else
    {
        continuation.promise().cancellation = {this, AwaitTaskTimeoutAwaiter::cancel};
    }
    return operationResult;
}

Result AwaitTaskTimeoutAwaiter::await_resume()
{
    clearCancellation(continuation, this);
    return operationResult;
}

void AwaitTaskTimeoutAwaiter::onTaskCompleted(void* object)
{
    static_cast<AwaitTaskTimeoutAwaiter*>(object)->onTaskCompleted();
}

Result AwaitTaskTimeoutAwaiter::cancel(void* object, AwaitEventLoop& eventLoop)
{
    return static_cast<AwaitTaskTimeoutAwaiter*>(object)->cancel(eventLoop);
}

Result AwaitTaskTimeoutAwaiter::cancel(AwaitEventLoop& eventLoop)
{
    operationResult = AwaitCancelledResult();
    cancelling      = true;
    if (outResult != nullptr)
    {
        outResult->timedOut = false;
    }

    Result cancelResult = task.cancel(eventLoop);
    if (not cancelResult)
    {
        return cancelResult;
    }

    if (timeoutFired)
    {
        return Result(true);
    }

    stopCallback = [this](AsyncResult&)
    {
        timeoutStopped = true;
        if (childCompleted)
        {
            finish(operationResult);
        }
    };
    return timeoutRequest.stop(eventLoop.asyncEventLoop(), &stopCallback);
}

void AwaitTaskTimeoutAwaiter::onTaskCompleted()
{
    if (finished)
    {
        return;
    }

    childCompleted = true;
    if (timeoutFired)
    {
        finish(operationResult);
        return;
    }
    if (cancelling)
    {
        if (timeoutStopped)
        {
            finish(operationResult);
        }
        return;
    }

    operationResult = task.result();
    stopCallback    = [this](AsyncResult&)
    {
        timeoutStopped = true;
        finish(operationResult);
    };
    Result stopResult = timeoutRequest.stop(await.asyncEventLoop(), &stopCallback);
    if (not stopResult)
    {
        finish(stopResult);
    }
}

void AwaitTaskTimeoutAwaiter::finish(Result result)
{
    if (finished)
    {
        return;
    }
    finished        = true;
    operationResult = result;

    AwaitTask::Promise& promise = task.handle.promise();
    if (promise.completionObject == this)
    {
        promise.completionObject   = nullptr;
        promise.completionCallback = nullptr;
    }

    continuation.resume();
}

AwaitLoopWorkAwaiter::AwaitLoopWorkAwaiter(AwaitEventLoop& await, ThreadPool& threadPool, Function<Result()> work)
    : await(await), threadPool(threadPool), work(move(work))
{}

bool AwaitLoopWorkAwaiter::await_ready() const { return false; }

bool AwaitLoopWorkAwaiter::await_suspend(AwaitTask::Handle newContinuation)
{
    setupCancellableAwait(continuation, stopCallback, newContinuation, this, AwaitLoopWorkAwaiter::cancel);

    if (not work.isValid())
    {
        operationResult = Result::Error("AwaitLoopWork callback is invalid");
        return false;
    }

    request.work     = [this] { return work(); };
    request.callback = [this](AsyncLoopWork::Result& result)
    {
        operationResult = result.isValid();
        continuation.resume();
    };

    operationResult = request.setThreadPool(threadPool);
    if (not operationResult)
    {
        return false;
    }
    operationResult = request.start(await.asyncEventLoop());
    return operationResult;
}

Result AwaitLoopWorkAwaiter::await_resume()
{
    clearCancellation(continuation, this);
    return operationResult;
}

Result AwaitLoopWorkAwaiter::cancel(void* object, AwaitEventLoop& eventLoop)
{
    return static_cast<AwaitLoopWorkAwaiter*>(object)->cancel(eventLoop);
}

Result AwaitLoopWorkAwaiter::cancel(AwaitEventLoop& eventLoop)
{
    return cancelCancellableAwait(request, operationResult, eventLoop, stopCallback);
}

} // namespace SC
#endif
