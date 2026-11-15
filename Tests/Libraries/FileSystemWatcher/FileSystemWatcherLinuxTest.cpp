// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "Libraries/Common/PlatformMacrosType.h"

#if SC_PLATFORM_LINUX
#include "Libraries/FileSystemWatcher/FileSystemWatcher.h"
#include "Libraries/Testing/Testing.h"

#include <errno.h>
#include <fcntl.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>

namespace SC
{
struct FileSystemWatcherLinuxTest : TestCase
{
    FileSystemWatcherLinuxTest(TestReport& report) : TestCase(report, "FileSystemWatcherLinuxTest")
    {
        if (test_section("one watcher owner per runner"))
            checkRunnerOwnership();
        if (test_section("shutdown descriptors close before watching"))
            checkDescriptors(Scenario::CloseWithoutWatch);
        if (test_section("shutdown pipe failure leaves watcher reusable"))
            checkDescriptors(Scenario::ShutdownPipeFailure);
        if (test_section("inotify initialization failure rolls back shutdown pipe"))
            checkDescriptors(Scenario::InotifyFailure);
    }

  private:
    void checkRunnerOwnership()
    {
        struct IdleRunner : FileSystemWatcher::EventLoopRunner
        {
            Result linuxStartSharedFileReadiness() override { return Result(true); }
            Result linuxStopSharedFileReadiness() override { return Result(true); }
        } idle;
        FileSystemWatcher::ThreadRunner thread;
        FileSystemWatcher               first, second;

        SC_TEST_EXPECT(first.init(thread));
        SC_TEST_EXPECT(first.init(idle).isError(FileSystemWatcherError::InitializationFailed));
        SC_TEST_EXPECT(second.init(thread).isError(FileSystemWatcherError::InitializationFailed));
        SC_TEST_EXPECT(first.close());
        SC_TEST_EXPECT(second.init(thread));
        SC_TEST_EXPECT(second.close());

        SC_TEST_EXPECT(first.init(idle));
        SC_TEST_EXPECT(first.init(thread).isError(FileSystemWatcherError::InitializationFailed));
        SC_TEST_EXPECT(second.init(idle).isError(FileSystemWatcherError::InitializationFailed));
        SC_TEST_EXPECT(first.close());
        SC_TEST_EXPECT(second.init(idle));
        SC_TEST_EXPECT(second.close());
    }

    enum class Scenario
    {
        CloseWithoutWatch,
        ShutdownPipeFailure,
        InotifyFailure,
    };

    static int descriptorScenario(Scenario scenario)
    {
        struct rlimit originalLimit;
        if (::getrlimit(RLIMIT_NOFILE, &originalLimit) == -1)
            return 1;

        // Locate the exact next descriptor numbers even if the test inherited gaps in its descriptor table.
        const int first = ::open("/dev/null", O_RDONLY | O_CLOEXEC);
        if (first == -1)
            return 2;
        const int second = ::dup(first);
        if (second == -1)
            return 3;
        if (::close(first) == -1 or ::close(second) == -1)
            return 4;

        struct rlimit scopedLimit = originalLimit;
        if (scenario != Scenario::CloseWithoutWatch)
        {
            // One slot makes pipe2 fail. Two slots allow pipe2 but leave none for inotify_init1.
            scopedLimit.rlim_cur = static_cast<rlim_t>(scenario == Scenario::ShutdownPipeFailure ? first : second) + 1;
            if (::setrlimit(RLIMIT_NOFILE, &scopedLimit) == -1)
                return 5;
        }

        FileSystemWatcher::ThreadRunner runner;
        FileSystemWatcher               watcher;
        const ResultFileSystemWatcher   initialized = watcher.init(runner);
        if (scenario == Scenario::CloseWithoutWatch)
        {
            if (not initialized or not watcher.close())
                return 6;
        }
        else
        {
            const auto detail = scenario == Scenario::ShutdownPipeFailure
                                    ? FileSystemWatcherErrorDetail::LinuxCreateShutdownPipe
                                    : FileSystemWatcherErrorDetail::LinuxInitializeInotify;
            if (not initialized.isError(FileSystemWatcherError::InitializationFailed) or initialized.detail != detail or
                initialized.nativeError != EMFILE or
                not watcher.close().isError(FileSystemWatcherError::NotInitialized))
                return 7;
        }

        const int firstReused  = ::open("/dev/null", O_RDONLY | O_CLOEXEC);
        const int secondReused = ::dup(firstReused);
        if (firstReused != first or (scenario == Scenario::ShutdownPipeFailure ? secondReused != -1 or errno != EMFILE
                                                                               : secondReused != second))
            return 8;
        if (::close(firstReused) == -1 or (secondReused >= 0 and ::close(secondReused) == -1))
            return 9;

        if (scenario != Scenario::CloseWithoutWatch)
        {
            if (::setrlimit(RLIMIT_NOFILE, &originalLimit) == -1)
                return 10;
            if (not watcher.init(runner) or not watcher.close())
                return 11;
        }
        return 0;
    }

    void checkDescriptors(Scenario scenario)
    {
        // Keep the descriptor limit and any failed cleanup isolated from the suite.
        const pid_t child = ::fork();
        SC_TEST_EXPECT(child >= 0);
        if (child == 0)
            ::_exit(descriptorScenario(scenario));
        if (child < 0)
            return;

        int   status = 0;
        pid_t waited;
        do
        {
            waited = ::waitpid(child, &status, 0);
        } while (waited == -1 and errno == EINTR);
        SC_TEST_EXPECT(waited == child and WIFEXITED(status));
        if (waited == child and WIFEXITED(status))
        {
            const int childExitCode = WEXITSTATUS(status);
            SC_TEST_EXPECT(childExitCode == 0);
        }
    }
};

void runFileSystemWatcherLinuxTest(TestReport& report) { FileSystemWatcherLinuxTest test(report); }
} // namespace SC
#endif
