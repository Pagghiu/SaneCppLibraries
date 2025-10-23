// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT

// Remember to compile this with SC_COMPILER_ENABLE_STD_CPP=1, and possibly exceptions and RTTI enabled

#include "Libraries/FileSystem/FileSystem.h"
#include "Libraries/Memory/Globals.h"
#include "Libraries/Strings/Console.h"
#include "Libraries/Testing/Testing.h"

namespace SC
{
void runCppSTLStringsTest(SC::TestReport& report);
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

    TestReport report(trConsole, argc, argv);
    FileSystem::Operations::getExecutablePath(report.executableFile);
    FileSystem::Operations::getApplicationRootDirectory(report.applicationRootDirectory);
    report.debugBreakOnFailedTest = true;

    runCppSTLStringsTest(report);

    return report.getTestReturnCode();
}
