// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "Libraries/Plugin/Plugin.h"
#include "Libraries/Async/Async.h"
#include "Libraries/Common/Deferred.h"
#include "Libraries/FileSystem/FileSystem.h"
#include "Libraries/FileSystemIterator/FileSystemIterator.h"
#include "Libraries/FileSystemWatcher/FileSystemWatcher.h"
#include "Libraries/Memory/Buffer.h"
#include "Libraries/Memory/String.h"
#include "Libraries/Plugin/Internal/PluginString.h"
#include "Libraries/Strings/Path.h"
#include "Libraries/Strings/StringBuilder.h"
#include "Libraries/Testing/Testing.h"
#include "Libraries/Threading/Threading.h"
#include "Libraries/Time/Time.h"
#include "PluginTestDirectory/TestPluginChild/Interfaces.h"

namespace SC
{
struct PluginTest;
using FileSystemWatcherAsync = FileSystemWatcherAsyncT<AsyncEventLoop>;
} // namespace SC

struct SC::PluginTest : public SC::TestCase
{
    StringPath testPluginsPath;

    PluginTest(SC::TestReport& report) : TestCase(report, "PluginTest")
    {
        using namespace SC;
        if (test_section("PluginDefinition"))
        {
            StringSpan test =
                R"(
                // SC_BEGIN_PLUGIN
                // Name:          Test Plugin
                // Version:       1
                // Description:   A Simple text plugin
                // Category:      Generic
                // Dependencies:  TestPluginChild,TestPlugin02
                // Build:         libc,libc++
                // SC_END_PLUGIN
            )";
            PluginDefinition definition;
            StringSpan       extracted;
            SC_TEST_EXPECT(PluginDefinition::find(test, extracted));
            SC_TEST_EXPECT(PluginDefinition::parse(extracted, definition));
            SC_TEST_EXPECT(definition.identity.name == "Test Plugin");
            SC_TEST_EXPECT(definition.identity.version == "1");
            SC_TEST_EXPECT(definition.description.view() == "A Simple text plugin");
            SC_TEST_EXPECT(definition.category.view() == "Generic");
            SC_TEST_EXPECT(definition.dependencies[0].view() == "TestPluginChild");
            SC_TEST_EXPECT(definition.dependencies[1].view() == "TestPlugin02");
            SC_TEST_EXPECT(definition.build[0] == "libc");
            SC_TEST_EXPECT(definition.build[1] == "libc++");
        }
        if (test_section("PluginScanner/PluginCompiler/PluginRegistry"))
        {
#if SC_COMPILER_FILC
            if (not report.quietMode)
            {
                report.console.printLine("PluginTest - Skipping plugin compile/load under Fil-C: plugin "
                                         "toolchain/runtime ABI integration is not implemented");
            }
#else
            StringPath sourcePluginsPath;
            SC_TEST_EXPECT(Path::join(sourcePluginsPath, {report.libraryRootDirectory.view(), "Tests", "Libraries",
                                                          "Plugin", "PluginTestDirectory"}));

            FileSystem fs;
            SC_TEST_EXPECT(fs.init(report.applicationRootDirectory.view()));

            StringPath pluginSandboxRoot;
            SC_TEST_EXPECT(Path::join(pluginSandboxRoot, {report.applicationRootDirectory.view(), "PluginTest"}));
            SC_TEST_EXPECT(fs.makeDirectoryIfNotExists(pluginSandboxRoot.view()));

            StringPath pluginSandboxName;
            SC_TEST_EXPECT(StringBuilder::format(pluginSandboxName, "PluginTestDirectory-{}-{}",
                                                 Time::Realtime::now().milliseconds, reinterpret_cast<size_t>(this)));
            SC_TEST_EXPECT(Path::join(testPluginsPath, {pluginSandboxRoot.view(), pluginSandboxName.view()}));
            SC_TEST_EXPECT(fs.copyDirectory(sourcePluginsPath.view(), testPluginsPath.view()));
            auto removeSandbox = MakeDeferred([&]() { (void)fs.removeDirectoryRecursive(testPluginsPath.view()); });

            // Scan for definitions
            {
                // Test that it fails with insufficient span
                PluginDefinition       definitions[1];
                Span<PluginDefinition> definitionsSpan;
                Buffer                 fileStorage;
                SC_TEST_EXPECT(not PluginScanner::scanDirectory(testPluginsPath.view(), definitions, fileStorage,
                                                                definitionsSpan));
            }
            PluginDefinition       definitions[3];
            Span<PluginDefinition> definitionsSpan;
            Buffer                 fileStorage;
            SC_TEST_EXPECT(
                PluginScanner::scanDirectory(testPluginsPath.view(), definitions, fileStorage, definitionsSpan));
            SC_TEST_EXPECT(definitionsSpan.sizeInElements() == 2);

            // Save parent and child plugin identifiers and paths
            const size_t parentIndex            = definitions[0].dependencies.isEmpty() ? 0 : 1;
            const size_t childIndex             = parentIndex == 0 ? 1 : 0;
            const auto   childItem              = definitions[childIndex];
            const auto   parentItem             = definitions[parentIndex];
            const auto   identifierChildString  = childItem.identity.identifier;
            const auto   identifierParentString = parentItem.identity.identifier;
            const auto   pluginScriptPath       = childItem.getMainPluginFile().absolutePath;

            const StringView identifierChild  = identifierChildString.view();
            const StringView identifierParent = identifierParentString.view();

#if SC_PLATFORM_WINDOWS
            StringPath debuggerHandlePath;
            SC_TEST_EXPECT(PluginString::assign(debuggerHandlePath,
                                                {SC_NATIVE_STR("\\Device\\Mup\\SCPluginTest\\"), identifierChild,
                                                 SC_NATIVE_STR("\\"), identifierChild, SC_NATIVE_STR(".pdb")}));
            StringPath slashSeparatedPDBPath;
            SC_TEST_EXPECT(PluginString::assign(
                slashSeparatedPDBPath, {identifierChild, SC_NATIVE_STR("/"), identifierChild, SC_NATIVE_STR(".pdb")}));
            SC_TEST_EXPECT(PluginString::pathEndsWith(debuggerHandlePath.view(), slashSeparatedPDBPath.view()));
#endif

            // Init compiler and sysroot
            PluginCompiler compiler;
            SC_TEST_EXPECT(PluginCompiler::findBestCompiler(compiler));
            PluginSysroot sysroot;
            SC_TEST_EXPECT(PluginSysroot::findBestSysroot(compiler.type, sysroot));
            SC_TEST_EXPECT(compiler.includePaths.push_back(report.libraryRootDirectory));

            // Setup registry
            PluginDynamicLibrary libraries[2]; // Provide storage space for Plugins

            PluginRegistry registry;
            registry.init(libraries);
            SC_TEST_EXPECT(registry.replaceDefinitions(move(definitionsSpan)));
            SC_TEST_EXPECT(registry.loadPlugin(identifierChild, compiler, sysroot, report.executableFile.view()));

            // Check that plugins have been compiled and are valid
            const PluginDynamicLibrary* pluginChild  = registry.findPlugin(identifierChild);
            const PluginDynamicLibrary* pluginParent = registry.findPlugin(identifierParent);
            SC_TEST_EXPECT(pluginChild->dynamicLibrary.isValid());
            SC_TEST_EXPECT(pluginParent->dynamicLibrary.isValid());

#if SC_PLATFORM_WINDOWS
            bool       childReloadMatched = false;
            StringPath watcherRelativePath;
            SC_TEST_EXPECT(watcherRelativePath.assign(identifierChild));
            SC_TEST_EXPECT(watcherRelativePath.append(SC_NATIVE_STR("\\")));
            SC_TEST_EXPECT(watcherRelativePath.append(identifierChild));
            SC_TEST_EXPECT(watcherRelativePath.append(SC_NATIVE_STR(".cpp")));
            registry.getPluginsToReloadBecauseOf(watcherRelativePath.view(), Time::Milliseconds(-1),
                                                 [&](const PluginIdentifier& plugin)
                                                 {
                                                     if (plugin.view() == identifierChild)
                                                     {
                                                         childReloadMatched = true;
                                                     }
                                                 });
            SC_TEST_EXPECT(childReloadMatched);

            bool       childReloadMatchedUppercase = false;
            StringPath watcherUppercasePath;
            SC_TEST_EXPECT(watcherUppercasePath.assign(SC_NATIVE_STR("TESTPLUGINCHILD\\TESTPLUGINCHILD.CPP")));
            registry.getPluginsToReloadBecauseOf(watcherUppercasePath.view(), Time::Milliseconds(-1),
                                                 [&](const PluginIdentifier& plugin)
                                                 {
                                                     if (plugin.view() == identifierChild)
                                                     {
                                                         childReloadMatchedUppercase = true;
                                                     }
                                                 });
            SC_TEST_EXPECT(childReloadMatchedUppercase);
#endif
            int reloadAllMatches = 0;
            registry.getPluginsToReloadBecauseOf(StringSpan(), Time::Milliseconds(-1),
                                                 [&](const PluginIdentifier&) { reloadAllMatches++; });
            SC_TEST_EXPECT(reloadAllMatches == 2);

            // Query two interfaces from the child plugins and check their expected behaviour
            ITestInterface1* interface1 = nullptr;
            SC_TEST_EXPECT(pluginChild->queryInterface(interface1));
            SC_TEST_EXPECT(interface1 != nullptr);
            SC_TEST_EXPECT(interface1->InterfaceHash == SC::PluginHash("ITestInterface1"));
            SC_TEST_EXPECT(interface1->multiplyInt(2) == 4);
            ITestInterface2* interface2 = nullptr;
            SC_TEST_EXPECT(pluginChild->queryInterface(interface2));
            SC_TEST_EXPECT(interface2 != nullptr);
            SC_TEST_EXPECT(interface2->divideFloat(4.0) == 2.0);

            // Manually grab an exported function and check its return value
            using FunctionIsPluginOriginal = bool (*)();
            FunctionIsPluginOriginal isPluginOriginal;
            SC_TEST_EXPECT(pluginChild->dynamicLibrary.getSymbol("isPluginOriginal", isPluginOriginal));
            SC_TEST_EXPECT(isPluginOriginal());

            // Modify child plugin to change return value of the exported function
            String sourceContent = StringEncoding::Ascii;
            SC_TEST_EXPECT(fs.read(pluginScriptPath.view(), sourceContent));
            String sourceMod1;
            SC_TEST_EXPECT(StringBuilder::create(sourceMod1)
                               .appendReplaceAll(sourceContent.view(), //
                                                 "bool isPluginOriginal() { return true; }",
                                                 "bool isPluginOriginal() { return false; }"));
            String sourceMod2;
            SC_TEST_EXPECT(
                StringBuilder::create(sourceMod2).appendReplaceAll(sourceMod1.view(), "original", "MODIFIED"));
            SC_TEST_EXPECT(fs.writeString(pluginScriptPath.view(), sourceMod2.view()));

            // Reload child plugin
            SC_TEST_EXPECT(registry.loadPlugin(identifierChild, compiler, sysroot, report.executableFile.view(),
                                               PluginRegistry::LoadMode::Reload));

            // Check child return value of the exported function for the modified plugin
            SC_TEST_EXPECT(pluginChild->dynamicLibrary.isValid());
            SC_TEST_EXPECT(pluginChild->dynamicLibrary.getSymbol("isPluginOriginal", isPluginOriginal));
            SC_TEST_EXPECT(not isPluginOriginal());

            // Unload parent plugin
            SC_TEST_EXPECT(registry.unloadPlugin(identifierParent));

            // Check that both parent and child plugin have been unloaded
            SC_TEST_EXPECT(not pluginChild->dynamicLibrary.isValid());
            SC_TEST_EXPECT(not pluginParent->dynamicLibrary.isValid());

            // Cleanup
            SC_TEST_EXPECT(registry.removeAllBuildProducts(identifierChild));
            SC_TEST_EXPECT(registry.removeAllBuildProducts(identifierParent));
#endif
        }
        if (test_section("PluginCompiler can include C++ headers without linking the C++ runtime"))
        {
#if SC_COMPILER_FILC
            if (not report.quietMode)
            {
                report.console.printLine("PluginTest - Skipping plugin compile under Fil-C: plugin "
                                         "toolchain/runtime ABI integration is not implemented");
            }
#else
            FileSystem fs;
            SC_TEST_EXPECT(fs.init(report.applicationRootDirectory.view()));

            StringPath pluginSandboxRoot;
            SC_TEST_EXPECT(Path::join(pluginSandboxRoot, {report.applicationRootDirectory.view(), "PluginTest"}));
            SC_TEST_EXPECT(fs.makeDirectoryIfNotExists(pluginSandboxRoot.view()));

            StringPath pluginSandboxName;
            SC_TEST_EXPECT(StringBuilder::format(pluginSandboxName, "PluginStdHeaderNoRuntime-{}-{}",
                                                 Time::Realtime::now().milliseconds, reinterpret_cast<size_t>(this)));
            StringPath pluginSandboxPath;
            SC_TEST_EXPECT(Path::join(pluginSandboxPath, {pluginSandboxRoot.view(), pluginSandboxName.view()}));
            SC_TEST_EXPECT(fs.makeDirectoryIfNotExists(pluginSandboxPath.view()));
            auto removeSandbox = MakeDeferred([&]() { (void)fs.removeDirectoryRecursive(pluginSandboxPath.view()); });

            StringPath pluginDirectory;
            SC_TEST_EXPECT(Path::join(pluginDirectory, {pluginSandboxPath.view(), "StdHeaderNoRuntime"}));
            SC_TEST_EXPECT(fs.makeDirectoryIfNotExists(pluginDirectory.view()));

            StringPath pluginSourcePath;
            SC_TEST_EXPECT(Path::join(pluginSourcePath, {pluginDirectory.view(), "StdHeaderNoRuntime.cpp"}));
            SC_TEST_EXPECT(fs.writeString(pluginSourcePath.view(),
                                          R"(// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include <initializer_list>
#include <Libraries/Plugin/PluginMacros.h>

struct StdHeaderNoRuntime
{
    [[nodiscard]] bool init()
    {
        std::initializer_list<int> values = {1, 2, 3};
        return values.size() == 3;
    }

    [[nodiscard]] bool close() { return true; }
};

// SC_BEGIN_PLUGIN
//
// Name:          StdHeaderNoRuntime
// Version:       1
// Description:   Includes C++ standard headers but does not request the C++ runtime
// Category:      Generic
// Build:         libc
// Dependencies:
//
// SC_END_PLUGIN
SC_PLUGIN_DEFINE(StdHeaderNoRuntime)
)"));

            PluginDefinition       definitions[1];
            Span<PluginDefinition> definitionsSpan;
            Buffer                 fileStorage;
            SC_TEST_EXPECT(
                PluginScanner::scanDirectory(pluginSandboxPath.view(), definitions, fileStorage, definitionsSpan));
            SC_TEST_EXPECT(definitionsSpan.sizeInElements() == 1);
            SC_TEST_EXPECT(definitions[0].build[0] == "libc");
            SC_TEST_EXPECT(definitions[0].build.size() == 1);

            PluginCompiler compiler;
            SC_TEST_EXPECT(PluginCompiler::findBestCompiler(compiler));
            PluginSysroot sysroot;
            SC_TEST_EXPECT(PluginSysroot::findBestSysroot(compiler.type, sysroot));
            SC_TEST_EXPECT(compiler.includePaths.push_back(report.libraryRootDirectory));

            PluginCompilerEnvironment environment;
            char                      compilerLogStorage[4096];
            Span<char>                compilerLog = {compilerLogStorage};
            SC_TEST_EXPECT(compiler.compile(definitions[0], sysroot, environment, compilerLog));
#endif
        }
    }
};

namespace SC
{
void runPluginTest(SC::TestReport& report) { PluginTest test(report); }
} // namespace SC

#define SC_PLUGIN_DEFINE(a)
#define SC_PLUGIN_EXPORT_INTERFACES(a, b)
//! [PluginSnippet]
#include "Libraries/Async/Async.h"
#include "Libraries/FileSystemWatcher/FileSystemWatcher.h"

namespace SC
{
//-----------------------------------------------------------------------------
// IPluginContract.h
//-----------------------------------------------------------------------------
// CLIENT - HOST Contract (Interface)
struct IPluginContract
{
    static constexpr auto InterfaceHash = PluginHash("IPluginContract");

    Function<void(void)> onDraw;
};

//-----------------------------------------------------------------------------
// PluginClient.cpp
//-----------------------------------------------------------------------------
// CLIENT Plugin (binds to contract functions)
struct PluginClient : public IPluginContract
{
    PluginClient()
    {
        IPluginContract::onDraw = []()
        {
            // Draw stuff...
        };
    }

    // Called when plugin is init
    bool init() { return true; }

    // Called when plugin is closed
    bool close() { return true; }
};
SC_PLUGIN_DEFINE(PluginClient);
SC_PLUGIN_EXPORT_INTERFACES(PluginClient, IPluginContract);

//-----------------------------------------------------------------------------
// PluginHost.cpp
//-----------------------------------------------------------------------------
// Plugin HOST (loads plugin, obtains interface and calls functions)
struct PluginHost
{
    // Fill directories before calling create
    StringPath executablePath;       // Where executable lives
    StringPath libraryRootDirectory; // Where Sane C++ Libraries live
    StringPath someLibraryDirectory; // Where 3rd party-lib headers live
    StringPath pluginsPath;          // Where Plugins live

    PluginRegistry registry;

    Result create(AsyncEventLoop& loop)
    {
        eventLoop = &loop;

        // Setup Compiler
        SC_TRY(PluginCompiler::findBestCompiler(compiler));
        SC_TRY(PluginSysroot::findBestSysroot(compiler.type, sysroot));

        // Add includes used by plugins...
        SC_TRY(compiler.includePaths.push_back(libraryRootDirectory));
        SC_TRY(compiler.includePaths.push_back(someLibraryDirectory));

        // Setup File System Watcher
        fileSystemWatcherRunner.init(*eventLoop);
        SC_TRY(fileSystemWatcher.init(fileSystemWatcherRunner));
        watcher.notifyCallback.bind<PluginHost, &PluginHost::onFileChanged>(*this);
        SC_TRY(fileSystemWatcher.watch(watcher, pluginsPath.view()));
        return Result(true);
    }

    Result close()
    {
        SC_TRY(fileSystemWatcher.close());
        eventLoop = nullptr;
        return Result(true);
    }

    Result syncRegistry()
    {
        // Max 16 definitions, but you can use heap allocation if you need an arbitrary limit
        PluginDefinition       definitions[16];
        Span<PluginDefinition> definitionsSpan;
        Buffer                 fileStorage;
        SC_TRY(PluginScanner::scanDirectory(pluginsPath.view(), definitions, fileStorage, definitionsSpan))
        SC_TRY(registry.replaceDefinitions(move(definitionsSpan)));
        return Result(true);
    }

    // Call this to load a plugin with a given identifier
    Result load(StringView identifier)
    {
        // Force reload of plugin if already loaded
        SC_TRY(registry.loadPlugin(identifier, compiler, sysroot, executablePath.view(),
                                   PluginRegistry::LoadMode::Reload));

        // Obtain contract
        const PluginDynamicLibrary* plugin = registry.findPlugin(identifier);
        SC_TRY(plugin->queryInterface(contract));
        return Result(true);
    }

    void draw()
    {
        if (contract)
        {
            contract->onDraw();
        }
    }

  private:
    AsyncEventLoop* eventLoop;
    PluginCompiler  compiler;
    PluginSysroot   sysroot;

    IPluginContract* contract = nullptr;

    FileSystemWatcher fileSystemWatcher;

    FileSystemWatcher::FolderWatcher watcher;
    FileSystemWatcherAsync           fileSystemWatcherRunner;

    void onFileChanged(const FileSystemWatcher::Notification& notification)
    {
        auto reload = [this](const PluginIdentifier& plugin) { (void)load(plugin.view()); };
        registry.getPluginsToReloadBecauseOf(notification.relativePath, Time::Milliseconds(500), reload);
    }
};
} // namespace SC
//! [PluginSnippet]
