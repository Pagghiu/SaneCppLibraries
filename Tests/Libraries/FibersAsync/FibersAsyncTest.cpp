// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "Libraries/FibersAsync/FibersAsync.h"
#include "Libraries/FileSystem/FileSystem.h"
#include "Libraries/Memory/String.h"
#include "Libraries/Process/Process.h"
#include "Libraries/Socket/Socket.h"
#include "Libraries/Strings/Path.h"
#include "Libraries/Strings/StringBuilder.h"
#include "Libraries/Testing/Testing.h"
#include "Libraries/Threading/Atomic.h"
#include "Libraries/Threading/Threading.h"

#include <signal.h>
#if SC_PLATFORM_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif
#if SC_PLATFORM_APPLE
#include <sys/sysctl.h>
#endif
#if not SC_PLATFORM_WINDOWS
#include <unistd.h>
#endif

namespace SC
{
struct FibersAsyncTest;
}

namespace
{
#if SC_PLATFORM_WINDOWS
static constexpr int WindowsSignalBreak = 21;
#endif

#if SC_PLATFORM_APPLE
static bool isDebuggerAttached()
{
    int               mib[4] = {CTL_KERN, KERN_PROC, KERN_PROC_PID, static_cast<int>(::getpid())};
    struct kinfo_proc info   = {};
    size_t            size   = sizeof(info);
    if (::sysctl(mib, 4, &info, &size, nullptr, 0) != 0)
    {
        return false;
    }
    return (info.kp_proc.p_flag & P_TRACED) != 0;
}
#endif
} // namespace

struct SC::FibersAsyncTest : public SC::TestCase
{
    FibersAsyncTest(SC::TestReport& report) : TestCase(report, "FibersAsyncTest")
    {
#if SC_COMPILER_FILC
        if (not report.quietMode)
        {
            report.console.printLine("FibersAsyncTest - Skipping under Fil-C: manual stack switching is unsupported");
        }
        return;
#else
        if (test_section("sleep"))
        {
            sleep();
        }
        if (test_section("run aliases"))
        {
            runAliases();
        }
        if (test_section("cpu io fairness"))
        {
            cpuIOFairness();
        }
        if (test_section("cancel sleep"))
        {
            cancelSleep();
        }
        if (test_section("cancel sleep stress"))
        {
            cancelSleepStress();
        }
        if (test_section("cross thread sleep"))
        {
            crossThreadSleep();
        }
        if (test_section("worker pool sleep"))
        {
            workerPoolSleep();
        }
        if (test_section("worker pool cancel sleep"))
        {
            workerPoolCancelSleep();
        }
        if (test_section("worker pool cancel sleep stress"))
        {
            workerPoolCancelSleepStress();
        }
        if (test_section("worker pool sleep cancel complete race"))
        {
            workerPoolSleepCancelCompleteRace();
        }
        if (test_section("worker pool owner command race stress"))
        {
            workerPoolOwnerCommandRaceStress();
        }
        if (test_section("command queue overflow"))
        {
            commandQueueOverflow();
        }
        if (test_section("socket accept"))
        {
            socketAccept();
        }
        if (test_section("socket connect"))
        {
            socketConnect();
        }
        if (test_section("socket send receive"))
        {
            socketSendReceive();
        }
        if (test_section("cross thread socket accept connect"))
        {
            crossThreadSocketAcceptConnect();
        }
        if (test_section("cross thread socket send receive"))
        {
            crossThreadSocketSendReceive();
        }
        if (test_section("worker pool socket send receive"))
        {
            workerPoolSocketSendReceive();
        }
        if (test_section("worker pool cancel socket receive"))
        {
            workerPoolCancelSocketReceive();
        }
        if (test_section("cancel socket operations"))
        {
            cancelSocketOperations();
        }
        if (test_section("cancel active socket operations"))
        {
            cancelActiveSocketOperations();
        }
        if (test_section("multi fiber echo"))
        {
            multiFiberEcho();
        }
        if (test_section("udp send receive"))
        {
            udpSendReceive();
        }
        if (test_section("file send"))
        {
            fileSend();
        }
        if (test_section("file read write"))
        {
            fileReadWrite();
        }
        if (test_section("file read exact"))
        {
            fileReadExact();
        }
        if (test_section("file offsets"))
        {
            fileOffsets();
        }
        if (test_section("file poll"))
        {
            filePoll();
        }
        if (test_section("cancel file poll"))
        {
            cancelFilePoll();
        }
        if (test_section("process exit"))
        {
            processExit();
        }
        if (test_section("cancel process exit"))
        {
            cancelProcessExit();
        }
        if (test_section("signal"))
        {
            signal();
        }
#if SC_PLATFORM_WINDOWS
        if (test_section("windows signal child", Execute::OnlyExplicit))
        {
            windowsSignalChild();
        }
#endif
        if (test_section("cancel signal"))
        {
            cancelSignal();
        }
        if (test_section("worker pool cancel signal"))
        {
            workerPoolCancelSignal();
        }
#endif
    }

    void createTCPSocketPair(AsyncEventLoop& eventLoop, SocketDescriptor& client, SocketDescriptor& serverSideClient)
    {
        SocketDescriptor serverSocket;
        uint16_t         tcpPort = report.mapPort(6050);
        SocketIPAddress  nativeAddress;
        SC_TEST_EXPECT(nativeAddress.fromAddressPort("127.0.0.1", tcpPort));
        SC_TEST_EXPECT(serverSocket.create(nativeAddress.getAddressFamily()));

        {
            SocketServer server(serverSocket);
            SC_TEST_EXPECT(server.bind(nativeAddress));
            SC_TEST_EXPECT(server.listen(1));
        }

        SC_TEST_EXPECT(client.create(nativeAddress.getAddressFamily()));
        SC_TEST_EXPECT(SocketClient(client).connect("127.0.0.1", tcpPort));
        SC_TEST_EXPECT(SocketServer(serverSocket).accept(nativeAddress.getAddressFamily(), serverSideClient));
        SC_TEST_EXPECT(client.setBlocking(false));
        SC_TEST_EXPECT(serverSideClient.setBlocking(false));

        SC_TEST_EXPECT(eventLoop.associateExternallyCreatedSocket(client));
        SC_TEST_EXPECT(eventLoop.associateExternallyCreatedSocket(serverSideClient));
        SC_TEST_EXPECT(serverSocket.close());
    }

    void cancelSubmittedOperationIfPending(FiberAsyncIO& io, FiberTask& task, bool& completedWithError)
    {
        SC_TEST_EXPECT(io.runOnce());
        SC_TEST_EXPECT(io.runNoWait());
        if (task.isActive())
        {
            SC_TEST_EXPECT(io.cancelAll());
            SC_TEST_EXPECT(io.run());
            SC_TEST_EXPECT(completedWithError);
            SC_TEST_EXPECT(not task.result());
        }
        else
        {
            SC_TEST_EXPECT(task.isCompleted());
        }
    }

    [[nodiscard]] bool waitForAllWorkersToPark(const FiberWorkerPool& workerPool)
    {
        for (size_t iteration = 0; iteration < 1000; ++iteration)
        {
            if (workerPool.parkedWorkerCount() == workerPool.workerCount())
            {
                return true;
            }
            Thread::Sleep(1);
        }
        return false;
    }

    bool skipProcessExitOnLinuxRosetta()
    {
#if SC_PLATFORM_LINUX && defined(__x86_64__)
        FileSystem fs;
        if (fs.init("/") and fs.exists("/proc/sys/fs/binfmt_misc/RosettaLinux"))
        {
            report.console.printLine("FibersAsyncTest - Skipping AsyncProcessExit under Linux x86_64 Rosetta");
            return true;
        }
#endif
        return false;
    }

    void sleep()
    {
        struct State
        {
            FiberAsyncIO* io        = nullptr;
            int           completed = 0;
        };

        AsyncEventLoop eventLoop;
        SC_TEST_EXPECT(eventLoop.create());

        FiberScheduler scheduler;
        FiberAsyncIO   io(scheduler, eventLoop);
        FiberTask      firstTask;
        FiberTask      secondTask;

        char       firstStackMemory[64 * 1024]  = {};
        char       secondStackMemory[64 * 1024] = {};
        FiberStack firstStack({firstStackMemory, sizeof(firstStackMemory)});
        FiberStack secondStack({secondStackMemory, sizeof(secondStackMemory)});
        firstStack.fillHighWaterMark();
        secondStack.fillHighWaterMark();

        State state;
        state.io = &io;

        SC_TEST_EXPECT(scheduler.spawn(firstTask, firstStack,
                                       FiberTask::Procedure(
                                           [&state](FiberScheduler&)
                                           {
                                               SC_TRY(state.io->sleep(TimeMs{1}));
                                               state.completed++;
                                               return Result(true);
                                           })));
        SC_TEST_EXPECT(scheduler.spawn(secondTask, secondStack,
                                       FiberTask::Procedure(
                                           [&state](FiberScheduler&)
                                           {
                                               SC_TRY(state.io->sleep(TimeMs{1}));
                                               state.completed++;
                                               return Result(true);
                                           })));

        SC_TEST_EXPECT(io.run());
        SC_TEST_EXPECT(state.completed == 2);
        SC_TEST_EXPECT(firstTask.isCompleted());
        SC_TEST_EXPECT(secondTask.isCompleted());
        SC_TEST_EXPECT(firstTask.result());
        SC_TEST_EXPECT(secondTask.result());
        SC_TEST_EXPECT(firstStack.highWaterUsedBytes() > 0);
        SC_TEST_EXPECT(secondStack.highWaterUsedBytes() > 0);
        SC_TEST_EXPECT(eventLoop.close());
    }

    void runAliases()
    {
        struct State
        {
            FiberAsyncIO* io              = nullptr;
            int           idleRan         = 0;
            int           completeRan     = 0;
            bool          ownerIdleCalled = false;
        };

        AsyncEventLoop eventLoop;
        SC_TEST_EXPECT(eventLoop.create());

        FiberScheduler scheduler;
        FiberAsyncIO   io(scheduler, eventLoop);

        FiberTask idleTask;
        FiberTask completeTask;

        char       idleStackMemory[64 * 1024]     = {};
        char       completeStackMemory[64 * 1024] = {};
        FiberStack idleStack({idleStackMemory, sizeof(idleStackMemory)});
        FiberStack completeStack({completeStackMemory, sizeof(completeStackMemory)});

        State state;
        state.io = &io;

        SC_TEST_EXPECT(scheduler.spawn(idleTask, idleStack,
                                       FiberTask::Procedure(
                                           [&state](FiberScheduler&)
                                           {
                                               state.idleRan++;
                                               return Result(true);
                                           })));
        SC_TEST_EXPECT(io.runUntilIdle());
        SC_TEST_EXPECT(state.idleRan == 1);
        SC_TEST_EXPECT(idleTask.isCompleted());

        SC_TEST_EXPECT(scheduler.spawn(completeTask, completeStack,
                                       FiberTask::Procedure(
                                           [&state](FiberScheduler&)
                                           {
                                               SC_TRY(state.io->sleep(TimeMs{1}));
                                               state.completeRan++;
                                               return Result(true);
                                           })));
        SC_TEST_EXPECT(io.runUntilComplete());
        SC_TEST_EXPECT(state.completeRan == 1);
        SC_TEST_EXPECT(completeTask.isCompleted());

        SC_TEST_EXPECT(io.runOwnerUntilIdle());
        state.ownerIdleCalled = true;
        SC_TEST_EXPECT(io.runOwnerUntilComplete());
        SC_TEST_EXPECT(state.ownerIdleCalled);
        SC_TEST_EXPECT(eventLoop.close());
    }

    void cpuIOFairness()
    {
        static constexpr int NumCpuSteps = 8;

        struct State
        {
            FiberAsyncIO* io                  = nullptr;
            int           cpuSteps            = 0;
            int           sleepCompletedAtCpu = -1;
            bool          cpuCompleted        = false;
        };

        AsyncEventLoop eventLoop;
        SC_TEST_EXPECT(eventLoop.create());

        FiberScheduler scheduler;
        FiberAsyncIO   io(scheduler, eventLoop);
        FiberTask      cpuTask;
        FiberTask      sleepTask;

        char       cpuStackMemory[64 * 1024]   = {};
        char       sleepStackMemory[64 * 1024] = {};
        FiberStack cpuStack({cpuStackMemory, sizeof(cpuStackMemory)});
        FiberStack sleepStack({sleepStackMemory, sizeof(sleepStackMemory)});

        State state;
        state.io = &io;

        SC_TEST_EXPECT(scheduler.spawn(cpuTask, cpuStack,
                                       FiberTask::Procedure(
                                           [&state](FiberScheduler& scheduler)
                                           {
                                               for (int idx = 0; idx < NumCpuSteps; ++idx)
                                               {
                                                   state.cpuSteps++;
                                                   Thread::Sleep(1);
                                                   SC_TRY(scheduler.yield());
                                               }
                                               state.cpuCompleted = true;
                                               return Result(true);
                                           })));
        SC_TEST_EXPECT(scheduler.spawn(sleepTask, sleepStack,
                                       FiberTask::Procedure(
                                           [&state](FiberScheduler&)
                                           {
                                               SC_TRY(state.io->sleep(TimeMs{1}));
                                               state.sleepCompletedAtCpu = state.cpuSteps;
                                               return Result(true);
                                           })));

        SC_TEST_EXPECT(io.run());
        SC_TEST_EXPECT(cpuTask.result());
        SC_TEST_EXPECT(sleepTask.result());
        SC_TEST_EXPECT(state.cpuCompleted);
        SC_TEST_EXPECT(state.sleepCompletedAtCpu >= 0);
        SC_TEST_EXPECT(state.sleepCompletedAtCpu < NumCpuSteps);
        SC_TEST_EXPECT(eventLoop.close());
    }

    void cancelSleep()
    {
        struct State
        {
            FiberAsyncIO* io       = nullptr;
            bool          canceled = false;
        };

        AsyncEventLoop eventLoop;
        SC_TEST_EXPECT(eventLoop.create());

        FiberScheduler scheduler;
        FiberAsyncIO   io(scheduler, eventLoop);
        FiberTask      task;

        char       stackMemory[64 * 1024] = {};
        FiberStack stack({stackMemory, sizeof(stackMemory)});

        State state;
        state.io = &io;

        SC_TEST_EXPECT(scheduler.spawn(task, stack,
                                       FiberTask::Procedure(
                                           [&state](FiberScheduler&)
                                           {
                                               Result result  = state.io->sleep(TimeMs{10 * 1000});
                                               state.canceled = not result;
                                               return result;
                                           })));

        SC_TEST_EXPECT(io.runOnce());
        SC_TEST_EXPECT(task.isActive());
        SC_TEST_EXPECT(io.cancelAll());
        SC_TEST_EXPECT(io.run());
        SC_TEST_EXPECT(state.canceled);
        SC_TEST_EXPECT(task.isCompleted());
        SC_TEST_EXPECT(not task.result());
        SC_TEST_EXPECT(eventLoop.close());
    }

    void cancelSleepStress()
    {
        static constexpr int NumIterations = 32;

        struct State
        {
            FiberAsyncIO* io                = nullptr;
            int           completed         = 0;
            int           canceled          = 0;
            int           immediateCanceled = 0;
        };

        AsyncEventLoop eventLoop;
        SC_TEST_EXPECT(eventLoop.create());

        FiberScheduler scheduler;
        FiberAsyncIO   io(scheduler, eventLoop);

        State state;
        state.io = &io;

        for (int idx = 0; idx < NumIterations; ++idx)
        {
            FiberTask  task;
            char       stackMemory[64 * 1024] = {};
            FiberStack stack({stackMemory, sizeof(stackMemory)});

            SC_TEST_EXPECT(scheduler.spawn(task, stack,
                                           FiberTask::Procedure(
                                               [&state](FiberScheduler&)
                                               {
                                                   Result result = state.io->sleep(TimeMs{1000});
                                                   if (result)
                                                   {
                                                       state.completed++;
                                                   }
                                                   else
                                                   {
                                                       state.canceled++;
                                                   }
                                                   return result;
                                               })));

            SC_TEST_EXPECT(io.runOnce());
            SC_TEST_EXPECT(task.isActive());
            SC_TEST_EXPECT(io.runNoWait());
            SC_TEST_EXPECT(io.cancelAll());
            SC_TEST_EXPECT(io.run());
            SC_TEST_EXPECT(task.isCompleted());
            SC_TEST_EXPECT(not task.result());
        }

        for (int idx = 0; idx < NumIterations; ++idx)
        {
            FiberTask  task;
            char       stackMemory[64 * 1024] = {};
            FiberStack stack({stackMemory, sizeof(stackMemory)});

            SC_TEST_EXPECT(scheduler.spawn(task, stack,
                                           FiberTask::Procedure(
                                               [&state](FiberScheduler&)
                                               {
                                                   Result result = state.io->sleep(TimeMs{1000});
                                                   if (not result)
                                                   {
                                                       state.immediateCanceled++;
                                                   }
                                                   return result;
                                               })));

            SC_TEST_EXPECT(io.cancelAll());
            SC_TEST_EXPECT(io.run());
            SC_TEST_EXPECT(task.isCompleted());
            SC_TEST_EXPECT(not task.result());
        }

        SC_TEST_EXPECT(state.completed == 0);
        SC_TEST_EXPECT(state.canceled == NumIterations);
        SC_TEST_EXPECT(state.immediateCanceled == NumIterations);
        SC_TEST_EXPECT(not scheduler.hasActiveFibers());
        SC_TEST_EXPECT(eventLoop.close());
    }

    void crossThreadSleep()
    {
        struct State
        {
            FiberScheduler* scheduler = nullptr;
            FiberAsyncIO*   io        = nullptr;

            Atomic<bool>    entered    = false;
            Atomic<bool>    workerDone = false;
            Atomic<int32_t> workerRuns = 0;

            uint64_t ownerThreadID  = 0;
            uint64_t startThreadID  = 0;
            uint64_t resumeThreadID = 0;
            Result   workerResult   = Result(true);
        };

        AsyncEventLoop eventLoop;
        SC_TEST_EXPECT(eventLoop.create());

        FiberScheduler    scheduler;
        FiberAsyncCommand commandStorage[4];
        FiberAsyncIO      io(scheduler, eventLoop, commandStorage);
        FiberTask         task;
        static char       stackMemory[64 * 1024] = {};
        FiberStack        stack({stackMemory, sizeof(stackMemory)});

        State state;
        state.scheduler     = &scheduler;
        state.io            = &io;
        state.ownerThreadID = Thread::CurrentThreadID();

        SC_TEST_EXPECT(scheduler.spawn(task, stack,
                                       FiberTask::Procedure(
                                           [&state](FiberScheduler&)
                                           {
                                               state.startThreadID = Thread::CurrentThreadID();
                                               state.entered.store(true);
                                               SC_TRY(state.io->sleep(TimeMs{1}));
                                               state.resumeThreadID = Thread::CurrentThreadID();
                                               return Result(true);
                                           })));

        Thread workerThread;
        auto   worker = [&state](Thread&)
        {
            while (state.scheduler->hasActiveFibers())
            {
                const bool hadReady = state.scheduler->hasReadyFibers();
                state.workerResult  = state.scheduler->runNoWait();
                if (not state.workerResult)
                    break;
                if (hadReady)
                    ++state.workerRuns;
                Thread::Sleep(1);
            }
            state.workerDone.store(true);
        };
        SC_TEST_EXPECT(workerThread.start(worker));

        for (int idx = 0; idx < 1000 and state.workerRuns.load() < 1 and not state.workerDone.load(); ++idx)
        {
            Thread::Sleep(1);
        }

        SC_TEST_EXPECT(state.entered.load());
        SC_TEST_EXPECT(state.workerRuns.load() >= 1);
        for (int idx = 0; idx < 1000 and not state.workerDone.load(); ++idx)
        {
            SC_TEST_EXPECT(io.runOwnerNoWait());
            Thread::Sleep(1);
        }
        SC_TEST_EXPECT(state.workerDone.load());
        SC_TEST_EXPECT(workerThread.join());
        SC_TEST_EXPECT(state.workerResult);
        SC_TEST_EXPECT(task.isCompleted());
        SC_TEST_EXPECT(task.result());
        SC_TEST_EXPECT(state.startThreadID != 0);
        SC_TEST_EXPECT(state.resumeThreadID != 0);
        SC_TEST_EXPECT(state.startThreadID != state.ownerThreadID);
        SC_TEST_EXPECT(eventLoop.close());
    }

    void workerPoolSleep()
    {
        static constexpr size_t NumWorkers = 2;

        struct State
        {
            FiberAsyncIO* io = nullptr;

            Atomic<bool> entered        = false;
            Atomic<bool> blockerEntered = false;

            uint64_t ownerThreadID  = 0;
            uint64_t startThreadID  = 0;
            uint64_t resumeThreadID = 0;
        };

        AsyncEventLoop eventLoop;
        SC_TEST_EXPECT(eventLoop.create());

        FiberScheduler    scheduler;
        FiberAsyncCommand commandStorage[4];
        FiberAsyncIO      io(scheduler, eventLoop, commandStorage);
        FiberTask         task;
        FiberTask         blockerTask;
        static char       stackMemory[64 * 1024]        = {};
        static char       blockerStackMemory[64 * 1024] = {};
        FiberStack        stack({stackMemory, sizeof(stackMemory)});
        FiberStack        blockerStack({blockerStackMemory, sizeof(blockerStackMemory)});
        FiberWorker       workers[NumWorkers];
        FiberWorkerThread threads[NumWorkers];
        FiberWorkerPool   workerPool;

        FiberWorkerPoolOptions workerPoolOptions;
        workerPoolOptions.idleSpinAttempts = 0;

        State state;
        state.io            = &io;
        state.ownerThreadID = Thread::CurrentThreadID();

        SC_TEST_EXPECT(scheduler.spawn(task, stack,
                                       FiberTask::Procedure(
                                           [&state](FiberScheduler&)
                                           {
                                               state.startThreadID = Thread::CurrentThreadID();
                                               state.entered.store(true);
                                               SC_TRY(state.io->sleep(TimeMs{1}));
                                               state.resumeThreadID = Thread::CurrentThreadID();
                                               return Result(true);
                                           })));
        SC_TEST_EXPECT(scheduler.spawn(blockerTask, blockerStack,
                                       FiberTask::Procedure(
                                           [&state](FiberScheduler&)
                                           {
                                               state.blockerEntered.store(true);
                                               return state.io->sleep(TimeMs{10 * 1000});
                                           })));

        SC_TEST_EXPECT(workerPool.start(scheduler, {workers, NumWorkers}, {threads, NumWorkers}, workerPoolOptions));
        for (int idx = 0; idx < 1000 and (not state.entered.load() or not state.blockerEntered.load()); ++idx)
        {
            Thread::Sleep(1);
        }
        SC_TEST_EXPECT(state.entered.load());
        SC_TEST_EXPECT(state.blockerEntered.load());
        SC_TEST_EXPECT(waitForAllWorkersToPark(workerPool));

        FiberWorkerDiagnostics beforeIODiagnostics;
        scheduler.workerDiagnostics({workers, NumWorkers}, beforeIODiagnostics);
        SC_TEST_EXPECT(beforeIODiagnostics.idleSpinIterations == 0);

        for (int idx = 0; idx < 1000 and not task.isCompleted(); ++idx)
        {
            SC_TEST_EXPECT(io.runOwnerNoWait());
            Thread::Sleep(1);
        }
        SC_TEST_EXPECT(task.isCompleted());

        FiberWorkerDiagnostics afterIODiagnostics;
        scheduler.workerDiagnostics({workers, NumWorkers}, afterIODiagnostics);
        SC_TEST_EXPECT(afterIODiagnostics.idleSpinIterations == 0);
        SC_TEST_EXPECT(afterIODiagnostics.parkedWakeups > beforeIODiagnostics.parkedWakeups);
        SC_TEST_EXPECT(scheduler.activeFiberCount() == 1);

        SC_TEST_EXPECT(scheduler.requestCancel(blockerTask));
        SC_TEST_EXPECT(io.runOwner());

        SC_TEST_EXPECT(workerPool.join());
        SC_TEST_EXPECT(workerPool.parkedWorkerCount() == 0);
        SC_TEST_EXPECT(task.isCompleted());
        SC_TEST_EXPECT(task.result());
        SC_TEST_EXPECT(blockerTask.isCompleted());
        SC_TEST_EXPECT(not blockerTask.result());
        SC_TEST_EXPECT(state.startThreadID != 0);
        SC_TEST_EXPECT(state.resumeThreadID != 0);
        SC_TEST_EXPECT(state.startThreadID != state.ownerThreadID);
        SC_TEST_EXPECT(state.resumeThreadID != state.ownerThreadID);
        SC_TEST_EXPECT(eventLoop.close());
    }

    void workerPoolCancelSleep()
    {
        static constexpr size_t NumWorkers = 2;

        struct State
        {
            FiberAsyncIO* io = nullptr;

            Atomic<bool> entered = false;

            bool     canceled       = false;
            uint64_t ownerThreadID  = 0;
            uint64_t startThreadID  = 0;
            uint64_t resumeThreadID = 0;
        };

        AsyncEventLoop eventLoop;
        SC_TEST_EXPECT(eventLoop.create());

        FiberScheduler    scheduler;
        FiberAsyncCommand commandStorage[4];
        FiberAsyncIO      io(scheduler, eventLoop, commandStorage);
        FiberTask         task;
        static char       stackMemory[64 * 1024] = {};
        FiberStack        stack({stackMemory, sizeof(stackMemory)});
        FiberWorker       workers[NumWorkers];
        FiberWorkerThread threads[NumWorkers];
        FiberWorkerPool   workerPool;

        State state;
        state.io            = &io;
        state.ownerThreadID = Thread::CurrentThreadID();

        SC_TEST_EXPECT(scheduler.spawn(task, stack,
                                       FiberTask::Procedure(
                                           [&state](FiberScheduler&)
                                           {
                                               state.startThreadID = Thread::CurrentThreadID();
                                               state.entered.store(true);
                                               Result result        = state.io->sleep(TimeMs{10 * 1000});
                                               state.resumeThreadID = Thread::CurrentThreadID();
                                               state.canceled       = not result;
                                               return result;
                                           })));

        SC_TEST_EXPECT(workerPool.start(scheduler, {workers, NumWorkers}, {threads, NumWorkers}));
        for (int idx = 0; idx < 1000 and not state.entered.load(); ++idx)
        {
            Thread::Sleep(1);
        }
        SC_TEST_EXPECT(state.entered.load());

        SC_TEST_EXPECT(io.runOwnerNoWait());
        SC_TEST_EXPECT(io.cancelAll());
        SC_TEST_EXPECT(io.runOwner());

        SC_TEST_EXPECT(not scheduler.hasActiveFibers());
        SC_TEST_EXPECT(workerPool.join());
        SC_TEST_EXPECT(task.isCompleted());
        SC_TEST_EXPECT(not task.result());
        SC_TEST_EXPECT(state.canceled);
        SC_TEST_EXPECT(state.startThreadID != 0);
        SC_TEST_EXPECT(state.resumeThreadID != 0);
        SC_TEST_EXPECT(state.startThreadID != state.ownerThreadID);
        SC_TEST_EXPECT(state.resumeThreadID != state.ownerThreadID);
        SC_TEST_EXPECT(eventLoop.close());
    }

    void workerPoolCancelSleepStress()
    {
        static constexpr size_t NumWorkers = 2;
        static constexpr size_t NumTasks   = 8;

        struct TaskState
        {
            FiberAsyncIO* io = nullptr;

            bool canceled = false;

            uint64_t startThreadID  = 0;
            uint64_t resumeThreadID = 0;
        };

        struct State
        {
            Atomic<int32_t> entered = 0;
            TaskState       tasks[NumTasks];
        };

        AsyncEventLoop eventLoop;
        SC_TEST_EXPECT(eventLoop.create());

        FiberScheduler    scheduler;
        FiberAsyncCommand commandStorage[NumTasks * 2];
        FiberAsyncIO      io(scheduler, eventLoop, commandStorage);
        FiberTask         tasks[NumTasks];
        static char       stackMemory[NumTasks][64 * 1024] = {};
        FiberWorker       workers[NumWorkers];
        FiberWorkerThread threads[NumWorkers];
        FiberWorkerPool   workerPool;

        State state;
        for (size_t idx = 0; idx < NumTasks; ++idx)
        {
            state.tasks[idx].io = &io;
            FiberStack stack({stackMemory[idx], sizeof(stackMemory[idx])});
            TaskState* taskState = &state.tasks[idx];
            SC_TEST_EXPECT(scheduler.spawn(tasks[idx], stack,
                                           FiberTask::Procedure(
                                               [&state, taskState](FiberScheduler&)
                                               {
                                                   taskState->startThreadID = Thread::CurrentThreadID();
                                                   state.entered.fetch_add(1);
                                                   Result result             = taskState->io->sleep(TimeMs{1000});
                                                   taskState->resumeThreadID = Thread::CurrentThreadID();
                                                   taskState->canceled       = not result;
                                                   return result;
                                               })));
        }

        SC_TEST_EXPECT(workerPool.start(scheduler, {workers, NumWorkers}, {threads, NumWorkers}));
        for (int idx = 0; idx < 1000 and state.entered.load() != static_cast<int32_t>(NumTasks); ++idx)
        {
            SC_TEST_EXPECT(io.runOwnerNoWait());
            Thread::Sleep(1);
        }
        SC_TEST_EXPECT(state.entered.load() == static_cast<int32_t>(NumTasks));

        SC_TEST_EXPECT(io.cancelAll());
        SC_TEST_EXPECT(io.runOwner());

        SC_TEST_EXPECT(not scheduler.hasActiveFibers());
        SC_TEST_EXPECT(workerPool.join());
        size_t canceled = 0;
        for (size_t idx = 0; idx < NumTasks; ++idx)
        {
            SC_TEST_EXPECT(tasks[idx].isCompleted());
            if (state.tasks[idx].canceled)
            {
                canceled += 1;
                SC_TEST_EXPECT(not tasks[idx].result());
            }
            SC_TEST_EXPECT(state.tasks[idx].startThreadID != 0);
            SC_TEST_EXPECT(state.tasks[idx].resumeThreadID != 0);
        }
        SC_TEST_EXPECT(canceled > 0);
        SC_TEST_EXPECT(eventLoop.close());
    }

    void workerPoolSleepCancelCompleteRace()
    {
        static constexpr size_t NumWorkers = 2;
        static constexpr size_t NumTasks   = 8;

        struct TaskState
        {
            FiberAsyncIO* io       = nullptr;
            TimeMs        duration = TimeMs{0};

            bool completed = false;
            bool canceled  = false;
        };

        struct State
        {
            Atomic<int32_t> entered = 0;
            TaskState       tasks[NumTasks];
        };

        AsyncEventLoop eventLoop;
        SC_TEST_EXPECT(eventLoop.create());

        FiberScheduler    scheduler;
        FiberAsyncCommand commandStorage[NumTasks];
        FiberAsyncIO      io(scheduler, eventLoop, commandStorage);
        FiberTask         tasks[NumTasks];
        static char       stackMemory[NumTasks][64 * 1024] = {};
        FiberWorker       workers[NumWorkers];
        FiberWorkerThread threads[NumWorkers];
        FiberWorkerPool   workerPool;

        State state;
        for (size_t idx = 0; idx < NumTasks; ++idx)
        {
            state.tasks[idx].io       = &io;
            state.tasks[idx].duration = (idx % 2) == 0 ? TimeMs{1} : TimeMs{10 * 1000};

            FiberStack stack({stackMemory[idx], sizeof(stackMemory[idx])});
            TaskState* taskState = &state.tasks[idx];
            SC_TEST_EXPECT(scheduler.spawn(tasks[idx], stack,
                                           FiberTask::Procedure(
                                               [&state, taskState](FiberScheduler&)
                                               {
                                                   state.entered.fetch_add(1);
                                                   Result result        = taskState->io->sleep(taskState->duration);
                                                   taskState->completed = result;
                                                   taskState->canceled  = not result;
                                                   return result;
                                               })));
        }

        SC_TEST_EXPECT(workerPool.start(scheduler, {workers, NumWorkers}, {threads, NumWorkers}));
        for (int idx = 0; idx < 1000 and state.entered.load() != static_cast<int32_t>(NumTasks); ++idx)
        {
            SC_TEST_EXPECT(io.runOwnerNoWait());
            Thread::Sleep(1);
        }
        SC_TEST_EXPECT(state.entered.load() == static_cast<int32_t>(NumTasks));

        for (int idx = 0; idx < 8; ++idx)
        {
            SC_TEST_EXPECT(io.runOwnerNoWait());
            Thread::Sleep(1);
        }
        SC_TEST_EXPECT(io.cancelAll());
        SC_TEST_EXPECT(io.runOwner());

        SC_TEST_EXPECT(workerPool.join());
        size_t completed = 0;
        size_t canceled  = 0;
        for (size_t idx = 0; idx < NumTasks; ++idx)
        {
            SC_TEST_EXPECT(tasks[idx].isCompleted());
            if (state.tasks[idx].completed)
            {
                completed += 1;
                SC_TEST_EXPECT(tasks[idx].result());
            }
            if (state.tasks[idx].canceled)
            {
                canceled += 1;
                SC_TEST_EXPECT(not tasks[idx].result());
            }
        }

        SC_TEST_EXPECT(completed + canceled == NumTasks);
        SC_TEST_EXPECT(completed > 0);
        SC_TEST_EXPECT(canceled > 0);
        SC_TEST_EXPECT(not scheduler.hasActiveFibers());
        SC_TEST_EXPECT(eventLoop.close());
    }

    void workerPoolOwnerCommandRaceStress()
    {
        static constexpr size_t NumRounds  = 12;
        static constexpr size_t NumTasks   = 8;
        static constexpr size_t NumWorkers = 2;

        struct TaskState
        {
            FiberAsyncIO* io       = nullptr;
            TimeMs        duration = TimeMs{0};

            Atomic<bool> completed = false;
            Atomic<bool> canceled  = false;
        };

        struct State
        {
            Atomic<int32_t> entered = 0;
            TaskState       tasks[NumTasks];
        };

        struct StopState
        {
            FiberWorkerPool* workerPool        = nullptr;
            uint32_t         delayMilliseconds = 0;
            Result           result            = Result(true);
        };

        static char stackMemory[NumTasks][64 * 1024] = {};

        size_t totalCompleted = 0;
        size_t totalCanceled  = 0;
        for (size_t round = 0; round < NumRounds; ++round)
        {
            AsyncEventLoop eventLoop;
            SC_TEST_EXPECT(eventLoop.create());

            FiberScheduler    scheduler;
            FiberAsyncCommand commandStorage[NumTasks * 4];
            FiberAsyncIO      io(scheduler, eventLoop, commandStorage);
            FiberTask         tasks[NumTasks];
            FiberWorker       workers[NumWorkers];
            FiberWorkerThread threads[NumWorkers];
            FiberWorkerPool   workerPool;

            State state;
            for (size_t taskIndex = 0; taskIndex < NumTasks; ++taskIndex)
            {
                TaskState& taskState = state.tasks[taskIndex];
                taskState.io         = &io;
                taskState.duration   = taskIndex < NumTasks / 2 ? TimeMs{1} : TimeMs{10 * 1000};

                FiberStack stack({stackMemory[taskIndex], sizeof(stackMemory[taskIndex])});
                SC_TEST_EXPECT(scheduler.spawn(tasks[taskIndex], stack,
                                               FiberTask::Procedure(
                                                   [&state, &taskState](FiberScheduler&)
                                                   {
                                                       state.entered.fetch_add(1);
                                                       Result result = taskState.io->sleep(taskState.duration);
                                                       taskState.completed.store(static_cast<bool>(result));
                                                       taskState.canceled.store(not result);
                                                       return result;
                                                   })));
            }

            SC_TEST_EXPECT(workerPool.start(scheduler, {workers, NumWorkers}, {threads, NumWorkers}));
            for (size_t iteration = 0; iteration < 1000 and state.entered.load() != static_cast<int32_t>(NumTasks);
                 ++iteration)
            {
                Thread::Sleep(1);
            }
            SC_TEST_EXPECT(state.entered.load() == static_cast<int32_t>(NumTasks));

            if (round == NumRounds - 1)
            {
                for (size_t iteration = 0; iteration < 1000 and not state.tasks[0].completed.load(); ++iteration)
                {
                    SC_TEST_EXPECT(io.runOwnerNoWait());
                    Thread::Sleep(1);
                }
                SC_TEST_EXPECT(state.tasks[0].completed.load());
            }

            StopState stopState;
            stopState.workerPool        = &workerPool;
            stopState.delayMilliseconds = round % 2 == 0 ? 0 : 4;

            Thread stopThread;
            auto   stop = [&stopState](Thread&)
            {
                Thread::Sleep(stopState.delayMilliseconds);
                stopState.result = stopState.workerPool->requestStop();
            };
            SC_TEST_EXPECT(stopThread.start(stop));

            SC_TEST_EXPECT(io.runOwner());
            SC_TEST_EXPECT(stopThread.join());
            SC_TEST_EXPECT(stopState.result);
            SC_TEST_EXPECT(workerPool.join());
            SC_TEST_EXPECT(not scheduler.hasActiveFibers());

            size_t roundCompleted = 0;
            size_t roundCanceled  = 0;
            for (size_t taskIndex = 0; taskIndex < NumTasks; ++taskIndex)
            {
                SC_TEST_EXPECT(tasks[taskIndex].isCompleted());
                if (state.tasks[taskIndex].completed.load())
                {
                    roundCompleted += 1;
                    SC_TEST_EXPECT(tasks[taskIndex].result());
                }
                if (state.tasks[taskIndex].canceled.load())
                {
                    roundCanceled += 1;
                    SC_TEST_EXPECT(not tasks[taskIndex].result());
                }
            }
            SC_TEST_EXPECT(roundCompleted + roundCanceled == NumTasks);
            SC_TEST_EXPECT(roundCanceled > 0);
            totalCompleted += roundCompleted;
            totalCanceled += roundCanceled;

            SC_TEST_EXPECT(eventLoop.close());
        }

        SC_TEST_EXPECT(totalCompleted > 0);
        SC_TEST_EXPECT(totalCanceled > 0);
        SC_TEST_EXPECT(totalCompleted + totalCanceled == NumRounds * NumTasks);
    }

    void commandQueueOverflow()
    {
        struct State
        {
            FiberScheduler* scheduler = nullptr;
            FiberAsyncIO*   io        = nullptr;

            Atomic<int32_t> attempted  = 0;
            Atomic<bool>    workerDone = false;
            Atomic<int32_t> workerRuns = 0;

            Result firstResult  = Result(true);
            Result secondResult = Result(true);
            Result workerResult = Result(true);
        };

        AsyncEventLoop eventLoop;
        SC_TEST_EXPECT(eventLoop.create());

        FiberScheduler    scheduler;
        FiberAsyncCommand commandStorage[1];
        FiberAsyncIO      io(scheduler, eventLoop, commandStorage);
        FiberTask         firstTask;
        FiberTask         secondTask;
        static char       firstStackMemory[64 * 1024]  = {};
        static char       secondStackMemory[64 * 1024] = {};
        FiberStack        firstStack({firstStackMemory, sizeof(firstStackMemory)});
        FiberStack        secondStack({secondStackMemory, sizeof(secondStackMemory)});

        State state;
        state.scheduler = &scheduler;
        state.io        = &io;

        SC_TEST_EXPECT(scheduler.spawn(firstTask, firstStack,
                                       FiberTask::Procedure(
                                           [&state](FiberScheduler&)
                                           {
                                               ++state.attempted;
                                               state.firstResult = state.io->sleep(TimeMs{1});
                                               return state.firstResult;
                                           })));
        SC_TEST_EXPECT(scheduler.spawn(secondTask, secondStack,
                                       FiberTask::Procedure(
                                           [&state](FiberScheduler&)
                                           {
                                               ++state.attempted;
                                               state.secondResult = state.io->sleep(TimeMs{1});
                                               return state.secondResult;
                                           })));

        Thread workerThread;
        auto   worker = [&state](Thread&)
        {
            while (state.workerRuns.load() < 2 and state.scheduler->hasReadyFibers())
            {
                const bool hadReady = state.scheduler->hasReadyFibers();
                state.workerResult  = state.scheduler->runNoWait();
                if (not state.workerResult)
                    break;
                if (hadReady)
                    ++state.workerRuns;
                Thread::Sleep(1);
            }
            state.workerDone.store(true);
        };
        SC_TEST_EXPECT(workerThread.start(worker));

        for (int idx = 0; idx < 1000 and not state.workerDone.load(); ++idx)
        {
            Thread::Sleep(1);
        }

        SC_TEST_EXPECT(workerThread.join());
        SC_TEST_EXPECT(state.attempted.load() == 2);
        SC_TEST_EXPECT(state.workerRuns.load() >= 2);
        SC_TEST_EXPECT(io.run());
        SC_TEST_EXPECT(state.workerResult);
        SC_TEST_EXPECT(firstTask.isCompleted());
        SC_TEST_EXPECT(secondTask.isCompleted());
        SC_TEST_EXPECT(firstTask.result());
        SC_TEST_EXPECT(not secondTask.result());
        SC_TEST_EXPECT(state.firstResult);
        SC_TEST_EXPECT(not state.secondResult);
        SC_TEST_EXPECT(eventLoop.close());
    }

    void socketAccept()
    {
        struct State
        {
            FiberAsyncIO*     io             = nullptr;
            SocketDescriptor* serverSocket   = nullptr;
            SocketDescriptor* acceptedClient = nullptr;
        };

        AsyncEventLoop eventLoop;
        SC_TEST_EXPECT(eventLoop.create());

        SocketDescriptor serverSocket;
        SocketDescriptor acceptedClient;
        uint16_t         tcpPort = report.mapPort(6051);
        SocketIPAddress  nativeAddress;
        SC_TEST_EXPECT(nativeAddress.fromAddressPort("127.0.0.1", tcpPort));
        SC_TEST_EXPECT(eventLoop.createAsyncTCPSocket(nativeAddress.getAddressFamily(), serverSocket));
        {
            SocketServer server(serverSocket);
            SC_TEST_EXPECT(server.bind(nativeAddress));
            SC_TEST_EXPECT(server.listen(1));
        }

        FiberScheduler scheduler;
        FiberAsyncIO   io(scheduler, eventLoop);
        FiberTask      task;

        char       stackMemory[64 * 1024] = {};
        FiberStack stack({stackMemory, sizeof(stackMemory)});

        State state;
        state.io             = &io;
        state.serverSocket   = &serverSocket;
        state.acceptedClient = &acceptedClient;

        SC_TEST_EXPECT(scheduler.spawn(
            task, stack,
            FiberTask::Procedure([&state](FiberScheduler&)
                                 { return state.io->accept(*state.serverSocket, *state.acceptedClient); })));

        SC_TEST_EXPECT(io.runOnce());
        SC_TEST_EXPECT(task.isActive());

        SocketDescriptor client;
        SC_TEST_EXPECT(client.create(nativeAddress.getAddressFamily()));
        SC_TEST_EXPECT(SocketClient(client).connect("127.0.0.1", tcpPort));

        SC_TEST_EXPECT(io.run());
        SC_TEST_EXPECT(task.isCompleted());
        SC_TEST_EXPECT(task.result());
        SC_TEST_EXPECT(acceptedClient.isValid());

        SC_TEST_EXPECT(client.close());
        SC_TEST_EXPECT(acceptedClient.close());
        SC_TEST_EXPECT(serverSocket.close());
        SC_TEST_EXPECT(eventLoop.close());
    }

    void socketConnect()
    {
        struct State
        {
            FiberAsyncIO*     io             = nullptr;
            SocketDescriptor* serverSocket   = nullptr;
            SocketDescriptor* acceptedClient = nullptr;
            SocketDescriptor* client         = nullptr;
            SocketIPAddress   address;
        };

        AsyncEventLoop eventLoop;
        SC_TEST_EXPECT(eventLoop.create());

        SocketDescriptor serverSocket;
        SocketDescriptor acceptedClient;
        SocketDescriptor client;
        uint16_t         tcpPort = report.mapPort(6052);
        SocketIPAddress  nativeAddress;
        SC_TEST_EXPECT(nativeAddress.fromAddressPort("127.0.0.1", tcpPort));
        SC_TEST_EXPECT(eventLoop.createAsyncTCPSocket(nativeAddress.getAddressFamily(), serverSocket));
        SC_TEST_EXPECT(eventLoop.createAsyncTCPSocket(nativeAddress.getAddressFamily(), client));
        {
            SocketServer server(serverSocket);
            SC_TEST_EXPECT(server.bind(nativeAddress));
            SC_TEST_EXPECT(server.listen(1));
        }

        FiberScheduler scheduler;
        FiberAsyncIO   io(scheduler, eventLoop);
        FiberTask      acceptTask;
        FiberTask      connectTask;

        char       acceptStackMemory[64 * 1024]  = {};
        char       connectStackMemory[64 * 1024] = {};
        FiberStack acceptStack({acceptStackMemory, sizeof(acceptStackMemory)});
        FiberStack connectStack({connectStackMemory, sizeof(connectStackMemory)});

        State state;
        state.io             = &io;
        state.serverSocket   = &serverSocket;
        state.acceptedClient = &acceptedClient;
        state.client         = &client;
        state.address        = nativeAddress;

        SC_TEST_EXPECT(scheduler.spawn(
            acceptTask, acceptStack,
            FiberTask::Procedure([&state](FiberScheduler&)
                                 { return state.io->accept(*state.serverSocket, *state.acceptedClient); })));
        SC_TEST_EXPECT(
            scheduler.spawn(connectTask, connectStack,
                            FiberTask::Procedure([&state](FiberScheduler&)
                                                 { return state.io->connect(*state.client, state.address); })));

        SC_TEST_EXPECT(io.run());
        SC_TEST_EXPECT(acceptTask.isCompleted());
        SC_TEST_EXPECT(connectTask.isCompleted());
        SC_TEST_EXPECT(acceptTask.result());
        SC_TEST_EXPECT(connectTask.result());
        SC_TEST_EXPECT(acceptedClient.isValid());

        SC_TEST_EXPECT(client.close());
        SC_TEST_EXPECT(acceptedClient.close());
        SC_TEST_EXPECT(serverSocket.close());
        SC_TEST_EXPECT(eventLoop.close());
    }

    void socketSendReceive()
    {
        struct State
        {
            FiberAsyncIO*     io         = nullptr;
            SocketDescriptor* sender     = nullptr;
            SocketDescriptor* receiver   = nullptr;
            size_t            dataLength = 0;

            char sendBuffer[5]    = {'P', 'I', 'N', 'G', '!'};
            char receiveBuffer[8] = {};

            FiberAsyncSocketSendResult    sendResult;
            FiberAsyncSocketReceiveResult receiveResult;
        };

        AsyncEventLoop eventLoop;
        SC_TEST_EXPECT(eventLoop.create());

        SocketDescriptor client;
        SocketDescriptor serverSideClient;
        createTCPSocketPair(eventLoop, client, serverSideClient);

        FiberScheduler scheduler;
        FiberAsyncIO   io(scheduler, eventLoop);
        FiberTask      task;

        char       stackMemory[64 * 1024] = {};
        FiberStack stack({stackMemory, sizeof(stackMemory)});

        State state;
        state.io         = &io;
        state.sender     = &client;
        state.receiver   = &serverSideClient;
        state.dataLength = sizeof(state.sendBuffer);

        SC_TEST_EXPECT(scheduler.spawn(
            task, stack,
            FiberTask::Procedure(
                [&state](FiberScheduler&)
                {
                    SC_TRY(state.io->send(*state.sender, {state.sendBuffer, state.dataLength}, &state.sendResult));
                    SC_TRY(state.io->receive(*state.receiver, {state.receiveBuffer, sizeof(state.receiveBuffer)},
                                             state.receiveResult));
                    return Result(true);
                })));

        SC_TEST_EXPECT(io.run());
        SC_TEST_EXPECT(task.isCompleted());
        SC_TEST_EXPECT(task.result());
        SC_TEST_EXPECT(state.sendResult.numBytes == state.dataLength);
        SC_TEST_EXPECT(not state.receiveResult.disconnected);
        SC_TEST_EXPECT(state.receiveResult.data.sizeInBytes() == state.dataLength);
        for (size_t idx = 0; idx < state.dataLength; ++idx)
        {
            SC_TEST_EXPECT(state.receiveResult.data.data()[idx] == state.sendBuffer[idx]);
        }

        SC_TEST_EXPECT(client.close());
        SC_TEST_EXPECT(serverSideClient.close());
        SC_TEST_EXPECT(eventLoop.close());
    }

    void crossThreadSocketAcceptConnect()
    {
        struct State
        {
            FiberScheduler* scheduler = nullptr;
            FiberAsyncIO*   io        = nullptr;

            SocketDescriptor* serverSocket   = nullptr;
            SocketDescriptor* acceptedClient = nullptr;
            SocketDescriptor* client         = nullptr;
            SocketIPAddress   address;

            Atomic<int32_t> attempted  = 0;
            Atomic<bool>    workerDone = false;
            Atomic<int32_t> workerRuns = 0;

            uint64_t ownerThreadID        = 0;
            uint64_t acceptStartThreadID  = 0;
            uint64_t connectStartThreadID = 0;
            Result   workerResult         = Result(true);
        };

        AsyncEventLoop eventLoop;
        SC_TEST_EXPECT(eventLoop.create());

        SocketDescriptor serverSocket;
        SocketDescriptor acceptedClient;
        SocketDescriptor client;
        uint16_t         tcpPort = report.mapPort(6061);
        SocketIPAddress  nativeAddress;
        SC_TEST_EXPECT(nativeAddress.fromAddressPort("127.0.0.1", tcpPort));
        SC_TEST_EXPECT(eventLoop.createAsyncTCPSocket(nativeAddress.getAddressFamily(), serverSocket));
        SC_TEST_EXPECT(eventLoop.createAsyncTCPSocket(nativeAddress.getAddressFamily(), client));
        {
            SocketServer server(serverSocket);
            SC_TEST_EXPECT(server.bind(nativeAddress));
            SC_TEST_EXPECT(server.listen(1));
        }

        FiberScheduler    scheduler;
        FiberAsyncCommand commandStorage[4];
        FiberAsyncIO      io(scheduler, eventLoop, commandStorage);
        FiberTask         acceptTask;
        FiberTask         connectTask;
        static char       acceptStackMemory[64 * 1024]  = {};
        static char       connectStackMemory[64 * 1024] = {};
        FiberStack        acceptStack({acceptStackMemory, sizeof(acceptStackMemory)});
        FiberStack        connectStack({connectStackMemory, sizeof(connectStackMemory)});

        State state;
        state.scheduler      = &scheduler;
        state.io             = &io;
        state.serverSocket   = &serverSocket;
        state.acceptedClient = &acceptedClient;
        state.client         = &client;
        state.address        = nativeAddress;
        state.ownerThreadID  = Thread::CurrentThreadID();

        SC_TEST_EXPECT(scheduler.spawn(acceptTask, acceptStack,
                                       FiberTask::Procedure(
                                           [&state](FiberScheduler&)
                                           {
                                               ++state.attempted;
                                               state.acceptStartThreadID = Thread::CurrentThreadID();
                                               return state.io->accept(*state.serverSocket, *state.acceptedClient);
                                           })));
        SC_TEST_EXPECT(scheduler.spawn(connectTask, connectStack,
                                       FiberTask::Procedure(
                                           [&state](FiberScheduler&)
                                           {
                                               ++state.attempted;
                                               state.connectStartThreadID = Thread::CurrentThreadID();
                                               return state.io->connect(*state.client, state.address);
                                           })));

        Thread workerThread;
        auto   worker = [&state](Thread&)
        {
            while (state.scheduler->hasActiveFibers())
            {
                const bool hadReady = state.scheduler->hasReadyFibers();
                state.workerResult  = state.scheduler->runNoWait();
                if (not state.workerResult)
                    break;
                if (hadReady)
                    ++state.workerRuns;
                Thread::Sleep(1);
            }
            state.workerDone.store(true);
        };
        SC_TEST_EXPECT(workerThread.start(worker));

        for (int idx = 0; idx < 1000 and state.workerRuns.load() < 2 and not state.workerDone.load(); ++idx)
        {
            Thread::Sleep(1);
        }

        SC_TEST_EXPECT(state.attempted.load() == 2);
        SC_TEST_EXPECT(state.workerRuns.load() >= 2);
        SC_TEST_EXPECT(io.run());
        SC_TEST_EXPECT(workerThread.join());
        SC_TEST_EXPECT(state.workerResult);
        SC_TEST_EXPECT(acceptTask.isCompleted());
        SC_TEST_EXPECT(connectTask.isCompleted());
        SC_TEST_EXPECT(acceptTask.result());
        SC_TEST_EXPECT(connectTask.result());
        SC_TEST_EXPECT(acceptedClient.isValid());
        SC_TEST_EXPECT(state.acceptStartThreadID != 0);
        SC_TEST_EXPECT(state.connectStartThreadID != 0);
        SC_TEST_EXPECT(state.acceptStartThreadID != state.ownerThreadID);
        SC_TEST_EXPECT(state.connectStartThreadID != state.ownerThreadID);

        SC_TEST_EXPECT(client.close());
        SC_TEST_EXPECT(acceptedClient.close());
        SC_TEST_EXPECT(serverSocket.close());
        SC_TEST_EXPECT(eventLoop.close());
    }

    void crossThreadSocketSendReceive()
    {
        struct State
        {
            FiberScheduler* scheduler = nullptr;
            FiberAsyncIO*   io        = nullptr;

            SocketDescriptor* sender   = nullptr;
            SocketDescriptor* receiver = nullptr;

            Atomic<int32_t> attempted  = 0;
            Atomic<bool>    workerDone = false;
            Atomic<int32_t> workerRuns = 0;

            uint64_t ownerThreadID        = 0;
            uint64_t sendStartThreadID    = 0;
            uint64_t receiveStartThreadID = 0;
            Result   workerResult         = Result(true);

            char sendBuffer[5]    = {'P', 'O', 'N', 'G', '?'};
            char receiveBuffer[8] = {};

            FiberAsyncSocketSendResult    sendResult;
            FiberAsyncSocketReceiveResult receiveResult;
        };

        AsyncEventLoop eventLoop;
        SC_TEST_EXPECT(eventLoop.create());

        SocketDescriptor client;
        SocketDescriptor serverSideClient;
        createTCPSocketPair(eventLoop, client, serverSideClient);

        FiberScheduler    scheduler;
        FiberAsyncCommand commandStorage[4];
        FiberAsyncIO      io(scheduler, eventLoop, commandStorage);
        FiberTask         sendTask;
        FiberTask         receiveTask;
        static char       sendStackMemory[64 * 1024]    = {};
        static char       receiveStackMemory[64 * 1024] = {};
        FiberStack        sendStack({sendStackMemory, sizeof(sendStackMemory)});
        FiberStack        receiveStack({receiveStackMemory, sizeof(receiveStackMemory)});

        State state;
        state.scheduler     = &scheduler;
        state.io            = &io;
        state.sender        = &client;
        state.receiver      = &serverSideClient;
        state.ownerThreadID = Thread::CurrentThreadID();

        SC_TEST_EXPECT(scheduler.spawn(receiveTask, receiveStack,
                                       FiberTask::Procedure(
                                           [&state](FiberScheduler&)
                                           {
                                               ++state.attempted;
                                               state.receiveStartThreadID = Thread::CurrentThreadID();
                                               return state.io->receive(
                                                   *state.receiver, {state.receiveBuffer, sizeof(state.receiveBuffer)},
                                                   state.receiveResult);
                                           })));
        SC_TEST_EXPECT(scheduler.spawn(sendTask, sendStack,
                                       FiberTask::Procedure(
                                           [&state](FiberScheduler&)
                                           {
                                               ++state.attempted;
                                               state.sendStartThreadID = Thread::CurrentThreadID();
                                               return state.io->send(*state.sender,
                                                                     {state.sendBuffer, sizeof(state.sendBuffer)},
                                                                     &state.sendResult);
                                           })));

        Thread workerThread;
        auto   worker = [&state](Thread&)
        {
            while (state.scheduler->hasActiveFibers())
            {
                const bool hadReady = state.scheduler->hasReadyFibers();
                state.workerResult  = state.scheduler->runNoWait();
                if (not state.workerResult)
                    break;
                if (hadReady)
                    ++state.workerRuns;
                Thread::Sleep(1);
            }
            state.workerDone.store(true);
        };
        SC_TEST_EXPECT(workerThread.start(worker));

        for (int idx = 0; idx < 1000 and state.workerRuns.load() < 2 and not state.workerDone.load(); ++idx)
        {
            Thread::Sleep(1);
        }

        SC_TEST_EXPECT(state.attempted.load() == 2);
        SC_TEST_EXPECT(state.workerRuns.load() >= 2);
        SC_TEST_EXPECT(io.run());
        SC_TEST_EXPECT(workerThread.join());
        SC_TEST_EXPECT(state.workerResult);
        SC_TEST_EXPECT(sendTask.isCompleted());
        SC_TEST_EXPECT(receiveTask.isCompleted());
        SC_TEST_EXPECT(sendTask.result());
        SC_TEST_EXPECT(receiveTask.result());
        SC_TEST_EXPECT(state.sendResult.numBytes == sizeof(state.sendBuffer));
        SC_TEST_EXPECT(not state.receiveResult.disconnected);
        SC_TEST_EXPECT(state.receiveResult.data.sizeInBytes() == sizeof(state.sendBuffer));
        for (size_t idx = 0; idx < sizeof(state.sendBuffer); ++idx)
        {
            SC_TEST_EXPECT(state.receiveResult.data.data()[idx] == state.sendBuffer[idx]);
        }
        SC_TEST_EXPECT(state.sendStartThreadID != 0);
        SC_TEST_EXPECT(state.receiveStartThreadID != 0);
        SC_TEST_EXPECT(state.sendStartThreadID != state.ownerThreadID);
        SC_TEST_EXPECT(state.receiveStartThreadID != state.ownerThreadID);

        SC_TEST_EXPECT(client.close());
        SC_TEST_EXPECT(serverSideClient.close());
        SC_TEST_EXPECT(eventLoop.close());
    }

    void workerPoolSocketSendReceive()
    {
        static constexpr size_t NumWorkers = 2;

        struct State
        {
            FiberAsyncIO* io = nullptr;

            SocketDescriptor* sender   = nullptr;
            SocketDescriptor* receiver = nullptr;

            Atomic<int32_t> attempted      = 0;
            Atomic<bool>    blockerEntered = false;

            uint64_t ownerThreadID         = 0;
            uint64_t sendStartThreadID     = 0;
            uint64_t sendResumeThreadID    = 0;
            uint64_t receiveStartThreadID  = 0;
            uint64_t receiveResumeThreadID = 0;

            char sendBuffer[5]    = {'P', 'O', 'O', 'L', '!'};
            char receiveBuffer[8] = {};

            FiberAsyncSocketSendResult    sendResult;
            FiberAsyncSocketReceiveResult receiveResult;
        };

        AsyncEventLoop eventLoop;
        SC_TEST_EXPECT(eventLoop.create());

        SocketDescriptor client;
        SocketDescriptor serverSideClient;
        createTCPSocketPair(eventLoop, client, serverSideClient);

        FiberScheduler    scheduler;
        FiberAsyncCommand commandStorage[4];
        FiberAsyncIO      io(scheduler, eventLoop, commandStorage);
        FiberTask         sendTask;
        FiberTask         receiveTask;
        FiberTask         blockerTask;
        static char       sendStackMemory[64 * 1024]    = {};
        static char       receiveStackMemory[64 * 1024] = {};
        static char       blockerStackMemory[64 * 1024] = {};
        FiberStack        sendStack({sendStackMemory, sizeof(sendStackMemory)});
        FiberStack        receiveStack({receiveStackMemory, sizeof(receiveStackMemory)});
        FiberStack        blockerStack({blockerStackMemory, sizeof(blockerStackMemory)});
        FiberWorker       workers[NumWorkers];
        FiberWorkerThread threads[NumWorkers];
        FiberWorkerPool   workerPool;

        FiberWorkerPoolOptions workerPoolOptions;
        workerPoolOptions.idleSpinAttempts = 0;

        State state;
        state.io            = &io;
        state.sender        = &client;
        state.receiver      = &serverSideClient;
        state.ownerThreadID = Thread::CurrentThreadID();

        SC_TEST_EXPECT(scheduler.spawn(receiveTask, receiveStack,
                                       FiberTask::Procedure(
                                           [&state](FiberScheduler&)
                                           {
                                               ++state.attempted;
                                               state.receiveStartThreadID = Thread::CurrentThreadID();
                                               SC_TRY(state.io->receive(
                                                   *state.receiver, {state.receiveBuffer, sizeof(state.receiveBuffer)},
                                                   state.receiveResult));
                                               state.receiveResumeThreadID = Thread::CurrentThreadID();
                                               return Result(true);
                                           })));
        SC_TEST_EXPECT(scheduler.spawn(sendTask, sendStack,
                                       FiberTask::Procedure(
                                           [&state](FiberScheduler&)
                                           {
                                               ++state.attempted;
                                               state.sendStartThreadID = Thread::CurrentThreadID();
                                               SC_TRY(state.io->send(*state.sender,
                                                                     {state.sendBuffer, sizeof(state.sendBuffer)},
                                                                     &state.sendResult));
                                               state.sendResumeThreadID = Thread::CurrentThreadID();
                                               return Result(true);
                                           })));
        SC_TEST_EXPECT(scheduler.spawn(blockerTask, blockerStack,
                                       FiberTask::Procedure(
                                           [&state](FiberScheduler&)
                                           {
                                               state.blockerEntered.store(true);
                                               return state.io->sleep(TimeMs{10 * 1000});
                                           })));

        SC_TEST_EXPECT(workerPool.start(scheduler, {workers, NumWorkers}, {threads, NumWorkers}, workerPoolOptions));
        for (int idx = 0; idx < 1000 and (state.attempted.load() < 2 or not state.blockerEntered.load()); ++idx)
        {
            Thread::Sleep(1);
        }
        SC_TEST_EXPECT(state.attempted.load() == 2);
        SC_TEST_EXPECT(state.blockerEntered.load());
        SC_TEST_EXPECT(waitForAllWorkersToPark(workerPool));

        FiberWorkerDiagnostics beforeIODiagnostics;
        scheduler.workerDiagnostics({workers, NumWorkers}, beforeIODiagnostics);
        SC_TEST_EXPECT(beforeIODiagnostics.idleSpinIterations == 0);

        for (int idx = 0; idx < 1000 and (not sendTask.isCompleted() or not receiveTask.isCompleted()); ++idx)
        {
            SC_TEST_EXPECT(io.runOwnerNoWait());
            Thread::Sleep(1);
        }
        SC_TEST_EXPECT(sendTask.isCompleted());
        SC_TEST_EXPECT(receiveTask.isCompleted());

        FiberWorkerDiagnostics afterIODiagnostics;
        scheduler.workerDiagnostics({workers, NumWorkers}, afterIODiagnostics);
        SC_TEST_EXPECT(afterIODiagnostics.idleSpinIterations == 0);
        SC_TEST_EXPECT(afterIODiagnostics.parkedWakeups > beforeIODiagnostics.parkedWakeups);
        SC_TEST_EXPECT(scheduler.activeFiberCount() == 1);

        SC_TEST_EXPECT(scheduler.requestCancel(blockerTask));
        SC_TEST_EXPECT(io.runOwner());

        SC_TEST_EXPECT(workerPool.join());
        SC_TEST_EXPECT(workerPool.parkedWorkerCount() == 0);
        SC_TEST_EXPECT(sendTask.isCompleted());
        SC_TEST_EXPECT(receiveTask.isCompleted());
        SC_TEST_EXPECT(sendTask.result());
        SC_TEST_EXPECT(receiveTask.result());
        SC_TEST_EXPECT(blockerTask.isCompleted());
        SC_TEST_EXPECT(not blockerTask.result());
        SC_TEST_EXPECT(state.sendResult.numBytes == sizeof(state.sendBuffer));
        SC_TEST_EXPECT(not state.receiveResult.disconnected);
        SC_TEST_EXPECT(state.receiveResult.data.sizeInBytes() == sizeof(state.sendBuffer));
        for (size_t idx = 0; idx < sizeof(state.sendBuffer); ++idx)
        {
            SC_TEST_EXPECT(state.receiveResult.data.data()[idx] == state.sendBuffer[idx]);
        }
        SC_TEST_EXPECT(state.sendStartThreadID != 0);
        SC_TEST_EXPECT(state.sendResumeThreadID != 0);
        SC_TEST_EXPECT(state.receiveStartThreadID != 0);
        SC_TEST_EXPECT(state.receiveResumeThreadID != 0);
        SC_TEST_EXPECT(state.sendStartThreadID != state.ownerThreadID);
        SC_TEST_EXPECT(state.sendResumeThreadID != state.ownerThreadID);
        SC_TEST_EXPECT(state.receiveStartThreadID != state.ownerThreadID);
        SC_TEST_EXPECT(state.receiveResumeThreadID != state.ownerThreadID);

        SC_TEST_EXPECT(client.close());
        SC_TEST_EXPECT(serverSideClient.close());
        SC_TEST_EXPECT(eventLoop.close());
    }

    void workerPoolCancelSocketReceive()
    {
        static constexpr size_t NumWorkers = 2;

        struct State
        {
            FiberAsyncIO*     io       = nullptr;
            SocketDescriptor* receiver = nullptr;

            Atomic<bool> entered = false;

            bool                          canceled = false;
            char                          buffer[8];
            FiberAsyncSocketReceiveResult receiveResult;

            uint64_t ownerThreadID  = 0;
            uint64_t startThreadID  = 0;
            uint64_t resumeThreadID = 0;
        };

        AsyncEventLoop eventLoop;
        SC_TEST_EXPECT(eventLoop.create());

        SocketDescriptor client;
        SocketDescriptor serverSideClient;
        createTCPSocketPair(eventLoop, client, serverSideClient);

        FiberScheduler    scheduler;
        FiberAsyncCommand commandStorage[4];
        FiberAsyncIO      io(scheduler, eventLoop, commandStorage);
        FiberTask         task;
        static char       stackMemory[64 * 1024] = {};
        FiberStack        stack({stackMemory, sizeof(stackMemory)});
        FiberWorker       workers[NumWorkers];
        FiberWorkerThread threads[NumWorkers];
        FiberWorkerPool   workerPool;

        State state;
        state.io            = &io;
        state.receiver      = &client;
        state.ownerThreadID = Thread::CurrentThreadID();

        SC_TEST_EXPECT(scheduler.spawn(task, stack,
                                       FiberTask::Procedure(
                                           [&state](FiberScheduler&)
                                           {
                                               state.startThreadID = Thread::CurrentThreadID();
                                               state.entered.store(true);
                                               Result result        = state.io->receive(*state.receiver,
                                                                                        {state.buffer, sizeof(state.buffer)},
                                                                                        state.receiveResult);
                                               state.resumeThreadID = Thread::CurrentThreadID();
                                               state.canceled       = not result;
                                               return result;
                                           })));

        SC_TEST_EXPECT(workerPool.start(scheduler, {workers, NumWorkers}, {threads, NumWorkers}));
        for (int idx = 0; idx < 1000 and not state.entered.load(); ++idx)
        {
            Thread::Sleep(1);
        }
        SC_TEST_EXPECT(state.entered.load());

        SC_TEST_EXPECT(io.runOwnerNoWait());
        SC_TEST_EXPECT(io.cancelAll());
        SC_TEST_EXPECT(io.runOwner());

        SC_TEST_EXPECT(not scheduler.hasActiveFibers());
        SC_TEST_EXPECT(workerPool.join());
        SC_TEST_EXPECT(task.isCompleted());
        SC_TEST_EXPECT(not task.result());
        SC_TEST_EXPECT(state.canceled);
        SC_TEST_EXPECT(state.startThreadID != 0);
        SC_TEST_EXPECT(state.resumeThreadID != 0);
        SC_TEST_EXPECT(state.startThreadID != state.ownerThreadID);
        SC_TEST_EXPECT(state.resumeThreadID != state.ownerThreadID);

        SC_TEST_EXPECT(client.close());
        SC_TEST_EXPECT(serverSideClient.close());
        SC_TEST_EXPECT(eventLoop.close());
    }

    void cancelSocketOperations()
    {
        {
            struct State
            {
                FiberAsyncIO*     io             = nullptr;
                SocketDescriptor* serverSocket   = nullptr;
                SocketDescriptor* acceptedClient = nullptr;
                bool              canceled       = false;
            };

            AsyncEventLoop eventLoop;
            SC_TEST_EXPECT(eventLoop.create());

            SocketDescriptor serverSocket;
            SocketDescriptor acceptedClient;
            uint16_t         tcpPort = report.mapPort(6053);
            SocketIPAddress  nativeAddress;
            SC_TEST_EXPECT(nativeAddress.fromAddressPort("127.0.0.1", tcpPort));
            SC_TEST_EXPECT(eventLoop.createAsyncTCPSocket(nativeAddress.getAddressFamily(), serverSocket));
            {
                SocketServer server(serverSocket);
                SC_TEST_EXPECT(server.bind(nativeAddress));
                SC_TEST_EXPECT(server.listen(1));
            }

            FiberScheduler scheduler;
            FiberAsyncIO   io(scheduler, eventLoop);
            FiberTask      task;
            char           stackMemory[64 * 1024] = {};
            FiberStack     stack({stackMemory, sizeof(stackMemory)});

            State state;
            state.io             = &io;
            state.serverSocket   = &serverSocket;
            state.acceptedClient = &acceptedClient;

            SC_TEST_EXPECT(scheduler.spawn(task, stack,
                                           FiberTask::Procedure(
                                               [&state](FiberScheduler&)
                                               {
                                                   Result result =
                                                       state.io->accept(*state.serverSocket, *state.acceptedClient);
                                                   state.canceled = not result;
                                                   return result;
                                               })));
            SC_TEST_EXPECT(io.runOnce());
            SC_TEST_EXPECT(task.isActive());
            SC_TEST_EXPECT(io.cancelAll());
            SC_TEST_EXPECT(io.run());
            SC_TEST_EXPECT(state.canceled);
            SC_TEST_EXPECT(not task.result());
            SC_TEST_EXPECT(serverSocket.close());
            SC_TEST_EXPECT(eventLoop.close());
        }

        {
            struct State
            {
                FiberAsyncIO*     io             = nullptr;
                SocketDescriptor* serverSocket   = nullptr;
                SocketDescriptor* acceptedClient = nullptr;
                bool              canceled       = false;
            };

            AsyncEventLoop eventLoop;
            SC_TEST_EXPECT(eventLoop.create());

            SocketDescriptor serverSocket;
            SocketDescriptor acceptedClient;
            uint16_t         tcpPort = report.mapPort(6064);
            SocketIPAddress  nativeAddress;
            SC_TEST_EXPECT(nativeAddress.fromAddressPort("127.0.0.1", tcpPort));
            SC_TEST_EXPECT(eventLoop.createAsyncTCPSocket(nativeAddress.getAddressFamily(), serverSocket));
            {
                SocketServer server(serverSocket);
                SC_TEST_EXPECT(server.bind(nativeAddress));
                SC_TEST_EXPECT(server.listen(1));
            }

            FiberScheduler scheduler;
            FiberAsyncIO   io(scheduler, eventLoop);
            FiberTask      task;
            char           stackMemory[64 * 1024] = {};
            FiberStack     stack({stackMemory, sizeof(stackMemory)});

            State state;
            state.io             = &io;
            state.serverSocket   = &serverSocket;
            state.acceptedClient = &acceptedClient;

            SC_TEST_EXPECT(scheduler.spawn(task, stack,
                                           FiberTask::Procedure(
                                               [&state](FiberScheduler&)
                                               {
                                                   Result result =
                                                       state.io->accept(*state.serverSocket, *state.acceptedClient);
                                                   state.canceled = not result;
                                                   return result;
                                               })));
            SC_TEST_EXPECT(io.cancelAll());
            SC_TEST_EXPECT(io.run());
            SC_TEST_EXPECT(state.canceled);
            SC_TEST_EXPECT(task.isCompleted());
            SC_TEST_EXPECT(not task.result());
            SC_TEST_EXPECT(serverSocket.close());
            SC_TEST_EXPECT(eventLoop.close());
        }

        {
            struct State
            {
                FiberAsyncIO*                 io       = nullptr;
                SocketDescriptor*             socket   = nullptr;
                bool                          canceled = false;
                char                          buffer[8];
                FiberAsyncSocketReceiveResult receiveResult;
            };

            AsyncEventLoop eventLoop;
            SC_TEST_EXPECT(eventLoop.create());

            SocketDescriptor client;
            SocketDescriptor serverSideClient;
            createTCPSocketPair(eventLoop, client, serverSideClient);

            FiberScheduler scheduler;
            FiberAsyncIO   io(scheduler, eventLoop);
            FiberTask      task;
            char           stackMemory[64 * 1024] = {};
            FiberStack     stack({stackMemory, sizeof(stackMemory)});

            State state;
            state.io     = &io;
            state.socket = &client;

            SC_TEST_EXPECT(scheduler.spawn(task, stack,
                                           FiberTask::Procedure(
                                               [&state](FiberScheduler&)
                                               {
                                                   Result result = state.io->receive(
                                                       *state.socket, {state.buffer, sizeof(state.buffer)},
                                                       state.receiveResult);
                                                   state.canceled = not result;
                                                   return result;
                                               })));
            SC_TEST_EXPECT(io.runOnce());
            SC_TEST_EXPECT(task.isActive());
            SC_TEST_EXPECT(io.cancelAll());
            SC_TEST_EXPECT(io.run());
            SC_TEST_EXPECT(state.canceled);
            SC_TEST_EXPECT(not task.result());
            SC_TEST_EXPECT(client.close());
            SC_TEST_EXPECT(serverSideClient.close());
            SC_TEST_EXPECT(eventLoop.close());
        }

        {
            struct State
            {
                FiberAsyncIO*                 io       = nullptr;
                SocketDescriptor*             socket   = nullptr;
                bool                          canceled = false;
                char                          buffer[8];
                FiberAsyncSocketReceiveResult receiveResult;
            };

            AsyncEventLoop eventLoop;
            SC_TEST_EXPECT(eventLoop.create());

            SocketDescriptor client;
            SocketDescriptor serverSideClient;
            createTCPSocketPair(eventLoop, client, serverSideClient);

            FiberScheduler scheduler;
            FiberAsyncIO   io(scheduler, eventLoop);
            FiberTask      task;
            char           stackMemory[64 * 1024] = {};
            FiberStack     stack({stackMemory, sizeof(stackMemory)});

            State state;
            state.io     = &io;
            state.socket = &client;

            SC_TEST_EXPECT(scheduler.spawn(task, stack,
                                           FiberTask::Procedure(
                                               [&state](FiberScheduler&)
                                               {
                                                   Result result = state.io->receive(
                                                       *state.socket, {state.buffer, sizeof(state.buffer)},
                                                       state.receiveResult);
                                                   state.canceled = not result;
                                                   return result;
                                               })));
            SC_TEST_EXPECT(io.cancelAll());
            SC_TEST_EXPECT(io.run());
            SC_TEST_EXPECT(state.canceled);
            SC_TEST_EXPECT(task.isCompleted());
            SC_TEST_EXPECT(not task.result());
            SC_TEST_EXPECT(client.close());
            SC_TEST_EXPECT(serverSideClient.close());
            SC_TEST_EXPECT(eventLoop.close());
        }

        {
            struct State
            {
                FiberAsyncIO*              io        = nullptr;
                SocketDescriptor*          socket    = nullptr;
                bool                       canceled  = false;
                char                       buffer[5] = {'P', 'I', 'N', 'G', '!'};
                FiberAsyncSocketSendResult sendResult;
            };

            AsyncEventLoop eventLoop;
            SC_TEST_EXPECT(eventLoop.create());

            SocketDescriptor client;
            SocketDescriptor serverSideClient;
            createTCPSocketPair(eventLoop, client, serverSideClient);

            FiberScheduler scheduler;
            FiberAsyncIO   io(scheduler, eventLoop);
            FiberTask      task;
            char           stackMemory[64 * 1024] = {};
            FiberStack     stack({stackMemory, sizeof(stackMemory)});

            State state;
            state.io     = &io;
            state.socket = &client;

            SC_TEST_EXPECT(scheduler.spawn(task, stack,
                                           FiberTask::Procedure(
                                               [&state](FiberScheduler&)
                                               {
                                                   Result result = state.io->sendAll(
                                                       *state.socket, {state.buffer, sizeof(state.buffer)},
                                                       &state.sendResult);
                                                   state.canceled = not result;
                                                   return result;
                                               })));
            SC_TEST_EXPECT(io.runOnce());
            SC_TEST_EXPECT(task.isActive());
            SC_TEST_EXPECT(io.cancelAll());
            SC_TEST_EXPECT(io.run());
            SC_TEST_EXPECT(state.canceled);
            SC_TEST_EXPECT(not task.result());
            SC_TEST_EXPECT(client.close());
            SC_TEST_EXPECT(serverSideClient.close());
            SC_TEST_EXPECT(eventLoop.close());
        }
    }

    void cancelActiveSocketOperations()
    {
        {
            struct State
            {
                FiberAsyncIO*     io                 = nullptr;
                SocketDescriptor* serverSocket       = nullptr;
                SocketDescriptor* acceptedClient     = nullptr;
                bool              completedWithError = false;
            };

            AsyncEventLoop eventLoop;
            SC_TEST_EXPECT(eventLoop.create());

            SocketDescriptor serverSocket;
            SocketDescriptor acceptedClient;
            uint16_t         tcpPort = report.mapPort(6056);
            SocketIPAddress  nativeAddress;
            SC_TEST_EXPECT(nativeAddress.fromAddressPort("127.0.0.1", tcpPort));
            SC_TEST_EXPECT(eventLoop.createAsyncTCPSocket(nativeAddress.getAddressFamily(), serverSocket));
            {
                SocketServer server(serverSocket);
                SC_TEST_EXPECT(server.bind(nativeAddress));
                SC_TEST_EXPECT(server.listen(1));
            }

            FiberScheduler scheduler;
            FiberAsyncIO   io(scheduler, eventLoop);
            FiberTask      task;
            char           stackMemory[64 * 1024] = {};
            FiberStack     stack({stackMemory, sizeof(stackMemory)});

            State state;
            state.io             = &io;
            state.serverSocket   = &serverSocket;
            state.acceptedClient = &acceptedClient;

            SC_TEST_EXPECT(scheduler.spawn(task, stack,
                                           FiberTask::Procedure(
                                               [&state](FiberScheduler&)
                                               {
                                                   Result result =
                                                       state.io->accept(*state.serverSocket, *state.acceptedClient);
                                                   state.completedWithError = not result;
                                                   return result;
                                               })));
            cancelSubmittedOperationIfPending(io, task, state.completedWithError);
            SC_TEST_EXPECT(serverSocket.close());
            SC_TEST_EXPECT(eventLoop.close());
        }

        {
            struct State
            {
                FiberAsyncIO*                 io                 = nullptr;
                SocketDescriptor*             socket             = nullptr;
                bool                          completedWithError = false;
                char                          buffer[8];
                FiberAsyncSocketReceiveResult receiveResult;
            };

            AsyncEventLoop eventLoop;
            SC_TEST_EXPECT(eventLoop.create());

            SocketDescriptor client;
            SocketDescriptor serverSideClient;
            createTCPSocketPair(eventLoop, client, serverSideClient);

            FiberScheduler scheduler;
            FiberAsyncIO   io(scheduler, eventLoop);
            FiberTask      task;
            char           stackMemory[64 * 1024] = {};
            FiberStack     stack({stackMemory, sizeof(stackMemory)});

            State state;
            state.io     = &io;
            state.socket = &client;

            SC_TEST_EXPECT(scheduler.spawn(task, stack,
                                           FiberTask::Procedure(
                                               [&state](FiberScheduler&)
                                               {
                                                   Result result = state.io->receive(
                                                       *state.socket, {state.buffer, sizeof(state.buffer)},
                                                       state.receiveResult);
                                                   state.completedWithError = not result;
                                                   return result;
                                               })));
            cancelSubmittedOperationIfPending(io, task, state.completedWithError);
            SC_TEST_EXPECT(client.close());
            SC_TEST_EXPECT(serverSideClient.close());
            SC_TEST_EXPECT(eventLoop.close());
        }

        {
            struct State
            {
                FiberAsyncIO*              io     = nullptr;
                SocketDescriptor*          socket = nullptr;
                Span<const char>           data;
                bool                       completedWithError = false;
                FiberAsyncSocketSendResult sendResult;
            };

            static char sendBuffer[16 * 1024 * 1024] = {};

            AsyncEventLoop eventLoop;
            SC_TEST_EXPECT(eventLoop.create());

            SocketDescriptor client;
            SocketDescriptor serverSideClient;
            createTCPSocketPair(eventLoop, client, serverSideClient);

            FiberScheduler scheduler;
            FiberAsyncIO   io(scheduler, eventLoop);
            FiberTask      task;
            char           stackMemory[64 * 1024] = {};
            FiberStack     stack({stackMemory, sizeof(stackMemory)});

            State state;
            state.io     = &io;
            state.socket = &client;
            state.data   = {sendBuffer, sizeof(sendBuffer)};

            SC_TEST_EXPECT(scheduler.spawn(task, stack,
                                           FiberTask::Procedure(
                                               [&state](FiberScheduler&)
                                               {
                                                   Result result =
                                                       state.io->sendAll(*state.socket, state.data, &state.sendResult);
                                                   state.completedWithError = not result;
                                                   return result;
                                               })));
            cancelSubmittedOperationIfPending(io, task, state.completedWithError);
            SC_TEST_EXPECT(client.close());
            SC_TEST_EXPECT(serverSideClient.close());
            SC_TEST_EXPECT(eventLoop.close());
        }
    }

    void multiFiberEcho()
    {
        struct State
        {
            FiberAsyncIO*     io             = nullptr;
            SocketDescriptor* serverSocket   = nullptr;
            SocketDescriptor* acceptedClient = nullptr;
            SocketDescriptor* client         = nullptr;
            SocketIPAddress   address;

            char clientSendBuffer[5]    = {'H', 'E', 'L', 'L', 'O'};
            char serverReceiveBuffer[8] = {};
            char clientReceiveBuffer[8] = {};

            FiberAsyncSocketReceiveResult serverReceiveResult;
            FiberAsyncSocketReceiveResult clientReceiveResult;
            FiberAsyncSocketSendResult    serverSendResult;
            FiberAsyncSocketSendResult    clientSendResult;
        };

        AsyncEventLoop eventLoop;
        SC_TEST_EXPECT(eventLoop.create());

        SocketDescriptor serverSocket;
        SocketDescriptor acceptedClient;
        SocketDescriptor client;
        uint16_t         tcpPort = report.mapPort(6054);
        SocketIPAddress  nativeAddress;
        SC_TEST_EXPECT(nativeAddress.fromAddressPort("127.0.0.1", tcpPort));
        SC_TEST_EXPECT(eventLoop.createAsyncTCPSocket(nativeAddress.getAddressFamily(), serverSocket));
        SC_TEST_EXPECT(eventLoop.createAsyncTCPSocket(nativeAddress.getAddressFamily(), client));
        {
            SocketServer server(serverSocket);
            SC_TEST_EXPECT(server.bind(nativeAddress));
            SC_TEST_EXPECT(server.listen(1));
        }

        FiberScheduler scheduler;
        FiberAsyncIO   io(scheduler, eventLoop);
        FiberTask      serverTask;
        FiberTask      clientTask;
        char           serverStackMemory[64 * 1024] = {};
        char           clientStackMemory[64 * 1024] = {};
        FiberStack     serverStack({serverStackMemory, sizeof(serverStackMemory)});
        FiberStack     clientStack({clientStackMemory, sizeof(clientStackMemory)});

        State state;
        state.io             = &io;
        state.serverSocket   = &serverSocket;
        state.acceptedClient = &acceptedClient;
        state.client         = &client;
        state.address        = nativeAddress;

        SC_TEST_EXPECT(scheduler.spawn(
            serverTask, serverStack,
            FiberTask::Procedure(
                [&state](FiberScheduler&)
                {
                    SC_TRY(state.io->accept(*state.serverSocket, *state.acceptedClient));
                    SC_TRY(state.io->receive(*state.acceptedClient,
                                             {state.serverReceiveBuffer, sizeof(state.serverReceiveBuffer)},
                                             state.serverReceiveResult));
                    SC_TRY(state.io->sendAll(*state.acceptedClient, state.serverReceiveResult.data,
                                             &state.serverSendResult));
                    return Result(true);
                })));
        SC_TEST_EXPECT(scheduler.spawn(
            clientTask, clientStack,
            FiberTask::Procedure(
                [&state](FiberScheduler&)
                {
                    SC_TRY(state.io->connect(*state.client, state.address));
                    SC_TRY(state.io->sendAll(*state.client, {state.clientSendBuffer, sizeof(state.clientSendBuffer)},
                                             &state.clientSendResult));
                    SC_TRY(state.io->receive(*state.client,
                                             {state.clientReceiveBuffer, sizeof(state.clientReceiveBuffer)},
                                             state.clientReceiveResult));
                    return Result(true);
                })));

        SC_TEST_EXPECT(io.run());
        SC_TEST_EXPECT(serverTask.result());
        SC_TEST_EXPECT(clientTask.result());
        SC_TEST_EXPECT(state.serverReceiveResult.data.sizeInBytes() == sizeof(state.clientSendBuffer));
        SC_TEST_EXPECT(state.clientReceiveResult.data.sizeInBytes() == sizeof(state.clientSendBuffer));
        SC_TEST_EXPECT(state.serverSendResult.numBytes == sizeof(state.clientSendBuffer));
        SC_TEST_EXPECT(state.clientSendResult.numBytes == sizeof(state.clientSendBuffer));
        for (size_t idx = 0; idx < sizeof(state.clientSendBuffer); ++idx)
        {
            SC_TEST_EXPECT(state.serverReceiveResult.data.data()[idx] == state.clientSendBuffer[idx]);
            SC_TEST_EXPECT(state.clientReceiveResult.data.data()[idx] == state.clientSendBuffer[idx]);
        }

        SC_TEST_EXPECT(client.close());
        SC_TEST_EXPECT(acceptedClient.close());
        SC_TEST_EXPECT(serverSocket.close());
        SC_TEST_EXPECT(eventLoop.close());
    }

    void udpSendReceive()
    {
        struct State
        {
            FiberAsyncIO*     io           = nullptr;
            SocketDescriptor* serverSocket = nullptr;
            SocketDescriptor* clientSocket = nullptr;
            SocketIPAddress   clientAddress;

            char sendBuffer[4]    = {'P', 'I', 'N', 'G'};
            char receiveBuffer[8] = {};

            FiberAsyncSocketSendResult        sendResult;
            FiberAsyncSocketReceiveFromResult receiveResult;
        };

        AsyncEventLoop eventLoop;
        SC_TEST_EXPECT(eventLoop.create());

        SocketIPAddress serverAddress;
        SocketIPAddress clientAddress;
        const uint16_t  udpPort = report.mapPort(6057);
        SC_TEST_EXPECT(serverAddress.fromAddressPort("0.0.0.0", udpPort));
        SC_TEST_EXPECT(clientAddress.fromAddressPort("127.0.0.1", udpPort));

        SocketDescriptor serverSocket;
        SocketDescriptor clientSocket;
        SC_TEST_EXPECT(eventLoop.createAsyncUDPSocket(serverAddress.getAddressFamily(), serverSocket));
        SC_TEST_EXPECT(eventLoop.createAsyncUDPSocket(clientAddress.getAddressFamily(), clientSocket));
        SC_TEST_EXPECT(SocketServer(serverSocket).bind(serverAddress));

        FiberScheduler scheduler;
        FiberAsyncIO   io(scheduler, eventLoop);
        FiberTask      receiveTask;
        FiberTask      sendTask;
        char           receiveStackMemory[64 * 1024] = {};
        char           sendStackMemory[64 * 1024]    = {};
        FiberStack     receiveStack({receiveStackMemory, sizeof(receiveStackMemory)});
        FiberStack     sendStack({sendStackMemory, sizeof(sendStackMemory)});

        State state;
        state.io            = &io;
        state.serverSocket  = &serverSocket;
        state.clientSocket  = &clientSocket;
        state.clientAddress = clientAddress;

        SC_TEST_EXPECT(scheduler.spawn(
            receiveTask, receiveStack,
            FiberTask::Procedure(
                [&state](FiberScheduler&)
                {
                    return state.io->receiveFrom(
                        *state.serverSocket, {state.receiveBuffer, sizeof(state.receiveBuffer)}, state.receiveResult);
                })));
        SC_TEST_EXPECT(scheduler.spawn(sendTask, sendStack,
                                       FiberTask::Procedure(
                                           [&state](FiberScheduler&)
                                           {
                                               return state.io->sendTo(*state.clientSocket, state.clientAddress,
                                                                       {state.sendBuffer, sizeof(state.sendBuffer)},
                                                                       &state.sendResult);
                                           })));

        SC_TEST_EXPECT(io.run());
        SC_TEST_EXPECT(receiveTask.result());
        SC_TEST_EXPECT(sendTask.result());
        SC_TEST_EXPECT(state.sendResult.numBytes == sizeof(state.sendBuffer));
        SC_TEST_EXPECT(state.receiveResult.sourceAddress.isValid());
        SC_TEST_EXPECT(state.receiveResult.data.sizeInBytes() == sizeof(state.sendBuffer));
        for (size_t idx = 0; idx < sizeof(state.sendBuffer); ++idx)
        {
            SC_TEST_EXPECT(state.receiveResult.data.data()[idx] == state.sendBuffer[idx]);
        }

        SC_TEST_EXPECT(serverSocket.close());
        SC_TEST_EXPECT(clientSocket.close());
        SC_TEST_EXPECT(eventLoop.close());
    }

    void fileSend()
    {
        struct State
        {
            FiberAsyncIO* io = nullptr;

            FileDescriptor*   file           = nullptr;
            SocketDescriptor* serverSocket   = nullptr;
            SocketDescriptor* acceptedClient = nullptr;
            SocketDescriptor* client         = nullptr;
            SocketIPAddress   address;

            char receiveBuffer[64] = {};

            FiberAsyncFileSendOptions     fileSendOptions;
            FiberAsyncFileSendResult      fileSendResult;
            FiberAsyncSocketReceiveResult receiveResult;
        };

        AsyncEventLoop eventLoop;
        SC_TEST_EXPECT(eventLoop.create());

        SmallString<64>        directoryName;
        SmallStringNative<255> directoryPath = StringEncoding::Native;
        SmallStringNative<255> filePath      = StringEncoding::Native;
        SmallStringNative<255> relativePath  = StringEncoding::Native;
        SC_TEST_EXPECT(StringBuilder::format(directoryName, "FiberAsyncFileSendTest{}", report.mapPort(6061)));
        SC_TEST_EXPECT(Path::join(directoryPath, {report.applicationRootDirectory.view(), directoryName.view()}));
        SC_TEST_EXPECT(Path::join(filePath, {directoryPath.view(), "sendfile.txt"}));
        SC_TEST_EXPECT(Path::join(relativePath, {directoryName.view(), "sendfile.txt"}));

        FileSystem fs;
        SC_TEST_EXPECT(fs.init(report.applicationRootDirectory.view()));
        SC_TEST_EXPECT(fs.makeDirectoryIfNotExists(directoryName.view()));
        SC_TEST_EXPECT(fs.removeFileIfExists(relativePath.view()));

        const char sendData[] = "FiberAsyncFileSend";
        SC_TEST_EXPECT(fs.write(relativePath.view(), {sendData, sizeof(sendData) - 1}));

        FileDescriptor file;
        FileOpen       openRead;
        openRead.mode     = FileOpen::Read;
        openRead.blocking = SC_PLATFORM_WINDOWS;
        SC_TEST_EXPECT(file.open(filePath.view(), openRead));
#if not SC_PLATFORM_WINDOWS
        SC_TEST_EXPECT(eventLoop.associateExternallyCreatedFileDescriptor(file));
#endif

        SocketDescriptor serverSocket;
        SocketDescriptor acceptedClient;
        SocketDescriptor client;
        const uint16_t   tcpPort = report.mapPort(6062);
        SocketIPAddress  nativeAddress;
        SC_TEST_EXPECT(nativeAddress.fromAddressPort("127.0.0.1", tcpPort));
        SC_TEST_EXPECT(eventLoop.createAsyncTCPSocket(nativeAddress.getAddressFamily(), serverSocket));
        SC_TEST_EXPECT(eventLoop.createAsyncTCPSocket(nativeAddress.getAddressFamily(), client));
        {
            SocketServer server(serverSocket);
            SC_TEST_EXPECT(server.bind(nativeAddress));
            SC_TEST_EXPECT(server.listen(1));
        }

        FiberScheduler scheduler;
        FiberAsyncIO   io(scheduler, eventLoop);
        FiberTask      serverTask;
        FiberTask      clientTask;
        char           serverStackMemory[64 * 1024] = {};
        char           clientStackMemory[64 * 1024] = {};
        FiberStack     serverStack({serverStackMemory, sizeof(serverStackMemory)});
        FiberStack     clientStack({clientStackMemory, sizeof(clientStackMemory)});

        State state;
        state.io                     = &io;
        state.file                   = &file;
        state.serverSocket           = &serverSocket;
        state.acceptedClient         = &acceptedClient;
        state.client                 = &client;
        state.address                = nativeAddress;
        state.fileSendOptions.length = sizeof(sendData) - 1;

        SC_TEST_EXPECT(scheduler.spawn(serverTask, serverStack,
                                       FiberTask::Procedure(
                                           [&state](FiberScheduler&)
                                           {
                                               SC_TRY(state.io->accept(*state.serverSocket, *state.acceptedClient));
                                               return state.io->fileSend(*state.file, *state.acceptedClient,
                                                                         state.fileSendOptions, &state.fileSendResult);
                                           })));
        SC_TEST_EXPECT(scheduler.spawn(clientTask, clientStack,
                                       FiberTask::Procedure(
                                           [&state](FiberScheduler&)
                                           {
                                               SC_TRY(state.io->connect(*state.client, state.address));
                                               return state.io->receive(
                                                   *state.client, {state.receiveBuffer, sizeof(state.receiveBuffer)},
                                                   state.receiveResult);
                                           })));

        SC_TEST_EXPECT(io.run());
        SC_TEST_EXPECT(serverTask.result());
        SC_TEST_EXPECT(clientTask.result());
        SC_TEST_EXPECT(state.fileSendResult.bytesTransferred == sizeof(sendData) - 1);
        SC_TEST_EXPECT(state.receiveResult.data.sizeInBytes() == sizeof(sendData) - 1);
        for (size_t idx = 0; idx < sizeof(sendData) - 1; ++idx)
        {
            SC_TEST_EXPECT(state.receiveResult.data.data()[idx] == sendData[idx]);
        }

        SC_TEST_EXPECT(file.close());
        SC_TEST_EXPECT(client.close());
        SC_TEST_EXPECT(acceptedClient.close());
        SC_TEST_EXPECT(serverSocket.close());
        SC_TEST_EXPECT(eventLoop.close());
        SC_TEST_EXPECT(fs.removeFile(relativePath.view()));
        SC_TEST_EXPECT(fs.removeEmptyDirectory(directoryName.view()));
    }

    void fileReadWrite()
    {
        struct State
        {
            FiberAsyncIO* io = nullptr;

            FileDescriptor* file           = nullptr;
            char            writeBuffer[4] = {'t', 'e', 's', 't'};
            char            readBuffer[8]  = {};

            FiberAsyncFileWriteResult writeResult;
            FiberAsyncFileReadResult  readResult;
        };

        AsyncEventLoop eventLoop;
        SC_TEST_EXPECT(eventLoop.create());

        SmallString<64>        directoryName;
        SmallStringNative<255> directoryPath = StringEncoding::Native;
        SmallStringNative<255> filePath      = StringEncoding::Native;
        SmallStringNative<255> relativePath  = StringEncoding::Native;
        SC_TEST_EXPECT(StringBuilder::format(directoryName, "FibersAsyncTest{}", report.mapPort(6055)));
        SC_TEST_EXPECT(Path::join(directoryPath, {report.applicationRootDirectory.view(), directoryName.view()}));
        SC_TEST_EXPECT(Path::join(filePath, {directoryPath.view(), "test.txt"}));
        SC_TEST_EXPECT(Path::join(relativePath, {directoryName.view(), "test.txt"}));

        FileSystem fs;
        SC_TEST_EXPECT(fs.init(report.applicationRootDirectory.view()));
        SC_TEST_EXPECT(fs.makeDirectoryIfNotExists(directoryName.view()));
        SC_TEST_EXPECT(fs.removeFileIfExists(relativePath.view()));

        FileDescriptor file;
        FileOpen       openWrite;
        openWrite.mode     = FileOpen::Write;
        openWrite.blocking = false;
        SC_TEST_EXPECT(file.open(filePath.view(), openWrite));
        SC_TEST_EXPECT(eventLoop.associateExternallyCreatedFileDescriptor(file));

        FiberScheduler scheduler;
        FiberAsyncIO   io(scheduler, eventLoop);
        FiberTask      writeTask;
        FiberTask      readTask;
        char           writeStackMemory[64 * 1024] = {};
        char           readStackMemory[64 * 1024]  = {};
        FiberStack     writeStack({writeStackMemory, sizeof(writeStackMemory)});
        FiberStack     readStack({readStackMemory, sizeof(readStackMemory)});

        State state;
        state.io   = &io;
        state.file = &file;

        SC_TEST_EXPECT(scheduler.spawn(writeTask, writeStack,
                                       FiberTask::Procedure(
                                           [&state](FiberScheduler&)
                                           {
                                               return state.io->fileWrite(
                                                   *state.file, {state.writeBuffer, sizeof(state.writeBuffer)},
                                                   &state.writeResult);
                                           })));
        SC_TEST_EXPECT(io.run());
        SC_TEST_EXPECT(writeTask.result());
        SC_TEST_EXPECT(state.writeResult.numBytes == sizeof(state.writeBuffer));
        SC_TEST_EXPECT(file.close());

        FileOpen openRead;
        openRead.mode     = FileOpen::Read;
        openRead.blocking = false;
        SC_TEST_EXPECT(file.open(filePath.view(), openRead));
        SC_TEST_EXPECT(eventLoop.associateExternallyCreatedFileDescriptor(file));

        SC_TEST_EXPECT(scheduler.spawn(readTask, readStack,
                                       FiberTask::Procedure(
                                           [&state](FiberScheduler&)
                                           {
                                               return state.io->fileRead(*state.file,
                                                                         {state.readBuffer, sizeof(state.readBuffer)},
                                                                         state.readResult);
                                           })));
        SC_TEST_EXPECT(io.run());
        SC_TEST_EXPECT(readTask.result());
        SC_TEST_EXPECT(not state.readResult.endOfFile);
        SC_TEST_EXPECT(state.readResult.data.sizeInBytes() == sizeof(state.writeBuffer));
        for (size_t idx = 0; idx < sizeof(state.writeBuffer); ++idx)
        {
            SC_TEST_EXPECT(state.readResult.data.data()[idx] == state.writeBuffer[idx]);
        }

        SC_TEST_EXPECT(file.close());
        SC_TEST_EXPECT(eventLoop.close());
        SC_TEST_EXPECT(fs.removeFile(relativePath.view()));
        SC_TEST_EXPECT(fs.removeEmptyDirectory(directoryName.view()));
    }

    void fileReadExact()
    {
        struct State
        {
            FiberAsyncIO* io = nullptr;

            FileDescriptor* file            = nullptr;
            char            writeBuffer[4]  = {'t', 'e', 's', 't'};
            char            exactBuffer[4]  = {};
            char            longBuffer[8]   = {};
            char            offsetBuffer[4] = {};

            FiberAsyncFileWriteResult writeResult;
            FiberAsyncFileReadResult  exactResult;
            FiberAsyncFileReadResult  eofResult;
            FiberAsyncFileReadResult  offsetResult;
        };

        AsyncEventLoop eventLoop;
        SC_TEST_EXPECT(eventLoop.create());

        SmallString<64>        directoryName;
        SmallStringNative<255> directoryPath = StringEncoding::Native;
        SmallStringNative<255> filePath      = StringEncoding::Native;
        SmallStringNative<255> relativePath  = StringEncoding::Native;
        SC_TEST_EXPECT(StringBuilder::format(directoryName, "FiberAsyncExactTest{}", report.mapPort(6060)));
        SC_TEST_EXPECT(Path::join(directoryPath, {report.applicationRootDirectory.view(), directoryName.view()}));
        SC_TEST_EXPECT(Path::join(filePath, {directoryPath.view(), "test.txt"}));
        SC_TEST_EXPECT(Path::join(relativePath, {directoryName.view(), "test.txt"}));

        FileSystem fs;
        SC_TEST_EXPECT(fs.init(report.applicationRootDirectory.view()));
        SC_TEST_EXPECT(fs.makeDirectoryIfNotExists(directoryName.view()));
        SC_TEST_EXPECT(fs.removeFileIfExists(relativePath.view()));

        FileDescriptor file;
        FileOpen       openWrite;
        openWrite.mode     = FileOpen::Write;
        openWrite.blocking = false;
        SC_TEST_EXPECT(file.open(filePath.view(), openWrite));
        SC_TEST_EXPECT(eventLoop.associateExternallyCreatedFileDescriptor(file));

        FiberScheduler scheduler;
        FiberAsyncIO   io(scheduler, eventLoop);
        FiberTask      writeTask;
        FiberTask      exactTask;
        FiberTask      eofTask;
        FiberTask      offsetTask;
        char           writeStackMemory[64 * 1024]  = {};
        char           exactStackMemory[64 * 1024]  = {};
        char           eofStackMemory[64 * 1024]    = {};
        char           offsetStackMemory[64 * 1024] = {};
        FiberStack     writeStack({writeStackMemory, sizeof(writeStackMemory)});
        FiberStack     exactStack({exactStackMemory, sizeof(exactStackMemory)});
        FiberStack     eofStack({eofStackMemory, sizeof(eofStackMemory)});
        FiberStack     offsetStack({offsetStackMemory, sizeof(offsetStackMemory)});

        State state;
        state.io   = &io;
        state.file = &file;

        SC_TEST_EXPECT(scheduler.spawn(writeTask, writeStack,
                                       FiberTask::Procedure(
                                           [&state](FiberScheduler&)
                                           {
                                               return state.io->fileWriteAll(
                                                   *state.file, {state.writeBuffer, sizeof(state.writeBuffer)},
                                                   &state.writeResult);
                                           })));
        SC_TEST_EXPECT(io.run());
        SC_TEST_EXPECT(writeTask.result());
        SC_TEST_EXPECT(state.writeResult.numBytes == sizeof(state.writeBuffer));
        SC_TEST_EXPECT(file.close());

        FileOpen openRead;
        openRead.mode     = FileOpen::Read;
        openRead.blocking = false;
        SC_TEST_EXPECT(file.open(filePath.view(), openRead));
        SC_TEST_EXPECT(eventLoop.associateExternallyCreatedFileDescriptor(file));

        SC_TEST_EXPECT(scheduler.spawn(exactTask, exactStack,
                                       FiberTask::Procedure(
                                           [&state](FiberScheduler&)
                                           {
                                               return state.io->fileReadExact(
                                                   *state.file, {state.exactBuffer, sizeof(state.exactBuffer)},
                                                   state.exactResult);
                                           })));
        SC_TEST_EXPECT(io.run());
        SC_TEST_EXPECT(exactTask.result());
        SC_TEST_EXPECT(state.exactResult.data.sizeInBytes() == sizeof(state.writeBuffer));
        for (size_t idx = 0; idx < sizeof(state.writeBuffer); ++idx)
        {
            SC_TEST_EXPECT(state.exactResult.data.data()[idx] == state.writeBuffer[idx]);
        }
        SC_TEST_EXPECT(file.close());

        SC_TEST_EXPECT(file.open(filePath.view(), openRead));
        SC_TEST_EXPECT(eventLoop.associateExternallyCreatedFileDescriptor(file));
        SC_TEST_EXPECT(scheduler.spawn(eofTask, eofStack,
                                       FiberTask::Procedure(
                                           [&state](FiberScheduler&)
                                           {
                                               return state.io->fileReadExact(
                                                   *state.file, {state.longBuffer, sizeof(state.longBuffer)},
                                                   state.eofResult);
                                           })));
        SC_TEST_EXPECT(io.run());
        SC_TEST_EXPECT(not eofTask.result());
        SC_TEST_EXPECT(state.eofResult.endOfFile);
        SC_TEST_EXPECT(state.eofResult.data.sizeInBytes() == sizeof(state.writeBuffer));
        for (size_t idx = 0; idx < sizeof(state.writeBuffer); ++idx)
        {
            SC_TEST_EXPECT(state.eofResult.data.data()[idx] == state.writeBuffer[idx]);
        }
        SC_TEST_EXPECT(file.close());

        SC_TEST_EXPECT(file.open(filePath.view(), openRead));
        SC_TEST_EXPECT(eventLoop.associateExternallyCreatedFileDescriptor(file));
        SC_TEST_EXPECT(scheduler.spawn(offsetTask, offsetStack,
                                       FiberTask::Procedure(
                                           [&state](FiberScheduler&)
                                           {
                                               return state.io->fileReadExactAt(
                                                   *state.file, 0, {state.offsetBuffer, sizeof(state.offsetBuffer)},
                                                   state.offsetResult);
                                           })));
        SC_TEST_EXPECT(io.run());
        SC_TEST_EXPECT(offsetTask.result());
        SC_TEST_EXPECT(state.offsetResult.data.sizeInBytes() == sizeof(state.writeBuffer));
        for (size_t idx = 0; idx < sizeof(state.writeBuffer); ++idx)
        {
            SC_TEST_EXPECT(state.offsetResult.data.data()[idx] == state.writeBuffer[idx]);
        }

        SC_TEST_EXPECT(file.close());
        SC_TEST_EXPECT(eventLoop.close());
        SC_TEST_EXPECT(fs.removeFile(relativePath.view()));
        SC_TEST_EXPECT(fs.removeEmptyDirectory(directoryName.view()));
    }

    void fileOffsets()
    {
        struct State
        {
            FiberAsyncIO* io = nullptr;

            FileDescriptor* file                 = nullptr;
            char            firstWriteBuffer[4]  = {'t', 'e', 's', 't'};
            char            secondWriteBuffer[4] = {'d', 'a', 't', 'a'};
            char            readBuffer[8]        = {};

            FiberAsyncFileWriteResult firstWriteResult;
            FiberAsyncFileWriteResult secondWriteResult;
            FiberAsyncFileReadResult  readResult;
        };

        AsyncEventLoop eventLoop;
        SC_TEST_EXPECT(eventLoop.create());

        SmallString<64>        directoryName;
        SmallStringNative<255> directoryPath = StringEncoding::Native;
        SmallStringNative<255> filePath      = StringEncoding::Native;
        SmallStringNative<255> relativePath  = StringEncoding::Native;
        SC_TEST_EXPECT(StringBuilder::format(directoryName, "FiberAsyncOffsetTest{}", report.mapPort(6058)));
        SC_TEST_EXPECT(Path::join(directoryPath, {report.applicationRootDirectory.view(), directoryName.view()}));
        SC_TEST_EXPECT(Path::join(filePath, {directoryPath.view(), "test.txt"}));
        SC_TEST_EXPECT(Path::join(relativePath, {directoryName.view(), "test.txt"}));

        FileSystem fs;
        SC_TEST_EXPECT(fs.init(report.applicationRootDirectory.view()));
        SC_TEST_EXPECT(fs.makeDirectoryIfNotExists(directoryName.view()));
        SC_TEST_EXPECT(fs.removeFileIfExists(relativePath.view()));

        FileDescriptor file;
        FileOpen       openWrite;
        openWrite.mode     = FileOpen::WriteRead;
        openWrite.blocking = false;
        SC_TEST_EXPECT(file.open(filePath.view(), openWrite));
        SC_TEST_EXPECT(eventLoop.associateExternallyCreatedFileDescriptor(file));

        FiberScheduler scheduler;
        FiberAsyncIO   io(scheduler, eventLoop);
        FiberTask      firstWriteTask;
        FiberTask      secondWriteTask;
        FiberTask      readTask;
        char           firstWriteStackMemory[64 * 1024]  = {};
        char           secondWriteStackMemory[64 * 1024] = {};
        char           readStackMemory[64 * 1024]        = {};
        FiberStack     firstWriteStack({firstWriteStackMemory, sizeof(firstWriteStackMemory)});
        FiberStack     secondWriteStack({secondWriteStackMemory, sizeof(secondWriteStackMemory)});
        FiberStack     readStack({readStackMemory, sizeof(readStackMemory)});

        State state;
        state.io   = &io;
        state.file = &file;

        SC_TEST_EXPECT(scheduler.spawn(firstWriteTask, firstWriteStack,
                                       FiberTask::Procedure(
                                           [&state](FiberScheduler&)
                                           {
                                               return state.io->fileWriteAllAt(
                                                   *state.file, 0,
                                                   {state.firstWriteBuffer, sizeof(state.firstWriteBuffer)},
                                                   &state.firstWriteResult);
                                           })));
        SC_TEST_EXPECT(io.run());
        SC_TEST_EXPECT(firstWriteTask.result());
        SC_TEST_EXPECT(state.firstWriteResult.numBytes == sizeof(state.firstWriteBuffer));

        SC_TEST_EXPECT(scheduler.spawn(secondWriteTask, secondWriteStack,
                                       FiberTask::Procedure(
                                           [&state](FiberScheduler&)
                                           {
                                               return state.io->fileWriteAt(
                                                   *state.file, 4,
                                                   {state.secondWriteBuffer, sizeof(state.secondWriteBuffer)},
                                                   &state.secondWriteResult);
                                           })));
        SC_TEST_EXPECT(io.run());
        SC_TEST_EXPECT(secondWriteTask.result());
        SC_TEST_EXPECT(state.secondWriteResult.numBytes == sizeof(state.secondWriteBuffer));

        SC_TEST_EXPECT(scheduler.spawn(readTask, readStack,
                                       FiberTask::Procedure(
                                           [&state](FiberScheduler&)
                                           {
                                               return state.io->fileReadAt(*state.file, 4,
                                                                           {state.readBuffer, sizeof(state.readBuffer)},
                                                                           state.readResult);
                                           })));
        SC_TEST_EXPECT(io.run());
        SC_TEST_EXPECT(readTask.result());
        SC_TEST_EXPECT(state.readResult.data.sizeInBytes() == sizeof(state.secondWriteBuffer));
        for (size_t idx = 0; idx < sizeof(state.secondWriteBuffer); ++idx)
        {
            SC_TEST_EXPECT(state.readResult.data.data()[idx] == state.secondWriteBuffer[idx]);
        }

        SC_TEST_EXPECT(file.close());
        SC_TEST_EXPECT(eventLoop.close());
        SC_TEST_EXPECT(fs.removeFile(relativePath.view()));
        SC_TEST_EXPECT(fs.removeEmptyDirectory(directoryName.view()));
    }

    void filePoll()
    {
        struct State
        {
            FiberAsyncIO*   io   = nullptr;
            FileDescriptor* file = nullptr;
        };

        AsyncEventLoop eventLoop;
        SC_TEST_EXPECT(eventLoop.create());

        PipeDescriptor pipe;
        PipeOptions    pipeOptions;
        pipeOptions.blocking = false;
        SC_TEST_EXPECT(pipe.createPipe(pipeOptions));
        SC_TEST_EXPECT(eventLoop.associateExternallyCreatedFileDescriptor(pipe.readPipe));

        FiberScheduler scheduler;
        FiberAsyncIO   io(scheduler, eventLoop);
        FiberTask      task;
        char           stackMemory[64 * 1024] = {};
        FiberStack     stack({stackMemory, sizeof(stackMemory)});

        State state;
        state.io   = &io;
        state.file = &pipe.readPipe;

        SC_TEST_EXPECT(scheduler.spawn(
            task, stack, FiberTask::Procedure([&state](FiberScheduler&) { return state.io->filePoll(*state.file); })));

#if SC_PLATFORM_WINDOWS
        SC_TEST_EXPECT(io.run());
        SC_TEST_EXPECT(task.isCompleted());
        SC_TEST_EXPECT(not task.result());
#else
        SC_TEST_EXPECT(pipe.writePipe.writeString("x"));
        SC_TEST_EXPECT(io.run());

        SC_TEST_EXPECT(task.result());
#endif
        SC_TEST_EXPECT(pipe.close());
        SC_TEST_EXPECT(eventLoop.close());
    }

    void cancelFilePoll()
    {
        struct State
        {
            FiberAsyncIO*   io       = nullptr;
            FileDescriptor* file     = nullptr;
            bool            canceled = false;
        };

        AsyncEventLoop eventLoop;
        SC_TEST_EXPECT(eventLoop.create());

        PipeDescriptor pipe;
        PipeOptions    pipeOptions;
        pipeOptions.blocking = false;
        SC_TEST_EXPECT(pipe.createPipe(pipeOptions));
        SC_TEST_EXPECT(eventLoop.associateExternallyCreatedFileDescriptor(pipe.readPipe));

        FiberScheduler scheduler;
        FiberAsyncIO   io(scheduler, eventLoop);
        FiberTask      task;
        char           stackMemory[64 * 1024] = {};
        FiberStack     stack({stackMemory, sizeof(stackMemory)});

        State state;
        state.io   = &io;
        state.file = &pipe.readPipe;

        SC_TEST_EXPECT(scheduler.spawn(task, stack,
                                       FiberTask::Procedure(
                                           [&state](FiberScheduler&)
                                           {
                                               Result result  = state.io->filePoll(*state.file);
                                               state.canceled = not result;
                                               return result;
                                           })));

#if SC_PLATFORM_WINDOWS
        SC_TEST_EXPECT(io.run());
        SC_TEST_EXPECT(state.canceled);
        SC_TEST_EXPECT(task.isCompleted());
        SC_TEST_EXPECT(not task.result());
#else
        SC_TEST_EXPECT(io.runOnce());
        SC_TEST_EXPECT(task.isActive());
        SC_TEST_EXPECT(io.cancelAll());
        SC_TEST_EXPECT(io.run());

        SC_TEST_EXPECT(state.canceled);
        SC_TEST_EXPECT(task.isCompleted());
        SC_TEST_EXPECT(not task.result());
#endif
        SC_TEST_EXPECT(pipe.close());
        SC_TEST_EXPECT(eventLoop.close());
    }

    void processExit()
    {
        if (skipProcessExitOnLinuxRosetta())
        {
            return;
        }

        struct State
        {
            FiberAsyncIO* io = nullptr;

            Process* processSuccess = nullptr;
            Process* processFailure = nullptr;

            FiberAsyncProcessExitResult successResult;
            FiberAsyncProcessExitResult failureResult;
        };

        AsyncEventLoop eventLoop;
        SC_TEST_EXPECT(eventLoop.create());

        Process processSuccess;
        Process processFailure;
#if SC_PLATFORM_WINDOWS
        SC_TEST_EXPECT(processSuccess.launch({"cmd", "/C", "exit", "/B", "0"}));
        SC_TEST_EXPECT(processFailure.launch({"cmd", "/C", "exit", "/B", "1"}));
#else
        SC_TEST_EXPECT(processSuccess.launch({"sh", "-c", "sleep 0.2; exit 0"}));
        SC_TEST_EXPECT(processFailure.launch({"sh", "-c", "sleep 0.1; exit 1"}));
#endif

        FiberScheduler scheduler;
        FiberAsyncIO   io(scheduler, eventLoop);
        FiberTask      successTask;
        FiberTask      failureTask;
        char           successStackMemory[64 * 1024] = {};
        char           failureStackMemory[64 * 1024] = {};
        FiberStack     successStack({successStackMemory, sizeof(successStackMemory)});
        FiberStack     failureStack({failureStackMemory, sizeof(failureStackMemory)});

        State state;
        state.io             = &io;
        state.processSuccess = &processSuccess;
        state.processFailure = &processFailure;

        SC_TEST_EXPECT(
            scheduler.spawn(successTask, successStack,
                            FiberTask::Procedure(
                                [&state](FiberScheduler&)
                                { return state.io->processExit(state.processSuccess->handle, state.successResult); })));
        SC_TEST_EXPECT(
            scheduler.spawn(failureTask, failureStack,
                            FiberTask::Procedure(
                                [&state](FiberScheduler&)
                                { return state.io->processExit(state.processFailure->handle, state.failureResult); })));

        SC_TEST_EXPECT(io.run());
        SC_TEST_EXPECT(successTask.result());
        SC_TEST_EXPECT(state.successResult.exitStatus == 0);
        SC_TEST_EXPECT(failureTask.result());
        SC_TEST_EXPECT(state.failureResult.exitStatus != 0);
        SC_TEST_EXPECT(eventLoop.close());
    }

    void cancelProcessExit()
    {
        if (skipProcessExitOnLinuxRosetta())
        {
            return;
        }

        AsyncEventLoop eventLoop;
        SC_TEST_EXPECT(eventLoop.create());

#if SC_PLATFORM_WINDOWS
        SC_TEST_EXPECT(eventLoop.close());
#else
        struct State
        {
            FiberAsyncIO* io      = nullptr;
            Process*      process = nullptr;

            FiberAsyncProcessExitResult result;
            bool                        canceled = false;
        };

        Process process;
        SC_TEST_EXPECT(process.launch({"sleep", "0.3"}));

        FiberScheduler scheduler;
        FiberAsyncIO   io(scheduler, eventLoop);
        FiberTask      task;
        char           stackMemory[64 * 1024] = {};
        FiberStack     stack({stackMemory, sizeof(stackMemory)});

        State state;
        state.io      = &io;
        state.process = &process;

        SC_TEST_EXPECT(scheduler.spawn(task, stack,
                                       FiberTask::Procedure(
                                           [&state](FiberScheduler&)
                                           {
                                               Result result =
                                                   state.io->processExit(state.process->handle, state.result);
                                               state.canceled = not result;
                                               return result;
                                           })));

        SC_TEST_EXPECT(io.runOnce());
        SC_TEST_EXPECT(task.isActive());
        SC_TEST_EXPECT(io.cancelAll());
        SC_TEST_EXPECT(io.run());
        SC_TEST_EXPECT(state.canceled);
        SC_TEST_EXPECT(task.isCompleted());
        SC_TEST_EXPECT(not task.result());
        SC_TEST_EXPECT(eventLoop.close());
        SC_TEST_EXPECT(process.waitForExitSync());
        SC_TEST_EXPECT(process.getExitStatus() == 0);
#endif
    }

    void signal()
    {
#if SC_PLATFORM_APPLE
        if (isDebuggerAttached())
        {
            report.console.printLine("FibersAsyncTest - Skipping signal section while debugger is attached");
            return;
        }
#endif
#if SC_PLATFORM_WINDOWS
        DWORD      consoleProcesses[1];
        const bool allocatedConsole = ::GetConsoleProcessList(consoleProcesses, 1) == 0;
        if (allocatedConsole)
        {
            SC_TEST_EXPECT(::AllocConsole() != FALSE);
        }

        Process process;
        process.options.windowsHide                  = false;
        process.options.windowsCreateNewProcessGroup = true;
        StringSpan childArguments[]                  = {
            report.executableFile.view(), "--quiet", "--test", "FibersAsyncTest", "--test-section",
            "windows signal child"};
        SC_TEST_EXPECT(
            process.launch(childArguments, Process::StdOut::Ignore(), Process::StdIn(), Process::StdErr::Ignore()));

        Thread::Sleep(1000);
        SC_TEST_EXPECT(::GenerateConsoleCtrlEvent(CTRL_BREAK_EVENT, static_cast<DWORD>(process.processID.pid)) !=
                       FALSE);
        SC_TEST_EXPECT(process.waitForExitSync());
        SC_TEST_EXPECT(process.getExitStatus() == 0);

        if (allocatedConsole)
        {
            SC_TEST_EXPECT(::FreeConsole() != FALSE);
        }
#else

        struct State
        {
            FiberAsyncIO* io = nullptr;

            FiberAsyncSignalResult result;
        };

        AsyncEventLoop eventLoop;
        SC_TEST_EXPECT(eventLoop.create());

        FiberScheduler scheduler;
        FiberAsyncIO   io(scheduler, eventLoop);
        FiberTask      task;
        char           stackMemory[64 * 1024] = {};
        FiberStack     stack({stackMemory, sizeof(stackMemory)});

        State state;
        state.io = &io;

        SC_TEST_EXPECT(scheduler.spawn(
            task, stack,
            FiberTask::Procedure([&state](FiberScheduler&) { return state.io->signal(SIGINT, state.result); })));

        AsyncLoopTimeout sendSignal;
        sendSignal.callback = [this](AsyncLoopTimeout::Result&) { SC_TEST_EXPECT(::kill(::getpid(), SIGINT) == 0); };
        SC_TEST_EXPECT(sendSignal.start(eventLoop, TimeMs{10}));

        SC_TEST_EXPECT(io.run());
        SC_TEST_EXPECT(task.result());
        SC_TEST_EXPECT(state.result.signalNumber == SIGINT);
        SC_TEST_EXPECT(state.result.deliveryCount >= 1);
        SC_TEST_EXPECT(eventLoop.close());
#endif
    }

#if SC_PLATFORM_WINDOWS
    void windowsSignalChild()
    {
        struct State
        {
            FiberAsyncIO* io       = nullptr;
            bool          timedOut = false;

            FiberAsyncSignalResult result;
        };

        AsyncEventLoop eventLoop;
        SC_TEST_EXPECT(eventLoop.create());

        FiberScheduler scheduler;
        FiberAsyncIO   io(scheduler, eventLoop);
        FiberTask      task;
        char           stackMemory[64 * 1024] = {};
        FiberStack     stack({stackMemory, sizeof(stackMemory)});

        State state;
        state.io = &io;

        SC_TEST_EXPECT(
            scheduler.spawn(task, stack,
                            FiberTask::Procedure([&state](FiberScheduler&)
                                                 { return state.io->signal(WindowsSignalBreak, state.result); })));

        AsyncLoopTimeout timeout;
        timeout.callback = [&state](AsyncLoopTimeout::Result&)
        {
            state.timedOut = true;
            SC_FIBER_ASYNC_TRUST_RESULT(state.io->cancelAll());
        };
        SC_TEST_EXPECT(timeout.start(eventLoop, TimeMs{5000}));
        eventLoop.excludeFromActiveCount(timeout);

        SC_TEST_EXPECT(io.run());
        if (not state.timedOut)
        {
            SC_TEST_EXPECT(timeout.stop(eventLoop));
            SC_TEST_EXPECT(eventLoop.runNoWait());
        }
        SC_TEST_EXPECT(task.result());
        SC_TEST_EXPECT(not state.timedOut);
        SC_TEST_EXPECT(state.result.signalNumber == WindowsSignalBreak);
        SC_TEST_EXPECT(state.result.deliveryCount >= 1);
        SC_TEST_EXPECT(eventLoop.close());
    }
#endif

    void cancelSignal()
    {
        struct State
        {
            FiberAsyncIO* io       = nullptr;
            bool          canceled = false;

            FiberAsyncSignalResult result;
        };

        AsyncEventLoop eventLoop;
        SC_TEST_EXPECT(eventLoop.create());

        FiberScheduler scheduler;
        FiberAsyncIO   io(scheduler, eventLoop);
        FiberTask      task;
        char           stackMemory[64 * 1024] = {};
        FiberStack     stack({stackMemory, sizeof(stackMemory)});

        State state;
        state.io = &io;

        SC_TEST_EXPECT(scheduler.spawn(task, stack,
                                       FiberTask::Procedure(
                                           [&state](FiberScheduler&)
                                           {
                                               Result result  = state.io->signal(SIGINT, state.result);
                                               state.canceled = not result;
                                               return result;
                                           })));

        SC_TEST_EXPECT(io.runOnce());
        SC_TEST_EXPECT(task.isActive());
        SC_TEST_EXPECT(io.cancelAll());
        SC_TEST_EXPECT(io.runNoWait());
        SC_TEST_EXPECT(state.canceled);
        SC_TEST_EXPECT(task.isCompleted());
        SC_TEST_EXPECT(not task.result());
        SC_TEST_EXPECT(eventLoop.close());
    }

    void workerPoolCancelSignal()
    {
        static constexpr size_t NumWorkers = 2;

        struct State
        {
            FiberAsyncIO* io = nullptr;

            Atomic<bool> entered = false;

            bool     canceled       = false;
            uint64_t ownerThreadID  = 0;
            uint64_t startThreadID  = 0;
            uint64_t resumeThreadID = 0;

            FiberAsyncSignalResult result;
        };

        AsyncEventLoop eventLoop;
        SC_TEST_EXPECT(eventLoop.create());

        FiberScheduler    scheduler;
        FiberAsyncCommand commandStorage[4];
        FiberAsyncIO      io(scheduler, eventLoop, commandStorage);
        FiberTask         task;
        static char       stackMemory[64 * 1024] = {};
        FiberStack        stack({stackMemory, sizeof(stackMemory)});
        FiberWorker       workers[NumWorkers];
        FiberWorkerThread threads[NumWorkers];
        FiberWorkerPool   workerPool;

        State state;
        state.io            = &io;
        state.ownerThreadID = Thread::CurrentThreadID();

        SC_TEST_EXPECT(scheduler.spawn(task, stack,
                                       FiberTask::Procedure(
                                           [&state](FiberScheduler&)
                                           {
                                               state.startThreadID = Thread::CurrentThreadID();
                                               state.entered.store(true);
                                               Result result        = state.io->signal(SIGINT, state.result);
                                               state.resumeThreadID = Thread::CurrentThreadID();
                                               state.canceled       = not result;
                                               return result;
                                           })));

        SC_TEST_EXPECT(workerPool.start(scheduler, {workers, NumWorkers}, {threads, NumWorkers}));
        for (int idx = 0; idx < 1000 and not state.entered.load(); ++idx)
        {
            SC_TEST_EXPECT(io.runOwnerNoWait());
            Thread::Sleep(1);
        }
        SC_TEST_EXPECT(state.entered.load());

        SC_TEST_EXPECT(io.runOwnerNoWait());
        SC_TEST_EXPECT(io.cancelAll());
        SC_TEST_EXPECT(io.runOwner());

        SC_TEST_EXPECT(not scheduler.hasActiveFibers());
        SC_TEST_EXPECT(workerPool.join());
        SC_TEST_EXPECT(task.isCompleted());
        SC_TEST_EXPECT(not task.result());
        SC_TEST_EXPECT(state.canceled);
        SC_TEST_EXPECT(state.startThreadID != 0);
        SC_TEST_EXPECT(state.resumeThreadID != 0);
        SC_TEST_EXPECT(state.startThreadID != state.ownerThreadID);
        SC_TEST_EXPECT(eventLoop.close());
    }
};

namespace SC
{
void runFibersAsyncTest(SC::TestReport& report) { FibersAsyncTest test(report); }
} // namespace SC
