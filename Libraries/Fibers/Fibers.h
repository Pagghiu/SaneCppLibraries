// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once

#include "../Common/CompilerMacrosExport.h"
#ifndef SC_EXPORT_LIBRARY_FIBERS
#define SC_EXPORT_LIBRARY_FIBERS 0
#endif
#define SC_FIBERS_EXPORT SC_COMPILER_LIBRARY_EXPORT(SC_EXPORT_LIBRARY_FIBERS)

#include "../Common/AlignedStorage.h"
#include "../Common/Assert.h"
#include "../Common/Function.h"
#include "../Common/OpaqueObject.h"
#include "../Common/PlatformMacrosInstructionSet.h"
#include "../Common/PlatformMacrosType.h"
#include "../Common/PrimitiveDefinitions.h"
#include "../Common/Result.h"
#include "../Common/Span.h"

//! @defgroup group_fibers Fibers
//! Experimental stackful cooperative task runtime.

//! @addtogroup group_fibers
//! @{
namespace SC
{
SC_DECLARE_ASSERT_PROVIDER(FibersAssert, SC_FIBERS_EXPORT);

#define SC_FIBERS_ASSERT_RELEASE(e)        SC_ASSERT_PROVIDER_RELEASE(SC::FibersAssert, e)
#define SC_FIBERS_ASSERT_DEBUG(e)          SC_ASSERT_PROVIDER_DEBUG(SC::FibersAssert, e)
#define SC_FIBERS_TRUST_RESULT(expression) SC_FIBERS_ASSERT_RELEASE(expression)

struct FiberCounter;
struct FiberAutoResetEvent;
struct FiberContext;
struct FiberEvent;
struct FiberMutex;
struct FiberSemaphore;
struct FiberScheduler;
struct FiberTaskGroup;
struct FiberTaskClass;
struct FiberTaskClassDiagnostics;
struct FiberTaskClassInternal;
struct FiberTaskClassOptions;
struct FiberTaskPool;
struct FiberTaskPoolDiagnostics;
struct FiberTask;
struct FiberVirtualStack;
struct FiberVirtualStackInternal;
struct FiberVirtualStackOptions;
struct FiberStackClass;
struct FiberStackClassDiagnostics;
struct FiberStackClassInternal;
struct FiberStackClassOptions;
struct FiberSchedulerDiagnostics;
struct FiberTraceEvent;
struct FiberTraceHooks;
struct FiberAllocator;
struct FiberAllocatorInterface;
struct FiberAllocatorVirtualOptions;
struct FiberWorker;
struct FiberWorkerPoolWakeEvent;
struct FiberWorkerPool;
struct FiberWorkerPoolOptions;
struct FiberWorkerPoolThreadEntry;
struct FiberWorkerThread;

#if SC_PLATFORM_WINDOWS
static constexpr int FiberContextStorageSize = 2048;
#elif SC_PLATFORM_ARM64
static constexpr int FiberContextStorageSize = 224;
#else
static constexpr int FiberContextStorageSize = 128;
#endif
static constexpr int FiberContextStorageAlignment = 16;
static constexpr int FiberStackAlignment          = 16;
static constexpr int FiberStackMinimumSize        = 4096;

//! Common requested sizes for virtual fiber stack classes.
//! Actual usable and committed size is page-rounded by the active platform.
struct SC_FIBERS_EXPORT FiberStackSize
{
    static constexpr size_t FourKiB      = 4 * 1024;
    static constexpr size_t EightKiB     = 8 * 1024;
    static constexpr size_t ThirtyTwoKiB = 32 * 1024;
    static constexpr size_t SixtyFourKiB = 64 * 1024;
};

//! Caller-owned stack storage used by fiber contexts.
struct SC_FIBERS_EXPORT FiberStack
{
    explicit FiberStack(Span<char> memory);

    [[nodiscard]] Span<char> memory() const;
    [[nodiscard]] size_t     sizeInBytes() const;
    [[nodiscard]] size_t     usableSizeInBytes() const;
    [[nodiscard]] size_t     alignmentWasteInBytes() const;
    [[nodiscard]] bool       isUsable() const;
    void                     fillHighWaterMark();
    [[nodiscard]] size_t     highWaterUsedBytes() const;
    [[nodiscard]] size_t     highWaterUnusedBytes() const;

  private:
    friend struct FiberScheduler;
    friend struct FiberVirtualStack;

    FiberStack(Span<char> memory, void* owner);

    Span<char> stackMemory;
    void*      stackOwner = nullptr;
};

struct FiberVirtualStackDefinition
{
    static constexpr int    Windows   = 64;
    static constexpr int    Apple     = 64;
    static constexpr int    Linux     = 64;
    static constexpr int    Default   = 64;
    static constexpr size_t Alignment = alignof(void*);

    using Object = FiberVirtualStackInternal;
};
using FiberVirtualStackOpaque = OpaqueObject<FiberVirtualStackDefinition>;

//! Options for reserving a virtual-memory-backed fiber stack.
struct SC_FIBERS_EXPORT FiberVirtualStackOptions
{
    size_t usableSizeInBytes = FiberStackSize::SixtyFourKiB;
    bool   guardPage         = true;
};

struct FiberStackClassDefinition
{
    static constexpr int    Windows   = 176;
    static constexpr int    Apple     = 176;
    static constexpr int    Linux     = 176;
    static constexpr int    Default   = 176;
    static constexpr size_t Alignment = alignof(void*);

    using Object = FiberStackClassInternal;
};
using FiberStackClassOpaque = OpaqueObject<FiberStackClassDefinition>;

struct SC_FIBERS_EXPORT FiberStackClassOptions
{
    size_t stackSizeInBytes = FiberStackSize::SixtyFourKiB;
    size_t maxStacks        = 0;
    bool   guardPage        = true;
};

struct SC_FIBERS_EXPORT FiberStackClassDiagnostics
{
    size_t capacity           = 0;
    size_t activeStacks       = 0;
    size_t peakActiveStacks   = 0;
    size_t stackSizeInBytes   = 0;
    size_t guardSizeInBytes   = 0;
    size_t reservedSizeBytes  = 0;
    size_t committedSizeBytes = 0;
    size_t peakCommittedBytes = 0;
    size_t highWaterUsedBytes = 0;
};

//! Virtual-memory-backed fixed-size stack slots with caller-controlled capacity.
struct SC_FIBERS_EXPORT FiberStackClass
{
    FiberStackClass();
    ~FiberStackClass();

    FiberStackClass(const FiberStackClass&)            = delete;
    FiberStackClass& operator=(const FiberStackClass&) = delete;
    FiberStackClass(FiberStackClass&&)                 = delete;
    FiberStackClass& operator=(FiberStackClass&&)      = delete;

    [[nodiscard]] Result reserve(const FiberStackClassOptions& options);
    [[nodiscard]] Result acquire(FiberStack& outStack);
    [[nodiscard]] Result release(FiberStack& stack);
    [[nodiscard]] Result waitForAvailableSlot(FiberScheduler& scheduler);
    void                 release();
    void                 fillHighWaterMarks();
    void                 diagnostics(FiberStackClassDiagnostics& outDiagnostics) const;

    [[nodiscard]] bool   isReserved() const;
    [[nodiscard]] bool   owns(const FiberStack& stack) const;
    [[nodiscard]] size_t capacity() const;
    [[nodiscard]] size_t activeCount() const;

  private:
    friend struct FiberTaskPool;

    FiberStackClassOpaque internal;
};

enum class FiberAllocatorMode : uint8_t
{
    None,
    Fixed,
    Virtual,
    Malloc,
    Polymorphic,
};

struct FiberAllocatorStatistics
{
    size_t numAllocations = 0;
    size_t numReleases    = 0;

    size_t requestedBytesAllocated = 0;
    size_t requestedBytesReleased  = 0;

    size_t bytesInUse     = 0;
    size_t peakBytesInUse = 0;

    size_t numAllocationFailures       = 0;
    size_t lastFailedAllocationSize    = 0;
    size_t largestFailedAllocationSize = 0;
};

struct FiberAllocatorVirtualOptions
{
    size_t reserveBytes       = 0;
    size_t initialCommitBytes = 0;
};

struct FiberAllocatorInterface
{
    virtual void* allocateImpl(const void* owner, size_t numBytes, size_t alignment) = 0;
    virtual void  releaseImpl(void* memory)                                          = 0;
    virtual ~FiberAllocatorInterface() {}
};

//! Explicit allocator for future fiber scheduler storage.
struct SC_FIBERS_EXPORT FiberAllocator
{
    FiberAllocator()                                 = default;
    FiberAllocator(const FiberAllocator&)            = delete;
    FiberAllocator& operator=(const FiberAllocator&) = delete;
    FiberAllocator(FiberAllocator&&)                 = delete;
    FiberAllocator& operator=(FiberAllocator&&)      = delete;
    ~FiberAllocator();

    [[nodiscard]] Result createFixed(Span<char> storage);
    [[nodiscard]] Result createVirtual(FiberAllocatorVirtualOptions options);
    [[nodiscard]] Result createMalloc();
    [[nodiscard]] Result createPolymorphic(FiberAllocatorInterface& customAllocatorInterface);
    [[nodiscard]] Result validateClose() const;
    [[nodiscard]] Result close();

    [[nodiscard]] void* allocate(const void* owner, size_t numBytes, size_t alignment);
    void                release(void* memory);
    static void         releaseFromAnyAllocator(void* memory);

    [[nodiscard]] FiberAllocatorMode       mode() const;
    [[nodiscard]] FiberAllocatorStatistics statistics() const;
    [[nodiscard]] bool                     isOpen() const;

    [[nodiscard]] size_t used() const;
    [[nodiscard]] size_t capacity() const;
    [[nodiscard]] size_t peakUsed() const;
    [[nodiscard]] size_t failedAllocationSize() const;
    [[nodiscard]] size_t reservedBytes() const;
    [[nodiscard]] size_t committedBytes() const;

  private:
    struct BlockHeader;

    Result initializeFixedStorage(Span<char> storage);
    void*  allocateFromBlocks(const void* owner, size_t numBytes, size_t alignment);
    void   releaseBlock(BlockHeader& header);
    bool   ensureCommitted(size_t sizeInBytes);
    void   releaseVirtualMemory();
    void   recordAllocationFailure(size_t numBytes);
    void   resetState();

    FiberAllocatorMode       currentMode = FiberAllocatorMode::None;
    FiberAllocatorStatistics currentStatistics;
    Span<char>               fixedStorage;
    BlockHeader*             firstBlock         = nullptr;
    FiberAllocatorInterface* allocatorInterface = nullptr;

    void*  virtualMemory         = nullptr;
    size_t virtualReservedBytes  = 0;
    size_t virtualCommittedBytes = 0;
};

//! Virtual-memory-backed stack storage with an optional no-access guard page below the stack.
struct SC_FIBERS_EXPORT FiberVirtualStack
{
    FiberVirtualStack();
    ~FiberVirtualStack();

    FiberVirtualStack(const FiberVirtualStack&)            = delete;
    FiberVirtualStack& operator=(const FiberVirtualStack&) = delete;
    FiberVirtualStack(FiberVirtualStack&&)                 = delete;
    FiberVirtualStack& operator=(FiberVirtualStack&&)      = delete;

    Result reserve(const FiberVirtualStackOptions& options);
    void   release();

    [[nodiscard]] FiberStack stack() const;
    [[nodiscard]] Span<char> memory() const;
    [[nodiscard]] size_t     usableSizeInBytes() const;
    [[nodiscard]] size_t     reservedSizeInBytes() const;
    [[nodiscard]] size_t     guardSizeInBytes() const;
    [[nodiscard]] bool       isReserved() const;

  private:
    FiberVirtualStackOpaque internal;
};

//! Execution state of a caller-owned fiber task.
enum class FiberTaskStatus
{
    Invalid,
    Ready,
    Running,
    Waiting,
    Completing,
    Completed
};

enum class FiberTaskSuspendAction
{
    None,
    Ready,
    CounterWait
};

struct FiberCancellationToken;

//! Caller-owned execution agent for running ready fibers on the current OS thread.
struct SC_FIBERS_EXPORT FiberWorker
{
    FiberWorker();
    ~FiberWorker();

    FiberWorker(const FiberWorker&)            = delete;
    FiberWorker& operator=(const FiberWorker&) = delete;

    [[nodiscard]] bool                  isActive() const;
    [[nodiscard]] FiberScheduler*       scheduler();
    [[nodiscard]] const FiberScheduler* scheduler() const;
    [[nodiscard]] FiberTask*            runningTask();
    [[nodiscard]] const FiberTask*      runningTask() const;

  private:
    friend struct FiberWorkerPool;
    friend struct FiberScheduler;

    Result begin(FiberScheduler& fiberScheduler);
    void   end();

    FiberScheduler*  workerScheduler        = nullptr;
    FiberScheduler*  localQueueScheduler    = nullptr;
    FiberTask*       workerTask             = nullptr;
    FiberTask*       localReadyHead         = nullptr;
    FiberTask*       localReadyTail         = nullptr;
    FiberTask*       activeHead             = nullptr;
    FiberTask**      localDeque             = nullptr;
    FiberAllocator*  localDequeAllocator    = nullptr;
    size_t           localReadyFibers       = 0;
    size_t           localDequeCapacity     = 0;
    size_t           localDequeHead         = 0;
    volatile size_t  localDequeTop          = 0;
    volatile size_t  localDequeBottom       = 0;
    size_t           localReadyPeakFibers   = 0;
    size_t           localSpilledFibers     = 0;
    size_t           stealAttempts          = 0;
    size_t           stealVictimProbes      = 0;
    size_t           stolenFibers           = 0;
    size_t           stolenBatches          = 0;
    size_t           stolenBatchPeak        = 0;
    size_t           failedSteals           = 0;
    size_t           stealCursor            = 0;
    volatile size_t  runAttempts            = 0;
    volatile size_t  idlePolls              = 0;
    volatile size_t  idleSpinIterations     = 0;
    size_t           parkAttempts           = 0;
    size_t           parkedWakeups          = 0;
    volatile size_t  executedFibers         = 0;
    size_t           completedFibers        = 0;
    volatile size_t  yieldedFibers          = 0;
    size_t           waitingFibers          = 0;
    volatile int32_t activeRegistryLock     = 0;
    bool             workerActive           = false;
    bool             localSchedulingActive  = false;
    bool             stealCursorInitialized = false;

    AlignedStorage<FiberContextStorageSize, FiberContextStorageAlignment> rootContextStorage;

    [[nodiscard]] FiberContext& rootContext();
};

#if SC_PLATFORM_WINDOWS
static constexpr int FiberWorkerThreadStorageSize = sizeof(void*);
#else
static constexpr int FiberWorkerThreadStorageSize = sizeof(void*) * 2;
#endif
static constexpr int FiberWorkerThreadStorageAlignment = alignof(void*);

//! Caller-owned OS thread storage used by FiberWorkerPool.
struct SC_FIBERS_EXPORT FiberWorkerThread
{
    FiberWorkerThread();
    ~FiberWorkerThread();

    FiberWorkerThread(const FiberWorkerThread&)            = delete;
    FiberWorkerThread& operator=(const FiberWorkerThread&) = delete;

    [[nodiscard]] bool   wasStarted() const;
    [[nodiscard]] Result result() const;

  private:
    friend struct FiberWorkerPool;
    friend struct FiberWorkerPoolThreadEntry;

    AlignedStorage<FiberWorkerThreadStorageSize, FiberWorkerThreadStorageAlignment> threadStorage;

    FiberWorkerPool* pool         = nullptr;
    size_t           workerIndex  = 0;
    uint64_t         affinityMask = 0;
    uint8_t          priority     = 0;
    Result           threadResult = Result(true);
    bool             started      = false;

    Result startThread();
    Result joinThread();
    Result runThreadEntry();
    Result applyThreadPolicy();
};

enum class FiberWorkerThreadPriority : uint8_t
{
    Default,
    Low,
    Normal,
    High,
};

struct SC_FIBERS_EXPORT FiberWorkerPoolOptions
{
    FiberAllocator*           dequeAllocator         = nullptr;
    size_t                    dequeCapacityPerWorker = 0;
    FiberAllocator*           injectionAllocator     = nullptr;
    size_t                    injectionCapacity      = 0;
    size_t                    idleSpinAttempts       = 32;
    Span<const uint64_t>      affinityMasks;
    FiberWorkerThreadPriority threadPriority = FiberWorkerThreadPriority::Default;
};

struct SC_FIBERS_EXPORT FiberWorkerDiagnostics
{
    size_t readyFibers        = 0;
    size_t readyPeakFibers    = 0;
    size_t dequeCapacity      = 0;
    size_t spilledFibers      = 0;
    size_t stealAttempts      = 0;
    size_t stealVictimProbes  = 0;
    size_t stolenFibers       = 0;
    size_t stolenBatches      = 0;
    size_t stolenBatchPeak    = 0;
    size_t failedSteals       = 0;
    size_t runAttempts        = 0;
    size_t idlePolls          = 0;
    size_t idleSpinIterations = 0;
    size_t parkAttempts       = 0;
    size_t parkedWakeups      = 0;
    size_t executedFibers     = 0;
    size_t completedFibers    = 0;
    size_t yieldedFibers      = 0;
    size_t waitingFibers      = 0;
};

struct SC_FIBERS_EXPORT FiberSchedulerDiagnostics
{
    size_t readyFibers         = 0;
    size_t activeFibers        = 0;
    size_t injectionCapacity   = 0;
    size_t injectionReady      = 0;
    size_t injectionPeak       = 0;
    size_t injectionSpills     = 0;
    size_t lockAcquisitions    = 0;
    size_t lockContentions     = 0;
    size_t lockSpinRetries     = 0;
    size_t lockPeakSpinRetries = 0;
};

enum class FiberTraceEventType : uint8_t
{
    TaskStarted,
    TaskYielded,
    TaskWaiting,
    TaskCompleted,
};

struct SC_FIBERS_EXPORT FiberTraceEvent
{
    FiberTraceEventType type      = FiberTraceEventType::TaskStarted;
    FiberScheduler*     scheduler = nullptr;
    FiberWorker*        worker    = nullptr;
    FiberTask*          task      = nullptr;
    size_t              value     = 0;
};

struct SC_FIBERS_EXPORT FiberTraceHooks
{
    using Callback = void (*)(void* userData, const FiberTraceEvent& event);

    Callback callback = nullptr;
    void*    userData = nullptr;
};

//! No-allocation OS-thread-owning worker pool using caller-provided worker and thread storage.
struct SC_FIBERS_EXPORT FiberWorkerPool
{
    FiberWorkerPool();
    ~FiberWorkerPool();

    FiberWorkerPool(const FiberWorkerPool&)            = delete;
    FiberWorkerPool& operator=(const FiberWorkerPool&) = delete;

    Result start(FiberScheduler& scheduler, Span<FiberWorker> workerStorage, Span<FiberWorkerThread> threadStorage);
    Result start(FiberScheduler& scheduler, Span<FiberWorker> workerStorage, Span<FiberWorkerThread> threadStorage,
                 const FiberWorkerPoolOptions& options);
    Result requestStop();
    Result join();
    Result shutdown();

    [[nodiscard]] bool   isRunning() const;
    [[nodiscard]] size_t workerCount() const;
    //! Workers currently blocked in the pool wake wait.
    [[nodiscard]] size_t parkedWorkerCount() const;

    struct WakeEventDefinition
    {
        static constexpr int Windows = sizeof(void*) * 16;
        static constexpr int Apple   = sizeof(void*) * 16;
        static constexpr int Linux   = sizeof(void*) * 16;
        static constexpr int Default = Linux;

        static constexpr size_t Alignment = alignof(void*);

        using Object = FiberWorkerPoolWakeEvent;
    };

    using WakeEventOpaque = OpaqueObject<WakeEventDefinition>;

  private:
    friend struct FiberScheduler;
    friend struct FiberWorkerThread;

    FiberScheduler*          poolScheduler = nullptr;
    Span<FiberWorker>        workers;
    Span<FiberWorkerThread>  threads;
    WakeEventOpaque          wakeEvent;
    mutable volatile int32_t stopRequested      = 0;
    mutable volatile int32_t running            = 0;
    size_t                   idleSpinAttempts   = 0;
    bool                     localDequesCreated = false;
    bool                     injectionCreated   = false;

    void                   wakeOneWorker();
    void                   wakeAllWorkers();
    [[nodiscard]] bool     waitForWork(uint32_t observedGeneration);
    [[nodiscard]] uint32_t wakeGeneration() const;

    Result workerMain(size_t workerIndex);
};

//! Caller-owned cancellation source shared by one or more fiber tasks.
struct SC_FIBERS_EXPORT FiberCancellationTokenSource
{
    FiberCancellationTokenSource();

    FiberCancellationTokenSource(const FiberCancellationTokenSource&)            = delete;
    FiberCancellationTokenSource& operator=(const FiberCancellationTokenSource&) = delete;

    void   requestCancel();
    void   reset();
    Result check() const;

    [[nodiscard]] bool                   isCancellationRequested() const;
    [[nodiscard]] FiberCancellationToken token() const;

  private:
    friend struct FiberCancellationToken;
    friend struct FiberScheduler;

    mutable volatile int32_t requested = 0;
};

//! Lightweight cancellation token copied into spawned fiber tasks.
struct SC_FIBERS_EXPORT FiberCancellationToken
{
    FiberCancellationToken();

    Result check() const;

    [[nodiscard]] bool isValid() const;
    [[nodiscard]] bool isCancellationRequested() const;

  private:
    friend struct FiberCancellationTokenSource;
    friend struct FiberScheduler;

    explicit FiberCancellationToken(const FiberCancellationTokenSource& tokenSource);

    const FiberCancellationTokenSource* source = nullptr;
};

//! Optional scheduling inputs used when the short spawn overloads are not expressive enough.
struct SC_FIBERS_EXPORT FiberTaskSpawnOptions
{
    FiberCancellationToken cancellationToken;
    FiberCounter*          counter     = nullptr;
    void*                  userData    = nullptr;
    bool                   setUserData = false;

  private:
    friend struct FiberScheduler;
    friend struct FiberTaskPool;

    FiberTaskPool*   originPool       = nullptr;
    FiberTaskClass*  originTaskClass  = nullptr;
    FiberStackClass* originStackClass = nullptr;
};

#if SC_PLATFORM_WINDOWS && (SC_COMPILER_MSVC || SC_COMPILER_CLANG_CL)
#pragma warning(push)
#pragma warning(disable : 4324)
#endif

//! Caller-owned task object scheduled by FiberScheduler.
struct SC_FIBERS_EXPORT FiberTask
{
    using Procedure = Function<Result(FiberScheduler&)>;

    FiberTask();
    ~FiberTask();

    FiberTask(const FiberTask&)            = delete;
    FiberTask& operator=(const FiberTask&) = delete;
    FiberTask(FiberTask&&)                 = delete;
    FiberTask& operator=(FiberTask&&)      = delete;

    [[nodiscard]] bool            isValid() const;
    [[nodiscard]] bool            isStarted() const;
    [[nodiscard]] bool            isCompleted() const;
    [[nodiscard]] bool            isActive() const;
    [[nodiscard]] bool            isCancellationRequested() const;
    [[nodiscard]] FiberTaskStatus status() const;
    [[nodiscard]] Result          result() const;
    void                          setUserData(void* data);
    [[nodiscard]] void*           userData() const;

  private:
    friend struct FiberTaskGroup;
    friend struct FiberScheduler;

    AlignedStorage<FiberContextStorageSize, FiberContextStorageAlignment> contextStorage;

    Procedure              procedure;
    FiberScheduler*        scheduler         = nullptr;
    FiberCounter*          completionCounter = nullptr;
    FiberCancellationToken cancellationToken;
    FiberTask*             nextReady        = nullptr;
    FiberTask*             previousReady    = nullptr;
    FiberTask*             nextWaiting      = nullptr;
    FiberTask*             nextActive       = nullptr;
    FiberTask*             previousActive   = nullptr;
    FiberTask*             nextGroup        = nullptr;
    FiberCounter*          waitingCounter   = nullptr;
    FiberCounter*          suspendCounter   = nullptr;
    FiberTaskPool*         originPool       = nullptr;
    FiberTaskClass*        originTaskClass  = nullptr;
    FiberStackClass*       originStackClass = nullptr;
    Span<char>             originStackMemory;
    FiberWorker*           preferredWorker      = nullptr;
    FiberWorker*           activeRegistryWorker = nullptr;
    void*                  runningWorker        = nullptr;
    void*                  stackOwner           = nullptr;
    void*                  taskUserData         = nullptr;
    Result                 taskResult           = Result(true);
    volatile int32_t       taskStatus           = static_cast<int32_t>(FiberTaskStatus::Invalid);
    FiberTaskSuspendAction suspendAction        = FiberTaskSuspendAction::None;
    volatile bool          cancelRequested      = false;
    bool                   suspendInterruptible = false;

    [[nodiscard]] FiberContext&       context();
    [[nodiscard]] const FiberContext& context() const;
};

struct FiberTaskClassDefinition
{
    static constexpr int    Windows   = 96;
    static constexpr int    Apple     = 96;
    static constexpr int    Linux     = 96;
    static constexpr int    Default   = 96;
    static constexpr size_t Alignment = alignof(void*);

    using Object = FiberTaskClassInternal;
};
using FiberTaskClassOpaque = OpaqueObject<FiberTaskClassDefinition>;

struct SC_FIBERS_EXPORT FiberTaskClassOptions
{
    size_t maxTasks = 0;
};

struct SC_FIBERS_EXPORT FiberTaskClassDiagnostics
{
    size_t capacity        = 0;
    size_t activeTasks     = 0;
    size_t availableTasks  = 0;
    size_t peakActiveTasks = 0;
};

//! Allocator-backed fixed-capacity storage for reusable FiberTask objects.
struct SC_FIBERS_EXPORT FiberTaskClass
{
    FiberTaskClass();
    ~FiberTaskClass();

    FiberTaskClass(const FiberTaskClass&)            = delete;
    FiberTaskClass& operator=(const FiberTaskClass&) = delete;
    FiberTaskClass(FiberTaskClass&&)                 = delete;
    FiberTaskClass& operator=(FiberTaskClass&&)      = delete;

    [[nodiscard]] Result create(FiberAllocator& allocator, const FiberTaskClassOptions& options);
    [[nodiscard]] Result acquire(FiberTask*& outTask);
    [[nodiscard]] Result release(FiberTask& task);
    [[nodiscard]] Result waitForAvailableSlot(FiberScheduler& scheduler);
    [[nodiscard]] Result validateClose() const;
    [[nodiscard]] Result close();
    void                 diagnostics(FiberTaskClassDiagnostics& outDiagnostics) const;

    [[nodiscard]] bool   isOpen() const;
    [[nodiscard]] bool   owns(const FiberTask& task) const;
    [[nodiscard]] size_t capacity() const;
    [[nodiscard]] size_t activeCount() const;
    [[nodiscard]] size_t availableCount() const;

  private:
    friend struct FiberTaskPool;

    FiberTaskClassOpaque internal;
};

//! Counter used to suspend fibers until a group of operations completes.
struct SC_FIBERS_EXPORT FiberCounter
{
    FiberCounter();
    ~FiberCounter();

    FiberCounter(const FiberCounter&)            = delete;
    FiberCounter& operator=(const FiberCounter&) = delete;

    [[nodiscard]] size_t value() const;

  private:
    friend struct FiberScheduler;

    size_t     counterValue = 0;
    FiberTask* waitingHead  = nullptr;
    FiberTask* waitingTail  = nullptr;
};

//! One failed task collected from a FiberTaskGroup.
struct SC_FIBERS_EXPORT FiberTaskGroupError
{
    FiberTask* task   = nullptr;
    Result     result = Result(true);
};

//! Convenience helper for spawning a group of child tasks and waiting on their completion.
struct SC_FIBERS_EXPORT FiberTaskGroup
{
    explicit FiberTaskGroup(FiberScheduler& scheduler);
    ~FiberTaskGroup();

    FiberTaskGroup(const FiberTaskGroup&)            = delete;
    FiberTaskGroup& operator=(const FiberTaskGroup&) = delete;

    Result spawn(FiberTask& task, FiberStack& stack, FiberTask::Procedure procedure);
    Result spawn(FiberTask& task, FiberStack& stack, FiberTask::Procedure procedure, FiberCancellationToken token);
    Result spawn(FiberTask& task, FiberStack& stack, FiberTask::Procedure procedure,
                 const FiberTaskSpawnOptions& options);
    Result spawn(FiberTaskPool& pool, FiberTask::Procedure procedure, FiberTask** outTask = nullptr);
    Result spawn(FiberTaskPool& pool, FiberTask::Procedure procedure, FiberCancellationToken token,
                 FiberTask** outTask = nullptr);
    Result spawn(FiberTaskPool& pool, FiberTask::Procedure procedure, const FiberTaskSpawnOptions& options,
                 FiberTask** outTask = nullptr);
    Result wait();
    Result waitAll(Result* outFirstError = nullptr);
    Result waitAllCancelOnParentCancel(Result* outFirstError = nullptr);
    Result waitCancelOnError(Result* outFirstError = nullptr);
    Result cancelAll();

    [[nodiscard]] size_t pending() const;
    [[nodiscard]] size_t countErrors() const;
    Result               collectErrors(Span<FiberTaskGroupError> errors, size_t& outErrors) const;

  private:
    FiberScheduler& scheduler;
    FiberCounter    counter;
    FiberTask*      taskHead = nullptr;

    void   linkTask(FiberTask& task);
    Result findFirstError(Result* outFirstError) const;
};

//! Caller-owned pool that pairs FiberTask objects with fixed-size stack slots.
struct SC_FIBERS_EXPORT FiberTaskPoolDiagnostics
{
    size_t capacity       = 0;
    size_t activeTasks    = 0;
    size_t availableTasks = 0;
    bool   classBacked    = false;

    FiberTaskClassDiagnostics  taskClass;
    FiberStackClassDiagnostics stackClass;
};

struct SC_FIBERS_EXPORT FiberTaskPool
{
    FiberTaskPool();
    FiberTaskPool(Span<FiberTask> taskStorage, Span<char> stackStorage, size_t stackSize);
    ~FiberTaskPool();

    FiberTaskPool(const FiberTaskPool&)            = delete;
    FiberTaskPool& operator=(const FiberTaskPool&) = delete;

    Result create(FiberTaskClass& taskClass, FiberStackClass& stackClass);
    Result close();

    Result spawn(FiberScheduler& scheduler, FiberTask::Procedure procedure, FiberTask** outTask = nullptr,
                 FiberCounter* counter = nullptr);
    Result spawn(FiberScheduler& scheduler, FiberTask::Procedure procedure, FiberCancellationToken token,
                 FiberTask** outTask = nullptr, FiberCounter* counter = nullptr);
    Result spawn(FiberScheduler& scheduler, FiberTask::Procedure procedure, const FiberTaskSpawnOptions& options,
                 FiberTask** outTask = nullptr);

    [[nodiscard]] size_t capacity() const;
    [[nodiscard]] size_t activeCount() const;
    [[nodiscard]] size_t availableCount() const;
    [[nodiscard]] bool   hasAvailableTask() const;
    void                 diagnostics(FiberTaskPoolDiagnostics& outDiagnostics) const;
    Result               waitForSpawnCapacity(FiberScheduler& scheduler);
    Result               waitForAvailableTask(FiberScheduler& scheduler);
    [[nodiscard]] size_t stackSizeInBytes() const;
    void                 fillHighWaterMarks();
    Result               stackHighWaterUsedBytes(size_t stackIndex, size_t& outBytes) const;
    Result               stackHighWaterUnusedBytes(size_t stackIndex, size_t& outBytes) const;

  private:
    friend struct FiberScheduler;

    struct WaitNode
    {
        FiberCounter counter;
        WaitNode*    next     = nullptr;
        bool         notified = false;
    };

    Span<FiberTask> tasks;
    Span<char>      stacks;
    size_t          stackSize = 0;
    size_t          nextTask  = 0;

    FiberTaskClass*  taskClass  = nullptr;
    FiberStackClass* stackClass = nullptr;

    WaitNode* availabilityWaitHead = nullptr;
    WaitNode* availabilityWaitTail = nullptr;

    mutable volatile int32_t primitiveLock = 0;

    Result        stackAt(size_t stackIndex, FiberStack& outStack) const;
    void          queueAvailabilityWaiter(WaitNode& node);
    FiberCounter* popAvailabilityWaiterForNotification();
    bool          removeAvailabilityWaiter(WaitNode& node);
};

//! Manual-reset event that wakes waiting fibers when signaled.
struct SC_FIBERS_EXPORT FiberEvent
{
    explicit FiberEvent(bool signaled = false);
    ~FiberEvent();

    FiberEvent(const FiberEvent&)            = delete;
    FiberEvent& operator=(const FiberEvent&) = delete;

    Result wait(FiberScheduler& scheduler);
    Result signal(FiberScheduler& scheduler);
    void   reset();

    [[nodiscard]] bool isSignaled() const;

  private:
    struct WaitNode
    {
        FiberCounter counter;
        WaitNode*    next     = nullptr;
        bool         notified = false;
    };

    WaitNode*                waitHead      = nullptr;
    WaitNode*                waitTail      = nullptr;
    bool                     signaled      = false;
    mutable volatile int32_t primitiveLock = 0;

    void queueWaiter(WaitNode& node);
    bool removeWaiter(WaitNode& node);
};

//! Auto-reset event that wakes one waiting fiber per signal.
struct SC_FIBERS_EXPORT FiberAutoResetEvent
{
    explicit FiberAutoResetEvent(bool signaled = false);
    ~FiberAutoResetEvent();

    FiberAutoResetEvent(const FiberAutoResetEvent&)            = delete;
    FiberAutoResetEvent& operator=(const FiberAutoResetEvent&) = delete;

    Result wait(FiberScheduler& scheduler);
    Result signal(FiberScheduler& scheduler);
    void   reset();

    [[nodiscard]] bool isSignaled() const;

  private:
    struct WaitNode
    {
        FiberCounter counter;
        WaitNode*    next     = nullptr;
        bool         notified = false;
    };

    WaitNode*                waitHead      = nullptr;
    WaitNode*                waitTail      = nullptr;
    bool                     signaled      = false;
    mutable volatile int32_t primitiveLock = 0;

    void queueWaiter(WaitNode& node);
    bool popWaiter(WaitNode*& node);
    bool removeWaiter(WaitNode& node);
};

//! Counting semaphore for cooperative fibers.
struct SC_FIBERS_EXPORT FiberSemaphore
{
    explicit FiberSemaphore(size_t initialCount = 0);
    ~FiberSemaphore();

    FiberSemaphore(const FiberSemaphore&)            = delete;
    FiberSemaphore& operator=(const FiberSemaphore&) = delete;

    Result wait(FiberScheduler& scheduler);
    Result signal(FiberScheduler& scheduler, size_t count = 1);

    [[nodiscard]] size_t available() const;

  private:
    struct WaitNode
    {
        FiberCounter counter;
        WaitNode*    next     = nullptr;
        bool         notified = false;
    };

    WaitNode*                waitHead       = nullptr;
    WaitNode*                waitTail       = nullptr;
    size_t                   availableCount = 0;
    mutable volatile int32_t primitiveLock  = 0;

    void      queueWaiter(WaitNode& node);
    WaitNode* popWaiter();
    bool      removeWaiter(WaitNode& node);
};

//! Cooperative mutex for fibers running on one FiberScheduler.
struct SC_FIBERS_EXPORT FiberMutex
{
    FiberMutex();
    ~FiberMutex();

    FiberMutex(const FiberMutex&)            = delete;
    FiberMutex& operator=(const FiberMutex&) = delete;

    Result lock(FiberScheduler& scheduler);
    Result unlock(FiberScheduler& scheduler);

    [[nodiscard]] bool isLocked() const;
    [[nodiscard]] bool isOwnedByCurrentTask(FiberScheduler& scheduler) const;

  private:
    struct WaitNode
    {
        FiberCounter counter;
        WaitNode*    next     = nullptr;
        bool         notified = false;
    };

    WaitNode*                waitHead      = nullptr;
    WaitNode*                waitTail      = nullptr;
    bool                     locked        = false;
    FiberTask*               owner         = nullptr;
    mutable volatile int32_t primitiveLock = 0;

    void      queueWaiter(WaitNode& node);
    WaitNode* popWaiter();
    bool      removeWaiter(WaitNode& node);
};

//! Cooperative fiber scheduler with explicit workers and caller-owned storage.
struct SC_FIBERS_EXPORT FiberScheduler
{
    FiberScheduler();
    ~FiberScheduler();

    FiberScheduler(const FiberScheduler&)            = delete;
    FiberScheduler& operator=(const FiberScheduler&) = delete;

    Result spawn(FiberTask& task, FiberStack& stack, FiberTask::Procedure procedure, FiberCounter* counter = nullptr);
    Result spawn(FiberTask& task, FiberStack& stack, FiberTask::Procedure procedure, FiberCancellationToken token,
                 FiberCounter* counter = nullptr);
    Result spawn(FiberTask& task, FiberStack& stack, FiberTask::Procedure procedure,
                 const FiberTaskSpawnOptions& options);

    Result runOnce();
    Result runOnce(FiberWorker& worker);
    Result runOnce(FiberWorker& worker, Span<FiberWorker> workerGroup);
    Result runNoWait();
    Result runNoWait(FiberWorker& worker);
    Result runNoWait(FiberWorker& worker, Span<FiberWorker> workerGroup);
    Result runReadyFibers();
    Result runReadyFibers(FiberWorker& worker);
    Result runReadyFibers(FiberWorker& worker, Span<FiberWorker> workerGroup);
    Result run();
    Result run(FiberWorker& worker);
    Result run(FiberWorker& worker, Span<FiberWorker> workerGroup);
    Result createWorkerDeques(FiberAllocator& allocator, Span<FiberWorker> workers, size_t capacityPerWorker);
    void   releaseWorkerDeques(Span<FiberWorker> workers);
    Result yield();
    Result shutdown();
    Result shutdown(FiberWorker& worker);
    Result shutdown(FiberWorker& worker, Span<FiberWorker> workerGroup);

    Result requestCancel(FiberTask& task);
    Result requestCancel(FiberCancellationTokenSource& tokenSource);
    Result requestCancelAll();

    void   add(FiberCounter& counter);
    Result done(FiberCounter& counter);
    Result wait(FiberCounter& counter);
    Result waitUninterruptible(FiberCounter& counter);

    void setTraceHooks(const FiberTraceHooks& hooks);
    void clearTraceHooks();

    [[nodiscard]] FiberTask*       currentTask();
    [[nodiscard]] const FiberTask* currentTask() const;
    [[nodiscard]] bool             isCurrentTaskCancellationRequested() const;

    [[nodiscard]] bool   hasReadyFibers() const;
    [[nodiscard]] bool   hasActiveFibers() const;
    [[nodiscard]] size_t readyFiberCount() const;
    [[nodiscard]] size_t readyFiberCount(const FiberWorker& worker) const;
    [[nodiscard]] size_t stolenFiberCount(const FiberWorker& worker) const;
    [[nodiscard]] size_t stolenFiberCount(Span<FiberWorker> workers) const;
    [[nodiscard]] size_t activeFiberCount() const;
    void                 schedulerDiagnostics(FiberSchedulerDiagnostics& diagnostics) const;
    void                 resetSchedulerDiagnostics();
    void                 workerDiagnostics(const FiberWorker& worker, FiberWorkerDiagnostics& diagnostics) const;
    void                 workerDiagnostics(Span<FiberWorker> workers, FiberWorkerDiagnostics& diagnostics) const;

    void resetWorkerDiagnostics(FiberWorker& worker);
    void resetWorkerDiagnostics(Span<FiberWorker> workers);

  private:
    friend struct FiberWorkerPool;

    FiberTask* readyHead  = nullptr;
    FiberTask* readyTail  = nullptr;
    FiberTask* activeHead = nullptr;

    FiberTask**     injectionQueue     = nullptr;
    FiberAllocator* injectionAllocator = nullptr;
    size_t          injectionCapacity  = 0;
    size_t          injectionHead      = 0;
    size_t          injectionTail      = 0;
    size_t          injectionReady     = 0;
    size_t          injectionPeak      = 0;
    size_t          injectionSpills    = 0;

    FiberWorkerPool* workerPool = nullptr;

    volatile size_t readyFibers  = 0;
    volatile size_t activeFibers = 0;

    mutable volatile int32_t schedulerLock = 0;

    mutable size_t schedulerLockAcquisitions    = 0;
    mutable size_t schedulerLockContentions     = 0;
    mutable size_t schedulerLockSpinRetries     = 0;
    mutable size_t schedulerLockPeakSpinRetries = 0;

    FiberTraceHooks traceHooks;

    struct LockGuard;

    void lock() const;
    void unlock() const;
    void trace(FiberTraceEventType type, FiberTask* task, FiberWorker* worker, size_t value = 0) const;

    void                     addUnlocked(FiberCounter& counter);
    Result                   createInjectionQueue(FiberAllocator& allocator, size_t capacity);
    void                     releaseInjectionQueue();
    [[nodiscard]] bool       tryPushInjectionUnlocked(FiberTask& task);
    [[nodiscard]] FiberTask* popInjectionUnlocked();
    void                     notifyReadyWorkUnlocked();
    void                     pushReadyUnlocked(FiberTask& task);
    void                     pushReadyUnlocked(FiberTask& task, FiberWorker* preferredWorker);
    [[nodiscard]] bool       tryPushWorkerReadyDeque(FiberWorker& worker, FiberTask& task);
    void                     pushWorkerReadyUnlocked(FiberWorker& worker, FiberTask& task);

    [[nodiscard]] FiberTask* popReadyUnlocked();
    [[nodiscard]] FiberTask* popReadyUnlocked(FiberWorker& worker, Span<FiberWorker> stealWorkers);
    [[nodiscard]] FiberTask* popWorkerReadyUnlocked(FiberWorker& worker);
    [[nodiscard]] FiberTask* stealWorkerReadyUnlocked(FiberWorker& worker);
    [[nodiscard]] FiberTask* stealReadyUnlocked(FiberWorker& worker, Span<FiberWorker> stealWorkers);
    Result                   runReadyTask(FiberTask& task);
    Result                   runReadyTask(FiberTask& task, FiberWorker& worker);
    [[nodiscard]] bool       preparePreferredWorkerReadyPublish(FiberTask& task, FiberWorker& worker,
                                                                FiberWorker*& outPreferredWorker);
    void                     publishSuspensionUnlocked(FiberTask& task);
    void                     finishCurrentTask(FiberTask& task, Result result);
    Result                   cancelTaskUnlocked(FiberTask& task);
    void                     linkActiveUnlocked(FiberTask& task);
    void                     unlinkActiveUnlocked(FiberTask& task);
    void                     moveActiveToWorkerUnlocked(FiberTask& task, FiberWorker& worker);
    void                     unlinkWorkerActive(FiberTask& task);
    void                     cancelWorkerActiveUnlocked(FiberWorker& worker, FiberCancellationTokenSource* tokenSource);
    bool                     removeCounterWaiterUnlocked(FiberCounter& counter, FiberTask& task);
    Result                   waitImpl(FiberCounter& counter, bool interruptible);
    Result                   doneUnlocked(FiberCounter& counter);
    void                     wakeCounterWaitersUnlocked(FiberCounter& counter);

    static void taskEntry(void* userData);
};

#if SC_PLATFORM_WINDOWS && (SC_COMPILER_MSVC || SC_COMPILER_CLANG_CL)
#pragma warning(pop)
#endif
} // namespace SC
//! @}
