// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "Libraries/FileSystem/FileSystem.h"
#include "Libraries/Plugin/Internal/DynamicLibrary.h"
#include "Libraries/Plugin/Plugin.h"
#include "Libraries/Strings/Path.h"
#include "Libraries/Testing/Testing.h"

namespace SC
{
struct PluginDynamicLibraryTest;
}

struct SC::PluginDynamicLibraryTest : public SC::TestCase
{
    PluginDynamicLibraryTest(TestReport& report) : TestCase(report, "PluginDynamicLibraryTest")
    {
        if (test_section("unloaded and missing library"))
            unloadedAndMissingLibrary();
        if (test_section("compiler discovery paths"))
        {
            PluginCompiler     compiler;
            const ResultPlugin result = PluginCompiler::findBestCompiler(compiler);
            SC_TEST_EXPECT(not result or (not compiler.compilerPath.isEmpty() and not compiler.linkerPath.isEmpty()));
        }
    }

    void unloadedAndMissingLibrary()
    {
        SystemDynamicLibrary library;
        using Function  = int (*)();
        Function symbol = nullptr;
        SC_TEST_EXPECT(not library.isValid());
        SC_TEST_EXPECT(library.getSymbol("missingSymbol", symbol).isError(PluginError::DynamicLibraryNotLoaded));
        SC_TEST_EXPECT(symbol == nullptr);
        SC_TEST_EXPECT(library.close());

        StringPath missingPath;
        const bool pathReady =
            Path::join(missingPath, {report.applicationRootDirectory.view(), "__sc_missing_dynamic_library__/missing"});
        SC_TEST_EXPECT(pathReady);
        if (not pathReady)
            return;
        const bool exists = FileSystem::Operations::existsAndIsFile(missingPath.view());
        SC_TEST_EXPECT(not exists);
        if (exists)
            return;
        SC_TEST_EXPECT(library.load(missingPath.view()).isError(PluginError::DynamicLibraryLoadFailed));
        SC_TEST_EXPECT(not library.isValid());
        SC_TEST_EXPECT(library.close());
    }
};

namespace SC
{
void runPluginDynamicLibraryTest(TestReport& report) { PluginDynamicLibraryTest test(report); }
} // namespace SC
