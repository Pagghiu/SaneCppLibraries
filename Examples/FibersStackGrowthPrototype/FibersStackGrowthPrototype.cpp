// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
//---------------------------------------------------------------------------------------------------------------------
// Description:
// Isolated experiment for bounded, fault-driven fiber stack commitment. This is intentionally not a Fibers API.
//---------------------------------------------------------------------------------------------------------------------
#if defined(__APPLE__) && !defined(_XOPEN_SOURCE)
#define _XOPEN_SOURCE 700
#define _DARWIN_C_SOURCE
#elif defined(__linux__) && !defined(_GNU_SOURCE)
#define _GNU_SOURCE
#endif

#include "../../Libraries/Fibers/FibersErrorFormatter.h"
#include "../../Libraries/Fibers/Internal/FiberContext.h"
#include "../../Libraries/Strings/Console.h"
#include "../../Libraries/Threading/Atomic.h"
#include "../../Libraries/Threading/Threading.h"
#include "../../Libraries/Threading/ThreadingErrorFormatter.h"
#include <string.h>

#if SC_PLATFORM_WINDOWS
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#define SC_FIBERS_STACK_GROWTH_NO_INLINE __declspec(noinline)
#else
#if SC_PLATFORM_APPLE
#include <sys/proc.h>
#include <sys/sysctl.h>
#elif SC_PLATFORM_LINUX
#include <fcntl.h>
#endif
#include <signal.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <ucontext.h>
#include <unistd.h>
#define SC_FIBERS_STACK_GROWTH_NO_INLINE __attribute__((noinline))
#endif

#if defined(__has_feature)
#if __has_feature(address_sanitizer)
#define SC_FIBERS_STACK_GROWTH_HAS_ASAN 1
#endif
#endif
#if defined(__SANITIZE_ADDRESS__)
#define SC_FIBERS_STACK_GROWTH_HAS_ASAN 1
#endif
#if !defined(SC_FIBERS_STACK_GROWTH_HAS_ASAN)
#define SC_FIBERS_STACK_GROWTH_HAS_ASAN 0
#endif

namespace SC
{
namespace
{
static constexpr ResultCategory PrototypeResultCategory = ResultCategory(0x80000006u);
enum class PrototypeError : uint32_t
{
    GuardProbeReturned = 1,
    ForeignFaultProbeReturned,
    NestedFaultProbeReturned,
    ProcessHandlersRequired,
    SignalStackInstallFailed,
    ProcessHandlerOwnerMismatch,
    SegmentationHandlerInstallFailed,
    BusHandlerInstallFailed,
    BusHandlerRestoreFailed,
    SegmentationHandlerRestoreFailed,
    WrongCloseThread,
    UnknownChildMode,
    ConcurrentWorkerNotRun,
    ConcurrentGrowthInsufficient,
    EarlyProcessCloseAccepted,
    ForeignThreadCloseAccepted,
    InsufficientGrowth,
    CommitmentDidNotExpand,
    RecursionNotObserved,
    PageSizeUnavailable,
    CommitSizeMisaligned,
    ReservationFailed,
    InitialCommitFailed,
    InitialGuardFailed,
    InitialCommitmentUnavailable,
    ThreadHandlerAlreadyActive,
    ProcessHandlersClosing,
    SignalLifecycleBusy,
    ThreadHandlersStillActive,
    SignalStackQueryFailed,
    SignalStackOwnerMismatch,
    SignalStackRestoreFailed,
    SignalStackVerificationFailed,
    SignalStackDisableMismatch,
    SignalStackRestoreMismatch,
    ReservationReleaseFailed,
    ProbeHandlerInstallFailed,
    ExecutablePathTooLong,
    ChildStartFailed,
    ChildWaitFailed,
    ForeignFaultNotForwarded,
    GuardOverflowNotClassified,
};

static constexpr Result prototypeFailure(PrototypeError error) { return Result::Error(PrototypeResultCategory, error); }

static constexpr Result prototypeCheck(bool condition, PrototypeError error)
{
    return condition ? Result(true) : prototypeFailure(error);
}

static ResultErrorFormat formatPrototypeError(Result result, Span<char> output)
{
    if (result.category() == FibersResultCategory)
        return formatFibersError(result, output);
    if (result.category() == ThreadingResultCategory)
        return formatThreadingError(result, output);
    if (result.category() != PrototypeResultCategory)
        return ResultErrorFormatter::failure(ResultErrorFormatStatus::ForeignCategory, output);
    ResultErrorFormatter formatter(output);
    switch (static_cast<PrototypeError>(result.errorValue()))
    {
    case PrototypeError::GuardProbeReturned: formatter.append("Guard-overflow probe returned unexpectedly"); break;
    case PrototypeError::ForeignFaultProbeReturned:
        formatter.append("Foreign-fault probe returned unexpectedly");
        break;
    case PrototypeError::NestedFaultProbeReturned: formatter.append("Nested-fault probe returned unexpectedly"); break;
    case PrototypeError::ProcessHandlersRequired:
        formatter.append("Complete process handlers must be installed before thread handlers");
        break;
    case PrototypeError::SignalStackInstallFailed:
        formatter.append("Could not install its alternate signal stack");
        break;
    case PrototypeError::ProcessHandlerOwnerMismatch:
        formatter.append("Another stack growth prototype owns the process signal handlers");
        break;
    case PrototypeError::SegmentationHandlerInstallFailed:
        formatter.append("Could not install its segmentation signal handler");
        break;
    case PrototypeError::BusHandlerInstallFailed: formatter.append("Could not install its bus signal handler"); break;
    case PrototypeError::BusHandlerRestoreFailed: formatter.append("Could not restore its bus signal handler"); break;
    case PrototypeError::SegmentationHandlerRestoreFailed:
        formatter.append("Could not restore its segmentation signal handler");
        break;
    case PrototypeError::WrongCloseThread: formatter.append("Must close on its handler-owning thread"); break;
    case PrototypeError::UnknownChildMode: formatter.append("Unknown stack growth prototype child mode"); break;
    case PrototypeError::ConcurrentWorkerNotRun: formatter.append("Concurrent worker did not run"); break;
    case PrototypeError::ConcurrentGrowthInsufficient:
        formatter.append("Concurrent worker did not cross enough commit boundaries");
        break;
    case PrototypeError::EarlyProcessCloseAccepted:
        formatter.append("Process signal handler teardown was not refused while workers were active");
        break;
    case PrototypeError::ForeignThreadCloseAccepted:
        formatter.append("Worker stack teardown was not refused from a foreign thread");
        break;
    case PrototypeError::InsufficientGrowth: formatter.append("Did not cross enough commit boundaries"); break;
    case PrototypeError::CommitmentDidNotExpand: formatter.append("Did not expand its committed range"); break;
    case PrototypeError::RecursionNotObserved: formatter.append("Recursion was optimized away"); break;
    case PrototypeError::PageSizeUnavailable: formatter.append("Could not query the page size"); break;
    case PrototypeError::CommitSizeMisaligned: formatter.append("Commit sizes must be page aligned"); break;
    case PrototypeError::ReservationFailed: formatter.append("Reservation failed"); break;
    case PrototypeError::InitialCommitFailed: formatter.append("Initial commit failed"); break;
    case PrototypeError::InitialGuardFailed: formatter.append("Initial guard preparation failed"); break;
    case PrototypeError::InitialCommitmentUnavailable: formatter.append("Could not measure initial commitment"); break;
    case PrototypeError::ThreadHandlerAlreadyActive:
        formatter.append("Another stack growth prototype is already active on this thread");
        break;
    case PrototypeError::ProcessHandlersClosing: formatter.append("Process handlers are closing"); break;
    case PrototypeError::SignalLifecycleBusy: formatter.append("Signal lifecycle is not idle"); break;
    case PrototypeError::ThreadHandlersStillActive:
        formatter.append("Cannot remove process handlers while thread handlers are active");
        break;
    case PrototypeError::SignalStackQueryFailed: formatter.append("Could not query its alternate signal stack"); break;
    case PrototypeError::SignalStackOwnerMismatch:
        formatter.append("No longer owns the active alternate signal stack");
        break;
    case PrototypeError::SignalStackRestoreFailed:
        formatter.append("Could not restore the previous alternate signal stack");
        break;
    case PrototypeError::SignalStackVerificationFailed:
        formatter.append("Could not verify the restored alternate signal stack");
        break;
    case PrototypeError::SignalStackDisableMismatch:
        formatter.append("Did not restore the disabled alternate signal stack");
        break;
    case PrototypeError::SignalStackRestoreMismatch:
        formatter.append("Did not restore the previous alternate signal stack");
        break;
    case PrototypeError::ReservationReleaseFailed: formatter.append("Could not release its reservation"); break;
    case PrototypeError::ProbeHandlerInstallFailed:
        formatter.append("Could not install the foreign-fault probe handler");
        break;
    case PrototypeError::ExecutablePathTooLong: formatter.append("Executable path is too long"); break;
    case PrototypeError::ChildStartFailed: formatter.append("Child process could not start"); break;
    case PrototypeError::ChildWaitFailed: formatter.append("Could not wait for its child process"); break;
    case PrototypeError::ForeignFaultNotForwarded: formatter.append("Foreign or nested fault was not forwarded"); break;
    case PrototypeError::GuardOverflowNotClassified:
        formatter.append("Terminal guard overflow was not classified");
        break;
    default: return ResultErrorFormatter::failure(ResultErrorFormatStatus::UnknownError, output);
    }
    return formatter.finish();
}

struct StackGrowthPrototype;

// Signal handlers are process-wide, but only the installing thread may own and grow this experimental stack.
static thread_local StackGrowthPrototype* activePrototype = nullptr;
#if !SC_PLATFORM_WINDOWS
static StackGrowthPrototype* installedPrototype                    = nullptr;
static volatile sig_atomic_t guardOverflowProbeActive              = 0;
static constexpr int         GuardOverflowExitCode                 = 74;
alignas(16) static thread_local char signalStackMemory[128 * 1024] = {};
static Atomic<int32_t>   threadHandlerLifecycle;
static constexpr int32_t HandlerLifecycleClosing = -1;
#endif

static bool isDebuggerAttached()
{
#if SC_PLATFORM_WINDOWS
    return IsDebuggerPresent() != FALSE;
#elif SC_PLATFORM_APPLE
    int mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_PID, getpid()};

    struct kinfo_proc processInformation = {};
    size_t            informationSize    = sizeof(processInformation);
    return sysctl(mib, 4, &processInformation, &informationSize, nullptr, 0) == 0 and
           (processInformation.kp_proc.p_flag & P_TRACED) != 0;
#elif SC_PLATFORM_LINUX
    const int statusFile = open("/proc/self/status", O_RDONLY);
    if (statusFile < 0)
    {
        return false;
    }

    char          status[4096];
    const ssize_t statusLength = read(statusFile, status, sizeof(status));
    close(statusFile);
    if (statusLength <= 0)
    {
        return false;
    }

    static constexpr char TracerPrefix[] = "TracerPid:";
    for (size_t index = 0; index + sizeof(TracerPrefix) - 1 < static_cast<size_t>(statusLength); ++index)
    {
        if (memcmp(status + index, TracerPrefix, sizeof(TracerPrefix) - 1) != 0)
        {
            continue;
        }
        index += sizeof(TracerPrefix) - 1;
        while (index < static_cast<size_t>(statusLength) and (status[index] == ' ' or status[index] == '\t'))
        {
            index += 1;
        }
        return index < static_cast<size_t>(statusLength) and status[index] >= '1' and status[index] <= '9';
    }
    return false;
#else
    return false;
#endif
}

#if SC_PLATFORM_WINDOWS && (SC_COMPILER_MSVC || SC_COMPILER_CLANG_CL)
#pragma warning(push)
#pragma warning(disable : 4324)
#endif
struct StackGrowthPrototype
{
    static constexpr size_t UsableBytes        = 256 * 1024;
    static constexpr size_t InitialCommitBytes = 32 * 1024;
    static constexpr size_t GrowthBytes        = 16 * 1024;

    void*  reservation           = nullptr;
    char*  guardEnd              = nullptr;
    char*  committedBegin        = nullptr;
    char*  stackEnd              = nullptr;
    size_t pageSize              = 0;
    size_t reservationSize       = 0;
    size_t growthEvents          = 0;
    size_t checksum              = 0;
    size_t initialCommittedBytes = 0;

    FiberContext mainContext;
    FiberContext fiberContext;

#if SC_PLATFORM_WINDOWS
    void* previousStackBase  = nullptr;
    void* previousStackLimit = nullptr;
    char* growthGuardPage    = nullptr;
    DWORD handlerThreadId    = 0;
#else
    struct sigaction      previousSegmentationAction  = {};
    struct sigaction      previousBusAction           = {};
    stack_t               previousSignalStack         = {};
    bool                  signalStackInstalled        = false;
    bool                  segmentationActionInstalled = false;
    bool                  busActionInstalled          = false;
    bool                  processHandlerOwner         = false;
    bool                  threadHandlerRegistered     = false;
    volatile sig_atomic_t handlingFault               = 0;
#endif

    ~StackGrowthPrototype() { SC_THREADING_ASSERT_RELEASE(close()); }

    Result run()
    {
        SC_TRY(prepare(fiberEntry));
        return runPrepared();
    }

    Result runPrepared()
    {
        switchToFiber();

        SC_TRY(prototypeCheck(growthEvents >= 2, PrototypeError::InsufficientGrowth));
        SC_TRY(prototypeCheck(committedBytes() > initialCommittedBytes, PrototypeError::CommitmentDidNotExpand));
        SC_TRY(prototypeCheck(checksum != 0, PrototypeError::RecursionNotObserved));
        return Result(true);
    }

    Result runGuardOverflow()
    {
        SC_TRY(prepare(overflowFiberEntry));
        switchToFiber();
        return prototypeFailure(PrototypeError::GuardProbeReturned);
    }

    Result runForeignFault()
    {
        SC_TRY(reserve());
#if !SC_PLATFORM_WINDOWS
        SC_TRY(installProcessHandler());
#endif
        SC_TRY(installHandler());
        *reinterpret_cast<volatile int*>(static_cast<size_t>(1)) = 1;
        return prototypeFailure(PrototypeError::ForeignFaultProbeReturned);
    }

    Result runNestedFault()
    {
        SC_TRY(reserve());
#if !SC_PLATFORM_WINDOWS
        SC_TRY(installProcessHandler());
#endif
        SC_TRY(installHandler());
#if !SC_PLATFORM_WINDOWS
        handlingFault = 1;
#endif
        *reinterpret_cast<volatile int*>(static_cast<size_t>(1)) = 1;
        return prototypeFailure(PrototypeError::NestedFaultProbeReturned);
    }

    Result prepare(FiberContextEntry entry)
    {
        SC_TRY(reserve());
#if !SC_PLATFORM_WINDOWS
        SC_TRY(installProcessHandler());
#endif
        SC_TRY(installHandler());
        return prepareContext(entry);
    }

    Result prepareContext(FiberContextEntry entry)
    {
        SC_TRY(FiberContextOperations::captureCurrent(mainContext));
        SC_TRY(FiberContextOperations::create(fiberContext, {guardEnd, UsableBytes}, entry, this));
        return Result(true);
    }

    void switchToFiber()
    {
#if SC_PLATFORM_WINDOWS
        NT_TIB& threadStack       = *reinterpret_cast<NT_TIB*>(NtCurrentTeb());
        previousStackBase         = threadStack.StackBase;
        previousStackLimit        = threadStack.StackLimit;
        threadStack.StackBase     = stackEnd;
        threadStack.StackLimit    = growthGuardPage != nullptr ? committedBegin - GrowthBytes : committedBegin;
        char* publishedStackLimit = static_cast<char*>(threadStack.StackLimit);
#endif
        FiberContextOperations::switchTo(mainContext, fiberContext);
#if SC_PLATFORM_WINDOWS
        char* finalStackLimit = static_cast<char*>(threadStack.StackLimit);
        if (finalStackLimit < publishedStackLimit)
        {
            growthEvents += static_cast<size_t>(publishedStackLimit - finalStackLimit) / pageSize;
            committedBegin = finalStackLimit;
        }
        threadStack.StackBase  = previousStackBase;
        threadStack.StackLimit = previousStackLimit;
#endif
    }

    Result reserve()
    {
#if SC_PLATFORM_WINDOWS
        SYSTEM_INFO systemInfo;
        GetSystemInfo(&systemInfo);
        pageSize = static_cast<size_t>(systemInfo.dwPageSize);
#else
        const long systemPageSize = sysconf(_SC_PAGESIZE);
        SC_TRY(prototypeCheck(systemPageSize > 0, PrototypeError::PageSizeUnavailable));
        pageSize = static_cast<size_t>(systemPageSize);
#endif
        SC_TRY(prototypeCheck(InitialCommitBytes % pageSize == 0 and GrowthBytes % pageSize == 0,
                              PrototypeError::CommitSizeMisaligned));

        reservationSize = pageSize + UsableBytes;
#if SC_PLATFORM_WINDOWS
        reservation = VirtualAlloc(nullptr, reservationSize, MEM_RESERVE, PAGE_NOACCESS);
#else
#if SC_PLATFORM_APPLE
        reservation = mmap(nullptr, reservationSize, PROT_NONE, MAP_PRIVATE | MAP_ANON, -1, 0);
#else
        reservation = mmap(nullptr, reservationSize, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
#endif
        if (reservation == MAP_FAILED)
        {
            reservation = nullptr;
        }
#endif
        SC_TRY(prototypeCheck(reservation != nullptr, PrototypeError::ReservationFailed));

        guardEnd       = static_cast<char*>(reservation) + pageSize;
        stackEnd       = guardEnd + UsableBytes;
        committedBegin = stackEnd - InitialCommitBytes;
        SC_TRY(prototypeCheck(commit(committedBegin, InitialCommitBytes), PrototypeError::InitialCommitFailed));
#if SC_PLATFORM_WINDOWS
        SC_TRY(prototypeCheck(prepareWindowsGrowthGuard(), PrototypeError::InitialGuardFailed));
#endif
        initialCommittedBytes = committedBytes();
        SC_TRY(prototypeCheck(initialCommittedBytes > 0, PrototypeError::InitialCommitmentUnavailable));
        return Result(true);
    }

#if !SC_PLATFORM_WINDOWS
    [[nodiscard]] bool tryGrow(void* faultAddress, void* stackPointer)
    {
        char* nextCommittedBegin = committedBegin - GrowthBytes;
        if (nextCommittedBegin < guardEnd)
        {
            nextCommittedBegin = guardEnd;
        }
        const size_t fault = reinterpret_cast<size_t>(faultAddress);
        const size_t stack = reinterpret_cast<size_t>(stackPointer);
        if (nextCommittedBegin == committedBegin or stack < reinterpret_cast<size_t>(nextCommittedBegin) or
            stack >= reinterpret_cast<size_t>(stackEnd) or fault < reinterpret_cast<size_t>(nextCommittedBegin) or
            fault >= reinterpret_cast<size_t>(committedBegin))
        {
            return false;
        }
        if (not commit(nextCommittedBegin, static_cast<size_t>(committedBegin - nextCommittedBegin)))
        {
            return false;
        }

        committedBegin = nextCommittedBegin;
        growthEvents += 1;
        return true;
    }
#endif

#if SC_PLATFORM_WINDOWS
    [[nodiscard]] bool prepareWindowsGrowthGuard()
    {
        char* nextCommittedBegin = committedBegin - GrowthBytes;
        if (nextCommittedBegin < guardEnd)
        {
            return true;
        }
        if (not commit(nextCommittedBegin, GrowthBytes))
        {
            return false;
        }

        growthGuardPage = committedBegin - pageSize;
        DWORD previousProtection;
        return VirtualProtect(growthGuardPage, pageSize, PAGE_READWRITE | PAGE_GUARD, &previousProtection) == TRUE;
    }
#endif

    [[nodiscard]] bool commit(void* address, size_t size)
    {
#if SC_PLATFORM_WINDOWS
        return VirtualAlloc(address, size, MEM_COMMIT, PAGE_READWRITE) != nullptr;
#else
        return mprotect(address, size, PROT_READ | PROT_WRITE) == 0;
#endif
    }

    [[nodiscard]] size_t committedBytes() const
    {
#if SC_PLATFORM_WINDOWS
        size_t committed = 0;
        for (size_t offset = 0; offset < reservationSize; offset += pageSize)
        {
            MEMORY_BASIC_INFORMATION information;
            if (VirtualQuery(static_cast<char*>(reservation) + offset, &information, sizeof(information)) == 0)
            {
                return 0;
            }
            if (information.State == MEM_COMMIT)
            {
                committed += pageSize;
            }
        }
        return committed;
#else
        return static_cast<size_t>(stackEnd - committedBegin);
#endif
    }

    Result installHandler()
    {
        SC_TRY(prototypeCheck(activePrototype == nullptr, PrototypeError::ThreadHandlerAlreadyActive));
#if !SC_PLATFORM_WINDOWS
        SC_TRY(registerThreadHandler());
        if (installedPrototype == nullptr or not installedPrototype->segmentationActionInstalled or
            not installedPrototype->busActionInstalled)
        {
            unregisterThreadHandler();
            return prototypeFailure(PrototypeError::ProcessHandlersRequired);
        }

        activePrototype     = this;
        stack_t signalStack = {};
        signalStack.ss_sp   = signalStackMemory;
        signalStack.ss_size = sizeof(signalStackMemory);
        if (sigaltstack(&signalStack, &previousSignalStack) != 0)
        {
            SC_THREADING_ASSERT_RELEASE(close());
            return prototypeFailure(PrototypeError::SignalStackInstallFailed);
        }
        signalStackInstalled = true;

#else
        activePrototype = this;
        handlerThreadId = GetCurrentThreadId();
#endif
        return Result(true);
    }

#if !SC_PLATFORM_WINDOWS
    Result registerThreadHandler()
    {
        int32_t handlerCount = threadHandlerLifecycle.load();
        do
        {
            SC_TRY(prototypeCheck(handlerCount >= 0, PrototypeError::ProcessHandlersClosing));
        } while (not threadHandlerLifecycle.compare_exchange_weak(handlerCount, handlerCount + 1));
        threadHandlerRegistered = true;
        return Result(true);
    }

    void unregisterThreadHandler()
    {
        SC_THREADING_ASSERT_RELEASE(threadHandlerRegistered);
        SC_THREADING_ASSERT_RELEASE(threadHandlerLifecycle.fetch_sub(1) > 0);
        threadHandlerRegistered = false;
    }

    Result installProcessHandler()
    {
        int32_t expectedHandlerCount = 0;
        SC_TRY(prototypeCheck(
            threadHandlerLifecycle.compare_exchange_strong(expectedHandlerCount, HandlerLifecycleClosing),
            PrototypeError::SignalLifecycleBusy));
        if (installedPrototype != nullptr)
        {
            threadHandlerLifecycle.store(0);
            return prototypeFailure(PrototypeError::ProcessHandlerOwnerMismatch);
        }
        installedPrototype  = this;
        processHandlerOwner = true;

        struct sigaction action = {};
        action.sa_sigaction     = signalHandler;
        action.sa_flags         = SA_SIGINFO | SA_ONSTACK;
        sigemptyset(&action.sa_mask);
        if (sigaction(SIGSEGV, &action, &previousSegmentationAction) != 0)
        {
            installedPrototype  = nullptr;
            processHandlerOwner = false;
            threadHandlerLifecycle.store(0);
            return prototypeFailure(PrototypeError::SegmentationHandlerInstallFailed);
        }
        segmentationActionInstalled = true;
        if (sigaction(SIGBUS, &action, &previousBusAction) != 0)
        {
            SC_THREADING_ASSERT_RELEASE(restoreSignalAction(SIGSEGV, previousSegmentationAction));
            segmentationActionInstalled = false;
            installedPrototype          = nullptr;
            processHandlerOwner         = false;
            threadHandlerLifecycle.store(0);
            return prototypeFailure(PrototypeError::BusHandlerInstallFailed);
        }
        busActionInstalled = true;
        threadHandlerLifecycle.store(0);
        return Result(true);
    }

    Result closeProcessHandler()
    {
        if (not processHandlerOwner)
        {
            return Result(true);
        }
        int32_t expectedHandlerCount = 0;
        SC_TRY(prototypeCheck(
            threadHandlerLifecycle.compare_exchange_strong(expectedHandlerCount, HandlerLifecycleClosing),
            PrototypeError::ThreadHandlersStillActive));
        if (busActionInstalled)
        {
            if (not restoreSignalAction(SIGBUS, previousBusAction))
            {
                threadHandlerLifecycle.store(0);
                return prototypeFailure(PrototypeError::BusHandlerRestoreFailed);
            }
            busActionInstalled = false;
        }
        if (segmentationActionInstalled)
        {
            if (not restoreSignalAction(SIGSEGV, previousSegmentationAction))
            {
                threadHandlerLifecycle.store(0);
                return prototypeFailure(PrototypeError::SegmentationHandlerRestoreFailed);
            }
            segmentationActionInstalled = false;
        }
        if (installedPrototype == this)
        {
            installedPrototype  = nullptr;
            processHandlerOwner = false;
        }
        threadHandlerLifecycle.store(0);
        return Result(true);
    }
#endif

    Result close()
    {
#if !SC_PLATFORM_WINDOWS
        if ((signalStackInstalled or threadHandlerRegistered) and activePrototype != this)
        {
            return prototypeFailure(PrototypeError::WrongCloseThread);
        }
        if (signalStackInstalled)
        {
            stack_t currentSignalStack = {};
            SC_TRY(
                prototypeCheck(sigaltstack(nullptr, &currentSignalStack) == 0, PrototypeError::SignalStackQueryFailed));
            SC_TRY(prototypeCheck(currentSignalStack.ss_sp == signalStackMemory,
                                  PrototypeError::SignalStackOwnerMismatch));
            stack_t signalStackToRestore = previousSignalStack;
            if ((signalStackToRestore.ss_flags & SS_DISABLE) != 0)
            {
                // Darwin validates the size even though disabled-stack storage is ignored.
                signalStackToRestore.ss_size = sizeof(signalStackMemory);
            }
            SC_TRY(prototypeCheck(sigaltstack(&signalStackToRestore, nullptr) == 0,
                                  PrototypeError::SignalStackRestoreFailed));
            stack_t restoredSignalStack = {};
            SC_TRY(prototypeCheck(sigaltstack(nullptr, &restoredSignalStack) == 0,
                                  PrototypeError::SignalStackVerificationFailed));
            if ((previousSignalStack.ss_flags & SS_DISABLE) != 0)
            {
                SC_TRY(prototypeCheck((restoredSignalStack.ss_flags & SS_DISABLE) != 0,
                                      PrototypeError::SignalStackDisableMismatch));
            }
            else
            {
                SC_TRY(prototypeCheck(restoredSignalStack.ss_sp == previousSignalStack.ss_sp and
                                          restoredSignalStack.ss_size == previousSignalStack.ss_size and
                                          restoredSignalStack.ss_flags == previousSignalStack.ss_flags,
                                      PrototypeError::SignalStackRestoreMismatch));
            }
            signalStackInstalled = false;
        }
        if (activePrototype == this)
        {
            activePrototype = nullptr;
        }
        if (threadHandlerRegistered)
        {
            unregisterThreadHandler();
        }
        SC_TRY(closeProcessHandler());
#else
        if (handlerThreadId != 0 and handlerThreadId != GetCurrentThreadId())
        {
            return prototypeFailure(PrototypeError::WrongCloseThread);
        }
        if (activePrototype == this)
        {
            activePrototype = nullptr;
        }
        handlerThreadId = 0;
#endif
        if (reservation != nullptr)
        {
#if SC_PLATFORM_WINDOWS
            SC_TRY(prototypeCheck(VirtualFree(reservation, 0, MEM_RELEASE) != FALSE,
                                  PrototypeError::ReservationReleaseFailed));
#else
            SC_TRY(prototypeCheck(munmap(reservation, reservationSize) == 0, PrototypeError::ReservationReleaseFailed));
#endif
            reservation = nullptr;
        }
        return Result(true);
    }

    static void fiberEntry(void* userData)
    {
        StackGrowthPrototype& prototype = *static_cast<StackGrowthPrototype*>(userData);
        consumeStack(prototype, 24);
        FiberContextOperations::switchTo(prototype.fiberContext, prototype.mainContext);
    }

    static void overflowFiberEntry(void* userData)
    {
        StackGrowthPrototype& prototype = *static_cast<StackGrowthPrototype*>(userData);
        consumeStack(prototype, 96);
        FiberContextOperations::switchTo(prototype.fiberContext, prototype.mainContext);
    }

    static SC_FIBERS_STACK_GROWTH_NO_INLINE void consumeStack(StackGrowthPrototype& prototype, size_t depth)
    {
        volatile char stackUse[4096];
        for (size_t index = 0; index < sizeof(stackUse); ++index)
        {
            stackUse[index] = static_cast<char>(depth + index);
        }
        prototype.checksum += static_cast<unsigned char>(stackUse[depth % sizeof(stackUse)]);
        if (depth > 0)
        {
            consumeStack(prototype, depth - 1);
        }
        prototype.checksum += static_cast<unsigned char>(stackUse[(depth + 1) % sizeof(stackUse)]);
    }

#if !SC_PLATFORM_WINDOWS
    static bool signalActionMatches(const struct sigaction& first, const struct sigaction& second)
    {
        if ((first.sa_flags & SA_SIGINFO) != 0 or (second.sa_flags & SA_SIGINFO) != 0)
        {
            return (first.sa_flags & SA_SIGINFO) == (second.sa_flags & SA_SIGINFO) and
                   first.sa_sigaction == second.sa_sigaction;
        }
        return first.sa_handler == second.sa_handler;
    }

    static bool restoreSignalAction(int signal, const struct sigaction& previous)
    {
        struct sigaction current = {};
        if (sigaction(signal, nullptr, &current) == 0 and (current.sa_flags & SA_SIGINFO) != 0 and
            current.sa_sigaction == signalHandler)
        {
            if (sigaction(signal, &previous, nullptr) != 0)
            {
                return false;
            }
            struct sigaction restored = {};
            return sigaction(signal, nullptr, &restored) == 0 and signalActionMatches(restored, previous);
        }
        return false;
    }

    static void* interruptedStackPointer(void* rawContext)
    {
        ucontext_t& context = *static_cast<ucontext_t*>(rawContext);
#if SC_PLATFORM_APPLE && SC_PLATFORM_ARM64
        return reinterpret_cast<void*>(context.uc_mcontext->__ss.__sp);
#elif SC_PLATFORM_APPLE && SC_PLATFORM_INTEL && SC_PLATFORM_64_BIT
        return reinterpret_cast<void*>(context.uc_mcontext->__ss.__rsp);
#elif SC_PLATFORM_LINUX && SC_PLATFORM_ARM64
        return reinterpret_cast<void*>(context.uc_mcontext.sp);
#elif SC_PLATFORM_LINUX && SC_PLATFORM_INTEL && SC_PLATFORM_64_BIT
        return reinterpret_cast<void*>(context.uc_mcontext.gregs[REG_RSP]);
#else
        return nullptr;
#endif
    }

    [[nodiscard]] bool isTerminalGuardFault(void* faultAddress, void* stackPointer) const
    {
        const size_t fault = reinterpret_cast<size_t>(faultAddress);
        const size_t stack = reinterpret_cast<size_t>(stackPointer);
        return guardOverflowProbeActive != 0 and committedBegin == guardEnd and
               fault >= reinterpret_cast<size_t>(reservation) and fault < reinterpret_cast<size_t>(guardEnd) and
               stack >= reinterpret_cast<size_t>(reservation) and stack < reinterpret_cast<size_t>(stackEnd);
    }

    static void forwardSignal(const StackGrowthPrototype& prototype, int signal)
    {
        struct sigaction previous =
            signal == SIGBUS ? prototype.previousBusAction : prototype.previousSegmentationAction;
        if (previous.sa_handler == SIG_IGN)
        {
            previous            = {};
            previous.sa_handler = SIG_DFL;
            sigemptyset(&previous.sa_mask);
        }
        if (sigaction(signal, &previous, nullptr) != 0)
        {
            _exit(128 + signal);
        }
    }

    static void signalHandler(int signal, siginfo_t* info, void* context)
    {
        StackGrowthPrototype* prototype = activePrototype;
        StackGrowthPrototype* installer = installedPrototype;
        void*                 stack     = interruptedStackPointer(context);
        if ((signal == SIGSEGV or signal == SIGBUS) and prototype != nullptr and info != nullptr and
            prototype->handlingFault == 0)
        {
            prototype->handlingFault = 1;
            const bool grown         = prototype->tryGrow(info->si_addr, stack);
            prototype->handlingFault = 0;
            if (grown)
            {
                return;
            }
            if (prototype->isTerminalGuardFault(info->si_addr, stack))
            {
                _exit(GuardOverflowExitCode);
            }
        }

        if (installer != nullptr)
        {
            forwardSignal(*installer, signal);
            return;
        }

        struct sigaction defaultAction = {};
        defaultAction.sa_handler       = SIG_DFL;
        sigemptyset(&defaultAction.sa_mask);
        if (sigaction(signal, &defaultAction, nullptr) != 0)
        {
            _exit(128 + signal);
        }
    }
#endif
};
#if SC_PLATFORM_WINDOWS && (SC_COMPILER_MSVC || SC_COMPILER_CLANG_CL)
#pragma warning(pop)
#endif

static constexpr const char* GuardOverflowMode      = "--guard-overflow";
static constexpr const char* ForeignFaultMode       = "--foreign-fault";
static constexpr const char* NestedFaultMode        = "--nested-fault";
static constexpr int         ForeignHandlerExitCode = 73;

#if !SC_PLATFORM_WINDOWS
static void foreignFaultHandler(int) { _exit(ForeignHandlerExitCode); }
#endif

static Result runChildMode(const char* mode)
{
#if SC_PLATFORM_WINDOWS
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
#else
    if (strcmp(mode, GuardOverflowMode) == 0)
    {
        guardOverflowProbeActive = 1;
    }
    if (strcmp(mode, ForeignFaultMode) == 0 or strcmp(mode, NestedFaultMode) == 0)
    {
        struct sigaction action = {};
        action.sa_handler       = foreignFaultHandler;
        sigemptyset(&action.sa_mask);
        SC_TRY(prototypeCheck(sigaction(SIGSEGV, &action, nullptr) == 0 and sigaction(SIGBUS, &action, nullptr) == 0,
                              PrototypeError::ProbeHandlerInstallFailed));
    }
#endif

    StackGrowthPrototype prototype;
    if (strcmp(mode, GuardOverflowMode) == 0)
    {
        Result result = prototype.runGuardOverflow();
#if !SC_PLATFORM_WINDOWS
        guardOverflowProbeActive = 0;
#endif
        return result;
    }
    if (strcmp(mode, ForeignFaultMode) == 0)
    {
        return prototype.runForeignFault();
    }
    if (strcmp(mode, NestedFaultMode) == 0)
    {
        return prototype.runNestedFault();
    }
    return prototypeFailure(PrototypeError::UnknownChildMode);
}

#if !SC_FIBERS_STACK_GROWTH_HAS_ASAN
static Result runChildProbe(const char* executable, const char* mode)
{
#if SC_PLATFORM_WINDOWS
    char         commandLine[4096];
    const size_t executableLength = strlen(executable);
    const size_t modeLength       = strlen(mode);
    SC_TRY(prototypeCheck(executableLength + modeLength + 5 <= sizeof(commandLine),
                          PrototypeError::ExecutablePathTooLong));

    size_t commandLength         = 0;
    commandLine[commandLength++] = '"';
    memcpy(commandLine + commandLength, executable, executableLength);
    commandLength += executableLength;
    commandLine[commandLength++] = '"';
    commandLine[commandLength++] = ' ';
    memcpy(commandLine + commandLength, mode, modeLength);
    commandLength += modeLength;
    commandLine[commandLength] = '\0';

    STARTUPINFOA startup        = {};
    startup.cb                  = sizeof(startup);
    PROCESS_INFORMATION process = {};
    SC_TRY(prototypeCheck(
        CreateProcessA(executable, commandLine, nullptr, nullptr, FALSE, 0, nullptr, nullptr, &startup, &process),
        PrototypeError::ChildStartFailed));
    const DWORD waitResult  = WaitForSingleObject(process.hProcess, INFINITE);
    DWORD       exitCode    = 0;
    const BOOL  gotExitCode = GetExitCodeProcess(process.hProcess, &exitCode);
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    SC_TRY(prototypeCheck(waitResult == WAIT_OBJECT_0 and gotExitCode == TRUE, PrototypeError::ChildWaitFailed));

    if (strcmp(mode, ForeignFaultMode) == 0 or strcmp(mode, NestedFaultMode) == 0)
    {
        SC_TRY(prototypeCheck(exitCode == static_cast<DWORD>(EXCEPTION_ACCESS_VIOLATION),
                              PrototypeError::ForeignFaultNotForwarded));
    }
    else
    {
        SC_TRY(prototypeCheck(exitCode == static_cast<DWORD>(EXCEPTION_ACCESS_VIOLATION) or
                                  exitCode == static_cast<DWORD>(EXCEPTION_STACK_OVERFLOW),
                              PrototypeError::GuardOverflowNotClassified));
    }
#else
    (void)executable;
    // Concurrent probes are joined before this fork. Running the mode directly preserves isolation under an explicit
    // emulator, where asking the kernel to execute the foreign-architecture binary again would require binfmt support.
    const pid_t child = fork();
    SC_TRY(prototypeCheck(child >= 0, PrototypeError::ChildStartFailed));
    if (child == 0)
    {
        const Result childResult = runChildMode(mode);
        _exit(childResult ? 0 : 127);
    }

    int status = 0;
    SC_TRY(prototypeCheck(waitpid(child, &status, 0) == child, PrototypeError::ChildWaitFailed));
    if (strcmp(mode, ForeignFaultMode) == 0 or strcmp(mode, NestedFaultMode) == 0)
    {
        SC_TRY(prototypeCheck(WIFEXITED(status) and WEXITSTATUS(status) == ForeignHandlerExitCode,
                              PrototypeError::ForeignFaultNotForwarded));
    }
    else
    {
        SC_TRY(prototypeCheck(WIFEXITED(status) and WEXITSTATUS(status) == GuardOverflowExitCode,
                              PrototypeError::GuardOverflowNotClassified));
    }
#endif
    return Result(true);
}
#endif

#if !SC_FIBERS_STACK_GROWTH_HAS_ASAN
static Result runConcurrentGrowthProbe()
{
    static constexpr size_t NumWorkers = 4;

    struct Shared
    {
        Semaphore ready;
        Semaphore start;
    };
    struct WorkerState
    {
        StackGrowthPrototype prototype;
        Result               result       = prototypeFailure(PrototypeError::ConcurrentWorkerNotRun);
        size_t               growthEvents = 0;
    };

    Shared      shared;
    WorkerState states[NumWorkers];
    Thread      threads[NumWorkers];

#if !SC_PLATFORM_WINDOWS
    StackGrowthPrototype processHandlerOwner;
    SC_TRY(processHandlerOwner.installProcessHandler());
#endif

    size_t startedThreads = 0;
    for (; startedThreads < NumWorkers; ++startedThreads)
    {
        WorkerState* state       = &states[startedThreads];
        Result       startResult = threads[startedThreads].start(Function<void(Thread&)>(
            [state, &shared](Thread&)
            {
                state->result = state->prototype.reserve();
                if (state->result)
                {
                    state->result = state->prototype.installHandler();
                }
                if (state->result)
                {
                    state->result = state->prototype.prepareContext(StackGrowthPrototype::fiberEntry);
                }
                shared.ready.release();
                shared.start.acquire();
                if (state->result)
                {
                    state->result = state->prototype.runPrepared();
                }
                if (state->result)
                {
                    state->growthEvents = state->prototype.growthEvents;
                }
                Result closeResult = state->prototype.close();
                if (state->result)
                {
                    state->result = closeResult;
                }
                else
                {
                    SC_THREADING_ASSERT_RELEASE(closeResult);
                }
            }));
        if (not startResult)
        {
            for (size_t threadIndex = 0; threadIndex < startedThreads; ++threadIndex)
            {
                shared.start.release();
                SC_THREADING_ASSERT_RELEASE(threads[threadIndex].join());
            }
            return startResult;
        }
    }

    for (size_t threadIndex = 0; threadIndex < NumWorkers; ++threadIndex)
    {
        shared.ready.acquire();
    }

    bool allWorkersPrepared  = true;
    bool foreignCloseRefused = true;
    for (size_t threadIndex = 0; threadIndex < NumWorkers; ++threadIndex)
    {
        allWorkersPrepared = allWorkersPrepared and states[threadIndex].result;
        if (allWorkersPrepared)
        {
            foreignCloseRefused = foreignCloseRefused and not states[threadIndex].prototype.close();
        }
    }
#if !SC_PLATFORM_WINDOWS
    bool earlyCloseRefused = false;
    if (allWorkersPrepared)
    {
        earlyCloseRefused = not processHandlerOwner.closeProcessHandler();
    }
#endif
    for (size_t threadIndex = 0; threadIndex < NumWorkers; ++threadIndex)
    {
        shared.start.release();
    }

    Result probeResult = Result(true);
    for (size_t threadIndex = 0; threadIndex < NumWorkers; ++threadIndex)
    {
        Result joinResult = threads[threadIndex].join();
        SC_THREADING_ASSERT_RELEASE(joinResult);
        if (not states[threadIndex].result)
        {
            probeResult = states[threadIndex].result;
        }
        else if (states[threadIndex].growthEvents < 2)
        {
            probeResult = prototypeFailure(PrototypeError::ConcurrentGrowthInsufficient);
        }
    }

#if !SC_PLATFORM_WINDOWS
    if (allWorkersPrepared and not earlyCloseRefused)
    {
        probeResult = prototypeFailure(PrototypeError::EarlyProcessCloseAccepted);
    }
    Result processCloseResult = processHandlerOwner.close();
    if (probeResult)
    {
        probeResult = processCloseResult;
    }
    else
    {
        SC_THREADING_ASSERT_RELEASE(processCloseResult);
    }
#endif
    if (allWorkersPrepared and not foreignCloseRefused)
    {
        probeResult = prototypeFailure(PrototypeError::ForeignThreadCloseAccepted);
    }
    return probeResult;
}
#endif

static Result runStackGrowthPrototype(const char* executable)
{
    Console console;
    Console::tryAttachingToParentConsole();
    if (isDebuggerAttached())
    {
        console.print("Fibers stack growth prototype skipped: attached debugger owns fault delivery\n");
        return Result(true);
    }
#if SC_FIBERS_STACK_GROWTH_HAS_ASAN
    (void)executable;
    console.print("Fibers stack growth prototype skipped: AddressSanitizer owns stack fault handling\n");
    return Result(true);
#else
    {
        StackGrowthPrototype prototype;
        SC_TRY(prototype.run());

        console.print("Fibers stack growth prototype succeeded\n");
        console.print("  reservedBytes={} initialCommittedBytes={} growthBytes={} growthEvents={} committedBytes={}\n",
                      prototype.reservationSize, prototype.initialCommittedBytes,
                      static_cast<size_t>(StackGrowthPrototype::GrowthBytes), prototype.growthEvents,
                      prototype.committedBytes());
    }
    SC_TRY(runConcurrentGrowthProbe());
    SC_TRY(runChildProbe(executable, GuardOverflowMode));
    SC_TRY(runChildProbe(executable, ForeignFaultMode));
    SC_TRY(runChildProbe(executable, NestedFaultMode));
#if SC_PLATFORM_WINDOWS
    console.print("  concurrentWorkers=4 foreignCloseProbe=passed guardOverflowProbe=passed "
                  "foreignFaultProbe=passed nestedFaultProbe=passed\n");
#else
    console.print("  concurrentWorkers=4 earlyCloseProbe=passed foreignCloseProbe=passed guardOverflowProbe=passed "
                  "foreignFaultProbe=passed nestedFaultProbe=passed\n");
#endif
    return Result(true);
#endif
}
} // namespace
} // namespace SC

int main(int argc, char** argv)
{
    SC::Result result = argc == 2 ? SC::runChildMode(argv[1]) : SC::runStackGrowthPrototype(argv[0]);
    if (not result)
    {
        SC::Console console;
        SC::Console::tryAttachingToParentConsole();
        char                        message[256];
        const SC::ResultErrorFormat formatted = SC::formatPrototypeError(result, message);
        if (formatted)
            console.print("FibersStackGrowthPrototype failed: {}\n",
                          SC::StringView::fromNullTerminated(message, SC::StringEncoding::Ascii));
        else
            console.print("FibersStackGrowthPrototype failed: error category {}, code {}\n", result.category().value,
                          result.errorValue());
        return -1;
    }
    return 0;
}
