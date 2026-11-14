// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "Libraries/Common/Deferred.h"
#include "Libraries/FileSystem/FileSystem.h"
#include "Libraries/Plugin/Plugin.h"
#include "Libraries/Strings/Path.h"
#include "Libraries/Strings/StringBuilder.h"
#include "Libraries/Testing/Testing.h"

namespace SC
{
struct PluginCompilerSmokeTest;
}

struct SC::PluginCompilerSmokeTest : public SC::TestCase
{
    PluginCompilerSmokeTest(TestReport& report) : TestCase(report, "PluginCompilerSmokeTest")
    {
        if (test_section("compile and load"))
            compileAndLoad();
    }

    void compileAndLoad()
    {
        PluginCompiler     compiler;
        const ResultPlugin discovery = PluginCompiler::findBestCompiler(compiler);
        if (not discovery)
        {
            SC_TEST_EXPECT(discovery.isError(PluginError::CompilerNotFound));
            if (not report.quietMode)
                report.console.printLine("PluginCompilerSmokeTest: compiler unavailable; discovery checked");
            return;
        }

        const ResultPlugin sysrootResult = PluginSysroot::findBestSysroot(compiler.type, sysroot);
        SC_TEST_EXPECT(sysrootResult);
        if (not sysrootResult)
            return;
        const bool includeAdded = compiler.includePaths.push_back(report.libraryRootDirectory);
        SC_TEST_EXPECT(includeAdded);
        if (not includeAdded)
            return;

        FileSystem   fs;
        const Result fsResult = fs.init(report.applicationRootDirectory.view());
        SC_TEST_EXPECT(fsResult);
        if (not fsResult)
            return;

        StringPath directoryName;
        bool       created = false;
        for (size_t attempt = 0; attempt < 128 and not created; ++attempt)
        {
            if (not StringBuilder::format(directoryName, "__sc plugin smoke {0}", attempt))
                break;
            created = static_cast<bool>(fs.makeDirectory(directoryName.view()));
        }
        SC_TEST_EXPECT(created);
        if (not created)
            return;

        PluginDynamicLibrary storage[1];
        PluginRegistry       registry;
        registry.init({storage, 1});
        auto cleanup = MakeDeferred(
            [&]
            {
                (void)registry.close();
                (void)fs.removeDirectoryRecursive(directoryName.view());
            });

        StringPath directoryPath;
        StringPath sourcePath;
        const bool pathsReady =
            Path::join(directoryPath, {report.applicationRootDirectory.view(), directoryName.view()}) and
            Path::join(sourcePath, {directoryPath.view(), "fixture.cpp"});
        SC_TEST_EXPECT(pathsReady);
        if (not pathsReady)
            return;

        FileSystem   fixtureFs;
        const Result fixtureFsResult = fixtureFs.init(directoryPath.view());
        SC_TEST_EXPECT(fixtureFsResult);
        if (not fixtureFsResult)
            return;
        const StringSpan source = R"(
#include "Libraries/Plugin/PluginMacros.h"
SC_PLUGIN_LINKER_DEFINITIONS
static int instance;
extern "C" SC_PLUGIN_EXPORT bool fixtureInit(void*& output) { output = &instance; return true; }
extern "C" SC_PLUGIN_EXPORT bool fixtureClose(void*) { return true; }
extern "C" SC_PLUGIN_EXPORT int fixtureValue()
{
    int* value = static_cast<int*>(SC::Memory::allocate(sizeof(int), alignof(int)));
    if (value == nullptr) return -1;
    *value = 84;
    const int result = *value;
    SC::Memory::release(value);
    return result;
}
)";

        const Result writeResult = fixtureFs.writeString("fixture.cpp", source);
        SC_TEST_EXPECT(writeResult);
        if (not writeResult)
            return;

        PluginDefinition definitions[1];
        PluginFile       sourceFile;

        const bool definitionReady = definitions[0].identity.identifier.assign("fixture") and
                                     definitions[0].directory.assign(directoryPath.view()) and
                                     sourceFile.absolutePath.assign(sourcePath.view()) and
                                     definitions[0].files.push_back(sourceFile);
        SC_TEST_EXPECT(definitionReady);
        if (not definitionReady)
            return;
        Span<PluginDefinition> definitionSpan = {definitions, 1};

        const ResultPlugin replaced = registry.replaceDefinitions(move(definitionSpan));
        SC_TEST_EXPECT(replaced);
        if (not replaced)
            return;
        const ResultPlugin loaded = registry.loadPlugin("fixture", compiler, sysroot, report.executableFile.view());
        SC_TEST_EXPECT(loaded);
        if (not loaded)
        {
            const PluginDynamicLibrary* failed = registry.findPlugin("fixture");
            if (failed != nullptr and not failed->lastErrorLog.isEmpty())
                report.console.printLine(failed->lastErrorLog);
            return;
        }

        PluginDynamicLibrary* plugin = registry.findPlugin("fixture");
        SC_TEST_EXPECT(plugin != nullptr);
        if (plugin == nullptr)
            return;
        using ValueFunction = int (*)();
        ValueFunction value = nullptr;

        const ResultPlugin found = plugin->dynamicLibrary.getSymbol("fixtureValue", value);
        SC_TEST_EXPECT(found and value != nullptr);
        if (found and value != nullptr)
            SC_TEST_EXPECT(value() == 84);
        const ResultPlugin unloaded = registry.unloadPlugin("fixture");
        SC_TEST_EXPECT(unloaded);
        if (not unloaded)
            return;
        const ResultPlugin productsRemoved = registry.removeAllBuildProducts("fixture");
        SC_TEST_EXPECT(productsRemoved);
        if (not productsRemoved)
            return;
        const ResultPlugin closed = registry.close();
        SC_TEST_EXPECT(closed);
        if (not closed)
            return;
        const ResultFileSystem removeResult = fs.removeDirectoryRecursive(directoryName.view());
        if (not removeResult)
        {
            const size_t nativeError = removeResult.contextKind == FileSystemErrorContextKind::NativeError
                                           ? removeResult.context.nativeError
                                           : 0;
            report.console.print("PluginCompilerSmokeTest cleanup detail={} native={}\n",
                                 static_cast<size_t>(removeResult.detail), nativeError);
        }
        SC_TEST_EXPECT(removeResult);
        if (removeResult)
            cleanup.disarm();
    }

    PluginSysroot sysroot;
};

namespace SC
{
void runPluginCompilerSmokeTest(TestReport& report) { PluginCompilerSmokeTest test(report); }
} // namespace SC
