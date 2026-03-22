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

    FileSystem::Operations::getExecutablePath(report.executableFile);
    FileSystem::Operations::getApplicationRootDirectory(report.applicationRootDirectory);

    SmallString<255> correctedPath;
    {
        StringView components[64];
        (void)Path::normalizeUNCAndTrimQuotes(correctedPath, SC_COMPILER_LIBRARY_PATH, Path::AsNative, components);
        SC_ASSERT_RELEASE(Path::isAbsolute(correctedPath.view(), SC::Path::AsNative));
    }
    SC_ASSERT_RELEASE(report.libraryRootDirectory.assign(correctedPath.view()));
    report.debugBreakOnFailedTest = true;

    runSCBuildTest(report);
    runBuildTest(report);
    return report.getTestReturnCode();
}
