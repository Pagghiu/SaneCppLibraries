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

#include "../../Libraries/Fibers/Internal/FiberContext.h"
#include "../../Libraries/Strings/Console.h"
#include <string.h>

#if SC_PLATFORM_WINDOWS
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#define SC_FIBERS_STACK_GROWTH_NO_INLINE __declspec(noinline)
#else
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
struct StackGrowthPrototype;

// Signal handlers are process-wide, but only the installing thread may own and grow this experimental stack.
static thread_local StackGrowthPrototype* activePrototype    = nullptr;
static StackGrowthPrototype*              installedPrototype = nullptr;

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
#else
    struct sigaction      previousSegmentationAction  = {};
    struct sigaction      previousBusAction           = {};
    stack_t               previousSignalStack         = {};
    bool                  signalStackInstalled        = false;
    bool                  segmentationActionInstalled = false;
    bool                  busActionInstalled          = false;
    volatile sig_atomic_t handlingFault               = 0;
    alignas(16) char signalStackMemory[128 * 1024]    = {};
#endif

    ~StackGrowthPrototype() { close(); }

    Result run()
    {
        SC_TRY(prepare(fiberEntry));

        switchToFiber();

        SC_TRY_MSG(growthEvents >= 2, "Stack growth prototype did not cross enough commit boundaries");
        SC_TRY_MSG(committedBytes() > initialCommittedBytes,
                   "Stack growth prototype did not expand its committed range");
        SC_TRY_MSG(checksum != 0, "Stack growth prototype recursion was optimized away");
        return Result(true);
    }

    Result runGuardOverflow()
    {
        SC_TRY(prepare(overflowFiberEntry));
        switchToFiber();
        return Result::Error("Stack growth guard-overflow probe returned unexpectedly");
    }

    Result runForeignFault()
    {
        SC_TRY(reserve());
        SC_TRY(installHandler());
        *reinterpret_cast<volatile int*>(static_cast<size_t>(1)) = 1;
        return Result::Error("Stack growth foreign-fault probe returned unexpectedly");
    }

    Result prepare(FiberContextEntry entry)
    {
        SC_TRY(reserve());
        SC_TRY(installHandler());
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
        SC_TRY_MSG(systemPageSize > 0, "Stack growth prototype could not query the page size");
        pageSize = static_cast<size_t>(systemPageSize);
#endif
        SC_TRY_MSG(InitialCommitBytes % pageSize == 0 and GrowthBytes % pageSize == 0,
                   "Stack growth prototype commit sizes must be page aligned");

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
        SC_TRY_MSG(reservation != nullptr, "Stack growth prototype reservation failed");

        guardEnd       = static_cast<char*>(reservation) + pageSize;
        stackEnd       = guardEnd + UsableBytes;
        committedBegin = stackEnd - InitialCommitBytes;
        SC_TRY_MSG(commit(committedBegin, InitialCommitBytes), "Stack growth prototype initial commit failed");
#if SC_PLATFORM_WINDOWS
        SC_TRY_MSG(prepareWindowsGrowthGuard(), "Stack growth prototype initial guard preparation failed");
#endif
        initialCommittedBytes = committedBytes();
        SC_TRY_MSG(initialCommittedBytes > 0, "Stack growth prototype could not measure initial commitment");
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
        SC_TRY_MSG(activePrototype == nullptr and installedPrototype == nullptr,
                   "Another stack growth prototype is already active");
        activePrototype    = this;
        installedPrototype = this;
#if !SC_PLATFORM_WINDOWS
        stack_t signalStack = {};
        signalStack.ss_sp   = signalStackMemory;
        signalStack.ss_size = sizeof(signalStackMemory);
        if (sigaltstack(&signalStack, &previousSignalStack) != 0)
        {
            activePrototype    = nullptr;
            installedPrototype = nullptr;
            return Result::Error("Stack growth prototype could not install its alternate signal stack");
        }
        signalStackInstalled = true;

        struct sigaction action = {};
        action.sa_sigaction     = signalHandler;
        action.sa_flags         = SA_SIGINFO | SA_ONSTACK;
        sigemptyset(&action.sa_mask);
        if (sigaction(SIGSEGV, &action, &previousSegmentationAction) != 0)
        {
            close();
            return Result::Error("Stack growth prototype could not install its segmentation signal handler");
        }
        segmentationActionInstalled = true;
        if (sigaction(SIGBUS, &action, &previousBusAction) != 0)
        {
            close();
            return Result::Error("Stack growth prototype could not install its bus signal handler");
        }
        busActionInstalled = true;
#endif
        return Result(true);
    }

    void close()
    {
#if !SC_PLATFORM_WINDOWS
        if (busActionInstalled)
        {
            restoreSignalAction(SIGBUS, previousBusAction);
            busActionInstalled = false;
        }
        if (segmentationActionInstalled)
        {
            restoreSignalAction(SIGSEGV, previousSegmentationAction);
            segmentationActionInstalled = false;
        }
        if (signalStackInstalled)
        {
            stack_t currentSignalStack = {};
            if (sigaltstack(nullptr, &currentSignalStack) == 0 and currentSignalStack.ss_sp == signalStackMemory)
            {
                sigaltstack(&previousSignalStack, nullptr);
            }
            signalStackInstalled = false;
        }
#endif
        if (activePrototype == this)
        {
            activePrototype = nullptr;
        }
        if (installedPrototype == this)
        {
            installedPrototype = nullptr;
        }
        if (reservation != nullptr)
        {
#if SC_PLATFORM_WINDOWS
            VirtualFree(reservation, 0, MEM_RELEASE);
#else
            munmap(reservation, reservationSize);
#endif
            reservation = nullptr;
        }
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
    static void restoreSignalAction(int signal, const struct sigaction& previous)
    {
        struct sigaction current = {};
        if (sigaction(signal, nullptr, &current) == 0 and (current.sa_flags & SA_SIGINFO) != 0 and
            current.sa_sigaction == signalHandler)
        {
            sigaction(signal, &previous, nullptr);
        }
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
        if ((signal == SIGSEGV or signal == SIGBUS) and prototype != nullptr and info != nullptr and
            prototype->handlingFault == 0)
        {
            prototype->handlingFault = 1;
            const bool grown         = prototype->tryGrow(info->si_addr, interruptedStackPointer(context));
            prototype->handlingFault = 0;
            if (grown)
            {
                return;
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

static constexpr const char* GuardOverflowMode      = "--guard-overflow";
static constexpr const char* ForeignFaultMode       = "--foreign-fault";
static constexpr int         ForeignHandlerExitCode = 73;

#if !SC_PLATFORM_WINDOWS
static void foreignFaultHandler(int) { _exit(ForeignHandlerExitCode); }
#endif

static Result runChildMode(const char* mode)
{
#if SC_PLATFORM_WINDOWS
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
#else
    if (strcmp(mode, ForeignFaultMode) == 0)
    {
        struct sigaction action = {};
        action.sa_handler       = foreignFaultHandler;
        sigemptyset(&action.sa_mask);
        SC_TRY_MSG(sigaction(SIGSEGV, &action, nullptr) == 0 and sigaction(SIGBUS, &action, nullptr) == 0,
                   "Stack growth prototype could not install the foreign-fault probe handler");
    }
#endif

    StackGrowthPrototype prototype;
    if (strcmp(mode, GuardOverflowMode) == 0)
    {
        return prototype.runGuardOverflow();
    }
    if (strcmp(mode, ForeignFaultMode) == 0)
    {
        return prototype.runForeignFault();
    }
    return Result::Error("Unknown stack growth prototype child mode");
}

#if !SC_FIBERS_STACK_GROWTH_HAS_ASAN
static Result runChildProbe(const char* executable, const char* mode)
{
#if SC_PLATFORM_WINDOWS
    char         commandLine[4096];
    const size_t executableLength = strlen(executable);
    const size_t modeLength       = strlen(mode);
    SC_TRY_MSG(executableLength + modeLength + 5 <= sizeof(commandLine),
               "Stack growth prototype executable path is too long");

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
    SC_TRY_MSG(
        CreateProcessA(executable, commandLine, nullptr, nullptr, FALSE, 0, nullptr, nullptr, &startup, &process),
        "Stack growth prototype could not create its child process");
    const DWORD waitResult  = WaitForSingleObject(process.hProcess, INFINITE);
    DWORD       exitCode    = 0;
    const BOOL  gotExitCode = GetExitCodeProcess(process.hProcess, &exitCode);
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    SC_TRY_MSG(waitResult == WAIT_OBJECT_0 and gotExitCode == TRUE,
               "Stack growth prototype could not wait for its child process");

    if (strcmp(mode, ForeignFaultMode) == 0)
    {
        SC_TRY_MSG(exitCode == static_cast<DWORD>(EXCEPTION_ACCESS_VIOLATION),
                   "Stack growth prototype swallowed a foreign Windows fault");
    }
    else
    {
        SC_TRY_MSG(exitCode == static_cast<DWORD>(EXCEPTION_ACCESS_VIOLATION) or
                       exitCode == static_cast<DWORD>(EXCEPTION_STACK_OVERFLOW),
                   "Stack growth prototype guard overflow did not terminate with a memory fault");
    }
#else
    const pid_t child = fork();
    SC_TRY_MSG(child >= 0, "Stack growth prototype could not fork its child process");
    if (child == 0)
    {
        execl(executable, executable, mode, static_cast<char*>(nullptr));
        _exit(127);
    }

    int status = 0;
    SC_TRY_MSG(waitpid(child, &status, 0) == child, "Stack growth prototype could not wait for its child process");
    if (strcmp(mode, ForeignFaultMode) == 0)
    {
        SC_TRY_MSG(WIFEXITED(status) and WEXITSTATUS(status) == ForeignHandlerExitCode,
                   "Stack growth prototype did not forward a foreign POSIX fault");
    }
    else
    {
        SC_TRY_MSG(WIFSIGNALED(status) and (WTERMSIG(status) == SIGSEGV or WTERMSIG(status) == SIGBUS),
                   "Stack growth prototype guard overflow did not terminate with a memory signal");
    }
#endif
    return Result(true);
}
#endif

static Result runStackGrowthPrototype(const char* executable)
{
    Console console;
    Console::tryAttachingToParentConsole();
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
    SC_TRY(runChildProbe(executable, GuardOverflowMode));
    SC_TRY(runChildProbe(executable, ForeignFaultMode));
    console.print("  guardOverflowProbe=passed foreignFaultProbe=passed\n");
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
        console.print("FibersStackGrowthPrototype failed: {}\n", result.message);
        return -1;
    }
    return 0;
}
