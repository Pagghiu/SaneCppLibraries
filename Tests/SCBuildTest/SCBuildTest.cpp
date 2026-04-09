// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT

#include "SCConfig.h"

#include "Libraries/FileSystem/FileSystem.h"
#include "Libraries/Memory/Globals.h"
#include "Libraries/Memory/String.h"
#include "Libraries/Strings/Console.h"
#include "Libraries/Strings/Path.h"
#include "Libraries/Testing/Testing.h"

namespace SC
{
struct TestReport;
void runSCBuildTest(TestReport& report);
void runBuildTest(TestReport& report);
} // namespace SC

SC::Console* globalConsole = nullptr;

int main(int argc, const char* argv[])
{
    using namespace SC;
    Globals::init(Globals::Global, {1024 * 1024}); // 1MB for ownership tracking
    Console::tryAttachingToParentConsole();

    Console console;
    globalConsole = &console;

    TestReport::Output<Console> trConsole = {console};
    TestReport                  report(trConsole, argc, argv);
    if (report.hasStartupFailure())
        return report.getTestReturnCode();
    report.debugBreakOnFailedTest = true;

    runSCBuildTest(report);
    runBuildTest(report);
    return report.getTestReturnCode();
}
