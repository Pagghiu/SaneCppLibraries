// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "Fibers.h"

#define SC_ASSERT_PROVIDER FibersAssert
#include "../Common/Assert.inl"

#include "Internal/FiberContext.h"
#include <stdlib.h>

#if SC_PLATFORM_WINDOWS
#include <windows.h>
#else
#include <errno.h>
#include <pthread.h>
#if SC_PLATFORM_LINUX
#include <sched.h>
#endif
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace SC
{
void fiberContextEnter(FiberContextEntry entry, void* userData);
}

extern "C" void SC_fiberContextEnter(SC::FiberContextEntry entry, void* userData)
{
    SC::fiberContextEnter(entry, userData);
}

namespace SC
{
namespace
{
static void* alignDown(void* pointer, size_t alignment)
{
    const size_t address        = reinterpret_cast<size_t>(pointer);
    const size_t alignedAddress = address & ~(alignment - 1);
    return reinterpret_cast<void*>(alignedAddress);
}

static void* alignPointer(void* pointer, size_t alignment)
{
    const size_t address        = reinterpret_cast<size_t>(pointer);
    const size_t alignedAddress = (address + alignment - 1) & ~(alignment - 1);
    return reinterpret_cast<void*>(alignedAddress);
}

static size_t alignSize(size_t value, size_t alignment) { return (value + alignment - 1) & ~(alignment - 1); }

static bool isPowerOfTwo(size_t value) { return value != 0 and (value & (value - 1)) == 0; }

static size_t fiberAllocatorPageSize()
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

static size_t roundUpToFiberAllocatorPageSize(size_t size)
{
    const size_t pageSize = fiberAllocatorPageSize();
    return (size + pageSize - 1) / pageSize * pageSize;
}

struct FiberVirtualMemory
{
    void*  memory         = nullptr;
    size_t reservedBytes  = 0;
    size_t committedBytes = 0;

    ~FiberVirtualMemory() { release(); }

    static size_t getPageSize() { return fiberAllocatorPageSize(); }

    static size_t roundUpToPageSize(size_t size) { return roundUpToFiberAllocatorPageSize(size); }

    bool reserve(size_t maxCapacityInBytes)
    {
        if (memory != nullptr)
        {
            return false;
        }
        reservedBytes  = roundUpToPageSize(maxCapacityInBytes);
        committedBytes = 0;
#if SC_PLATFORM_WINDOWS
        memory = ::VirtualAlloc(nullptr, reservedBytes, MEM_RESERVE, PAGE_NOACCESS);
#else
        memory = ::mmap(nullptr, reservedBytes, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (memory == MAP_FAILED)
        {
            memory = nullptr;
        }
#endif
        return memory != nullptr;
    }

    bool commit(size_t sizeInBytes)
    {
        if (memory == nullptr or sizeInBytes > reservedBytes)
        {
            return false;
        }
        if (sizeInBytes <= committedBytes)
        {
            return true;
        }

        const size_t targetCommittedBytes = roundUpToPageSize(sizeInBytes);
        void*        commitAddress        = static_cast<char*>(memory) + committedBytes;
        const size_t bytesToCommit        = targetCommittedBytes - committedBytes;
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
        committedBytes = targetCommittedBytes;
        return true;
    }

    void release()
    {
        if (memory == nullptr)
        {
            return;
        }
#if SC_PLATFORM_WINDOWS
        const bool result = ::VirtualFree(memory, 0, MEM_RELEASE) == TRUE;
#else
        const bool result = ::munmap(memory, reservedBytes) == 0;
#endif
        SC_FIBERS_ASSERT_RELEASE(result);
        memory         = nullptr;
        reservedBytes  = 0;
        committedBytes = 0;
    }

    void* data() const { return memory; }

    size_t capacity() const { return reservedBytes; }
};

struct FiberAllocationHeader
{
    FiberAllocator* allocator          = nullptr;
    void*           rawAllocation      = nullptr;
    void*           blockHeader        = nullptr;
    size_t          requestedBytes     = 0;
    size_t          allocatedBytes     = 0;
    size_t          requestedAlignment = 0;
};

static FiberAllocationHeader& fiberAllocationHeaderFromMemory(void* memory)
{
    return *(reinterpret_cast<FiberAllocationHeader*>(memory) - 1);
}

static constexpr char FiberStackHighWaterMark = static_cast<char>(0xA5);

#if defined(__has_feature)
#if __has_feature(address_sanitizer)
#define SC_FIBERS_HAS_ADDRESS_SANITIZER 1
#endif
#endif

#if defined(__SANITIZE_ADDRESS__)
#define SC_FIBERS_HAS_ADDRESS_SANITIZER 1
#endif

#if !defined(SC_FIBERS_HAS_ADDRESS_SANITIZER)
#define SC_FIBERS_HAS_ADDRESS_SANITIZER 0
#endif

#if SC_PLATFORM_APPLE
#define SC_FIBER_CONTEXT_ENTER_SYMBOL "_SC_fiberContextEnter"
#else
#define SC_FIBER_CONTEXT_ENTER_SYMBOL "SC_fiberContextEnter"
#endif

#if SC_FIBERS_HAS_ADDRESS_SANITIZER
extern "C" void __sanitizer_start_switch_fiber(void** fakeStackSave, const void* bottom, size_t size);
extern "C" void __sanitizer_finish_switch_fiber(void* fakeStackSave, const void** bottomOld, size_t* sizeOld);

static thread_local FiberContext* fiberContextPreviousContext = nullptr;

static void fiberContextStartSwitch(FiberContext& from, FiberContext& to)
{
    fiberContextPreviousContext = &from;
    __sanitizer_start_switch_fiber(&from.asanFakeStack, to.stackBottom, to.stackSize);
}

static void fiberContextFinishSwitch(void* fakeStackSave)
{
    const void* oldStackBottom = nullptr;
    size_t      oldStackSize   = 0;
    __sanitizer_finish_switch_fiber(fakeStackSave, &oldStackBottom, &oldStackSize);
    if (fiberContextPreviousContext != nullptr)
    {
        fiberContextPreviousContext->stackBottom = oldStackBottom;
        fiberContextPreviousContext->stackSize   = oldStackSize;
        fiberContextPreviousContext              = nullptr;
    }
}
#else
static void fiberContextStartSwitch(FiberContext&, FiberContext&) {}
static void fiberContextFinishSwitch(void*) {}
#endif

static void fiberContextReturned()
{
    SC_FIBERS_ASSERT_RELEASE(false);
    Assert::unreachable();
}

static Result fiberProtectNoAccess(void* memory, size_t size)
{
    if (memory == nullptr or size == 0)
    {
        return Result(true);
    }
#if SC_PLATFORM_WINDOWS
    DWORD previousProtection = 0;
    if (::VirtualProtect(memory, size, PAGE_NOACCESS, &previousProtection) == FALSE)
    {
        return Result::Error("Failed to protect fiber stack guard page");
    }
#else
    if (::mprotect(memory, size, PROT_NONE) != 0)
    {
        return Result::Error("Failed to protect fiber stack guard page");
    }
#endif
    return Result(true);
}

static Result fiberCommitReadWrite(void* memory, size_t size)
{
    if (memory == nullptr or size == 0)
    {
        return Result(true);
    }
#if SC_PLATFORM_WINDOWS
    if (::VirtualAlloc(memory, size, MEM_COMMIT, PAGE_READWRITE) == nullptr)
    {
        return Result::Error("Failed to commit fiber stack pages");
    }
#else
    if (::mprotect(memory, size, PROT_READ | PROT_WRITE) != 0)
    {
        return Result::Error("Failed to commit fiber stack pages");
    }
#endif
    return Result(true);
}

static Result fiberDecommitNoAccess(void* memory, size_t size)
{
    if (memory == nullptr or size == 0)
    {
        return Result(true);
    }
#if SC_PLATFORM_WINDOWS
    if (::VirtualFree(memory, size, MEM_DECOMMIT) == FALSE)
    {
        return Result::Error("Failed to decommit fiber stack pages");
    }
#else
    if (::mprotect(memory, size, PROT_NONE) != 0)
    {
        return Result::Error("Failed to decommit fiber stack pages");
    }
    if (::madvise(memory, size, MADV_DONTNEED) != 0)
    {
        return Result::Error("Failed to release fiber stack pages");
    }
#endif
    return Result(true);
}

} // namespace

struct FiberVirtualStackInternal
{
    FiberVirtualMemory virtualMemory;
    Span<char>         stackMemory;
    size_t             guardBytes  = 0;
    size_t             activeTasks = 0;

    ~FiberVirtualStackInternal() { release(); }

    Result reserve(const FiberVirtualStackOptions& options)
    {
        if (virtualMemory.data() != nullptr)
        {
            return Result::Error("FiberVirtualStack is already reserved");
        }
        if (options.usableSizeInBytes < FiberStackMinimumSize)
        {
            return Result::Error("FiberVirtualStack usable size is too small");
        }

        const size_t usableBytes = FiberVirtualMemory::roundUpToPageSize(options.usableSizeInBytes);
        const size_t guardSize   = options.guardPage ? FiberVirtualMemory::getPageSize() : 0;
        const size_t totalBytes  = usableBytes + guardSize;

        if (not virtualMemory.reserve(totalBytes))
        {
            return Result::Error("Failed to reserve FiberVirtualStack memory");
        }
        if (not virtualMemory.commit(totalBytes))
        {
            virtualMemory.release();
            return Result::Error("Failed to commit FiberVirtualStack memory");
        }
        if (guardSize > 0)
        {
            Result protectResult = fiberProtectNoAccess(virtualMemory.data(), guardSize);
            if (not protectResult)
            {
                virtualMemory.release();
                return protectResult;
            }
        }

        guardBytes  = guardSize;
        stackMemory = {static_cast<char*>(virtualMemory.data()) + guardSize, usableBytes};
        return Result(true);
    }

    void release()
    {
        SC_FIBERS_ASSERT_RELEASE(activeTasks == 0);
        stackMemory = {};
        guardBytes  = 0;
        virtualMemory.release();
    }

    void acquireStack() { activeTasks += 1; }

    void releaseStack()
    {
        SC_FIBERS_ASSERT_RELEASE(activeTasks > 0);
        activeTasks -= 1;
    }

    bool isReserved() const { return virtualMemory.data() != nullptr; }
};

static_assert(sizeof(FiberVirtualStackInternal) <= FiberVirtualStackDefinition::Default,
              "Increase FiberVirtualStackDefinition opaque storage size");

static size_t fiberSchedulerLock(volatile int32_t& lockValue)
{
    size_t spinRetries = 0;
#if SC_PLATFORM_WINDOWS
    while (InterlockedCompareExchange(reinterpret_cast<volatile long*>(&lockValue), 1, 0) != 0)
    {
        spinRetries += 1;
    }
#else
    int32_t expected = 0;
    while (not __atomic_compare_exchange_n(&lockValue, &expected, 1, false, __ATOMIC_ACQUIRE, __ATOMIC_RELAXED))
    {
        spinRetries += 1;
        expected = 0;
    }
#endif
    return spinRetries;
}

static void fiberSchedulerUnlock(volatile int32_t& lockValue)
{
#if SC_PLATFORM_WINDOWS
    InterlockedExchange(reinterpret_cast<volatile long*>(&lockValue), 0);
#else
    __atomic_store_n(&lockValue, 0, __ATOMIC_RELEASE);
#endif
}

static FiberTaskStatus fiberTaskStatusLoad(const volatile int32_t& status)
{
#if SC_PLATFORM_WINDOWS
    return static_cast<FiberTaskStatus>(
        InterlockedCompareExchange(reinterpret_cast<volatile long*>(const_cast<volatile int32_t*>(&status)), 0, 0));
#else
    return static_cast<FiberTaskStatus>(__atomic_load_n(&status, __ATOMIC_ACQUIRE));
#endif
}

static void fiberTaskStatusStore(volatile int32_t& status, FiberTaskStatus desired)
{
#if SC_PLATFORM_WINDOWS
    InterlockedExchange(reinterpret_cast<volatile long*>(&status), static_cast<long>(desired));
#else
    __atomic_store_n(&status, static_cast<int32_t>(desired), __ATOMIC_RELEASE);
#endif
}

struct FiberAvailabilityQueue
{
    using Predicate = bool (*)(const void* owner);

    struct WaitNode
    {
        FiberCounter    counter;
        FiberScheduler* scheduler = nullptr;
        WaitNode*       next      = nullptr;
        bool            notified  = false;
    };

    WaitNode*                head = nullptr;
    WaitNode*                tail = nullptr;
    mutable volatile int32_t lock = 0;

    ~FiberAvailabilityQueue()
    {
        SC_FIBERS_ASSERT_RELEASE(head == nullptr);
        SC_FIBERS_ASSERT_RELEASE(tail == nullptr);
    }

    Result wait(FiberScheduler& scheduler, Predicate predicate, const void* owner)
    {
        WaitNode node;
        node.scheduler = &scheduler;
        scheduler.add(node.counter);

        fiberSchedulerLock(lock);
        if (predicate(owner))
        {
            fiberSchedulerUnlock(lock);
            SC_FIBERS_TRUST_RESULT(scheduler.done(node.counter));
            return Result(true);
        }
        pushUnlocked(node);
        fiberSchedulerUnlock(lock);

        Result waitResult = scheduler.wait(node.counter);
        fiberSchedulerLock(lock);
        if (not waitResult and not node.notified)
        {
            SC_FIBERS_ASSERT_RELEASE(removeUnlocked(node));
            fiberSchedulerUnlock(lock);
            SC_FIBERS_TRUST_RESULT(scheduler.done(node.counter));
            return waitResult;
        }
        fiberSchedulerUnlock(lock);

        if (not waitResult)
        {
            SC_TRY(notifyOneIfAvailable(predicate, owner));
        }
        return waitResult;
    }

    Result notifyOneIfAvailable(Predicate predicate, const void* owner)
    {
        fiberSchedulerLock(lock);
        WaitNode* node = predicate(owner) ? popUnlocked() : nullptr;
        if (node != nullptr)
        {
            node->notified = true;
        }
        fiberSchedulerUnlock(lock);
        return node != nullptr ? node->scheduler->done(node->counter) : Result(true);
    }

    void pushUnlocked(WaitNode& node)
    {
        node.next = nullptr;
        if (tail == nullptr)
        {
            head = &node;
            tail = &node;
        }
        else
        {
            tail->next = &node;
            tail       = &node;
        }
    }

    WaitNode* popUnlocked()
    {
        WaitNode* node = head;
        if (node == nullptr)
        {
            return nullptr;
        }
        head       = node->next;
        node->next = nullptr;
        if (head == nullptr)
        {
            tail = nullptr;
        }
        return node;
    }

    bool removeUnlocked(WaitNode& node)
    {
        WaitNode* previous = nullptr;
        WaitNode* current  = head;
        while (current != nullptr)
        {
            WaitNode* next = current->next;
            if (current == &node)
            {
                if (previous == nullptr)
                {
                    head = next;
                }
                else
                {
                    previous->next = next;
                }
                if (tail == &node)
                {
                    tail = previous;
                }
                node.next = nullptr;
                return true;
            }
            previous = current;
            current  = next;
        }
        return false;
    }

    bool hasWaitersUnlocked() const { return head != nullptr; }
};

struct FiberStackClassInternal
{
    FiberVirtualMemory virtualMemory;
    uint8_t*           stackStates   = nullptr;
    size_t*            freeStackNext = nullptr;

    FiberAvailabilityQueue availabilityQueue;
    FiberTaskPool*         boundPool = nullptr;

    size_t metadataBytes       = 0;
    size_t stackBytes          = 0;
    size_t guardBytes          = 0;
    size_t slotStrideBytes     = 0;
    size_t maxStacks           = 0;
    size_t nextFreeStack       = 0;
    size_t activeStacks        = 0;
    size_t peakActiveStacks    = 0;
    size_t committedStackBytes = 0;
    size_t peakCommittedBytes  = 0;
    size_t peakHighWaterBytes  = 0;
    bool   highWaterAvailable  = false;

    ~FiberStackClassInternal() { release(); }

    Result reserve(const FiberStackClassOptions& options)
    {
        if (isReserved())
        {
            return Result::Error("FiberStackClass is already reserved");
        }
        if (options.stackSizeInBytes < FiberStackMinimumSize)
        {
            return Result::Error("FiberStackClass stack size is too small");
        }
        if (options.maxStacks == 0)
        {
            return Result::Error("FiberStackClass max stacks is zero");
        }

        const size_t freeStackOffset = alignSize(options.maxStacks, alignof(size_t));
        metadataBytes   = FiberVirtualMemory::roundUpToPageSize(freeStackOffset + options.maxStacks * sizeof(size_t));
        stackBytes      = FiberVirtualMemory::roundUpToPageSize(options.stackSizeInBytes);
        guardBytes      = options.guardPage ? FiberVirtualMemory::getPageSize() : 0;
        slotStrideBytes = guardBytes + stackBytes;
        maxStacks       = options.maxStacks;

        const size_t totalBytes = metadataBytes + slotStrideBytes * maxStacks;
        if (not virtualMemory.reserve(totalBytes))
        {
            resetMetadata();
            return Result::Error("Failed to reserve FiberStackClass memory");
        }
        if (not virtualMemory.commit(metadataBytes))
        {
            virtualMemory.release();
            resetMetadata();
            return Result::Error("Failed to commit FiberStackClass metadata");
        }
        peakCommittedBytes = metadataBytes;

        stackStates   = static_cast<uint8_t*>(virtualMemory.data());
        freeStackNext = reinterpret_cast<size_t*>(static_cast<char*>(virtualMemory.data()) + freeStackOffset);
        nextFreeStack = 0;
        for (size_t idx = 0; idx < maxStacks; ++idx)
        {
            stackStates[idx]   = 0;
            freeStackNext[idx] = idx + 1 < maxStacks ? idx + 1 : maxStacks;
        }
        return Result(true);
    }

    Result acquire(FiberStack& outStack)
    {
        fiberSchedulerLock(availabilityQueue.lock);
        if (not isReserved())
        {
            fiberSchedulerUnlock(availabilityQueue.lock);
            return Result::Error("FiberStackClass is not reserved");
        }
        if (nextFreeStack == maxStacks)
        {
            fiberSchedulerUnlock(availabilityQueue.lock);
            return Result::Error("FiberStackClass has no available stack");
        }

        const size_t index  = nextFreeStack;
        nextFreeStack       = freeStackNext[index];
        Result commitResult = fiberCommitReadWrite(slotMemory(index).data(), stackBytes);
        if (not commitResult)
        {
            freeStackNext[index] = nextFreeStack;
            nextFreeStack        = index;
            fiberSchedulerUnlock(availabilityQueue.lock);
            return commitResult;
        }
        stackStates[index] = 1;
        activeStacks += 1;
        committedStackBytes += stackBytes;
        if (activeStacks > peakActiveStacks)
        {
            peakActiveStacks = activeStacks;
        }
        updatePeakCommittedBytes();
        outStack = FiberStack(slotMemory(index));
        if (highWaterAvailable)
        {
            outStack.fillHighWaterMark();
        }
        fiberSchedulerUnlock(availabilityQueue.lock);
        return Result(true);
    }

    Result releaseStack(FiberStack& stack)
    {
        fiberSchedulerLock(availabilityQueue.lock);
        const size_t index = indexOf(stack);
        if (index >= maxStacks)
        {
            fiberSchedulerUnlock(availabilityQueue.lock);
            return Result::Error("FiberStackClass does not own stack");
        }
        if (stackStates[index] == 0)
        {
            fiberSchedulerUnlock(availabilityQueue.lock);
            return Result::Error("FiberStackClass stack is not active");
        }
        stackStates[index] = 0;
        if (highWaterAvailable)
        {
            updatePeakHighWater(FiberStack(slotMemory(index)).highWaterUsedBytes());
        }
        Result decommitResult = fiberDecommitNoAccess(slotMemory(index).data(), stackBytes);
        if (not decommitResult)
        {
            stackStates[index] = 1;
            fiberSchedulerUnlock(availabilityQueue.lock);
            return decommitResult;
        }
        SC_FIBERS_ASSERT_RELEASE(activeStacks > 0);
        activeStacks -= 1;
        SC_FIBERS_ASSERT_RELEASE(committedStackBytes >= stackBytes);
        committedStackBytes -= stackBytes;
        freeStackNext[index] = nextFreeStack;
        nextFreeStack        = index;
        stack                = FiberStack({});
        fiberSchedulerUnlock(availabilityQueue.lock);
        return availabilityQueue.notifyOneIfAvailable(hasAvailableSlot, this);
    }

    Result waitForAvailableSlot(FiberScheduler& scheduler)
    {
        if (not isReserved())
        {
            return Result::Error("FiberStackClass is not reserved");
        }
        return availabilityQueue.wait(scheduler, hasAvailableSlot, this);
    }

    Result bind(FiberTaskPool& pool)
    {
        fiberSchedulerLock(availabilityQueue.lock);
        if (not isReserved() or activeStacks != 0 or boundPool != nullptr)
        {
            fiberSchedulerUnlock(availabilityQueue.lock);
            return Result::Error("FiberStackClass cannot be bound");
        }
        boundPool = &pool;
        fiberSchedulerUnlock(availabilityQueue.lock);
        return Result(true);
    }

    Result unbind(FiberTaskPool& pool)
    {
        fiberSchedulerLock(availabilityQueue.lock);
        if (boundPool != &pool or activeStacks != 0 or availabilityQueue.hasWaitersUnlocked())
        {
            fiberSchedulerUnlock(availabilityQueue.lock);
            return Result::Error("FiberStackClass cannot be unbound");
        }
        boundPool = nullptr;
        fiberSchedulerUnlock(availabilityQueue.lock);
        return Result(true);
    }

    void release()
    {
        fiberSchedulerLock(availabilityQueue.lock);
        SC_FIBERS_ASSERT_RELEASE(activeStacks == 0);
        SC_FIBERS_ASSERT_RELEASE(not availabilityQueue.hasWaitersUnlocked());
        SC_FIBERS_ASSERT_RELEASE(boundPool == nullptr);
        virtualMemory.release();
        resetMetadata();
        fiberSchedulerUnlock(availabilityQueue.lock);
    }

    void fillHighWaterMarks()
    {
        SC_FIBERS_ASSERT_RELEASE(activeStacks == 0);
        peakHighWaterBytes = 0;
        highWaterAvailable = true;
    }

    void diagnostics(FiberStackClassDiagnostics& outDiagnostics) const
    {
        fiberSchedulerLock(availabilityQueue.lock);
        outDiagnostics.capacity           = maxStacks;
        outDiagnostics.activeStacks       = activeStacks;
        outDiagnostics.peakActiveStacks   = peakActiveStacks;
        outDiagnostics.stackSizeInBytes   = stackBytes;
        outDiagnostics.guardSizeInBytes   = guardBytes;
        outDiagnostics.reservedSizeBytes  = virtualMemory.capacity();
        outDiagnostics.committedSizeBytes = metadataBytes + committedStackBytes;
        outDiagnostics.peakCommittedBytes = peakCommittedBytes;
        outDiagnostics.highWaterUsedBytes = highWaterAvailable ? highWaterUsedBytes() : 0;
        fiberSchedulerUnlock(availabilityQueue.lock);
    }

    bool isReserved() const { return virtualMemory.data() != nullptr; }

    static bool hasAvailableSlot(const void* owner)
    {
        const FiberStackClassInternal& self = *static_cast<const FiberStackClassInternal*>(owner);
        return self.isReserved() and self.activeStacks < self.maxStacks;
    }

    bool owns(const FiberStack& stack) const { return indexOf(stack) < maxStacks; }

    Span<char> slotMemory(size_t index) const { return {static_cast<char*>(slotBase(index)) + guardBytes, stackBytes}; }

    void* slotBase(size_t index) const
    {
        return static_cast<char*>(const_cast<void*>(virtualMemory.data())) + metadataBytes + slotStrideBytes * index;
    }

    size_t indexOf(const FiberStack& stack) const
    {
        if (not isReserved() or stack.memory().data() == nullptr)
        {
            return maxStacks;
        }

        const char* memoryData = static_cast<const char*>(virtualMemory.data());
        const char* stackData  = stack.memory().data();
        const char* firstSlot  = memoryData + metadataBytes;
        const char* end        = firstSlot + slotStrideBytes * maxStacks;
        if (stackData < firstSlot or stackData >= end)
        {
            return maxStacks;
        }

        const size_t offset = static_cast<size_t>(stackData - firstSlot);
        const size_t index  = offset / slotStrideBytes;
        if (index >= maxStacks)
        {
            return maxStacks;
        }
        return stackData == static_cast<const char*>(slotBase(index)) + guardBytes ? index : maxStacks;
    }

    size_t highWaterUsedBytes() const
    {
        size_t highWater = peakHighWaterBytes;
        for (size_t idx = 0; idx < maxStacks; ++idx)
        {
            if (stackStates[idx] == 0)
            {
                continue;
            }
            const size_t usedBytes = FiberStack(slotMemory(idx)).highWaterUsedBytes();
            if (usedBytes > highWater)
            {
                highWater = usedBytes;
            }
        }
        return highWater;
    }

    void updatePeakCommittedBytes()
    {
        const size_t currentCommittedBytes = metadataBytes + committedStackBytes;
        if (currentCommittedBytes > peakCommittedBytes)
        {
            peakCommittedBytes = currentCommittedBytes;
        }
    }

    void updatePeakHighWater(size_t usedBytes)
    {
        if (not highWaterAvailable)
        {
            return;
        }
        if (usedBytes > peakHighWaterBytes)
        {
            peakHighWaterBytes = usedBytes;
        }
    }

    void resetMetadata()
    {
        stackStates         = nullptr;
        freeStackNext       = nullptr;
        metadataBytes       = 0;
        stackBytes          = 0;
        guardBytes          = 0;
        slotStrideBytes     = 0;
        maxStacks           = 0;
        nextFreeStack       = 0;
        activeStacks        = 0;
        peakActiveStacks    = 0;
        committedStackBytes = 0;
        peakCommittedBytes  = 0;
        peakHighWaterBytes  = 0;
        highWaterAvailable  = false;
        boundPool           = nullptr;
    }
};

static_assert(sizeof(FiberStackClassInternal) <= FiberStackClassDefinition::Default,
              "Increase FiberStackClassDefinition opaque storage size");

template <>
void FiberVirtualStackOpaque::construct(Handle& buffer)
{
    placementNew(buffer.reinterpret_as<Object>());
}

template <>
void FiberVirtualStackOpaque::destruct(Object& obj)
{
    obj.~Object();
}

template <>
void FiberStackClassOpaque::construct(Handle& buffer)
{
    placementNew(buffer.reinterpret_as<Object>());
}

template <>
void FiberStackClassOpaque::destruct(Object& obj)
{
    obj.~Object();
}

void fiberContextEnter(FiberContextEntry entry, void* userData)
{
    fiberContextFinishSwitch(nullptr);
    entry(userData);
    fiberContextReturned();
}

namespace
{
#if SC_PLATFORM_WINDOWS
static void fiberContextTrampoline(FiberContextEntry entry, void* userData) { SC_fiberContextEnter(entry, userData); }
#elif SC_PLATFORM_INTEL && SC_PLATFORM_64_BIT
extern "C" void SC_fiberContextTrampoline();
extern "C" void SC_fiberContextSwitch(FiberContextPlatform* from, FiberContextPlatform* to);

__attribute__((naked)) void SC_fiberContextTrampoline()
{
    __asm__ __volatile__("movq %r12, %rdi\n"
                         "movq %r13, %rsi\n"
                         "subq $8, %rsp\n"
                         "call " SC_FIBER_CONTEXT_ENTER_SYMBOL "\n"
                         "ud2\n");
}

__attribute__((naked)) void SC_fiberContextSwitch(FiberContextPlatform* from, FiberContextPlatform* to)
{
    __asm__ __volatile__("movq %rsp, 0(%rdi)\n"
                         "movq %rbp, 8(%rdi)\n"
                         "movq %rbx, 16(%rdi)\n"
                         "movq %r12, 24(%rdi)\n"
                         "movq %r13, 32(%rdi)\n"
                         "movq %r14, 40(%rdi)\n"
                         "movq %r15, 48(%rdi)\n"
                         "movq 0(%rsi), %rsp\n"
                         "movq 8(%rsi), %rbp\n"
                         "movq 16(%rsi), %rbx\n"
                         "movq 24(%rsi), %r12\n"
                         "movq 32(%rsi), %r13\n"
                         "movq 40(%rsi), %r14\n"
                         "movq 48(%rsi), %r15\n"
                         "retq\n");
}
#elif SC_PLATFORM_ARM64
extern "C" void SC_fiberContextTrampoline();
extern "C" void SC_fiberContextSwitch(FiberContextPlatform* from, FiberContextPlatform* to);

__attribute__((naked)) void SC_fiberContextTrampoline()
{
    __asm__ __volatile__("mov x0, x19\n"
                         "mov x1, x20\n"
                         "bl " SC_FIBER_CONTEXT_ENTER_SYMBOL "\n"
                         "brk #0\n");
}

__attribute__((naked)) void SC_fiberContextSwitch(FiberContextPlatform* from, FiberContextPlatform* to)
{
    __asm__ __volatile__("mov x2, sp\n"
                         "str x2, [x0, #0]\n"
                         "stp x19, x20, [x0, #8]\n"
                         "stp x21, x22, [x0, #24]\n"
                         "stp x23, x24, [x0, #40]\n"
                         "stp x25, x26, [x0, #56]\n"
                         "stp x27, x28, [x0, #72]\n"
                         "stp x29, x30, [x0, #88]\n"
                         "stp d8, d9, [x0, #104]\n"
                         "stp d10, d11, [x0, #120]\n"
                         "stp d12, d13, [x0, #136]\n"
                         "stp d14, d15, [x0, #152]\n"
                         "ldr x2, [x1, #0]\n"
                         "mov sp, x2\n"
                         "ldp x19, x20, [x1, #8]\n"
                         "ldp x21, x22, [x1, #24]\n"
                         "ldp x23, x24, [x1, #40]\n"
                         "ldp x25, x26, [x1, #56]\n"
                         "ldp x27, x28, [x1, #72]\n"
                         "ldp x29, x30, [x1, #88]\n"
                         "ldp d8, d9, [x1, #104]\n"
                         "ldp d10, d11, [x1, #120]\n"
                         "ldp d12, d13, [x1, #136]\n"
                         "ldp d14, d15, [x1, #152]\n"
                         "ret\n");
}
#endif

static thread_local FiberWorker* currentFiberWorker = nullptr;

static FiberWorker* currentWorkerFor(FiberScheduler& scheduler)
{
    return currentFiberWorker != nullptr and currentFiberWorker->isActive() and
                   currentFiberWorker->scheduler() == &scheduler
               ? currentFiberWorker
               : nullptr;
}

static const FiberWorker* currentWorkerFor(const FiberScheduler& scheduler)
{
    return currentFiberWorker != nullptr and currentFiberWorker->isActive() and
                   currentFiberWorker->scheduler() == &scheduler
               ? currentFiberWorker
               : nullptr;
}

static int32_t fiberAtomicLoad(volatile int32_t& value)
{
#if SC_PLATFORM_WINDOWS
    return InterlockedCompareExchange(reinterpret_cast<volatile long*>(&value), 0, 0);
#else
    return __atomic_load_n(&value, __ATOMIC_ACQUIRE);
#endif
}

static void fiberAtomicStore(volatile int32_t& value, int32_t newValue)
{
#if SC_PLATFORM_WINDOWS
    InterlockedExchange(reinterpret_cast<volatile long*>(&value), newValue);
#else
    __atomic_store_n(&value, newValue, __ATOMIC_RELEASE);
#endif
}

static size_t fiberAtomicLoadSize(const volatile size_t& value)
{
#if SC_PLATFORM_WINDOWS
#if SC_PLATFORM_64_BIT
    return static_cast<size_t>(InterlockedCompareExchange64(
        reinterpret_cast<volatile long long*>(const_cast<volatile size_t*>(&value)), 0, 0));
#else
    return static_cast<size_t>(
        InterlockedCompareExchange(reinterpret_cast<volatile long*>(const_cast<volatile size_t*>(&value)), 0, 0));
#endif
#else
    return __atomic_load_n(&value, __ATOMIC_ACQUIRE);
#endif
}

static void fiberAtomicStoreSize(volatile size_t& value, size_t newValue)
{
#if SC_PLATFORM_WINDOWS
#if SC_PLATFORM_64_BIT
    InterlockedExchange64(reinterpret_cast<volatile long long*>(&value), static_cast<long long>(newValue));
#else
    InterlockedExchange(reinterpret_cast<volatile long*>(&value), static_cast<long>(newValue));
#endif
#else
    __atomic_store_n(&value, newValue, __ATOMIC_RELEASE);
#endif
}

static size_t fiberAtomicFetchAddSize(volatile size_t& value, size_t amount)
{
#if SC_PLATFORM_WINDOWS
#if SC_PLATFORM_64_BIT
    return static_cast<size_t>(
        InterlockedExchangeAdd64(reinterpret_cast<volatile long long*>(&value), static_cast<long long>(amount)));
#else
    return static_cast<size_t>(
        InterlockedExchangeAdd(reinterpret_cast<volatile long*>(&value), static_cast<long>(amount)));
#endif
#else
    return __atomic_fetch_add(&value, amount, __ATOMIC_ACQ_REL);
#endif
}

static size_t fiberAtomicFetchSubSize(volatile size_t& value, size_t amount)
{
#if SC_PLATFORM_WINDOWS
#if SC_PLATFORM_64_BIT
    return static_cast<size_t>(
        InterlockedExchangeAdd64(reinterpret_cast<volatile long long*>(&value), -static_cast<long long>(amount)));
#else
    return static_cast<size_t>(
        InterlockedExchangeAdd(reinterpret_cast<volatile long*>(&value), -static_cast<long>(amount)));
#endif
#else
    return __atomic_fetch_sub(&value, amount, __ATOMIC_ACQ_REL);
#endif
}

static bool fiberAtomicCompareExchangeSize(volatile size_t& value, size_t& expected, size_t desired)
{
#if SC_PLATFORM_WINDOWS
#if SC_PLATFORM_64_BIT
    const long long original =
        InterlockedCompareExchange64(reinterpret_cast<volatile long long*>(&value), static_cast<long long>(desired),
                                     static_cast<long long>(expected));
#else
    const long original = InterlockedCompareExchange(reinterpret_cast<volatile long*>(&value),
                                                     static_cast<long>(desired), static_cast<long>(expected));
#endif
    const size_t observed = static_cast<size_t>(original);
    if (observed == expected)
    {
        return true;
    }
    expected = observed;
    return false;
#else
    return __atomic_compare_exchange_n(&value, &expected, desired, false, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE);
#endif
}

static void fiberAtomicSequentialFence()
{
#if SC_PLATFORM_WINDOWS
    MemoryBarrier();
#else
    __atomic_thread_fence(__ATOMIC_SEQ_CST);
#endif
}

static void fiberCpuRelax()
{
#if SC_PLATFORM_WINDOWS
    YieldProcessor();
#elif SC_PLATFORM_ARM64
    __asm__ volatile("yield" ::: "memory");
#else
    __asm__ volatile("pause" ::: "memory");
#endif
}

} // namespace

struct FiberWorkerPoolWakeEvent
{
#if SC_PLATFORM_WINDOWS
    mutable CRITICAL_SECTION mutex;
    CONDITION_VARIABLE       condition;
    uint32_t                 generation = 0;
    uint32_t                 parked     = 0;

    FiberWorkerPoolWakeEvent()
    {
        ::InitializeCriticalSection(&mutex);
        ::InitializeConditionVariable(&condition);
    }

    ~FiberWorkerPoolWakeEvent() { ::DeleteCriticalSection(&mutex); }

    void notifyOne()
    {
        ::EnterCriticalSection(&mutex);
        generation += 1;
        ::WakeConditionVariable(&condition);
        ::LeaveCriticalSection(&mutex);
    }

    void notifyAll()
    {
        ::EnterCriticalSection(&mutex);
        generation += 1;
        ::WakeAllConditionVariable(&condition);
        ::LeaveCriticalSection(&mutex);
    }

    [[nodiscard]] bool wait(uint32_t observedGeneration)
    {
        ::EnterCriticalSection(&mutex);
        bool waited = false;
        while (generation == observedGeneration)
        {
            waited = true;
            parked += 1;
            ::SleepConditionVariableCS(&condition, &mutex, INFINITE);
            parked -= 1;
        }
        ::LeaveCriticalSection(&mutex);
        return waited;
    }
#else
    mutable pthread_mutex_t mutex;
    pthread_cond_t          condition;
    uint32_t                generation = 0;
    uint32_t                parked     = 0;

    FiberWorkerPoolWakeEvent()
    {
        ::pthread_mutex_init(&mutex, nullptr);
        ::pthread_cond_init(&condition, nullptr);
    }

    ~FiberWorkerPoolWakeEvent()
    {
        ::pthread_cond_destroy(&condition);
        ::pthread_mutex_destroy(&mutex);
    }

    void notifyOne()
    {
        ::pthread_mutex_lock(&mutex);
        generation += 1;
        ::pthread_cond_signal(&condition);
        ::pthread_mutex_unlock(&mutex);
    }

    void notifyAll()
    {
        ::pthread_mutex_lock(&mutex);
        generation += 1;
        ::pthread_cond_broadcast(&condition);
        ::pthread_mutex_unlock(&mutex);
    }

    [[nodiscard]] bool wait(uint32_t observedGeneration)
    {
        ::pthread_mutex_lock(&mutex);
        bool waited = false;
        while (generation == observedGeneration)
        {
            waited = true;
            parked += 1;
            const int res = ::pthread_cond_wait(&condition, &mutex);
            parked -= 1;
            (void)res;
        }
        ::pthread_mutex_unlock(&mutex);
        return waited;
    }
#endif

    uint32_t currentGeneration() const
    {
#if SC_PLATFORM_WINDOWS
        ::EnterCriticalSection(&mutex);
        const uint32_t current = generation;
        ::LeaveCriticalSection(&mutex);
        return current;
#else
        ::pthread_mutex_lock(&mutex);
        const uint32_t current = generation;
        ::pthread_mutex_unlock(&mutex);
        return current;
#endif
    }

    [[nodiscard]] uint32_t parkedCount() const
    {
#if SC_PLATFORM_WINDOWS
        ::EnterCriticalSection(&mutex);
        const uint32_t current = parked;
        ::LeaveCriticalSection(&mutex);
        return current;
#else
        ::pthread_mutex_lock(&mutex);
        const uint32_t current = parked;
        ::pthread_mutex_unlock(&mutex);
        return current;
#endif
    }
};

static_assert(sizeof(FiberWorkerPoolWakeEvent) <= FiberWorkerPool::WakeEventDefinition::Default,
              "Increase FiberWorkerPool::WakeEventDefinition opaque storage size");

template <>
void FiberWorkerPool::WakeEventOpaque::construct(Handle& buffer)
{
    placementNew(buffer.reinterpret_as<Object>());
}

template <>
void FiberWorkerPool::WakeEventOpaque::destruct(Object& obj)
{
    obj.~Object();
}

struct FiberScheduler::LockGuard
{
    explicit LockGuard(const FiberScheduler& fiberScheduler) : scheduler(fiberScheduler) { scheduler.lock(); }
    ~LockGuard() { scheduler.unlock(); }

    LockGuard(const LockGuard&)            = delete;
    LockGuard& operator=(const LockGuard&) = delete;

    const FiberScheduler& scheduler;
};

FiberWorker::FiberWorker() = default;

FiberWorker::~FiberWorker()
{
    SC_FIBERS_ASSERT_RELEASE(not workerActive);
    SC_FIBERS_ASSERT_RELEASE(localReadyHead == nullptr);
    SC_FIBERS_ASSERT_RELEASE(localReadyTail == nullptr);
    SC_FIBERS_ASSERT_RELEASE(localDeque == nullptr);
    SC_FIBERS_ASSERT_RELEASE(localDequeAllocator == nullptr);
    SC_FIBERS_ASSERT_RELEASE(localReadyFibers == 0);
    SC_FIBERS_ASSERT_RELEASE(localDequeCapacity == 0);
    SC_FIBERS_ASSERT_RELEASE(localDequeHead == 0);
    SC_FIBERS_ASSERT_RELEASE(fiberAtomicLoadSize(localDequeTop) == 0);
    SC_FIBERS_ASSERT_RELEASE(fiberAtomicLoadSize(localDequeBottom) == 0);
}

bool FiberWorker::isActive() const { return workerActive; }

FiberScheduler* FiberWorker::scheduler() { return workerScheduler; }

const FiberScheduler* FiberWorker::scheduler() const { return workerScheduler; }

FiberTask* FiberWorker::runningTask() { return workerTask; }

const FiberTask* FiberWorker::runningTask() const { return workerTask; }

Result FiberWorker::begin(FiberScheduler& fiberScheduler)
{
    SC_FIBERS_ASSERT_RELEASE(not workerActive);
    workerScheduler = &fiberScheduler;
    if (localSchedulingActive)
    {
        localQueueScheduler = &fiberScheduler;
    }
    workerTask   = nullptr;
    workerActive = true;
    return FiberContextOperations::captureCurrent(rootContext());
}

void FiberWorker::end()
{
    workerActive    = false;
    workerScheduler = nullptr;
    workerTask      = nullptr;
    if (not localSchedulingActive)
    {
        SC_FIBERS_ASSERT_RELEASE(localReadyHead == nullptr);
        SC_FIBERS_ASSERT_RELEASE(localReadyTail == nullptr);
        SC_FIBERS_ASSERT_RELEASE(localReadyFibers == 0);
        localDequeHead = 0;
        fiberAtomicStoreSize(localDequeTop, 0);
        fiberAtomicStoreSize(localDequeBottom, 0);
        localQueueScheduler = nullptr;
    }
}

FiberContext& FiberWorker::rootContext() { return rootContextStorage.reinterpret_as<FiberContext>(); }

struct FiberWorkerPoolThreadEntry
{
#if SC_PLATFORM_WINDOWS
    static DWORD WINAPI run(void* argument)
#else
    static void* run(void* argument)
#endif
    {
        FiberWorkerThread& thread = *static_cast<FiberWorkerThread*>(argument);
        thread.threadResult       = thread.runThreadEntry();
#if SC_PLATFORM_WINDOWS
        return 0;
#else
        return nullptr;
#endif
    }
};

FiberWorkerThread::FiberWorkerThread() = default;

FiberWorkerThread::~FiberWorkerThread() { SC_FIBERS_ASSERT_RELEASE(not started); }

bool FiberWorkerThread::wasStarted() const { return started; }

Result FiberWorkerThread::result() const { return threadResult; }

Result FiberWorkerThread::applyThreadPolicy()
{
    const FiberWorkerThreadPriority threadPriority = static_cast<FiberWorkerThreadPriority>(priority);
#if SC_PLATFORM_WINDOWS
    if (threadPriority != FiberWorkerThreadPriority::Default)
    {
        int nativePriority = THREAD_PRIORITY_NORMAL;
        switch (threadPriority)
        {
        case FiberWorkerThreadPriority::Default:
        case FiberWorkerThreadPriority::Normal: nativePriority = THREAD_PRIORITY_NORMAL; break;
        case FiberWorkerThreadPriority::Low: nativePriority = THREAD_PRIORITY_BELOW_NORMAL; break;
        case FiberWorkerThreadPriority::High: nativePriority = THREAD_PRIORITY_ABOVE_NORMAL; break;
        }
        SC_TRY_MSG(::SetThreadPriority(::GetCurrentThread(), nativePriority) != 0,
                   "FiberWorkerThread SetThreadPriority failed");
    }
    if (affinityMask != 0)
    {
        SC_TRY_MSG(::SetThreadAffinityMask(::GetCurrentThread(), static_cast<DWORD_PTR>(affinityMask)) != 0,
                   "FiberWorkerThread SetThreadAffinityMask failed");
    }
#elif SC_PLATFORM_LINUX
    if (threadPriority != FiberWorkerThreadPriority::Default and threadPriority != FiberWorkerThreadPriority::Normal)
    {
        return Result::Error("FiberWorkerThread priority is not supported on this platform");
    }
    if (affinityMask != 0)
    {
        cpu_set_t cpuSet;
        CPU_ZERO(&cpuSet);
        for (size_t bit = 0; bit < 64; ++bit)
        {
            if ((affinityMask & (uint64_t(1) << bit)) != 0)
            {
                CPU_SET(bit, &cpuSet);
            }
        }
        SC_TRY_MSG(::pthread_setaffinity_np(::pthread_self(), sizeof(cpuSet), &cpuSet) == 0,
                   "FiberWorkerThread pthread_setaffinity_np failed");
    }
#else
    if (threadPriority != FiberWorkerThreadPriority::Default and threadPriority != FiberWorkerThreadPriority::Normal)
    {
        return Result::Error("FiberWorkerThread priority is not supported on this platform");
    }
    if (affinityMask != 0)
    {
        return Result::Error("FiberWorkerThread affinity is not supported on this platform");
    }
#endif
    return Result(true);
}

Result FiberWorkerThread::startThread()
{
    SC_FIBERS_ASSERT_RELEASE(not started);
    threadResult = Result(true);
#if SC_PLATFORM_WINDOWS
    DWORD   threadID     = 0;
    HANDLE& threadHandle = threadStorage.reinterpret_as<HANDLE>();
    threadHandle = ::CreateThread(0, 512 * 1024, FiberWorkerPoolThreadEntry::run, this, CREATE_SUSPENDED, &threadID);
    if (threadHandle == nullptr)
    {
        return Result::Error("FiberWorkerThread CreateThread failed");
    }
    started = true;
    ::ResumeThread(threadHandle);
#else
    pthread_t& threadHandle = threadStorage.reinterpret_as<pthread_t>();
    static_assert(sizeof(pthread_t) <= FiberWorkerThreadStorageSize, "Increase FiberWorkerThreadStorageSize");
    const int res = ::pthread_create(&threadHandle, nullptr, FiberWorkerPoolThreadEntry::run, this);
    if (res != 0)
    {
        return Result::Error("FiberWorkerThread pthread_create failed");
    }
    started = true;
#endif
    return Result(true);
}

Result FiberWorkerThread::joinThread()
{
    if (not started)
    {
        return Result(true);
    }
#if SC_PLATFORM_WINDOWS
    HANDLE& threadHandle = threadStorage.reinterpret_as<HANDLE>();
    ::WaitForSingleObject(threadHandle, INFINITE);
    ::CloseHandle(threadHandle);
    threadHandle = nullptr;
#else
    pthread_t& threadHandle = threadStorage.reinterpret_as<pthread_t>();
    const int  res          = ::pthread_join(threadHandle, nullptr);
    if (res != 0)
    {
        return Result::Error("FiberWorkerThread pthread_join failed");
    }
#endif
    started = false;
    return threadResult;
}

Result FiberWorkerThread::runThreadEntry()
{
    if (pool == nullptr)
    {
        return Result::Error("FiberWorkerThread has no pool");
    }
    SC_TRY(applyThreadPolicy());
    return pool->workerMain(workerIndex);
}

FiberWorkerPool::FiberWorkerPool() = default;

FiberWorkerPool::~FiberWorkerPool() { SC_FIBERS_ASSERT_RELEASE(not isRunning()); }

Result FiberWorkerPool::start(FiberScheduler& scheduler, Span<FiberWorker> workerStorage,
                              Span<FiberWorkerThread> threadStorage)
{
    FiberWorkerPoolOptions options;
    return start(scheduler, workerStorage, threadStorage, options);
}

Result FiberWorkerPool::start(FiberScheduler& scheduler, Span<FiberWorker> workerStorage,
                              Span<FiberWorkerThread> threadStorage, const FiberWorkerPoolOptions& options)
{
    if (isRunning())
    {
        return Result::Error("FiberWorkerPool already running");
    }
    if (workerStorage.empty() or threadStorage.empty())
    {
        return Result::Error("FiberWorkerPool storage is empty");
    }
    if (workerStorage.sizeInElements() != threadStorage.sizeInElements())
    {
        return Result::Error("FiberWorkerPool worker/thread storage size mismatch");
    }
    if ((options.dequeAllocator == nullptr) != (options.dequeCapacityPerWorker == 0))
    {
        return Result::Error("FiberWorkerPool deque allocator and capacity must be provided together");
    }
    if (not options.affinityMasks.empty() and options.affinityMasks.sizeInElements() != workerStorage.sizeInElements())
    {
        return Result::Error("FiberWorkerPool affinity mask count must match worker count");
    }
#if SC_PLATFORM_APPLE
    if (not options.affinityMasks.empty())
    {
        return Result::Error("FiberWorkerPool affinity is not supported on this platform");
    }
#endif
#if not SC_PLATFORM_WINDOWS
    if (options.threadPriority != FiberWorkerThreadPriority::Default and
        options.threadPriority != FiberWorkerThreadPriority::Normal)
    {
        return Result::Error("FiberWorkerPool priority is not supported on this platform");
    }
#endif

    bool createdDeques = false;
    if (options.dequeAllocator != nullptr)
    {
        SC_TRY(scheduler.createWorkerDeques(*options.dequeAllocator, workerStorage, options.dequeCapacityPerWorker));
        createdDeques = true;
    }

    poolScheduler      = &scheduler;
    workers            = workerStorage;
    threads            = threadStorage;
    idleSpinAttempts   = options.idleSpinAttempts;
    localDequesCreated = createdDeques;
    fiberAtomicStore(stopRequested, 0);
    fiberAtomicStore(running, 1);
    {
        FiberScheduler::LockGuard guard(scheduler);
        if (scheduler.workerPool != nullptr)
        {
            if (localDequesCreated)
            {
                scheduler.releaseWorkerDeques(workers);
                localDequesCreated = false;
            }
            poolScheduler    = nullptr;
            workers          = {};
            threads          = {};
            idleSpinAttempts = 0;
            fiberAtomicStore(running, 0);
            return Result::Error("FiberScheduler already has a running worker pool");
        }
        scheduler.workerPool = this;
    }

    for (size_t idx = 0; idx < threads.sizeInElements(); ++idx)
    {
        FiberWorker& worker = workers[idx];
        SC_FIBERS_ASSERT_RELEASE(not worker.workerActive);
        SC_FIBERS_ASSERT_RELEASE(worker.localReadyHead == nullptr);
        SC_FIBERS_ASSERT_RELEASE(worker.localReadyTail == nullptr);
        SC_FIBERS_ASSERT_RELEASE(worker.localReadyFibers == 0);
        worker.localDequeHead         = 0;
        worker.localSchedulingActive  = true;
        worker.localQueueScheduler    = &scheduler;
        worker.stealCursor            = (idx + 1) % workers.sizeInElements();
        worker.stealCursorInitialized = true;

        FiberWorkerThread& thread = threads[idx];
        SC_FIBERS_ASSERT_RELEASE(not thread.started);
        thread.pool         = this;
        thread.workerIndex  = idx;
        thread.affinityMask = options.affinityMasks.empty() ? 0 : options.affinityMasks[idx];
        thread.priority     = static_cast<uint8_t>(options.threadPriority);
        Result result       = thread.startThread();
        if (not result)
        {
            SC_FIBERS_TRUST_RESULT(requestStop());
            SC_FIBERS_TRUST_RESULT(join());
            return result;
        }
    }
    return Result(true);
}

Result FiberWorkerPool::requestStop()
{
    fiberAtomicStore(stopRequested, 1);
    wakeAllWorkers();
    return Result(true);
}

Result FiberWorkerPool::join()
{
    Result firstError = Result(true);
    bool   hasError   = false;
    for (FiberWorkerThread& thread : threads)
    {
        Result result = thread.joinThread();
        if (not result and not hasError)
        {
            firstError = result;
            hasError   = true;
        }
        thread.pool         = nullptr;
        thread.workerIndex  = 0;
        thread.affinityMask = 0;
        thread.priority     = 0;
    }

    for (FiberWorker& worker : workers)
    {
        SC_FIBERS_ASSERT_RELEASE(worker.localReadyHead == nullptr);
        SC_FIBERS_ASSERT_RELEASE(worker.localReadyTail == nullptr);
        SC_FIBERS_ASSERT_RELEASE(worker.localReadyFibers == 0);
        worker.localDequeHead        = 0;
        worker.localSchedulingActive = false;
        worker.localQueueScheduler   = nullptr;
    }

    if (localDequesCreated and poolScheduler != nullptr)
    {
        poolScheduler->releaseWorkerDeques(workers);
        localDequesCreated = false;
    }

    if (poolScheduler != nullptr)
    {
        FiberScheduler::LockGuard guard(*poolScheduler);
        if (poolScheduler->workerPool == this)
        {
            poolScheduler->workerPool = nullptr;
        }
    }

    workers          = {};
    threads          = {};
    poolScheduler    = nullptr;
    idleSpinAttempts = 0;
    fiberAtomicStore(stopRequested, 0);
    fiberAtomicStore(running, 0);
    return hasError ? firstError : Result(true);
}

Result FiberWorkerPool::shutdown()
{
    SC_TRY(requestStop());
    return join();
}

bool FiberWorkerPool::isRunning() const { return fiberAtomicLoad(running) != 0; }

size_t FiberWorkerPool::workerCount() const { return workers.sizeInElements(); }

size_t FiberWorkerPool::parkedWorkerCount() const { return wakeEvent.get().parkedCount(); }

void FiberWorkerPool::wakeOneWorker() { wakeEvent.get().notifyOne(); }

void FiberWorkerPool::wakeAllWorkers() { wakeEvent.get().notifyAll(); }

bool FiberWorkerPool::waitForWork(uint32_t observedGeneration) { return wakeEvent.get().wait(observedGeneration); }

uint32_t FiberWorkerPool::wakeGeneration() const { return wakeEvent.get().currentGeneration(); }

Result FiberWorkerPool::workerMain(size_t workerIndex)
{
    if (poolScheduler == nullptr or workerIndex >= workers.sizeInElements())
    {
        return Result::Error("FiberWorkerPool worker index is invalid");
    }

    FiberScheduler& scheduler            = *poolScheduler;
    FiberWorker&    worker               = workers[workerIndex];
    bool            cancelRequested      = false;
    size_t          consecutiveIdleSpins = 0;
    while (true)
    {
        if (fiberAtomicLoad(stopRequested) != 0 and not cancelRequested)
        {
            SC_TRY(scheduler.requestCancelAll());
            cancelRequested = true;
        }

        if (fiberAtomicLoadSize(scheduler.readyFibers) != 0)
        {
            consecutiveIdleSpins = 0;
            SC_TRY(scheduler.runNoWait(worker, workers));
        }
        else if (consecutiveIdleSpins < idleSpinAttempts)
        {
            consecutiveIdleSpins += 1;
            fiberAtomicFetchAddSize(worker.idleSpinIterations, 1);
            fiberCpuRelax();
        }
        else
        {
            consecutiveIdleSpins              = 0;
            const uint32_t observedGeneration = wakeGeneration();
            if (fiberAtomicLoad(stopRequested) == 0 and scheduler.hasActiveFibers() and
                fiberAtomicLoadSize(scheduler.readyFibers) == 0)
            {
                {
                    FiberScheduler::LockGuard guard(scheduler);
                    worker.parkAttempts += 1;
                }
                if (waitForWork(observedGeneration))
                {
                    FiberScheduler::LockGuard guard(scheduler);
                    worker.parkedWakeups += 1;
                }
            }
            else if (not scheduler.hasActiveFibers())
            {
                break;
            }
        }
    }
    return Result(true);
}

FiberStack::FiberStack(Span<char> memory) : stackMemory(memory) {}

FiberStack::FiberStack(Span<char> memory, void* owner) : stackMemory(memory), stackOwner(owner) {}

Span<char> FiberStack::memory() const { return stackMemory; }

size_t FiberStack::sizeInBytes() const { return stackMemory.sizeInBytes(); }

size_t FiberStack::usableSizeInBytes() const
{
    if (stackMemory.data() == nullptr)
    {
        return 0;
    }
    const size_t waste = alignmentWasteInBytes();
    return waste <= stackMemory.sizeInBytes() ? stackMemory.sizeInBytes() - waste : 0;
}

size_t FiberStack::alignmentWasteInBytes() const
{
    if (stackMemory.data() == nullptr)
    {
        return 0;
    }
    const char* stackTop        = stackMemory.data() + stackMemory.sizeInBytes();
    const char* alignedStackTop = static_cast<const char*>(alignDown(const_cast<char*>(stackTop), FiberStackAlignment));
    return static_cast<size_t>(stackTop - alignedStackTop);
}

bool FiberStack::isUsable() const { return usableSizeInBytes() >= FiberStackMinimumSize; }

void FiberStack::fillHighWaterMark()
{
    char*        data       = stackMemory.data();
    const size_t usableSize = usableSizeInBytes();
    for (size_t idx = 0; idx < usableSize; ++idx)
    {
        data[idx] = FiberStackHighWaterMark;
    }
}

size_t FiberStack::highWaterUsedBytes() const
{
    const char*  data       = stackMemory.data();
    const size_t usableSize = usableSizeInBytes();
    if (data == nullptr or usableSize == 0)
    {
        return 0;
    }

    size_t unusedBytes = 0;
    while (unusedBytes < usableSize and data[unusedBytes] == FiberStackHighWaterMark)
    {
        unusedBytes += 1;
    }
    return usableSize - unusedBytes;
}

size_t FiberStack::highWaterUnusedBytes() const
{
    const size_t usableSize = usableSizeInBytes();
    const size_t usedSize   = highWaterUsedBytes();
    return usedSize <= usableSize ? usableSize - usedSize : 0;
}

FiberVirtualStack::FiberVirtualStack() = default;

FiberVirtualStack::~FiberVirtualStack() = default;

Result FiberVirtualStack::reserve(const FiberVirtualStackOptions& options) { return internal.get().reserve(options); }

void FiberVirtualStack::release() { internal.get().release(); }

FiberStack FiberVirtualStack::stack() const
{
    const FiberVirtualStackInternal& self = internal.get();
    return FiberStack(self.stackMemory, const_cast<FiberVirtualStackInternal*>(&self));
}

Span<char> FiberVirtualStack::memory() const { return internal.get().stackMemory; }

size_t FiberVirtualStack::usableSizeInBytes() const { return internal.get().stackMemory.sizeInBytes(); }

size_t FiberVirtualStack::reservedSizeInBytes() const { return internal.get().virtualMemory.capacity(); }

size_t FiberVirtualStack::guardSizeInBytes() const { return internal.get().guardBytes; }

bool FiberVirtualStack::isReserved() const { return internal.get().isReserved(); }

FiberStackClass::FiberStackClass() = default;

FiberStackClass::~FiberStackClass() = default;

Result FiberStackClass::reserve(const FiberStackClassOptions& options) { return internal.get().reserve(options); }

Result FiberStackClass::acquire(FiberStack& outStack) { return internal.get().acquire(outStack); }

Result FiberStackClass::release(FiberStack& stack) { return internal.get().releaseStack(stack); }

Result FiberStackClass::waitForAvailableSlot(FiberScheduler& scheduler)
{
    return internal.get().waitForAvailableSlot(scheduler);
}

void FiberStackClass::release() { internal.get().release(); }

void FiberStackClass::fillHighWaterMarks() { internal.get().fillHighWaterMarks(); }

void FiberStackClass::diagnostics(FiberStackClassDiagnostics& outDiagnostics) const
{
    internal.get().diagnostics(outDiagnostics);
}

bool FiberStackClass::isReserved() const { return internal.get().isReserved(); }

bool FiberStackClass::owns(const FiberStack& stack) const { return internal.get().owns(stack); }

size_t FiberStackClass::capacity() const
{
    FiberStackClassDiagnostics currentDiagnostics;
    diagnostics(currentDiagnostics);
    return currentDiagnostics.capacity;
}

size_t FiberStackClass::activeCount() const
{
    FiberStackClassDiagnostics currentDiagnostics;
    diagnostics(currentDiagnostics);
    return currentDiagnostics.activeStacks;
}

struct FiberAllocator::BlockHeader
{
    FiberAllocator* allocator  = nullptr;
    BlockHeader*    previous   = nullptr;
    BlockHeader*    next       = nullptr;
    size_t          blockBytes = 0;
    bool            free       = true;
};

FiberAllocator::~FiberAllocator()
{
    Result result = close();
    SC_FIBERS_ASSERT_RELEASE(result);
}

Result FiberAllocator::createFixed(Span<char> storage)
{
    if (isOpen())
    {
        return Result::Error("FiberAllocator is already open");
    }
    if (storage.empty())
    {
        return Result::Error("FiberAllocator fixed storage is empty");
    }

    resetState();
    SC_TRY(initializeFixedStorage(storage));
    currentMode = FiberAllocatorMode::Fixed;
    return Result(true);
}

Result FiberAllocator::createVirtual(FiberAllocatorVirtualOptions options)
{
    if (isOpen())
    {
        return Result::Error("FiberAllocator is already open");
    }
    if (options.reserveBytes == 0)
    {
        return Result::Error("FiberAllocator virtual reservation is empty");
    }

    resetState();
    virtualReservedBytes  = roundUpToFiberAllocatorPageSize(options.reserveBytes);
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
        return Result::Error("FiberAllocator virtual reservation failed");
    }

    currentMode = FiberAllocatorMode::Virtual;
    if (options.initialCommitBytes > 0 and not ensureCommitted(options.initialCommitBytes))
    {
        releaseVirtualMemory();
        resetState();
        return Result::Error("FiberAllocator virtual initial commit failed");
    }
    return Result(true);
}

Result FiberAllocator::createMalloc()
{
    if (isOpen())
    {
        return Result::Error("FiberAllocator is already open");
    }
    resetState();
    currentMode = FiberAllocatorMode::Malloc;
    return Result(true);
}

Result FiberAllocator::createPolymorphic(FiberAllocatorInterface& customAllocatorInterface)
{
    if (isOpen())
    {
        return Result::Error("FiberAllocator is already open");
    }
    resetState();
    allocatorInterface = &customAllocatorInterface;
    currentMode        = FiberAllocatorMode::Polymorphic;
    return Result(true);
}

Result FiberAllocator::validateClose() const
{
    if (not isOpen())
    {
        return Result(true);
    }
    if (currentStatistics.bytesInUse != 0 or currentStatistics.numAllocations != currentStatistics.numReleases)
    {
        return Result::Error("FiberAllocator closed with live allocations");
    }
    return Result(true);
}

Result FiberAllocator::close()
{
    Result validation = validateClose();
    if (not validation)
    {
        SC_FIBERS_ASSERT_RELEASE(false);
        return validation;
    }
    if (not isOpen())
    {
        return Result(true);
    }

    releaseVirtualMemory();
    currentMode           = FiberAllocatorMode::None;
    fixedStorage          = {};
    firstBlock            = nullptr;
    allocatorInterface    = nullptr;
    virtualMemory         = nullptr;
    virtualReservedBytes  = 0;
    virtualCommittedBytes = 0;
    return Result(true);
}

void* FiberAllocator::allocate(const void* owner, size_t numBytes, size_t alignment)
{
    if (not isOpen() or numBytes == 0 or not isPowerOfTwo(alignment))
    {
        recordAllocationFailure(numBytes);
        return nullptr;
    }

    if (currentMode == FiberAllocatorMode::Fixed or currentMode == FiberAllocatorMode::Virtual)
    {
        return allocateFromBlocks(owner, numBytes, alignment);
    }

    const size_t rawBytes = numBytes + sizeof(FiberAllocationHeader) + alignment;
    void*        raw      = nullptr;
    if (currentMode == FiberAllocatorMode::Malloc)
    {
        raw = ::malloc(rawBytes);
    }
    else if (currentMode == FiberAllocatorMode::Polymorphic)
    {
        raw = allocatorInterface->allocateImpl(owner, rawBytes, alignof(FiberAllocationHeader));
    }

    if (raw == nullptr)
    {
        recordAllocationFailure(numBytes);
        return nullptr;
    }

    void*                  memory = alignPointer(static_cast<char*>(raw) + sizeof(FiberAllocationHeader), alignment);
    FiberAllocationHeader& header = fiberAllocationHeaderFromMemory(memory);
    header.allocator              = this;
    header.rawAllocation          = raw;
    header.blockHeader            = nullptr;
    header.requestedBytes         = numBytes;
    header.allocatedBytes         = rawBytes;
    header.requestedAlignment     = alignment;

    currentStatistics.numAllocations++;
    currentStatistics.requestedBytesAllocated += numBytes;
    currentStatistics.bytesInUse += rawBytes;
    if (currentStatistics.bytesInUse > currentStatistics.peakBytesInUse)
    {
        currentStatistics.peakBytesInUse = currentStatistics.bytesInUse;
    }
    return memory;
}

void FiberAllocator::release(void* memory)
{
    if (memory == nullptr)
    {
        return;
    }

    FiberAllocationHeader& header = fiberAllocationHeaderFromMemory(memory);
    SC_FIBERS_ASSERT_RELEASE(header.allocator == this);
    if (header.allocator != this)
    {
        return;
    }

    currentStatistics.numReleases++;
    currentStatistics.requestedBytesReleased += header.requestedBytes;
    SC_FIBERS_ASSERT_RELEASE(currentStatistics.bytesInUse >= header.allocatedBytes);
    currentStatistics.bytesInUse -= header.allocatedBytes;

    if (currentMode == FiberAllocatorMode::Fixed or currentMode == FiberAllocatorMode::Virtual)
    {
        releaseBlock(*static_cast<BlockHeader*>(header.blockHeader));
    }
    else if (currentMode == FiberAllocatorMode::Malloc)
    {
        ::free(header.rawAllocation);
    }
    else if (currentMode == FiberAllocatorMode::Polymorphic)
    {
        allocatorInterface->releaseImpl(header.rawAllocation);
    }
}

void FiberAllocator::releaseFromAnyAllocator(void* memory)
{
    if (memory == nullptr)
    {
        return;
    }
    FiberAllocationHeader& header = fiberAllocationHeaderFromMemory(memory);
    SC_FIBERS_ASSERT_RELEASE(header.allocator != nullptr);
    if (header.allocator != nullptr)
    {
        header.allocator->release(memory);
    }
}

FiberAllocatorMode FiberAllocator::mode() const { return currentMode; }

FiberAllocatorStatistics FiberAllocator::statistics() const { return currentStatistics; }

bool FiberAllocator::isOpen() const { return currentMode != FiberAllocatorMode::None; }

size_t FiberAllocator::used() const { return currentStatistics.bytesInUse; }

size_t FiberAllocator::capacity() const
{
    if (currentMode == FiberAllocatorMode::Fixed)
    {
        return fixedStorage.sizeInBytes();
    }
    if (currentMode == FiberAllocatorMode::Virtual)
    {
        return virtualReservedBytes;
    }
    return 0;
}

size_t FiberAllocator::peakUsed() const { return currentStatistics.peakBytesInUse; }

size_t FiberAllocator::failedAllocationSize() const { return currentStatistics.lastFailedAllocationSize; }

size_t FiberAllocator::reservedBytes() const
{
    return currentMode == FiberAllocatorMode::Virtual ? virtualReservedBytes : 0;
}

size_t FiberAllocator::committedBytes() const
{
    return currentMode == FiberAllocatorMode::Virtual ? virtualCommittedBytes : 0;
}

Result FiberAllocator::initializeFixedStorage(Span<char> storage)
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

void* FiberAllocator::allocateFromBlocks(const void*, size_t numBytes, size_t alignment)
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
            void* memory    = alignPointer(blockStart + sizeof(BlockHeader) + sizeof(FiberAllocationHeader), alignment);
            char* headerPtr = static_cast<char*>(memory) - sizeof(FiberAllocationHeader);
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
            if (remainingBytes > sizeof(BlockHeader) + sizeof(FiberAllocationHeader) + alignof(BlockHeader))
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

            FiberAllocationHeader& header = fiberAllocationHeaderFromMemory(memory);
            header.allocator              = this;
            header.rawAllocation          = block;
            header.blockHeader            = block;
            header.requestedBytes         = numBytes;
            header.allocatedBytes         = block->blockBytes;
            header.requestedAlignment     = alignment;

            currentStatistics.numAllocations++;
            currentStatistics.requestedBytesAllocated += numBytes;
            currentStatistics.bytesInUse += block->blockBytes;
            if (currentStatistics.bytesInUse > currentStatistics.peakBytesInUse)
            {
                currentStatistics.peakBytesInUse = currentStatistics.bytesInUse;
            }
            return memory;
        }

        if (currentMode != FiberAllocatorMode::Virtual)
        {
            recordAllocationFailure(numBytes);
            return nullptr;
        }

        const size_t previousCommittedBytes = virtualCommittedBytes;
        const size_t requestedBytes =
            previousCommittedBytes + sizeof(BlockHeader) + sizeof(FiberAllocationHeader) + alignment + numBytes;
        if (not ensureCommitted(requestedBytes) or virtualCommittedBytes == previousCommittedBytes)
        {
            recordAllocationFailure(numBytes);
            return nullptr;
        }
    }
}

void FiberAllocator::releaseBlock(BlockHeader& header)
{
    SC_FIBERS_ASSERT_RELEASE(header.allocator == this);
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

bool FiberAllocator::ensureCommitted(size_t sizeInBytes)
{
    if (currentMode != FiberAllocatorMode::Virtual or virtualMemory == nullptr)
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
    const size_t targetCommittedBytes   = roundUpToFiberAllocatorPageSize(sizeInBytes);
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

void FiberAllocator::releaseVirtualMemory()
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
    SC_FIBERS_ASSERT_RELEASE(result);
    virtualMemory         = nullptr;
    virtualReservedBytes  = 0;
    virtualCommittedBytes = 0;
}

void FiberAllocator::recordAllocationFailure(size_t numBytes)
{
    currentStatistics.numAllocationFailures++;
    currentStatistics.lastFailedAllocationSize = numBytes;
    if (numBytes > currentStatistics.largestFailedAllocationSize)
    {
        currentStatistics.largestFailedAllocationSize = numBytes;
    }
}

void FiberAllocator::resetState()
{
    currentMode           = FiberAllocatorMode::None;
    currentStatistics     = {};
    fixedStorage          = {};
    firstBlock            = nullptr;
    allocatorInterface    = nullptr;
    virtualMemory         = nullptr;
    virtualReservedBytes  = 0;
    virtualCommittedBytes = 0;
}

FiberContext& FiberTask::context() { return contextStorage.reinterpret_as<FiberContext>(); }

const FiberContext& FiberTask::context() const { return contextStorage.reinterpret_as<FiberContext>(); }

FiberCancellationTokenSource::FiberCancellationTokenSource() = default;

void FiberCancellationTokenSource::requestCancel() { fiberAtomicStore(requested, 1); }

void FiberCancellationTokenSource::reset() { fiberAtomicStore(requested, 0); }

Result FiberCancellationTokenSource::check() const
{
    return isCancellationRequested() ? Result::Error("Fiber cancellation requested") : Result(true);
}

bool FiberCancellationTokenSource::isCancellationRequested() const { return fiberAtomicLoad(requested) != 0; }

FiberCancellationToken FiberCancellationTokenSource::token() const { return FiberCancellationToken(*this); }

FiberCancellationToken::FiberCancellationToken() = default;

FiberCancellationToken::FiberCancellationToken(const FiberCancellationTokenSource& tokenSource) : source(&tokenSource)
{}

Result FiberCancellationToken::check() const
{
    return isCancellationRequested() ? Result::Error("Fiber cancellation requested") : Result(true);
}

bool FiberCancellationToken::isValid() const { return source != nullptr; }

bool FiberCancellationToken::isCancellationRequested() const
{
    return source != nullptr and source->isCancellationRequested();
}

void FiberScheduler::taskEntry(void* userData)
{
    FiberTask&      task      = *static_cast<FiberTask*>(userData);
    FiberScheduler& scheduler = *task.scheduler;

    Result result = task.procedure(scheduler);
    scheduler.finishCurrentTask(task, result);

    FiberWorker* worker = static_cast<FiberWorker*>(task.runningWorker);
    SC_FIBERS_ASSERT_RELEASE(worker != nullptr);
    SC_FIBERS_ASSERT_RELEASE(worker->scheduler() == &scheduler);
    FiberContextOperations::switchTo(task.context(), worker->rootContext());

    SC_FIBERS_ASSERT_RELEASE(false);
    Assert::unreachable();
}

FiberTask::FiberTask() = default;

FiberTask::~FiberTask() { SC_FIBERS_ASSERT_RELEASE(not isActive()); }

bool FiberTask::isValid() const { return status() != FiberTaskStatus::Invalid; }

bool FiberTask::isStarted() const
{
    const FiberTaskStatus currentStatus = status();
    return currentStatus != FiberTaskStatus::Invalid and currentStatus != FiberTaskStatus::Ready;
}

bool FiberTask::isCompleted() const
{
    const FiberTaskStatus currentStatus = status();
    return currentStatus == FiberTaskStatus::Completing or currentStatus == FiberTaskStatus::Completed;
}

bool FiberTask::isActive() const
{
    const FiberTaskStatus currentStatus = status();
    return currentStatus == FiberTaskStatus::Ready or currentStatus == FiberTaskStatus::Running or
           currentStatus == FiberTaskStatus::Waiting or currentStatus == FiberTaskStatus::Completing;
}

bool FiberTask::isCancellationRequested() const
{
    return cancelRequested or cancellationToken.isCancellationRequested();
}

FiberTaskStatus FiberTask::status() const { return fiberTaskStatusLoad(taskStatus); }

Result FiberTask::result() const { return taskResult; }

void FiberTask::setUserData(void* data) { taskUserData = data; }

void* FiberTask::userData() const { return taskUserData; }

struct FiberTaskClassInternal
{
    FiberAllocator* allocator  = nullptr;
    FiberTask*      tasks      = nullptr;
    uint8_t*        taskStates = nullptr;

    FiberAvailabilityQueue availabilityQueue;
    FiberTaskPool*         boundPool = nullptr;

    size_t maxTasks        = 0;
    size_t activeTasks     = 0;
    size_t peakActiveTasks = 0;
    size_t nextTask        = 0;

    ~FiberTaskClassInternal()
    {
        Result result = close();
        SC_FIBERS_ASSERT_RELEASE(result);
    }

    Result create(FiberAllocator& taskAllocator, const FiberTaskClassOptions& options)
    {
        if (isOpen())
        {
            return Result::Error("FiberTaskClass is already open");
        }
        if (not taskAllocator.isOpen())
        {
            return Result::Error("FiberTaskClass allocator is not open");
        }
        if (options.maxTasks == 0)
        {
            return Result::Error("FiberTaskClass max tasks is zero");
        }
        if (options.maxTasks > static_cast<size_t>(-1) / sizeof(FiberTask))
        {
            return Result::Error("FiberTaskClass task storage size overflow");
        }

        uint8_t* states = static_cast<uint8_t*>(taskAllocator.allocate(this, options.maxTasks, alignof(uint8_t)));
        if (states == nullptr)
        {
            return Result::Error("FiberTaskClass state allocation failed");
        }

        FiberTask* taskStorage = static_cast<FiberTask*>(
            taskAllocator.allocate(this, options.maxTasks * sizeof(FiberTask), alignof(FiberTask)));
        if (taskStorage == nullptr)
        {
            taskAllocator.release(states);
            return Result::Error("FiberTaskClass task allocation failed");
        }

        allocator       = &taskAllocator;
        tasks           = taskStorage;
        taskStates      = states;
        maxTasks        = options.maxTasks;
        activeTasks     = 0;
        peakActiveTasks = 0;
        nextTask        = 0;
        for (size_t idx = 0; idx < maxTasks; ++idx)
        {
            taskStates[idx] = 0;
            placementNew(tasks[idx]);
        }
        return Result(true);
    }

    Result acquire(FiberTask*& outTask)
    {
        outTask = nullptr;
        fiberSchedulerLock(availabilityQueue.lock);
        if (not isOpen())
        {
            fiberSchedulerUnlock(availabilityQueue.lock);
            return Result::Error("FiberTaskClass is not open");
        }
        for (size_t offset = 0; offset < maxTasks; ++offset)
        {
            const size_t index = (nextTask + offset) % maxTasks;
            if (taskStates[index] != 0)
            {
                continue;
            }

            taskStates[index] = 1;
            activeTasks += 1;
            if (activeTasks > peakActiveTasks)
            {
                peakActiveTasks = activeTasks;
            }
            nextTask = (index + 1) % maxTasks;
            outTask  = &tasks[index];
            fiberSchedulerUnlock(availabilityQueue.lock);
            return Result(true);
        }
        fiberSchedulerUnlock(availabilityQueue.lock);
        return Result::Error("FiberTaskClass has no available task");
    }

    Result releaseTask(FiberTask& task)
    {
        fiberSchedulerLock(availabilityQueue.lock);
        const size_t index = indexOf(task);
        if (index >= maxTasks)
        {
            fiberSchedulerUnlock(availabilityQueue.lock);
            return Result::Error("FiberTaskClass does not own task");
        }
        if (taskStates[index] == 0)
        {
            fiberSchedulerUnlock(availabilityQueue.lock);
            return Result::Error("FiberTaskClass task is not active");
        }
        if (task.isActive())
        {
            fiberSchedulerUnlock(availabilityQueue.lock);
            return Result::Error("FiberTaskClass cannot release active task");
        }

        taskStates[index] = 0;
        SC_FIBERS_ASSERT_RELEASE(activeTasks > 0);
        activeTasks -= 1;
        nextTask = index;
        fiberSchedulerUnlock(availabilityQueue.lock);
        return availabilityQueue.notifyOneIfAvailable(hasAvailableSlot, this);
    }

    Result waitForAvailableSlot(FiberScheduler& scheduler)
    {
        if (not isOpen())
        {
            return Result::Error("FiberTaskClass is not open");
        }
        return availabilityQueue.wait(scheduler, hasAvailableSlot, this);
    }

    Result bind(FiberTaskPool& pool)
    {
        fiberSchedulerLock(availabilityQueue.lock);
        if (not isOpen() or activeTasks != 0 or boundPool != nullptr)
        {
            fiberSchedulerUnlock(availabilityQueue.lock);
            return Result::Error("FiberTaskClass cannot be bound");
        }
        boundPool = &pool;
        fiberSchedulerUnlock(availabilityQueue.lock);
        return Result(true);
    }

    Result unbind(FiberTaskPool& pool)
    {
        fiberSchedulerLock(availabilityQueue.lock);
        if (boundPool != &pool or activeTasks != 0 or availabilityQueue.hasWaitersUnlocked())
        {
            fiberSchedulerUnlock(availabilityQueue.lock);
            return Result::Error("FiberTaskClass cannot be unbound");
        }
        boundPool = nullptr;
        fiberSchedulerUnlock(availabilityQueue.lock);
        return Result(true);
    }

    Result validateClose() const
    {
        fiberSchedulerLock(availabilityQueue.lock);
        const bool canClose = activeTasks == 0 and not availabilityQueue.hasWaitersUnlocked() and boundPool == nullptr;
        fiberSchedulerUnlock(availabilityQueue.lock);
        return canClose ? Result(true) : Result::Error("FiberTaskClass closed with active tasks or waiters");
    }

    Result close()
    {
        SC_TRY(validateClose());
        if (not isOpen())
        {
            return Result(true);
        }

        for (size_t idx = 0; idx < maxTasks; ++idx)
        {
            tasks[idx].~FiberTask();
        }
        allocator->release(tasks);
        allocator->release(taskStates);
        reset();
        return Result(true);
    }

    void diagnostics(FiberTaskClassDiagnostics& outDiagnostics) const
    {
        fiberSchedulerLock(availabilityQueue.lock);
        outDiagnostics.capacity        = maxTasks;
        outDiagnostics.activeTasks     = activeTasks;
        outDiagnostics.availableTasks  = maxTasks - activeTasks;
        outDiagnostics.peakActiveTasks = peakActiveTasks;
        fiberSchedulerUnlock(availabilityQueue.lock);
    }

    bool isOpen() const { return allocator != nullptr; }

    static bool hasAvailableSlot(const void* owner)
    {
        const FiberTaskClassInternal& self = *static_cast<const FiberTaskClassInternal*>(owner);
        return self.isOpen() and self.activeTasks < self.maxTasks;
    }

    bool owns(const FiberTask& task) const { return indexOf(task) < maxTasks; }

    size_t indexOf(const FiberTask& task) const
    {
        if (not isOpen())
        {
            return maxTasks;
        }
        const size_t taskAddress  = reinterpret_cast<size_t>(&task);
        const size_t firstAddress = reinterpret_cast<size_t>(tasks);
        const size_t endAddress   = firstAddress + maxTasks * sizeof(FiberTask);
        if (taskAddress < firstAddress or taskAddress >= endAddress)
        {
            return maxTasks;
        }
        const size_t offset = taskAddress - firstAddress;
        return offset % sizeof(FiberTask) == 0 ? offset / sizeof(FiberTask) : maxTasks;
    }

    void reset()
    {
        allocator       = nullptr;
        tasks           = nullptr;
        taskStates      = nullptr;
        maxTasks        = 0;
        activeTasks     = 0;
        peakActiveTasks = 0;
        nextTask        = 0;
        boundPool       = nullptr;
    }
};

static_assert(sizeof(FiberTaskClassInternal) <= FiberTaskClassDefinition::Default,
              "Increase FiberTaskClassDefinition opaque storage size");

template <>
void FiberTaskClassOpaque::construct(Handle& buffer)
{
    placementNew(buffer.reinterpret_as<Object>());
}

template <>
void FiberTaskClassOpaque::destruct(Object& obj)
{
    obj.~Object();
}

FiberTaskClass::FiberTaskClass() = default;

FiberTaskClass::~FiberTaskClass() = default;

Result FiberTaskClass::create(FiberAllocator& allocator, const FiberTaskClassOptions& options)
{
    return internal.get().create(allocator, options);
}

Result FiberTaskClass::acquire(FiberTask*& outTask) { return internal.get().acquire(outTask); }

Result FiberTaskClass::release(FiberTask& task) { return internal.get().releaseTask(task); }

Result FiberTaskClass::waitForAvailableSlot(FiberScheduler& scheduler)
{
    return internal.get().waitForAvailableSlot(scheduler);
}

Result FiberTaskClass::validateClose() const { return internal.get().validateClose(); }

Result FiberTaskClass::close() { return internal.get().close(); }

void FiberTaskClass::diagnostics(FiberTaskClassDiagnostics& outDiagnostics) const
{
    internal.get().diagnostics(outDiagnostics);
}

bool FiberTaskClass::isOpen() const { return internal.get().isOpen(); }

bool FiberTaskClass::owns(const FiberTask& task) const { return internal.get().owns(task); }

size_t FiberTaskClass::capacity() const
{
    FiberTaskClassDiagnostics currentDiagnostics;
    diagnostics(currentDiagnostics);
    return currentDiagnostics.capacity;
}

size_t FiberTaskClass::activeCount() const
{
    FiberTaskClassDiagnostics currentDiagnostics;
    diagnostics(currentDiagnostics);
    return currentDiagnostics.activeTasks;
}

size_t FiberTaskClass::availableCount() const
{
    FiberTaskClassDiagnostics currentDiagnostics;
    diagnostics(currentDiagnostics);
    return currentDiagnostics.availableTasks;
}

FiberCounter::FiberCounter() = default;

FiberCounter::~FiberCounter()
{
    SC_FIBERS_ASSERT_RELEASE(waitingHead == nullptr);
    SC_FIBERS_ASSERT_RELEASE(waitingTail == nullptr);
}

size_t FiberCounter::value() const { return counterValue; }

static Result fiberTaskGroupSpawnOptions(const FiberTaskSpawnOptions& input, FiberCounter& counter,
                                         FiberTaskSpawnOptions& output)
{
    if (input.counter != nullptr)
    {
        return Result::Error("FiberTaskGroup owns the spawn counter");
    }
    output         = input;
    output.counter = &counter;
    return Result(true);
}

FiberTaskGroup::FiberTaskGroup(FiberScheduler& fiberScheduler) : scheduler(fiberScheduler) {}

FiberTaskGroup::~FiberTaskGroup() { SC_FIBERS_ASSERT_RELEASE(counter.value() == 0); }

Result FiberTaskGroup::spawn(FiberTask& task, FiberStack& stack, FiberTask::Procedure procedure)
{
    SC_TRY(scheduler.spawn(task, stack, procedure, &counter));
    linkTask(task);
    return Result(true);
}

Result FiberTaskGroup::spawn(FiberTask& task, FiberStack& stack, FiberTask::Procedure procedure,
                             FiberCancellationToken token)
{
    SC_TRY(scheduler.spawn(task, stack, procedure, token, &counter));
    linkTask(task);
    return Result(true);
}

Result FiberTaskGroup::spawn(FiberTask& task, FiberStack& stack, FiberTask::Procedure procedure,
                             const FiberTaskSpawnOptions& options)
{
    FiberTaskSpawnOptions groupOptions;
    SC_TRY(fiberTaskGroupSpawnOptions(options, counter, groupOptions));
    SC_TRY(scheduler.spawn(task, stack, procedure, groupOptions));
    linkTask(task);
    return Result(true);
}

Result FiberTaskGroup::spawn(FiberTaskPool& pool, FiberTask::Procedure procedure, FiberTask** outTask)
{
    if (outTask != nullptr)
    {
        *outTask = nullptr;
    }
    FiberTask* task = nullptr;
    SC_TRY(pool.spawn(scheduler, procedure, &task, &counter));
    SC_FIBERS_ASSERT_RELEASE(task != nullptr);
    linkTask(*task);
    if (outTask != nullptr)
    {
        *outTask = task;
    }
    return Result(true);
}

Result FiberTaskGroup::spawn(FiberTaskPool& pool, FiberTask::Procedure procedure, FiberCancellationToken token,
                             FiberTask** outTask)
{
    if (outTask != nullptr)
    {
        *outTask = nullptr;
    }
    FiberTask* task = nullptr;
    SC_TRY(pool.spawn(scheduler, procedure, token, &task, &counter));
    SC_FIBERS_ASSERT_RELEASE(task != nullptr);
    linkTask(*task);
    if (outTask != nullptr)
    {
        *outTask = task;
    }
    return Result(true);
}

Result FiberTaskGroup::spawn(FiberTaskPool& pool, FiberTask::Procedure procedure, const FiberTaskSpawnOptions& options,
                             FiberTask** outTask)
{
    if (outTask != nullptr)
    {
        *outTask = nullptr;
    }
    FiberTaskSpawnOptions groupOptions;
    SC_TRY(fiberTaskGroupSpawnOptions(options, counter, groupOptions));

    FiberTask* task = nullptr;
    SC_TRY(pool.spawn(scheduler, procedure, groupOptions, &task));
    SC_FIBERS_ASSERT_RELEASE(task != nullptr);
    linkTask(*task);
    if (outTask != nullptr)
    {
        *outTask = task;
    }
    return Result(true);
}

Result FiberTaskGroup::wait() { return scheduler.wait(counter); }

Result FiberTaskGroup::waitAll(Result* outFirstError)
{
    SC_TRY(wait());
    return findFirstError(outFirstError);
}

Result FiberTaskGroup::waitAllCancelOnParentCancel(Result* outFirstError)
{
    Result waitResult = wait();
    if (not waitResult)
    {
        if (scheduler.currentTask() != nullptr and scheduler.isCurrentTaskCancellationRequested())
        {
            Result cancelResult = cancelAll();
            Result drainResult  = scheduler.waitUninterruptible(counter);
            SC_TRY(cancelResult);
            SC_TRY(drainResult);
        }
        return waitResult;
    }
    return findFirstError(outFirstError);
}

Result FiberTaskGroup::waitCancelOnError(Result* outFirstError)
{
    if (scheduler.currentTask() != nullptr)
    {
        return Result::Error("FiberTaskGroup::waitCancelOnError must be called from the root scheduler");
    }

    Result firstError = Result(true);
    bool   hasError   = false;
    while (counter.value() != 0)
    {
        SC_TRY(scheduler.runOnce());
        if (not hasError)
        {
            Result error = findFirstError(nullptr);
            if (not error)
            {
                firstError = error;
                hasError   = true;
                SC_TRY(cancelAll());
            }
        }
    }

    Result finalError = findFirstError(outFirstError);
    if (hasError)
    {
        if (outFirstError != nullptr)
        {
            *outFirstError = firstError;
        }
        return firstError;
    }
    return finalError;
}

Result FiberTaskGroup::cancelAll()
{
    FiberTask* task = taskHead;
    while (task != nullptr)
    {
        SC_TRY(scheduler.requestCancel(*task));
        task = task->nextGroup;
    }
    return Result(true);
}

size_t FiberTaskGroup::pending() const { return counter.value(); }

size_t FiberTaskGroup::countErrors() const
{
    size_t     count = 0;
    FiberTask* task  = taskHead;
    while (task != nullptr)
    {
        if (task->isCompleted() and not task->result())
        {
            count++;
        }
        task = task->nextGroup;
    }
    return count;
}

Result FiberTaskGroup::collectErrors(Span<FiberTaskGroupError> errors, size_t& outErrors) const
{
    outErrors       = 0;
    FiberTask* task = taskHead;
    while (task != nullptr)
    {
        if (task->isCompleted() and not task->result())
        {
            if (outErrors >= errors.sizeInElements())
            {
                return Result::Error("FiberTaskGroup error storage is too small");
            }
            errors[outErrors].task   = task;
            errors[outErrors].result = task->result();
            outErrors++;
        }
        task = task->nextGroup;
    }
    return Result(true);
}

void FiberTaskGroup::linkTask(FiberTask& task)
{
    task.nextGroup = taskHead;
    taskHead       = &task;
}

Result FiberTaskGroup::findFirstError(Result* outFirstError) const
{
    FiberTask* task = taskHead;
    while (task != nullptr)
    {
        if (task->isCompleted() and not task->result())
        {
            if (outFirstError != nullptr)
            {
                *outFirstError = task->result();
            }
            return task->result();
        }
        task = task->nextGroup;
    }
    if (outFirstError != nullptr)
    {
        *outFirstError = Result(true);
    }
    return Result(true);
}

FiberTaskPool::FiberTaskPool() = default;

FiberTaskPool::FiberTaskPool(Span<FiberTask> taskStorage, Span<char> stackStorage, size_t stackSizeInBytes)
    : tasks(taskStorage), stacks(stackStorage), stackSize(stackSizeInBytes)
{}

FiberTaskPool::~FiberTaskPool()
{
    SC_FIBERS_ASSERT_RELEASE(availabilityWaitHead == nullptr);
    SC_FIBERS_ASSERT_RELEASE(availabilityWaitTail == nullptr);
    SC_FIBERS_ASSERT_RELEASE(taskClass == nullptr);
    SC_FIBERS_ASSERT_RELEASE(stackClass == nullptr);
}

Result FiberTaskPool::create(FiberTaskClass& newTaskClass, FiberStackClass& newStackClass)
{
    if (taskClass != nullptr or stackClass != nullptr or tasks.sizeInElements() != 0 or stacks.sizeInBytes() != 0 or
        stackSize != 0)
    {
        return Result::Error("FiberTaskPool is already configured");
    }
    SC_TRY(newTaskClass.internal.get().bind(*this));
    Result stackBindResult = newStackClass.internal.get().bind(*this);
    if (not stackBindResult)
    {
        SC_FIBERS_TRUST_RESULT(newTaskClass.internal.get().unbind(*this));
        return stackBindResult;
    }
    taskClass  = &newTaskClass;
    stackClass = &newStackClass;
    return Result(true);
}

Result FiberTaskPool::close()
{
    if (taskClass == nullptr and stackClass == nullptr)
    {
        return Result(true);
    }
    if (taskClass == nullptr or stackClass == nullptr)
    {
        return Result::Error("FiberTaskPool class configuration is incomplete");
    }

    SC_TRY(taskClass->internal.get().unbind(*this));
    Result stackUnbindResult = stackClass->internal.get().unbind(*this);
    if (not stackUnbindResult)
    {
        SC_FIBERS_TRUST_RESULT(taskClass->internal.get().bind(*this));
        return stackUnbindResult;
    }
    taskClass  = nullptr;
    stackClass = nullptr;
    return Result(true);
}

Result FiberTaskPool::spawn(FiberScheduler& scheduler, FiberTask::Procedure procedure, FiberTask** outTask,
                            FiberCounter* counter)
{
    FiberTaskSpawnOptions options;
    options.counter = counter;
    return spawn(scheduler, procedure, options, outTask);
}

Result FiberTaskPool::spawn(FiberScheduler& scheduler, FiberTask::Procedure procedure, FiberCancellationToken token,
                            FiberTask** outTask, FiberCounter* counter)
{
    FiberTaskSpawnOptions options;
    options.cancellationToken = token;
    options.counter           = counter;
    return spawn(scheduler, procedure, options, outTask);
}

Result FiberTaskPool::spawn(FiberScheduler& scheduler, FiberTask::Procedure procedure,
                            const FiberTaskSpawnOptions& options, FiberTask** outTask)
{
    if (outTask != nullptr)
    {
        *outTask = nullptr;
    }
    if (taskClass != nullptr and stackClass != nullptr)
    {
        FiberTask* task = nullptr;
        SC_TRY(taskClass->acquire(task));

        FiberStack stack({});
        Result     stackAcquireResult = stackClass->acquire(stack);
        if (not stackAcquireResult)
        {
            SC_FIBERS_TRUST_RESULT(taskClass->release(*task));
            return stackAcquireResult;
        }

        FiberTaskSpawnOptions poolOptions = options;
        poolOptions.originTaskClass       = taskClass;
        poolOptions.originStackClass      = stackClass;
        Result spawnResult                = scheduler.spawn(*task, stack, procedure, poolOptions);
        if (not spawnResult)
        {
            SC_FIBERS_TRUST_RESULT(stackClass->release(stack));
            SC_FIBERS_TRUST_RESULT(taskClass->release(*task));
            return spawnResult;
        }
        if (outTask != nullptr)
        {
            *outTask = task;
        }
        return Result(true);
    }
    if (stackSize == 0)
    {
        return Result::Error("FiberTaskPool stack size is zero");
    }

    const size_t numTasks  = capacity();
    FiberTask*   task      = nullptr;
    size_t       taskIndex = 0;
    for (size_t offset = 0; offset < numTasks; ++offset)
    {
        const size_t candidateIndex = (nextTask + offset) % numTasks;
        FiberTask&   candidate      = tasks[candidateIndex];
        if (not candidate.isActive())
        {
            task      = &candidate;
            taskIndex = candidateIndex;
            break;
        }
    }
    if (task == nullptr)
    {
        return Result::Error("FiberTaskPool has no available task");
    }

    if ((taskIndex + 1) * stackSize > stacks.sizeInBytes())
    {
        return Result::Error("FiberTaskPool stack storage is too small");
    }

    Span<char> stackMemory;
    SC_TRY_MSG(stacks.sliceStartLength(taskIndex * stackSize, stackSize, stackMemory),
               "FiberTaskPool stack storage is too small");

    FiberTaskSpawnOptions poolOptions = options;
    poolOptions.originPool            = this;

    FiberStack stack(stackMemory);
    SC_TRY(scheduler.spawn(*task, stack, procedure, poolOptions));
    nextTask = (taskIndex + 1) % numTasks;
    if (outTask != nullptr)
    {
        *outTask = task;
    }
    return Result(true);
}

size_t FiberTaskPool::capacity() const
{
    if (taskClass != nullptr and stackClass != nullptr)
    {
        const size_t taskCapacity  = taskClass->capacity();
        const size_t stackCapacity = stackClass->capacity();
        return taskCapacity < stackCapacity ? taskCapacity : stackCapacity;
    }
    if (stackSize == 0)
    {
        return 0;
    }
    const size_t stackCapacity = stacks.sizeInBytes() / stackSize;
    return tasks.sizeInElements() < stackCapacity ? tasks.sizeInElements() : stackCapacity;
}

size_t FiberTaskPool::activeCount() const
{
    if (taskClass != nullptr and stackClass != nullptr)
    {
        return taskClass->activeCount();
    }
    const size_t numTasks = capacity();
    size_t       active   = 0;
    for (size_t idx = 0; idx < numTasks; ++idx)
    {
        if (tasks[idx].isActive())
        {
            active++;
        }
    }
    return active;
}

size_t FiberTaskPool::availableCount() const
{
    if (taskClass != nullptr and stackClass != nullptr)
    {
        const size_t taskAvailable  = taskClass->availableCount();
        const size_t stackAvailable = stackClass->capacity() - stackClass->activeCount();
        return taskAvailable < stackAvailable ? taskAvailable : stackAvailable;
    }
    const size_t numTasks = capacity();
    const size_t active   = activeCount();
    return numTasks >= active ? numTasks - active : 0;
}

bool FiberTaskPool::hasAvailableTask() const { return availableCount() != 0; }

void FiberTaskPool::diagnostics(FiberTaskPoolDiagnostics& outDiagnostics) const
{
    outDiagnostics                = FiberTaskPoolDiagnostics();
    outDiagnostics.capacity       = capacity();
    outDiagnostics.activeTasks    = activeCount();
    outDiagnostics.availableTasks = availableCount();
    outDiagnostics.classBacked    = taskClass != nullptr and stackClass != nullptr;

    if (outDiagnostics.classBacked)
    {
        taskClass->diagnostics(outDiagnostics.taskClass);
        stackClass->diagnostics(outDiagnostics.stackClass);
        return;
    }

    outDiagnostics.taskClass.capacity    = tasks.sizeInElements();
    outDiagnostics.taskClass.activeTasks = outDiagnostics.activeTasks;
    outDiagnostics.taskClass.availableTasks =
        tasks.sizeInElements() >= outDiagnostics.activeTasks ? tasks.sizeInElements() - outDiagnostics.activeTasks : 0;

    const size_t stackCapacity                   = stackSize == 0 ? 0 : stacks.sizeInBytes() / stackSize;
    const size_t stackBytes                      = stackCapacity * stackSize;
    outDiagnostics.stackClass.capacity           = stackCapacity;
    outDiagnostics.stackClass.activeStacks       = outDiagnostics.activeTasks;
    outDiagnostics.stackClass.stackSizeInBytes   = stackSize;
    outDiagnostics.stackClass.reservedSizeBytes  = stackBytes;
    outDiagnostics.stackClass.committedSizeBytes = stackBytes;
    outDiagnostics.stackClass.peakCommittedBytes = stackBytes;
}

Result FiberTaskPool::waitForSpawnCapacity(FiberScheduler& scheduler)
{
    if (taskClass != nullptr and stackClass != nullptr)
    {
        while (availableCount() == 0)
        {
            if (taskClass->availableCount() == 0)
            {
                SC_TRY(taskClass->waitForAvailableSlot(scheduler));
            }
            if (stackClass->activeCount() == stackClass->capacity())
            {
                SC_TRY(stackClass->waitForAvailableSlot(scheduler));
            }
        }
        return Result(true);
    }

    WaitNode node;
    scheduler.add(node.counter);

    fiberSchedulerLock(primitiveLock);
    if (hasAvailableTask())
    {
        fiberSchedulerUnlock(primitiveLock);
        SC_FIBERS_TRUST_RESULT(scheduler.done(node.counter));
        return Result(true);
    }
    queueAvailabilityWaiter(node);
    fiberSchedulerUnlock(primitiveLock);

    Result waitResult = scheduler.wait(node.counter);
    fiberSchedulerLock(primitiveLock);
    if (not waitResult and not node.notified)
    {
        SC_FIBERS_ASSERT_RELEASE(removeAvailabilityWaiter(node));
        fiberSchedulerUnlock(primitiveLock);
        SC_FIBERS_TRUST_RESULT(scheduler.done(node.counter));
        return waitResult;
    }
    fiberSchedulerUnlock(primitiveLock);

    if (not waitResult)
    {
        FiberCounter* nextCounter = popAvailabilityWaiterForNotification();
        if (nextCounter != nullptr)
        {
            SC_TRY(scheduler.done(*nextCounter));
        }
    }
    return waitResult;
}

Result FiberTaskPool::waitForAvailableTask(FiberScheduler& scheduler) { return waitForSpawnCapacity(scheduler); }

size_t FiberTaskPool::stackSizeInBytes() const
{
    if (stackClass != nullptr)
    {
        FiberStackClassDiagnostics diagnostics;
        stackClass->diagnostics(diagnostics);
        return diagnostics.stackSizeInBytes;
    }
    return stackSize;
}

void FiberTaskPool::fillHighWaterMarks()
{
    if (stackClass != nullptr)
    {
        stackClass->fillHighWaterMarks();
        return;
    }
    const size_t numStacks = capacity();
    for (size_t idx = 0; idx < numStacks; ++idx)
    {
        FiberStack stack({nullptr, 0});
        if (stackAt(idx, stack))
        {
            stack.fillHighWaterMark();
        }
    }
}

Result FiberTaskPool::stackHighWaterUsedBytes(size_t stackIndex, size_t& outBytes) const
{
    if (stackClass != nullptr)
    {
        return Result::Error("FiberTaskPool class-backed per-stack high water is unavailable");
    }
    FiberStack stack({nullptr, 0});
    SC_TRY(stackAt(stackIndex, stack));
    outBytes = stack.highWaterUsedBytes();
    return Result(true);
}

Result FiberTaskPool::stackHighWaterUnusedBytes(size_t stackIndex, size_t& outBytes) const
{
    if (stackClass != nullptr)
    {
        return Result::Error("FiberTaskPool class-backed per-stack high water is unavailable");
    }
    FiberStack stack({nullptr, 0});
    SC_TRY(stackAt(stackIndex, stack));
    outBytes = stack.highWaterUnusedBytes();
    return Result(true);
}

void FiberTaskPool::queueAvailabilityWaiter(WaitNode& node)
{
    node.next = nullptr;
    if (availabilityWaitTail == nullptr)
    {
        availabilityWaitHead = &node;
        availabilityWaitTail = &node;
    }
    else
    {
        availabilityWaitTail->next = &node;
        availabilityWaitTail       = &node;
    }
}

FiberCounter* FiberTaskPool::popAvailabilityWaiterForNotification()
{
    fiberSchedulerLock(primitiveLock);
    WaitNode* node = availabilityWaitHead;
    if (node == nullptr or not hasAvailableTask())
    {
        fiberSchedulerUnlock(primitiveLock);
        return nullptr;
    }
    availabilityWaitHead = node->next;
    node->next           = nullptr;
    if (availabilityWaitHead == nullptr)
    {
        availabilityWaitTail = nullptr;
    }
    node->notified = true;
    fiberSchedulerUnlock(primitiveLock);
    return &node->counter;
}

bool FiberTaskPool::removeAvailabilityWaiter(WaitNode& node)
{
    WaitNode* previous = nullptr;
    WaitNode* current  = availabilityWaitHead;
    while (current != nullptr)
    {
        WaitNode* next = current->next;
        if (current == &node)
        {
            if (previous == nullptr)
            {
                availabilityWaitHead = next;
            }
            else
            {
                previous->next = next;
            }
            if (availabilityWaitTail == &node)
            {
                availabilityWaitTail = previous;
            }
            node.next = nullptr;
            return true;
        }
        previous = current;
        current  = next;
    }
    return false;
}

Result FiberTaskPool::stackAt(size_t stackIndex, FiberStack& outStack) const
{
    if (stackIndex >= capacity())
    {
        return Result::Error("FiberTaskPool stack index out of range");
    }
    Span<char> stackMemory;
    SC_TRY_MSG(stacks.sliceStartLength(stackIndex * stackSize, stackSize, stackMemory),
               "FiberTaskPool stack storage is too small");
    outStack = FiberStack(stackMemory);
    return Result(true);
}

FiberEvent::FiberEvent(bool initialSignaled) : signaled(initialSignaled) {}

FiberEvent::~FiberEvent()
{
    SC_FIBERS_ASSERT_RELEASE(waitHead == nullptr);
    SC_FIBERS_ASSERT_RELEASE(waitTail == nullptr);
}

Result FiberEvent::wait(FiberScheduler& scheduler)
{
    fiberSchedulerLock(primitiveLock);
    if (signaled)
    {
        fiberSchedulerUnlock(primitiveLock);
        return Result(true);
    }

    WaitNode node;
    scheduler.add(node.counter);
    queueWaiter(node);
    fiberSchedulerUnlock(primitiveLock);

    Result waitResult = scheduler.wait(node.counter);
    fiberSchedulerLock(primitiveLock);
    if (not waitResult and not node.notified)
    {
        SC_FIBERS_ASSERT_RELEASE(removeWaiter(node));
        fiberSchedulerUnlock(primitiveLock);
        SC_FIBERS_TRUST_RESULT(scheduler.done(node.counter));
        return waitResult;
    }
    fiberSchedulerUnlock(primitiveLock);
    return waitResult;
}

Result FiberEvent::signal(FiberScheduler& scheduler)
{
    fiberSchedulerLock(primitiveLock);
    signaled = true;
    while (waitHead != nullptr)
    {
        WaitNode& node = *waitHead;
        waitHead       = node.next;
        node.next      = nullptr;
        node.notified  = true;
        fiberSchedulerUnlock(primitiveLock);
        SC_TRY(scheduler.done(node.counter));
        fiberSchedulerLock(primitiveLock);
    }
    waitTail = nullptr;
    fiberSchedulerUnlock(primitiveLock);
    return Result(true);
}

void FiberEvent::reset()
{
    fiberSchedulerLock(primitiveLock);
    signaled = false;
    fiberSchedulerUnlock(primitiveLock);
}

bool FiberEvent::isSignaled() const
{
    fiberSchedulerLock(primitiveLock);
    const bool result = signaled;
    fiberSchedulerUnlock(primitiveLock);
    return result;
}

void FiberEvent::queueWaiter(WaitNode& node)
{
    node.next = nullptr;
    if (waitTail == nullptr)
    {
        waitHead = &node;
        waitTail = &node;
    }
    else
    {
        waitTail->next = &node;
        waitTail       = &node;
    }
}

bool FiberEvent::removeWaiter(WaitNode& node)
{
    WaitNode* previous = nullptr;
    WaitNode* current  = waitHead;
    while (current != nullptr)
    {
        WaitNode* next = current->next;
        if (current == &node)
        {
            if (previous == nullptr)
            {
                waitHead = next;
            }
            else
            {
                previous->next = next;
            }
            if (waitTail == &node)
            {
                waitTail = previous;
            }
            node.next = nullptr;
            return true;
        }
        previous = current;
        current  = next;
    }
    return false;
}

FiberAutoResetEvent::FiberAutoResetEvent(bool initialSignaled) : signaled(initialSignaled) {}

FiberAutoResetEvent::~FiberAutoResetEvent()
{
    SC_FIBERS_ASSERT_RELEASE(waitHead == nullptr);
    SC_FIBERS_ASSERT_RELEASE(waitTail == nullptr);
}

Result FiberAutoResetEvent::wait(FiberScheduler& scheduler)
{
    fiberSchedulerLock(primitiveLock);
    if (signaled)
    {
        signaled = false;
        fiberSchedulerUnlock(primitiveLock);
        return Result(true);
    }

    WaitNode node;
    scheduler.add(node.counter);
    queueWaiter(node);
    fiberSchedulerUnlock(primitiveLock);

    Result waitResult = scheduler.wait(node.counter);
    fiberSchedulerLock(primitiveLock);
    if (not waitResult and not node.notified)
    {
        SC_FIBERS_ASSERT_RELEASE(removeWaiter(node));
        fiberSchedulerUnlock(primitiveLock);
        SC_FIBERS_TRUST_RESULT(scheduler.done(node.counter));
        return waitResult;
    }
    fiberSchedulerUnlock(primitiveLock);
    return waitResult;
}

Result FiberAutoResetEvent::signal(FiberScheduler& scheduler)
{
    fiberSchedulerLock(primitiveLock);
    WaitNode* node = nullptr;
    if (popWaiter(node))
    {
        node->notified = true;
        fiberSchedulerUnlock(primitiveLock);
        SC_TRY(scheduler.done(node->counter));
        return Result(true);
    }
    signaled = true;
    fiberSchedulerUnlock(primitiveLock);
    return Result(true);
}

void FiberAutoResetEvent::reset()
{
    fiberSchedulerLock(primitiveLock);
    signaled = false;
    fiberSchedulerUnlock(primitiveLock);
}

bool FiberAutoResetEvent::isSignaled() const
{
    fiberSchedulerLock(primitiveLock);
    const bool result = signaled;
    fiberSchedulerUnlock(primitiveLock);
    return result;
}

void FiberAutoResetEvent::queueWaiter(WaitNode& node)
{
    node.next = nullptr;
    if (waitTail == nullptr)
    {
        waitHead = &node;
        waitTail = &node;
    }
    else
    {
        waitTail->next = &node;
        waitTail       = &node;
    }
}

bool FiberAutoResetEvent::popWaiter(WaitNode*& node)
{
    node = waitHead;
    if (node == nullptr)
    {
        return false;
    }
    waitHead   = node->next;
    node->next = nullptr;
    if (waitHead == nullptr)
    {
        waitTail = nullptr;
    }
    return true;
}

bool FiberAutoResetEvent::removeWaiter(WaitNode& node)
{
    WaitNode* previous = nullptr;
    WaitNode* current  = waitHead;
    while (current != nullptr)
    {
        WaitNode* next = current->next;
        if (current == &node)
        {
            if (previous == nullptr)
            {
                waitHead = next;
            }
            else
            {
                previous->next = next;
            }
            if (waitTail == &node)
            {
                waitTail = previous;
            }
            node.next = nullptr;
            return true;
        }
        previous = current;
        current  = next;
    }
    return false;
}

FiberSemaphore::FiberSemaphore(size_t initialCount) : availableCount(initialCount) {}

FiberSemaphore::~FiberSemaphore()
{
    SC_FIBERS_ASSERT_RELEASE(waitHead == nullptr);
    SC_FIBERS_ASSERT_RELEASE(waitTail == nullptr);
}

Result FiberSemaphore::wait(FiberScheduler& scheduler)
{
    fiberSchedulerLock(primitiveLock);
    if (availableCount != 0)
    {
        availableCount -= 1;
        fiberSchedulerUnlock(primitiveLock);
        return Result(true);
    }

    WaitNode node;
    scheduler.add(node.counter);
    queueWaiter(node);
    fiberSchedulerUnlock(primitiveLock);

    Result waitResult = scheduler.wait(node.counter);
    fiberSchedulerLock(primitiveLock);
    if (not waitResult and not node.notified)
    {
        SC_FIBERS_ASSERT_RELEASE(removeWaiter(node));
        fiberSchedulerUnlock(primitiveLock);
        SC_FIBERS_TRUST_RESULT(scheduler.done(node.counter));
        return waitResult;
    }
    if (not waitResult)
    {
        fiberSchedulerUnlock(primitiveLock);
        SC_TRY(signal(scheduler, 1));
    }
    else
    {
        fiberSchedulerUnlock(primitiveLock);
    }
    return waitResult;
}

Result FiberSemaphore::signal(FiberScheduler& scheduler, size_t count)
{
    fiberSchedulerLock(primitiveLock);
    for (size_t idx = 0; idx < count; ++idx)
    {
        WaitNode* node = popWaiter();
        if (node == nullptr)
        {
            availableCount += 1;
        }
        else
        {
            node->notified = true;
            fiberSchedulerUnlock(primitiveLock);
            SC_TRY(scheduler.done(node->counter));
            fiberSchedulerLock(primitiveLock);
        }
    }
    fiberSchedulerUnlock(primitiveLock);
    return Result(true);
}

size_t FiberSemaphore::available() const
{
    fiberSchedulerLock(primitiveLock);
    const size_t result = availableCount;
    fiberSchedulerUnlock(primitiveLock);
    return result;
}

void FiberSemaphore::queueWaiter(WaitNode& node)
{
    node.next = nullptr;
    if (waitTail == nullptr)
    {
        waitHead = &node;
        waitTail = &node;
    }
    else
    {
        waitTail->next = &node;
        waitTail       = &node;
    }
}

FiberSemaphore::WaitNode* FiberSemaphore::popWaiter()
{
    WaitNode* node = waitHead;
    if (node == nullptr)
    {
        return nullptr;
    }
    waitHead   = node->next;
    node->next = nullptr;
    if (waitHead == nullptr)
    {
        waitTail = nullptr;
    }
    return node;
}

bool FiberSemaphore::removeWaiter(WaitNode& node)
{
    WaitNode* previous = nullptr;
    WaitNode* current  = waitHead;
    while (current != nullptr)
    {
        WaitNode* next = current->next;
        if (current == &node)
        {
            if (previous == nullptr)
            {
                waitHead = next;
            }
            else
            {
                previous->next = next;
            }
            if (waitTail == &node)
            {
                waitTail = previous;
            }
            node.next = nullptr;
            return true;
        }
        previous = current;
        current  = next;
    }
    return false;
}

FiberMutex::FiberMutex() = default;

FiberMutex::~FiberMutex()
{
    SC_FIBERS_ASSERT_RELEASE(waitHead == nullptr);
    SC_FIBERS_ASSERT_RELEASE(waitTail == nullptr);
}

Result FiberMutex::lock(FiberScheduler& scheduler)
{
    FiberTask* currentTask = scheduler.currentTask();
    fiberSchedulerLock(primitiveLock);
    if (not locked)
    {
        locked = true;
        owner  = currentTask;
        fiberSchedulerUnlock(primitiveLock);
        return Result(true);
    }
    if (owner != nullptr and owner == currentTask)
    {
        fiberSchedulerUnlock(primitiveLock);
        SC_FIBERS_ASSERT_RELEASE(false);
        return Result::Error("FiberMutex cannot be locked recursively");
    }

    WaitNode node;
    scheduler.add(node.counter);
    queueWaiter(node);
    fiberSchedulerUnlock(primitiveLock);

    Result waitResult = scheduler.wait(node.counter);
    fiberSchedulerLock(primitiveLock);
    if (not waitResult and not node.notified)
    {
        SC_FIBERS_ASSERT_RELEASE(removeWaiter(node));
        fiberSchedulerUnlock(primitiveLock);
        SC_FIBERS_TRUST_RESULT(scheduler.done(node.counter));
        return waitResult;
    }
    if (not waitResult)
    {
        fiberSchedulerUnlock(primitiveLock);
        SC_TRY(unlock(scheduler));
    }
    else
    {
        owner = currentTask;
        fiberSchedulerUnlock(primitiveLock);
    }
    return waitResult;
}

Result FiberMutex::unlock(FiberScheduler& scheduler)
{
    fiberSchedulerLock(primitiveLock);
    if (not locked)
    {
        fiberSchedulerUnlock(primitiveLock);
        return Result::Error("FiberMutex is not locked");
    }
    if (owner != nullptr and owner != scheduler.currentTask())
    {
        fiberSchedulerUnlock(primitiveLock);
        SC_FIBERS_ASSERT_RELEASE(false);
        return Result::Error("FiberMutex cannot be unlocked by a different fiber");
    }

    WaitNode* node = popWaiter();
    if (node == nullptr)
    {
        locked = false;
        owner  = nullptr;
        fiberSchedulerUnlock(primitiveLock);
    }
    else
    {
        owner          = nullptr;
        node->notified = true;
        fiberSchedulerUnlock(primitiveLock);
        SC_TRY(scheduler.done(node->counter));
    }
    return Result(true);
}

bool FiberMutex::isLocked() const
{
    fiberSchedulerLock(primitiveLock);
    const bool result = locked;
    fiberSchedulerUnlock(primitiveLock);
    return result;
}

bool FiberMutex::isOwnedByCurrentTask(FiberScheduler& scheduler) const
{
    fiberSchedulerLock(primitiveLock);
    const bool result = owner == scheduler.currentTask();
    fiberSchedulerUnlock(primitiveLock);
    return result;
}

void FiberMutex::queueWaiter(WaitNode& node)
{
    node.next = nullptr;
    if (waitTail == nullptr)
    {
        waitHead = &node;
        waitTail = &node;
    }
    else
    {
        waitTail->next = &node;
        waitTail       = &node;
    }
}

FiberMutex::WaitNode* FiberMutex::popWaiter()
{
    WaitNode* node = waitHead;
    if (node == nullptr)
    {
        return nullptr;
    }
    waitHead   = node->next;
    node->next = nullptr;
    if (waitHead == nullptr)
    {
        waitTail = nullptr;
    }
    return node;
}

bool FiberMutex::removeWaiter(WaitNode& node)
{
    WaitNode* previous = nullptr;
    WaitNode* current  = waitHead;
    while (current != nullptr)
    {
        WaitNode* next = current->next;
        if (current == &node)
        {
            if (previous == nullptr)
            {
                waitHead = next;
            }
            else
            {
                previous->next = next;
            }
            if (waitTail == &node)
            {
                waitTail = previous;
            }
            node.next = nullptr;
            return true;
        }
        previous = current;
        current  = next;
    }
    return false;
}

FiberScheduler::FiberScheduler() = default;

FiberScheduler::~FiberScheduler() { SC_FIBERS_ASSERT_RELEASE(not hasActiveFibers()); }

void FiberScheduler::lock() const
{
    const size_t spinRetries = fiberSchedulerLock(schedulerLock);
    schedulerLockAcquisitions += 1;
    schedulerLockSpinRetries += spinRetries;
    if (spinRetries > 0)
    {
        schedulerLockContentions += 1;
    }
    if (spinRetries > schedulerLockPeakSpinRetries)
    {
        schedulerLockPeakSpinRetries = spinRetries;
    }
}

void FiberScheduler::unlock() const { fiberSchedulerUnlock(schedulerLock); }

void FiberScheduler::trace(FiberTraceEventType type, FiberTask* task, FiberWorker* worker, size_t value) const
{
    if (traceHooks.callback == nullptr)
    {
        return;
    }

    FiberTraceEvent event;
    event.type      = type;
    event.scheduler = const_cast<FiberScheduler*>(this);
    event.worker    = worker;
    event.task      = task;
    event.value     = value;
    traceHooks.callback(traceHooks.userData, event);
}

Result FiberScheduler::spawn(FiberTask& task, FiberStack& stack, FiberTask::Procedure procedure, FiberCounter* counter)
{
    FiberTaskSpawnOptions options;
    options.counter = counter;
    return spawn(task, stack, procedure, options);
}

Result FiberScheduler::spawn(FiberTask& task, FiberStack& stack, FiberTask::Procedure procedure,
                             FiberCancellationToken token, FiberCounter* counter)
{
    FiberTaskSpawnOptions options;
    options.cancellationToken = token;
    options.counter           = counter;
    return spawn(task, stack, procedure, options);
}

Result FiberScheduler::spawn(FiberTask& task, FiberStack& stack, FiberTask::Procedure procedure,
                             const FiberTaskSpawnOptions& options)
{
    LockGuard guard(*this);

    if (task.isActive())
    {
        return Result::Error("FiberTask is already active");
    }
    if (not procedure.isValid())
    {
        return Result::Error("FiberTask procedure is not valid");
    }

    task.procedure         = procedure;
    task.scheduler         = this;
    task.completionCounter = options.counter;
    task.cancellationToken = options.cancellationToken;
    task.nextReady         = nullptr;
    task.previousReady     = nullptr;
    task.nextWaiting       = nullptr;
    task.nextActive        = nullptr;
    task.previousActive    = nullptr;
    task.nextGroup         = nullptr;
    task.waitingCounter    = nullptr;
    task.suspendCounter    = nullptr;
    task.originPool        = options.originPool;
    task.originTaskClass   = options.originTaskClass;
    task.originStackClass  = options.originStackClass;
    task.originStackMemory = options.originStackClass != nullptr ? stack.memory() : Span<char>();
    task.runningWorker     = nullptr;
    task.stackOwner        = nullptr;
    task.taskResult        = Result(true);
    fiberTaskStatusStore(task.taskStatus, FiberTaskStatus::Ready);
    task.suspendAction        = FiberTaskSuspendAction::None;
    task.cancelRequested      = options.cancellationToken.isCancellationRequested();
    task.suspendInterruptible = false;

    Result createResult = FiberContextOperations::create(task.context(), stack.memory(), taskEntry, &task);
    if (not createResult)
    {
        task.procedure         = FiberTask::Procedure();
        task.scheduler         = nullptr;
        task.completionCounter = nullptr;
        task.cancellationToken = FiberCancellationToken();
        task.previousReady     = nullptr;
        task.nextActive        = nullptr;
        task.previousActive    = nullptr;
        task.nextGroup         = nullptr;
        task.waitingCounter    = nullptr;
        task.suspendCounter    = nullptr;
        task.originPool        = nullptr;
        task.originTaskClass   = nullptr;
        task.originStackClass  = nullptr;
        task.originStackMemory = {};
        task.runningWorker     = nullptr;
        task.stackOwner        = nullptr;
        fiberTaskStatusStore(task.taskStatus, FiberTaskStatus::Invalid);
        task.suspendAction        = FiberTaskSuspendAction::None;
        task.cancelRequested      = false;
        task.suspendInterruptible = false;
        return createResult;
    }

    if (stack.stackOwner != nullptr)
    {
        static_cast<FiberVirtualStackInternal*>(stack.stackOwner)->acquireStack();
        task.stackOwner = stack.stackOwner;
    }
    if (options.setUserData)
    {
        task.taskUserData = options.userData;
    }

    if (options.counter != nullptr)
    {
        addUnlocked(*options.counter);
    }

    activeFibers += 1;
    linkActiveUnlocked(task);
    pushReadyUnlocked(task);
    return Result(true);
}

Result FiberScheduler::runOnce()
{
    FiberWorker worker;
    return runOnce(worker);
}

Result FiberScheduler::runOnce(FiberWorker& worker)
{
    FiberTask* task = nullptr;
    {
        LockGuard guard(*this);
        worker.runAttempts += 1;
        task = popReadyUnlocked();
        if (task == nullptr)
        {
            worker.idlePolls += 1;
            return activeFibers == 0 ? Result(true) : Result::Error("FiberScheduler cannot make progress");
        }
        fiberTaskStatusStore(task->taskStatus, FiberTaskStatus::Running);
    }
    return runReadyTask(*task, worker);
}

Result FiberScheduler::runOnce(FiberWorker& worker, Span<FiberWorker> workerGroup)
{
    FiberTask* task = nullptr;
    {
        LockGuard guard(*this);
        for (size_t workerIndex = 0; workerIndex < workerGroup.sizeInElements(); ++workerIndex)
        {
            FiberWorker& groupWorker = workerGroup[workerIndex];
            SC_FIBERS_ASSERT_RELEASE(groupWorker.localQueueScheduler == nullptr or
                                     groupWorker.localQueueScheduler == this);
            groupWorker.localSchedulingActive = true;
            groupWorker.localQueueScheduler   = this;
            if (not groupWorker.stealCursorInitialized)
            {
                groupWorker.stealCursor            = (workerIndex + 1) % workerGroup.sizeInElements();
                groupWorker.stealCursorInitialized = true;
            }
        }
        worker.runAttempts += 1;
        task = popReadyUnlocked(worker, workerGroup);
        if (task == nullptr)
        {
            worker.idlePolls += 1;
            return activeFibers == 0 ? Result(true) : Result::Error("FiberScheduler cannot make progress");
        }
        fiberTaskStatusStore(task->taskStatus, FiberTaskStatus::Running);
    }
    return runReadyTask(*task, worker);
}

Result FiberScheduler::runNoWait()
{
    FiberWorker worker;
    return runNoWait(worker);
}

Result FiberScheduler::runNoWait(FiberWorker& worker) { return runNoWait(worker, {}); }

Result FiberScheduler::runNoWait(FiberWorker& worker, Span<FiberWorker> stealWorkers)
{
    FiberTask* task = nullptr;
    {
        LockGuard guard(*this);
        for (size_t workerIndex = 0; workerIndex < stealWorkers.sizeInElements(); ++workerIndex)
        {
            FiberWorker& groupWorker = stealWorkers[workerIndex];
            SC_FIBERS_ASSERT_RELEASE(groupWorker.localQueueScheduler == nullptr or
                                     groupWorker.localQueueScheduler == this);
            groupWorker.localSchedulingActive = true;
            groupWorker.localQueueScheduler   = this;
            if (not groupWorker.stealCursorInitialized)
            {
                groupWorker.stealCursor            = (workerIndex + 1) % stealWorkers.sizeInElements();
                groupWorker.stealCursorInitialized = true;
            }
        }
        worker.runAttempts += 1;
    }

    if (worker.localDeque != nullptr and worker.localQueueScheduler == this)
    {
        task = popWorkerReadyUnlocked(worker);
    }

    if (task != nullptr)
    {
        LockGuard guard(*this);
        fiberTaskStatusStore(task->taskStatus, FiberTaskStatus::Running);
    }
    else
    {
        LockGuard guard(*this);
        task = worker.localDeque != nullptr ? popReadyUnlocked() : popReadyUnlocked(worker, stealWorkers);
        if (task == nullptr and worker.localDeque != nullptr)
        {
            task = stealReadyUnlocked(worker, stealWorkers);
        }
        if (task == nullptr and worker.localDeque != nullptr)
        {
            task = popWorkerReadyUnlocked(worker);
        }
        if (task == nullptr)
        {
            worker.idlePolls += 1;
            if (stealWorkers.sizeInElements() == 0 and fiberAtomicLoadSize(readyFibers) != 0)
            {
                return Result::Error("FiberScheduler has ready fibers queued on another worker");
            }
            return Result(true);
        }
        fiberTaskStatusStore(task->taskStatus, FiberTaskStatus::Running);
    }
    return runReadyTask(*task, worker);
}

Result FiberScheduler::runReadyFibers()
{
    FiberWorker worker;
    return runReadyFibers(worker);
}

Result FiberScheduler::runReadyFibers(FiberWorker& worker)
{
    while (hasReadyFibers())
    {
        SC_TRY(runNoWait(worker));
    }
    return Result(true);
}

Result FiberScheduler::runReadyFibers(FiberWorker& worker, Span<FiberWorker> workerGroup)
{
    while (hasReadyFibers())
    {
        SC_TRY(runNoWait(worker, workerGroup));
    }
    return Result(true);
}

Result FiberScheduler::run()
{
    FiberWorker worker;
    return run(worker);
}

Result FiberScheduler::run(FiberWorker& worker)
{
    while (hasActiveFibers())
    {
        SC_TRY(runOnce(worker));
    }
    return Result(true);
}

Result FiberScheduler::run(FiberWorker& worker, Span<FiberWorker> workerGroup)
{
    while (hasActiveFibers())
    {
        SC_TRY(runOnce(worker, workerGroup));
    }
    return Result(true);
}

Result FiberScheduler::createWorkerDeques(FiberAllocator& allocator, Span<FiberWorker> workers,
                                          size_t capacityPerWorker)
{
    if (not allocator.isOpen())
    {
        return Result::Error("FiberAllocator is not open");
    }
    if (capacityPerWorker == 0)
    {
        return Result::Error("Fiber worker deque capacity is zero");
    }

    size_t allocatedWorkers = 0;
    for (FiberWorker& worker : workers)
    {
        if (worker.localDeque != nullptr)
        {
            return Result::Error("FiberWorker already has a local deque");
        }
        if (worker.localReadyFibers != 0)
        {
            return Result::Error("FiberWorker local queue is not empty");
        }

        const size_t dequeBytes = capacityPerWorker * sizeof(FiberTask*);
        void*        memory     = allocator.allocate(&worker, dequeBytes, alignof(FiberTask*));
        if (memory == nullptr)
        {
            releaseWorkerDeques({workers.data(), allocatedWorkers});
            return Result::Error("FiberWorker deque allocation failed");
        }

        worker.localDeque          = static_cast<FiberTask**>(memory);
        worker.localDequeAllocator = &allocator;
        worker.localDequeCapacity  = capacityPerWorker;
        worker.localDequeHead      = 0;
        fiberAtomicStoreSize(worker.localDequeTop, 0);
        fiberAtomicStoreSize(worker.localDequeBottom, 0);
        worker.localReadyFibers = 0;
        for (size_t idx = 0; idx < capacityPerWorker; ++idx)
        {
            worker.localDeque[idx] = nullptr;
        }
        allocatedWorkers++;
    }
    return Result(true);
}

void FiberScheduler::releaseWorkerDeques(Span<FiberWorker> workers)
{
    for (FiberWorker& worker : workers)
    {
        SC_FIBERS_ASSERT_RELEASE(worker.localReadyFibers == 0);
        SC_FIBERS_ASSERT_RELEASE(worker.localReadyHead == nullptr);
        SC_FIBERS_ASSERT_RELEASE(worker.localReadyTail == nullptr);
        if (worker.localDeque != nullptr and worker.localDequeAllocator != nullptr)
        {
            worker.localDequeAllocator->release(worker.localDeque);
        }
        worker.localDeque          = nullptr;
        worker.localDequeAllocator = nullptr;
        worker.localDequeCapacity  = 0;
        worker.localDequeHead      = 0;
        fiberAtomicStoreSize(worker.localDequeTop, 0);
        fiberAtomicStoreSize(worker.localDequeBottom, 0);
        worker.localReadyFibers = 0;
    }
}

Result FiberScheduler::yield()
{
    FiberWorker* worker = currentWorkerFor(*this);
    if (worker == nullptr or worker->workerTask == nullptr)
    {
        return Result::Error("FiberScheduler::yield must be called from a fiber");
    }

    FiberTask& task = *worker->workerTask;
    {
        LockGuard guard(*this);
        if (task.isCancellationRequested())
        {
            return Result::Error("FiberTask cancelled");
        }

        task.suspendAction        = FiberTaskSuspendAction::Ready;
        task.suspendCounter       = nullptr;
        task.suspendInterruptible = false;
        task.preferredWorker      = worker->localSchedulingActive ? worker : nullptr;
        task.runningWorker        = nullptr;
        worker->yieldedFibers += 1;
        worker->workerTask = nullptr;
    }

    trace(FiberTraceEventType::TaskYielded, &task, worker);
    FiberContextOperations::switchTo(task.context(), worker->rootContext());
    {
        LockGuard guard(*this);
        return task.isCancellationRequested() ? Result::Error("FiberTask cancelled") : Result(true);
    }
}

Result FiberScheduler::shutdown()
{
    SC_TRY(requestCancelAll());
    return run();
}

Result FiberScheduler::shutdown(FiberWorker& worker)
{
    SC_TRY(requestCancelAll());
    return run(worker);
}

Result FiberScheduler::shutdown(FiberWorker& worker, Span<FiberWorker> workerGroup)
{
    SC_TRY(requestCancelAll());
    return run(worker, workerGroup);
}

Result FiberScheduler::requestCancel(FiberTask& task)
{
    LockGuard guard(*this);

    if (not task.isActive())
    {
        return Result(true);
    }
    if (task.scheduler != this)
    {
        return Result::Error("FiberTask belongs to another scheduler");
    }

    return cancelTaskUnlocked(task);
}

Result FiberScheduler::requestCancel(FiberCancellationTokenSource& tokenSource)
{
    tokenSource.requestCancel();

    LockGuard  guard(*this);
    FiberTask* task = activeHead;
    while (task != nullptr)
    {
        FiberTask* next = task->nextActive;
        if (task->cancellationToken.source == &tokenSource)
        {
            SC_TRY(cancelTaskUnlocked(*task));
        }
        task = next;
    }
    return Result(true);
}

Result FiberScheduler::requestCancelAll()
{
    LockGuard guard(*this);

    FiberTask* task = activeHead;
    while (task != nullptr)
    {
        FiberTask* next = task->nextActive;
        SC_TRY(cancelTaskUnlocked(*task));
        task = next;
    }
    return Result(true);
}

void FiberScheduler::add(FiberCounter& counter)
{
    LockGuard guard(*this);
    addUnlocked(counter);
}

Result FiberScheduler::done(FiberCounter& counter)
{
    LockGuard guard(*this);
    return doneUnlocked(counter);
}

void FiberScheduler::addUnlocked(FiberCounter& counter) { counter.counterValue += 1; }

Result FiberScheduler::doneUnlocked(FiberCounter& counter)
{
    if (counter.counterValue == 0)
    {
        return Result::Error("FiberCounter already reached zero");
    }

    counter.counterValue -= 1;
    if (counter.counterValue == 0)
    {
        wakeCounterWaitersUnlocked(counter);
    }
    return Result(true);
}

Result FiberScheduler::wait(FiberCounter& counter) { return waitImpl(counter, true); }

Result FiberScheduler::waitUninterruptible(FiberCounter& counter) { return waitImpl(counter, false); }

void FiberScheduler::setTraceHooks(const FiberTraceHooks& hooks)
{
    LockGuard guard(*this);
    traceHooks = hooks;
}

void FiberScheduler::clearTraceHooks()
{
    LockGuard guard(*this);
    traceHooks = {};
}

Result FiberScheduler::waitImpl(FiberCounter& counter, bool interruptible)
{
    {
        LockGuard guard(*this);
        if (counter.counterValue == 0)
        {
            return Result(true);
        }
    }

    FiberWorker* worker = currentWorkerFor(*this);
    if (worker == nullptr or worker->workerTask == nullptr)
    {
        while (true)
        {
            {
                LockGuard guard(*this);
                if (counter.counterValue == 0)
                {
                    return Result(true);
                }
            }
            SC_TRY(runOnce());
        }
    }

    FiberTask& task = *worker->workerTask;
    {
        LockGuard guard(*this);
        if (counter.counterValue == 0)
        {
            return Result(true);
        }
        if (interruptible and task.isCancellationRequested())
        {
            return Result::Error("FiberTask cancelled");
        }

        task.suspendAction        = FiberTaskSuspendAction::CounterWait;
        task.suspendCounter       = &counter;
        task.suspendInterruptible = interruptible;
        task.preferredWorker      = worker->localSchedulingActive ? worker : nullptr;
        task.runningWorker        = nullptr;
        worker->waitingFibers += 1;
        worker->workerTask = nullptr;
    }

    trace(FiberTraceEventType::TaskWaiting, &task, worker);
    FiberContextOperations::switchTo(task.context(), worker->rootContext());
    {
        LockGuard guard(*this);
        return interruptible and task.isCancellationRequested() ? Result::Error("FiberTask cancelled") : Result(true);
    }
}

FiberTask* FiberScheduler::currentTask()
{
    FiberWorker* worker = currentWorkerFor(*this);
    return worker == nullptr ? nullptr : worker->workerTask;
}

const FiberTask* FiberScheduler::currentTask() const
{
    const FiberWorker* worker = currentWorkerFor(*this);
    return worker == nullptr ? nullptr : worker->workerTask;
}

bool FiberScheduler::isCurrentTaskCancellationRequested() const
{
    const FiberWorker* worker = currentWorkerFor(*this);
    if (worker == nullptr or worker->workerTask == nullptr)
    {
        return false;
    }
    LockGuard guard(*this);
    return worker->workerTask->isCancellationRequested();
}

bool FiberScheduler::hasReadyFibers() const
{
    LockGuard guard(*this);
    return fiberAtomicLoadSize(readyFibers) != 0;
}

bool FiberScheduler::hasActiveFibers() const
{
    LockGuard guard(*this);
    return activeFibers != 0;
}

size_t FiberScheduler::readyFiberCount() const
{
    LockGuard guard(*this);
    return fiberAtomicLoadSize(readyFibers);
}

size_t FiberScheduler::readyFiberCount(const FiberWorker& worker) const
{
    LockGuard guard(*this);
    if (worker.localQueueScheduler != this)
    {
        return 0;
    }
    if (worker.localDeque != nullptr)
    {
        const size_t top    = fiberAtomicLoadSize(worker.localDequeTop);
        const size_t bottom = fiberAtomicLoadSize(worker.localDequeBottom);
        return bottom >= top ? bottom - top : 0;
    }
    return worker.localReadyFibers;
}

size_t FiberScheduler::stolenFiberCount(const FiberWorker& worker) const
{
    LockGuard guard(*this);
    return worker.stolenFibers;
}

size_t FiberScheduler::stolenFiberCount(Span<FiberWorker> workers) const
{
    LockGuard guard(*this);
    size_t    stolen = 0;
    for (const FiberWorker& worker : workers)
    {
        stolen += worker.stolenFibers;
    }
    return stolen;
}

size_t FiberScheduler::activeFiberCount() const
{
    LockGuard guard(*this);
    return activeFibers;
}

void FiberScheduler::schedulerDiagnostics(FiberSchedulerDiagnostics& diagnostics) const
{
    LockGuard guard(*this);

    diagnostics.readyFibers         = fiberAtomicLoadSize(readyFibers);
    diagnostics.activeFibers        = activeFibers;
    diagnostics.lockAcquisitions    = schedulerLockAcquisitions;
    diagnostics.lockContentions     = schedulerLockContentions;
    diagnostics.lockSpinRetries     = schedulerLockSpinRetries;
    diagnostics.lockPeakSpinRetries = schedulerLockPeakSpinRetries;
}

void FiberScheduler::resetSchedulerDiagnostics()
{
    LockGuard guard(*this);

    schedulerLockAcquisitions    = 0;
    schedulerLockContentions     = 0;
    schedulerLockSpinRetries     = 0;
    schedulerLockPeakSpinRetries = 0;
}

void FiberScheduler::workerDiagnostics(const FiberWorker& worker, FiberWorkerDiagnostics& diagnostics) const
{
    LockGuard guard(*this);

    size_t readyFibersForWorker = 0;
    if (worker.localQueueScheduler == this)
    {
        if (worker.localDeque != nullptr)
        {
            const size_t top     = fiberAtomicLoadSize(worker.localDequeTop);
            const size_t bottom  = fiberAtomicLoadSize(worker.localDequeBottom);
            readyFibersForWorker = bottom >= top ? bottom - top : 0;
        }
        else
        {
            readyFibersForWorker = worker.localReadyFibers;
        }
    }

    diagnostics.readyFibers        = readyFibersForWorker;
    diagnostics.readyPeakFibers    = worker.localReadyPeakFibers;
    diagnostics.dequeCapacity      = worker.localDequeCapacity;
    diagnostics.spilledFibers      = worker.localSpilledFibers;
    diagnostics.stealAttempts      = worker.stealAttempts;
    diagnostics.stealVictimProbes  = worker.stealVictimProbes;
    diagnostics.stolenFibers       = worker.stolenFibers;
    diagnostics.stolenBatches      = worker.stolenBatches;
    diagnostics.stolenBatchPeak    = worker.stolenBatchPeak;
    diagnostics.failedSteals       = worker.failedSteals;
    diagnostics.runAttempts        = worker.runAttempts;
    diagnostics.idlePolls          = worker.idlePolls;
    diagnostics.idleSpinIterations = fiberAtomicLoadSize(worker.idleSpinIterations);
    diagnostics.parkAttempts       = worker.parkAttempts;
    diagnostics.parkedWakeups      = worker.parkedWakeups;
    diagnostics.executedFibers     = worker.executedFibers;
    diagnostics.completedFibers    = worker.completedFibers;
    diagnostics.yieldedFibers      = worker.yieldedFibers;
    diagnostics.waitingFibers      = worker.waitingFibers;
}

void FiberScheduler::workerDiagnostics(Span<FiberWorker> workers, FiberWorkerDiagnostics& diagnostics) const
{
    LockGuard guard(*this);

    diagnostics = {};
    for (const FiberWorker& worker : workers)
    {
        size_t readyFibersForWorker = 0;
        if (worker.localQueueScheduler == this)
        {
            if (worker.localDeque != nullptr)
            {
                const size_t top     = fiberAtomicLoadSize(worker.localDequeTop);
                const size_t bottom  = fiberAtomicLoadSize(worker.localDequeBottom);
                readyFibersForWorker = bottom >= top ? bottom - top : 0;
            }
            else
            {
                readyFibersForWorker = worker.localReadyFibers;
            }
        }
        diagnostics.readyFibers += readyFibersForWorker;
        diagnostics.readyPeakFibers += worker.localReadyPeakFibers;
        diagnostics.dequeCapacity += worker.localDequeCapacity;
        diagnostics.spilledFibers += worker.localSpilledFibers;
        diagnostics.stealAttempts += worker.stealAttempts;
        diagnostics.stealVictimProbes += worker.stealVictimProbes;
        diagnostics.stolenFibers += worker.stolenFibers;
        diagnostics.stolenBatches += worker.stolenBatches;
        if (worker.stolenBatchPeak > diagnostics.stolenBatchPeak)
        {
            diagnostics.stolenBatchPeak = worker.stolenBatchPeak;
        }
        diagnostics.failedSteals += worker.failedSteals;
        diagnostics.runAttempts += worker.runAttempts;
        diagnostics.idlePolls += worker.idlePolls;
        diagnostics.idleSpinIterations += fiberAtomicLoadSize(worker.idleSpinIterations);
        diagnostics.parkAttempts += worker.parkAttempts;
        diagnostics.parkedWakeups += worker.parkedWakeups;
        diagnostics.executedFibers += worker.executedFibers;
        diagnostics.completedFibers += worker.completedFibers;
        diagnostics.yieldedFibers += worker.yieldedFibers;
        diagnostics.waitingFibers += worker.waitingFibers;
    }
}

void FiberScheduler::resetWorkerDiagnostics(FiberWorker& worker)
{
    LockGuard guard(*this);
    if (worker.localDeque != nullptr)
    {
        const size_t top            = fiberAtomicLoadSize(worker.localDequeTop);
        const size_t bottom         = fiberAtomicLoadSize(worker.localDequeBottom);
        worker.localReadyPeakFibers = bottom >= top ? bottom - top : 0;
    }
    else
    {
        worker.localReadyPeakFibers = worker.localReadyFibers;
    }
    worker.localSpilledFibers = 0;
    worker.stealAttempts      = 0;
    worker.stealVictimProbes  = 0;
    worker.stolenFibers       = 0;
    worker.stolenBatches      = 0;
    worker.stolenBatchPeak    = 0;
    worker.failedSteals       = 0;
    worker.runAttempts        = 0;
    worker.idlePolls          = 0;
    fiberAtomicStoreSize(worker.idleSpinIterations, 0);
    worker.parkAttempts    = 0;
    worker.parkedWakeups   = 0;
    worker.executedFibers  = 0;
    worker.completedFibers = 0;
    worker.yieldedFibers   = 0;
    worker.waitingFibers   = 0;
}

void FiberScheduler::resetWorkerDiagnostics(Span<FiberWorker> workers)
{
    LockGuard guard(*this);
    for (FiberWorker& worker : workers)
    {
        if (worker.localDeque != nullptr)
        {
            const size_t top            = fiberAtomicLoadSize(worker.localDequeTop);
            const size_t bottom         = fiberAtomicLoadSize(worker.localDequeBottom);
            worker.localReadyPeakFibers = bottom >= top ? bottom - top : 0;
        }
        else
        {
            worker.localReadyPeakFibers = worker.localReadyFibers;
        }
        worker.localSpilledFibers = 0;
        worker.stealAttempts      = 0;
        worker.stealVictimProbes  = 0;
        worker.stolenFibers       = 0;
        worker.stolenBatches      = 0;
        worker.stolenBatchPeak    = 0;
        worker.failedSteals       = 0;
        worker.runAttempts        = 0;
        worker.idlePolls          = 0;
        fiberAtomicStoreSize(worker.idleSpinIterations, 0);
        worker.parkAttempts    = 0;
        worker.parkedWakeups   = 0;
        worker.executedFibers  = 0;
        worker.completedFibers = 0;
        worker.yieldedFibers   = 0;
        worker.waitingFibers   = 0;
    }
}

void FiberScheduler::notifyReadyWorkUnlocked()
{
    if (workerPool != nullptr)
    {
        workerPool->wakeOneWorker();
    }
}

void FiberScheduler::pushReadyUnlocked(FiberTask& task)
{
    task.nextReady     = nullptr;
    task.previousReady = nullptr;
    fiberAtomicFetchAddSize(readyFibers, 1);
    if (readyTail == nullptr)
    {
        readyHead = &task;
        readyTail = &task;
    }
    else
    {
        readyTail->nextReady = &task;
        readyTail            = &task;
    }
    notifyReadyWorkUnlocked();
}

void FiberScheduler::pushReadyUnlocked(FiberTask& task, FiberWorker* preferredWorker)
{
    if (preferredWorker != nullptr and preferredWorker->localSchedulingActive and
        preferredWorker->localQueueScheduler == this)
    {
        const bool isDequeOwnedByCurrentWorker =
            preferredWorker->localDeque != nullptr and currentWorkerFor(*this) == preferredWorker;
        if (preferredWorker->localDeque == nullptr or isDequeOwnedByCurrentWorker)
        {
            pushWorkerReadyUnlocked(*preferredWorker, task);
            return;
        }
    }
    pushReadyUnlocked(task);
}

bool FiberScheduler::tryPushWorkerReadyDeque(FiberWorker& worker, FiberTask& task)
{
    SC_FIBERS_ASSERT_RELEASE(worker.localDeque != nullptr);
    const size_t top    = fiberAtomicLoadSize(worker.localDequeTop);
    const size_t bottom = fiberAtomicLoadSize(worker.localDequeBottom);
    // `top` is advanced by thieves, `bottom` by the owner. Full/empty checks use the published indices only; slots can
    // contain stale pointers after pop/steal and are overwritten by the next owning push.
    if (bottom - top >= worker.localDequeCapacity)
    {
        return false;
    }

    const size_t tailIndex       = bottom % worker.localDequeCapacity;
    worker.localDeque[tailIndex] = &task;
    task.nextReady               = nullptr;
    task.previousReady           = nullptr;
    // Count the ready task before publishing `bottom`; after `bottom` advances, thieves can immediately own the slot.
    fiberAtomicFetchAddSize(readyFibers, 1);
    // Publish the task pointer before advancing bottom; thieves acquire bottom before reading the slot.
    fiberAtomicStoreSize(worker.localDequeBottom, bottom + 1);
    const size_t readyFibersForWorker = bottom + 1 - top;
    if (readyFibersForWorker > worker.localReadyPeakFibers)
    {
        worker.localReadyPeakFibers = readyFibersForWorker;
    }
    notifyReadyWorkUnlocked();
    return true;
}

void FiberScheduler::pushWorkerReadyUnlocked(FiberWorker& worker, FiberTask& task)
{
    if (worker.localDeque != nullptr)
    {
        if (not tryPushWorkerReadyDeque(worker, task))
        {
            worker.localSpilledFibers += 1;
            pushReadyUnlocked(task);
        }
        return;
    }

    task.nextReady     = nullptr;
    task.previousReady = worker.localReadyTail;
    fiberAtomicFetchAddSize(readyFibers, 1);
    worker.localReadyFibers += 1;
    if (worker.localReadyFibers > worker.localReadyPeakFibers)
    {
        worker.localReadyPeakFibers = worker.localReadyFibers;
    }
    if (worker.localReadyTail == nullptr)
    {
        worker.localReadyHead = &task;
        worker.localReadyTail = &task;
    }
    else
    {
        worker.localReadyTail->nextReady = &task;
        worker.localReadyTail            = &task;
    }
    notifyReadyWorkUnlocked();
}

FiberTask* FiberScheduler::popReadyUnlocked()
{
    FiberTask* task = readyHead;
    if (task == nullptr)
    {
        return nullptr;
    }

    readyHead           = task->nextReady;
    task->nextReady     = nullptr;
    task->previousReady = nullptr;
    SC_FIBERS_ASSERT_RELEASE(fiberAtomicLoadSize(readyFibers) > 0);
    fiberAtomicFetchSubSize(readyFibers, 1);
    if (readyHead == nullptr)
    {
        readyTail = nullptr;
    }
    return task;
}

FiberTask* FiberScheduler::popReadyUnlocked(FiberWorker& worker, Span<FiberWorker> stealWorkers)
{
    FiberTask* task = popWorkerReadyUnlocked(worker);
    if (task != nullptr)
    {
        return task;
    }

    task = popReadyUnlocked();
    if (task != nullptr)
    {
        return task;
    }

    return stealReadyUnlocked(worker, stealWorkers);
}

FiberTask* FiberScheduler::popWorkerReadyUnlocked(FiberWorker& worker)
{
    if (worker.localDeque != nullptr)
    {
        size_t bottom = fiberAtomicLoadSize(worker.localDequeBottom);
        if (bottom == fiberAtomicLoadSize(worker.localDequeTop))
        {
            return nullptr;
        }

        // The owner reserves the tail slot by moving `bottom` first, then races thieves only if this was the last item.
        bottom -= 1;
        fiberAtomicStoreSize(worker.localDequeBottom, bottom);
        fiberAtomicSequentialFence();

        size_t top = fiberAtomicLoadSize(worker.localDequeTop);
        if (top > bottom)
        {
            fiberAtomicStoreSize(worker.localDequeBottom, top);
            return nullptr;
        }

        const size_t tailIndex = bottom % worker.localDequeCapacity;
        FiberTask*   task      = worker.localDeque[tailIndex];
        SC_FIBERS_ASSERT_RELEASE(task != nullptr);
        if (top == bottom)
        {
            size_t expectedTop = top;
            if (not fiberAtomicCompareExchangeSize(worker.localDequeTop, expectedTop, top + 1))
            {
                task = nullptr;
            }
            fiberAtomicStoreSize(worker.localDequeBottom, top + 1);
        }
        if (task != nullptr)
        {
            SC_FIBERS_ASSERT_RELEASE(fiberAtomicLoadSize(readyFibers) > 0);
            fiberAtomicFetchSubSize(readyFibers, 1);
        }
        return task;
    }

    FiberTask* task = worker.localReadyTail;
    if (task == nullptr)
    {
        return nullptr;
    }

    worker.localReadyTail = task->previousReady;
    task->previousReady   = nullptr;
    task->nextReady       = nullptr;
    SC_FIBERS_ASSERT_RELEASE(fiberAtomicLoadSize(readyFibers) > 0);
    SC_FIBERS_ASSERT_RELEASE(worker.localReadyFibers > 0);
    fiberAtomicFetchSubSize(readyFibers, 1);
    worker.localReadyFibers -= 1;
    if (worker.localReadyTail == nullptr)
    {
        worker.localReadyHead = nullptr;
    }
    else
    {
        worker.localReadyTail->nextReady = nullptr;
    }
    return task;
}

FiberTask* FiberScheduler::stealWorkerReadyUnlocked(FiberWorker& worker)
{
    if (worker.localDeque != nullptr)
    {
        size_t top = fiberAtomicLoadSize(worker.localDequeTop);
        fiberAtomicSequentialFence();
        const size_t bottom = fiberAtomicLoadSize(worker.localDequeBottom);
        if (top >= bottom)
        {
            return nullptr;
        }

        const size_t headIndex = top % worker.localDequeCapacity;
        FiberTask*   task      = worker.localDeque[headIndex];
        SC_FIBERS_ASSERT_RELEASE(task != nullptr);
        // A thief owns the head slot only after advancing `top`; failed CAS means the owner or another thief won.
        if (not fiberAtomicCompareExchangeSize(worker.localDequeTop, top, top + 1))
        {
            return nullptr;
        }
        SC_FIBERS_ASSERT_RELEASE(fiberAtomicLoadSize(readyFibers) > 0);
        fiberAtomicFetchSubSize(readyFibers, 1);
        return task;
    }

    FiberTask* task = worker.localReadyHead;
    if (task == nullptr)
    {
        return nullptr;
    }

    worker.localReadyHead = task->nextReady;
    task->nextReady       = nullptr;
    task->previousReady   = nullptr;
    SC_FIBERS_ASSERT_RELEASE(fiberAtomicLoadSize(readyFibers) > 0);
    SC_FIBERS_ASSERT_RELEASE(worker.localReadyFibers > 0);
    fiberAtomicFetchSubSize(readyFibers, 1);
    worker.localReadyFibers -= 1;
    if (worker.localReadyHead == nullptr)
    {
        worker.localReadyTail = nullptr;
    }
    else
    {
        worker.localReadyHead->previousReady = nullptr;
    }
    return task;
}

FiberTask* FiberScheduler::stealReadyUnlocked(FiberWorker& worker, Span<FiberWorker> stealWorkers)
{
    static constexpr size_t NumVictimSamples   = 2;
    static constexpr size_t MaxStolenBatchSize = 4;

    const size_t numWorkers = stealWorkers.sizeInElements();
    if (numWorkers <= 1)
    {
        return nullptr;
    }

    worker.stealAttempts += 1;

    const size_t numVictimsToSample = numWorkers - 1 < NumVictimSamples ? numWorkers - 1 : NumVictimSamples;
    const size_t startIndex         = worker.stealCursor % numWorkers;
    size_t       sampledVictims     = 0;
    size_t       cursorAdvance      = 0;

    FiberWorker* victimWithMostReady = nullptr;
    size_t       mostReadyFibers     = 0;
    while (sampledVictims < numVictimsToSample and cursorAdvance < numWorkers)
    {
        FiberWorker& victim = stealWorkers[(startIndex + cursorAdvance) % numWorkers];
        cursorAdvance += 1;
        if (&victim == &worker)
        {
            continue;
        }

        sampledVictims += 1;
        worker.stealVictimProbes += 1;
        if (victim.localQueueScheduler != this)
        {
            continue;
        }

        size_t victimReadyFibers = victim.localReadyFibers;
        if (victim.localDeque != nullptr)
        {
            const size_t top    = fiberAtomicLoadSize(victim.localDequeTop);
            const size_t bottom = fiberAtomicLoadSize(victim.localDequeBottom);
            victimReadyFibers   = bottom >= top ? bottom - top : 0;
        }
        if (victimReadyFibers > mostReadyFibers)
        {
            victimWithMostReady = &victim;
            mostReadyFibers     = victimReadyFibers;
        }
    }
    worker.stealCursor = (startIndex + cursorAdvance) % numWorkers;
    if (victimWithMostReady == nullptr)
    {
        worker.failedSteals += 1;
        return nullptr;
    }

    FiberTask*   stolenTasks[MaxStolenBatchSize] = {};
    size_t       numStolenTasks                  = 0;
    const size_t requestedBatchSize =
        worker.localDeque == nullptr ? 1
                                     : (mostReadyFibers < MaxStolenBatchSize ? mostReadyFibers : MaxStolenBatchSize);
    while (numStolenTasks < requestedBatchSize)
    {
        FiberTask* stolenTask = stealWorkerReadyUnlocked(*victimWithMostReady);
        if (stolenTask == nullptr)
        {
            break;
        }
        stolenTask->preferredWorker = &worker;
        stolenTasks[numStolenTasks] = stolenTask;
        numStolenTasks += 1;
    }

    if (numStolenTasks == 0)
    {
        worker.failedSteals += 1;
        return nullptr;
    }

    worker.stolenFibers += numStolenTasks;
    worker.stolenBatches += 1;
    if (numStolenTasks > worker.stolenBatchPeak)
    {
        worker.stolenBatchPeak = numStolenTasks;
    }

    // Keep the source deque's FIFO steal order when the thief later pops its LIFO local deque.
    for (size_t taskIndex = numStolenTasks; taskIndex > 1; --taskIndex)
    {
        FiberTask& task = *stolenTasks[taskIndex - 1];
        if (not tryPushWorkerReadyDeque(worker, task))
        {
            worker.localSpilledFibers += 1;
            pushReadyUnlocked(task);
        }
    }
    return stolenTasks[0];
}

Result FiberScheduler::runReadyTask(FiberTask& task)
{
    FiberWorker worker;
    return runReadyTask(task, worker);
}

Result FiberScheduler::runReadyTask(FiberTask& task, FiberWorker& worker)
{
    FiberWorker* previousWorker = currentWorkerFor(*this);
    if (previousWorker != nullptr)
    {
        SC_FIBERS_ASSERT_RELEASE(previousWorker->workerTask == nullptr);
        currentFiberWorker = nullptr;
        previousWorker     = nullptr;
    }

    SC_TRY(worker.begin(*this));
    previousWorker     = currentFiberWorker;
    currentFiberWorker = &worker;

    {
        LockGuard guard(*this);
        SC_FIBERS_ASSERT_RELEASE(task.status() == FiberTaskStatus::Running);
        SC_FIBERS_ASSERT_RELEASE(task.runningWorker == nullptr);
        worker.workerTask  = &task;
        task.runningWorker = &worker;
        worker.executedFibers += 1;
    }
    trace(FiberTraceEventType::TaskStarted, &task, &worker);
    FiberContextOperations::switchTo(worker.rootContext(), task.context());
    FiberWorker*     preferredReadyWorker = nullptr;
    FiberTaskClass*  completedTaskClass   = nullptr;
    FiberStackClass* completedStackClass  = nullptr;
    Span<char>       completedStackMemory;
    {
        LockGuard guard(*this);
        if (not preparePreferredWorkerReadyPublishUnlocked(task, worker, preferredReadyWorker))
        {
            publishSuspensionUnlocked(task);
            if (task.status() == FiberTaskStatus::Completing)
            {
                fiberTaskStatusStore(task.taskStatus, FiberTaskStatus::Completed);
                FiberTaskPool* originPool = task.originPool;
                task.originPool           = nullptr;
                completedTaskClass        = task.originTaskClass;
                completedStackClass       = task.originStackClass;
                completedStackMemory      = task.originStackMemory;
                task.originTaskClass      = nullptr;
                task.originStackClass     = nullptr;
                task.originStackMemory    = {};
                if (originPool != nullptr)
                {
                    FiberCounter* availableCounter = originPool->popAvailabilityWaiterForNotification();
                    if (availableCounter != nullptr)
                    {
                        SC_FIBERS_TRUST_RESULT(doneUnlocked(*availableCounter));
                    }
                }
            }
        }
        if (task.runningWorker == &worker)
        {
            task.runningWorker = nullptr;
        }
        if (worker.workerTask == &task)
        {
            worker.workerTask = nullptr;
        }
    }
    if (completedStackClass != nullptr)
    {
        FiberStack completedStack(completedStackMemory);
        SC_FIBERS_TRUST_RESULT(completedStackClass->release(completedStack));
    }
    if (completedTaskClass != nullptr)
    {
        SC_FIBERS_TRUST_RESULT(completedTaskClass->release(task));
    }
    if (preferredReadyWorker != nullptr and not tryPushWorkerReadyDeque(*preferredReadyWorker, task))
    {
        LockGuard guard(*this);
        preferredReadyWorker->localSpilledFibers += 1;
        pushReadyUnlocked(task);
    }
    currentFiberWorker = previousWorker;
    worker.end();
    return Result(true);
}

bool FiberScheduler::preparePreferredWorkerReadyPublishUnlocked(FiberTask& task, FiberWorker& worker,
                                                                FiberWorker*& outPreferredWorker)
{
    outPreferredWorker = nullptr;
    if (task.suspendAction != FiberTaskSuspendAction::Ready)
    {
        return false;
    }

    FiberWorker* preferredWorker = task.preferredWorker;
    if (preferredWorker != &worker or preferredWorker->localDeque == nullptr or
        not preferredWorker->localSchedulingActive or preferredWorker->localQueueScheduler != this)
    {
        return false;
    }

    task.suspendAction        = FiberTaskSuspendAction::None;
    task.suspendCounter       = nullptr;
    task.suspendInterruptible = false;
    fiberTaskStatusStore(task.taskStatus, FiberTaskStatus::Ready);
    outPreferredWorker = preferredWorker;
    return true;
}

void FiberScheduler::publishSuspensionUnlocked(FiberTask& task)
{
    if (task.suspendAction == FiberTaskSuspendAction::None)
    {
        return;
    }

    FiberTaskSuspendAction suspendAction = task.suspendAction;
    FiberCounter*          counter       = task.suspendCounter;
    const bool             interruptible = task.suspendInterruptible;

    task.suspendAction        = FiberTaskSuspendAction::None;
    task.suspendCounter       = nullptr;
    task.suspendInterruptible = false;

    if (suspendAction == FiberTaskSuspendAction::Ready)
    {
        fiberTaskStatusStore(task.taskStatus, FiberTaskStatus::Ready);
        pushReadyUnlocked(task, task.preferredWorker);
        return;
    }

    SC_FIBERS_ASSERT_RELEASE(suspendAction == FiberTaskSuspendAction::CounterWait);
    SC_FIBERS_ASSERT_RELEASE(counter != nullptr);
    if (counter->counterValue == 0 or (interruptible and task.isCancellationRequested()))
    {
        fiberTaskStatusStore(task.taskStatus, FiberTaskStatus::Ready);
        pushReadyUnlocked(task, task.preferredWorker);
        return;
    }

    fiberTaskStatusStore(task.taskStatus, FiberTaskStatus::Waiting);
    task.nextWaiting          = nullptr;
    task.waitingCounter       = counter;
    task.suspendInterruptible = interruptible;
    if (counter->waitingTail == nullptr)
    {
        counter->waitingHead = &task;
        counter->waitingTail = &task;
    }
    else
    {
        counter->waitingTail->nextWaiting = &task;
        counter->waitingTail              = &task;
    }
}

void FiberScheduler::finishCurrentTask(FiberTask& task, Result result)
{
    FiberWorker* worker = static_cast<FiberWorker*>(task.runningWorker);
    SC_FIBERS_ASSERT_RELEASE(worker != nullptr);
    SC_FIBERS_ASSERT_RELEASE(worker->workerTask == &task);
    SC_FIBERS_ASSERT_RELEASE(worker->scheduler() == this);
    {
        LockGuard guard(*this);
        SC_FIBERS_ASSERT_RELEASE(activeFibers > 0);

        task.taskResult = result;
        fiberTaskStatusStore(task.taskStatus, FiberTaskStatus::Completing);
        task.cancellationToken = FiberCancellationToken();
        task.suspendAction     = FiberTaskSuspendAction::None;
        task.suspendCounter    = nullptr;
        task.preferredWorker   = nullptr;
        activeFibers -= 1;
        worker->completedFibers += 1;
        if (activeFibers == 0 and workerPool != nullptr)
        {
            // Every parked worker must recheck the now-empty scheduler before the pool can join.
            workerPool->wakeAllWorkers();
        }
        unlinkActiveUnlocked(task);
        worker->workerTask = nullptr;
        if (task.stackOwner != nullptr)
        {
            static_cast<FiberVirtualStackInternal*>(task.stackOwner)->releaseStack();
            task.stackOwner = nullptr;
        }

        if (task.completionCounter != nullptr)
        {
            SC_FIBERS_TRUST_RESULT(doneUnlocked(*task.completionCounter));
        }
    }
    trace(FiberTraceEventType::TaskCompleted, &task, worker, result ? 1 : 0);
}

Result FiberScheduler::cancelTaskUnlocked(FiberTask& task)
{
    task.cancelRequested = true;
    if (task.status() == FiberTaskStatus::Waiting and task.waitingCounter != nullptr and task.suspendInterruptible)
    {
        SC_FIBERS_ASSERT_RELEASE(removeCounterWaiterUnlocked(*task.waitingCounter, task));
        task.waitingCounter       = nullptr;
        task.suspendInterruptible = false;
        fiberTaskStatusStore(task.taskStatus, FiberTaskStatus::Ready);
        pushReadyUnlocked(task, task.preferredWorker);
    }
    return Result(true);
}

void FiberScheduler::linkActiveUnlocked(FiberTask& task)
{
    task.nextActive     = activeHead;
    task.previousActive = nullptr;
    if (activeHead != nullptr)
    {
        activeHead->previousActive = &task;
    }
    activeHead = &task;
}

void FiberScheduler::unlinkActiveUnlocked(FiberTask& task)
{
    if (task.previousActive != nullptr)
    {
        task.previousActive->nextActive = task.nextActive;
    }
    else
    {
        SC_FIBERS_ASSERT_RELEASE(activeHead == &task);
        activeHead = task.nextActive;
    }
    if (task.nextActive != nullptr)
    {
        task.nextActive->previousActive = task.previousActive;
    }
    task.nextActive     = nullptr;
    task.previousActive = nullptr;
}

bool FiberScheduler::removeCounterWaiterUnlocked(FiberCounter& counter, FiberTask& task)
{
    FiberTask* previous = nullptr;
    FiberTask* current  = counter.waitingHead;
    while (current != nullptr)
    {
        FiberTask* next = current->nextWaiting;
        if (current == &task)
        {
            if (previous == nullptr)
            {
                counter.waitingHead = next;
            }
            else
            {
                previous->nextWaiting = next;
            }
            if (counter.waitingTail == &task)
            {
                counter.waitingTail = previous;
            }
            task.nextWaiting = nullptr;
            return true;
        }
        previous = current;
        current  = next;
    }
    return false;
}

void FiberScheduler::wakeCounterWaitersUnlocked(FiberCounter& counter)
{
    FiberTask* task = counter.waitingHead;
    while (task != nullptr)
    {
        FiberTask* next            = task->nextWaiting;
        task->nextWaiting          = nullptr;
        task->waitingCounter       = nullptr;
        task->suspendInterruptible = false;
        fiberTaskStatusStore(task->taskStatus, FiberTaskStatus::Ready);
        pushReadyUnlocked(*task, task->preferredWorker);
        task = next;
    }

    counter.waitingHead = nullptr;
    counter.waitingTail = nullptr;
}

Result FiberContextOperations::captureCurrent(FiberContext& context)
{
#if SC_PLATFORM_WINDOWS
    RtlCaptureContext(&context.platform.context);
#endif
    context.stackBottom   = nullptr;
    context.stackSize     = 0;
    context.asanFakeStack = nullptr;
    context.initialized   = true;
    return Result(true);
}

Result FiberContextOperations::create(FiberContext& context, Span<char> stack, FiberContextEntry entry, void* userData)
{
    if (entry == nullptr)
    {
        return Result::Error("FiberContext entry is null");
    }
    FiberStack fiberStack(stack);
    if (not fiberStack.isUsable())
    {
        return Result::Error("FiberContext stack is too small");
    }

    const size_t usableStackSize = fiberStack.usableSizeInBytes();
    char*        stackTop        = stack.data() + stack.sizeInBytes();
    stackTop                     = static_cast<char*>(alignDown(stackTop, FiberStackAlignment));

#if SC_PLATFORM_WINDOWS
    RtlCaptureContext(&context.platform.context);
#if defined(_M_X64)
    stackTop -= 40;
    *reinterpret_cast<void**>(stackTop) = nullptr;
    context.platform.context.Rip        = reinterpret_cast<DWORD64>(&fiberContextTrampoline);
    context.platform.context.Rsp        = reinterpret_cast<DWORD64>(stackTop);
    context.platform.context.Rcx        = reinterpret_cast<DWORD64>(entry);
    context.platform.context.Rdx        = reinterpret_cast<DWORD64>(userData);
#elif defined(_M_ARM64)
    context.platform.context.Pc = reinterpret_cast<DWORD64>(&fiberContextTrampoline);
    context.platform.context.Sp = reinterpret_cast<DWORD64>(stackTop);
    context.platform.context.X0 = reinterpret_cast<DWORD64>(entry);
    context.platform.context.X1 = reinterpret_cast<DWORD64>(userData);
#else
#error "Unsupported Windows Fibers context architecture"
#endif
#elif SC_PLATFORM_INTEL && SC_PLATFORM_64_BIT
    stackTop -= 16;
    *reinterpret_cast<void**>(stackTop) = reinterpret_cast<void*>(&SC_fiberContextTrampoline);
    context.platform.rsp                = stackTop;
    context.platform.r12                = reinterpret_cast<void*>(entry);
    context.platform.r13                = userData;
#elif SC_PLATFORM_ARM64
    context.platform.sp  = stackTop;
    context.platform.x19 = reinterpret_cast<void*>(entry);
    context.platform.x20 = userData;
    context.platform.x30 = reinterpret_cast<void*>(&SC_fiberContextTrampoline);
#endif

    context.stackBottom   = stack.data();
    context.stackSize     = usableStackSize;
    context.asanFakeStack = nullptr;
    context.initialized   = true;
    return Result(true);
}

void FiberContextOperations::switchTo(FiberContext& from, FiberContext& to)
{
    SC_FIBERS_ASSERT_RELEASE(from.initialized);
    SC_FIBERS_ASSERT_RELEASE(to.initialized);

    fiberContextStartSwitch(from, to);
#if SC_PLATFORM_WINDOWS
    from.platform.restoring = 0;
    RtlCaptureContext(&from.platform.context);
    if (from.platform.restoring == 0)
    {
        from.platform.restoring = 1;
        RtlRestoreContext(&to.platform.context, nullptr);
    }
    from.platform.restoring = 0;
#else
    SC_fiberContextSwitch(&from.platform, &to.platform);
#endif
    fiberContextFinishSwitch(from.asanFakeStack);
}
} // namespace SC
