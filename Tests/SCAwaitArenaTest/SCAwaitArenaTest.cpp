// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT

#include "Libraries/Await/Await.h"
#include "Libraries/Memory/Globals.h"
#include "Libraries/Socket/Socket.h"
#include "Libraries/Strings/Console.h"
#include "Libraries/Testing/Testing.h"
#include "Libraries/Time/Time.h"

SC::Console* globalConsole;

namespace SC
{
static AwaitTask arenaRequiredTask(AwaitEventLoop& await)
{
    SC_CO_TRY(co_await await.sleep(1_ms));
    co_return Result(true);
}

struct AwaitArenaRequiredTest : public TestCase
{
    AwaitArenaRequiredTest(TestReport& report) : TestCase(report, "AwaitArenaRequiredTest")
    {
        if (test_section("missing arena fails"))
        {
            missingArenaFails();
        }
        if (test_section("arena succeeds"))
        {
            arenaSucceeds();
        }
    }

    void missingArenaFails()
    {
        AsyncEventLoop async;
        SC_TEST_EXPECT(async.create());

        AwaitEventLoop await(async);
        SC_TEST_EXPECT(not await.hasArena());

        AwaitTask task = arenaRequiredTask(await);
        SC_TEST_EXPECT(not task.isValid());
        SC_TEST_EXPECT(not await.spawn(task));
        SC_TEST_EXPECT(async.close());
    }

    void arenaSucceeds()
    {
        AsyncEventLoop async;
        SC_TEST_EXPECT(async.create());

        char           arenaMemory[16 * 1024] = {};
        AwaitArena     arena({arenaMemory, sizeof(arenaMemory)});
        AwaitEventLoop await(async, &arena);
        SC_TEST_EXPECT(await.hasArena());

        AwaitTask task = arenaRequiredTask(await);
        SC_TEST_EXPECT(task.isValid());
        SC_TEST_EXPECT(await.spawn(task));
        SC_TEST_EXPECT(await.run());
        SC_TEST_EXPECT(task.result());
        SC_TEST_EXPECT(arena.used() > 0);
        SC_TEST_EXPECT(arena.failedAllocationSize() == 0);
        SC_TEST_EXPECT(async.close());
    }
};
} // namespace SC

int main(int argc, const char* argv[])
{
    using namespace SC;

    Globals::init(Globals::Global, {1024 * 1024});
    Console::tryAttachingToParentConsole();
    SocketNetworking::initNetworking();

    Console console;
    globalConsole = &console;

    TestReport::Output<Console> trConsole = {console};
    TestReport                  report(trConsole, argc, argv);
    if (report.hasStartupFailure())
    {
        return report.getTestReturnCode();
    }
    report.debugBreakOnFailedTest = true;

    AwaitArenaRequiredTest test(report);
    return report.getTestReturnCode();
}
