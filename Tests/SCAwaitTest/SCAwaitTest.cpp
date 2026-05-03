// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT

namespace SC
{
struct TestReport;
void runAwaitTest(TestReport& report);
} // namespace SC

#include "Libraries/Memory/Globals.h"
#include "Libraries/Socket/Socket.h"
#include "Libraries/Strings/Console.h"
#include "Libraries/Testing/Testing.h"

SC::Console* globalConsole;

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

    runAwaitTest(report);

    return report.getTestReturnCode();
}
