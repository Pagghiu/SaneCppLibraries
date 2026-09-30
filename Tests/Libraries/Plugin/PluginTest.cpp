// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include <memory.h>

#include "Libraries/Async/Async.h"
#include "Libraries/Common/Deferred.h"
#include "Libraries/FileSystem/FileSystem.h"
#include "Libraries/FileSystemIterator/FileSystemIterator.h"
#include "Libraries/FileSystemWatcher/FileSystemWatcher.h"
#include "Libraries/Memory/Buffer.h"
#include "Libraries/Memory/String.h"
#include "Libraries/Plugin/Internal/PluginFileSystem.h"
#include "Libraries/Plugin/Internal/PluginString.h"
#include "Libraries/Plugin/Plugin.h"
#include "Libraries/Plugin/PluginErrorFormatter.h"
#include "Libraries/Process/Internal/StringsArena.h"
#include "Libraries/Process/Process.h"
#include "Libraries/Strings/Path.h"
#include "Libraries/Strings/StringBuilder.h"
#include "Libraries/Testing/Testing.h"
#include "Libraries/Threading/Threading.h"
#include "Libraries/Time/Time.h"
#include "PluginTestDirectory/TestPluginChild/Interfaces.h"

#if SC_PLATFORM_WINDOWS
#include "Libraries/Plugin/Internal/VisualStudioPathFinder.h"
#endif

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
#if SC_PLATFORM_WINDOWS
        if (test_section("Visual Studio discovery respects captured output length"))
        {
            const char paths[] = "C:\\VS2022\r\nC:\\VS2025\r\n";
            char       output[2048];
            memset(output, 'x', sizeof(output));
            memcpy(output, paths, sizeof(paths) - 1);
            output[sizeof(output) - 1] = 0;

            FixedVector<StringPath, 8> parsed;
            SC_TEST_EXPECT(VisualStudioPathFinder::parsePaths({output, sizeof(paths) - 1}, parsed));
            SC_TEST_EXPECT(parsed.size() == 2);
            SC_TEST_EXPECT(parsed[0].view() == SC_NATIVE_STR("C:\\VS2022"));
            SC_TEST_EXPECT(parsed[1].view() == SC_NATIVE_STR("C:\\VS2025"));
        }
#endif
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

            PluginDefinition   malformedDefinition;
            bool               parsed    = true;
            const ResultPlugin malformed = PluginDefinition::parse("Name: only one field", malformedDefinition, parsed);
            SC_TEST_EXPECT(malformed.isError(PluginError::InvalidDefinition));
            SC_TEST_EXPECT(malformed.detail == PluginErrorDetail::MetadataVersion);
            SC_TEST_EXPECT(not parsed);

            PluginDefinition oversizedDefinition;
            constexpr char oversizedName[] = "Name: aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\n"
                                             "Version: 1\n"
                                             "Description: valid\n"
                                             "Category: valid\n";
            const ResultPlugin oversized   = PluginDefinition::parse(oversizedName, oversizedDefinition, parsed);
            SC_TEST_EXPECT(not oversized);
            SC_TEST_EXPECT(oversized.isError(PluginError::DefinitionCapacityExceeded));
            SC_TEST_EXPECT(oversized.detail == PluginErrorDetail::MetadataName);
            SC_TEST_EXPECT(oversized.contextKind == PluginErrorContextKind::RequiredBytes);
        }
        if (test_section("Plugin structured errors and formatter"))
        {
            structuredErrorsAndFormatter();
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
                const ResultPlugin     scanResult =
                    PluginScanner::scanDirectory(testPluginsPath.view(), definitions, fileStorage, definitionsSpan);
                SC_TEST_EXPECT(scanResult.isError(PluginError::ScannerCapacityExceeded));
                SC_TEST_EXPECT(scanResult.detail == PluginErrorDetail::ScannerDefinitionStorage);
                SC_TEST_EXPECT(scanResult.contextKind == PluginErrorContextKind::RequiredElements);
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
            const ResultPlugin invalidIdentifier =
                registry.loadPlugin("PluginThatDoesNotExist", compiler, sysroot, report.executableFile.view());
            SC_TEST_EXPECT(invalidIdentifier.isError(PluginError::PluginNotFound));
            SC_TEST_EXPECT(invalidIdentifier.detail == PluginErrorDetail::RegistryFindPlugin);
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
            FunctionIsPluginOriginal missingSymbol = nullptr;
            const ResultPlugin       missingSymbolResult =
                pluginChild->dynamicLibrary.getSymbol("PluginSymbolThatDoesNotExist", missingSymbol);
            SC_TEST_EXPECT(missingSymbolResult.isError(PluginError::SymbolNotFound));
#if SC_PLATFORM_WINDOWS
            SC_TEST_EXPECT(missingSymbolResult.detail == PluginErrorDetail::WindowsDynamicLibraryGetSymbol);
#else
            SC_TEST_EXPECT(missingSymbolResult.detail == PluginErrorDetail::PosixDynamicLibraryGetSymbol);
            SC_TEST_EXPECT(missingSymbolResult.contextKind == PluginErrorContextKind::None);
#endif

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

    void structuredErrorsAndFormatter()
    {
        {
            native_char_t              storage[8] = {};
            StringSpan::NativeWritable writable;
            writable.writableSpan  = storage;
            size_t           count = 0;
            StringsArena     arena(writable, count, Span<size_t>(),
                                   Result::Error(PluginResultCategory, PluginError::ArgumentCapacityExceeded));
            const StringSpan argument = SC_NATIVE_STR("arg");
            SC_TEST_EXPECT(arena.appendAsSingleString(argument).isError(PluginResultCategory,
                                                                        PluginError::ArgumentCapacityExceeded));
        }
        static_assert(sizeof(ResultPlugin) == 16, "ResultPlugin must meet the final 16-byte target");
        static_assert(__is_standard_layout(ResultPlugin), "ResultPlugin must remain standard-layout");
        static_assert(TypeTraits::IsTriviallyCopyable<ResultPlugin>::value,
                      "ResultPlugin must remain trivially copyable");

        static_assert(sizeof(PluginErrorContext) == 4, "PluginErrorContext must retain its four-byte payload");

        constexpr ResultPlugin nativeErrorContext =
            ResultPlugin::withNativeError(PluginError::FileOpenFailed, PluginErrorDetail::PosixFileOpen, 0xffffffffu);
        static_assert(nativeErrorContext.contextKind == PluginErrorContextKind::NativeError and
                          nativeErrorContext.context.nativeError == 0xffffffffu,
                      "withNativeError must initialize the nativeError member in constant evaluation");
        constexpr ResultPlugin exitCodeContext = ResultPlugin::withExitCode(
            PluginError::CompilerExitedWithFailure, PluginErrorDetail::CompilerBuildArguments, (-2147483647 - 1));
        static_assert(exitCodeContext.contextKind == PluginErrorContextKind::ExitCode and
                          exitCodeContext.context.exitCode == (-2147483647 - 1),
                      "withExitCode must initialize the exitCode member in constant evaluation");
        constexpr ResultPlugin requiredBytesContext = ResultPlugin::withRequiredBytes(
            PluginError::PathCapacityExceeded, PluginErrorDetail::CompilerBuildArguments, 0xffffffffu);
        static_assert(requiredBytesContext.contextKind == PluginErrorContextKind::RequiredBytes and
                          requiredBytesContext.context.requiredBytes == 0xffffffffu,
                      "withRequiredBytes must initialize the requiredBytes member in constant evaluation");
        constexpr ResultPlugin requiredElementsContext = ResultPlugin::withRequiredElements(
            PluginError::ArgumentCapacityExceeded, PluginErrorDetail::CompilerBuildArguments, 7);
        static_assert(requiredElementsContext.contextKind == PluginErrorContextKind::RequiredElements and
                          requiredElementsContext.context.requiredElements == 7,
                      "withRequiredElements must initialize the requiredElements member in constant evaluation");

        const ResultPlugin detailed =
            ResultPlugin::withNativeError(PluginError::FileOpenFailed, PluginErrorDetail::PosixFileOpen, 12345);

        const Result plain = detailed;
        SC_TEST_EXPECT(plain.isError(PluginResultCategory, PluginError::FileOpenFailed));
        const ResultPlugin fromPlain(plain);
        SC_TEST_EXPECT(fromPlain.detail == PluginErrorDetail::None);
        SC_TEST_EXPECT(fromPlain.contextKind == PluginErrorContextKind::None);
        SC_TEST_EXPECT(fromPlain.context.nativeError == 0);

        const Result       foreignResult = Result::Error(ResultCategory(5), 1);
        const ResultPlugin foreign(foreignResult);
        SC_TEST_EXPECT(not foreign);
        SC_TEST_EXPECT(foreign.toResult().isError(ResultCategory(5), 1));
        SC_TEST_EXPECT(foreign.detail == PluginErrorDetail::None);
        SC_TEST_EXPECT(foreign.contextKind == PluginErrorContextKind::None);

        char              message[128];
        ResultErrorFormat formatted = formatPluginError(detailed, message);
        SC_TEST_EXPECT(formatted);
        SC_TEST_EXPECT(formatted.requiredCapacity <= sizeof(message));
        SC_TEST_EXPECT(StringView::fromNullTerminated(message, StringEncoding::Ascii).containsString("12345"));

        const ResultPlugin exit = ResultPlugin::withExitCode(PluginError::CompilerExitedWithFailure,
                                                             PluginErrorDetail::CompilerBuildArguments, -42);
        char               exitMessage[128];
        formatted = formatPluginError(exit, exitMessage);
        SC_TEST_EXPECT(formatted);
        SC_TEST_EXPECT(formatted.requiredCapacity <= sizeof(exitMessage));
        SC_TEST_EXPECT(StringView::fromNullTerminated(exitMessage, StringEncoding::Ascii).containsString("-42"));

        formatted = formatPluginError(PluginError::PathCapacityExceeded, {});
        SC_TEST_EXPECT(formatted.status == ResultErrorFormatStatus::InsufficientCapacity);
        SC_TEST_EXPECT(formatted.requiredCapacity > 1);
        char tooSmall[2] = {'x', 0};
        formatted        = formatPluginError(PluginError::PathCapacityExceeded, tooSmall);
        SC_TEST_EXPECT(formatted.status == ResultErrorFormatStatus::InsufficientCapacity);
        SC_TEST_EXPECT(tooSmall[0] == 0);
        SC_TEST_EXPECT(formatPluginError(Result(true), message).status == ResultErrorFormatStatus::NotAnError);
        SC_TEST_EXPECT(formatPluginError(Result::Error(ResultCategory(99), 1), message).status ==
                       ResultErrorFormatStatus::ForeignCategory);
        SC_TEST_EXPECT(formatPluginError(Result::Error(PluginResultCategory, 999), message).status ==
                       ResultErrorFormatStatus::UnknownError);
        SC_TEST_EXPECT(
            formatPluginError(ResultPlugin(PluginError::FileOpenFailed, static_cast<PluginErrorDetail>(999)), message)
                .status == ResultErrorFormatStatus::UnknownError);
        SC_TEST_EXPECT(formatPluginError(ResultPlugin(PluginError::FileOpenFailed, PluginErrorDetail::None,
                                                      static_cast<PluginErrorContextKind>(999), {}),
                                         message)
                           .status == ResultErrorFormatStatus::UnknownError);

        {
            StringPath    path;
            native_char_t pathStorage[StringPath::MaxPath];
            for (size_t idx = 0; idx < StringPath::MaxPath; ++idx)
                pathStorage[idx] = static_cast<native_char_t>('a');
            const StringSpan   fullPath   = {{pathStorage, StringPath::MaxPath}, false, StringEncoding::Native};
            const ResultPlugin pathResult = PluginString::append(path, {fullPath, "x"});
            SC_TEST_EXPECT(pathResult.isError(PluginError::PathCapacityExceeded));
            SC_TEST_EXPECT(pathResult.detail == PluginErrorDetail::PathAppend);
        }

        {
            Buffer             fileBuffer;
            const ResultPlugin fileResult = PluginFileSystem::readAbsoluteFile(
                SC_NATIVE_STR("plugin-file-that-does-not-exist"), GrowableBuffer<Buffer>{fileBuffer});
            SC_TEST_EXPECT(fileResult.isError(PluginError::FileOpenFailed));
            SC_TEST_EXPECT(fileResult.contextKind == PluginErrorContextKind::NativeError);
#if SC_PLATFORM_WINDOWS
            SC_TEST_EXPECT(fileResult.detail == PluginErrorDetail::WindowsFileCreate);
#else
            SC_TEST_EXPECT(fileResult.detail == PluginErrorDetail::PosixFileOpen);
#endif
        }

        {
            SystemDynamicLibrary library;
            using MissingFunction        = bool (*)();
            MissingFunction    symbol    = nullptr;
            const ResultPlugin notLoaded = library.getSymbol("Missing", symbol);
            SC_TEST_EXPECT(notLoaded.isError(PluginError::DynamicLibraryNotLoaded));
#if SC_PLATFORM_WINDOWS
            SC_TEST_EXPECT(notLoaded.detail == PluginErrorDetail::WindowsDynamicLibraryGetSymbol);
#else
            SC_TEST_EXPECT(notLoaded.detail == PluginErrorDetail::PosixDynamicLibraryGetSymbol);
#endif
        }

        {
            PluginDefinition definition;
            PluginFile       file;
            SC_TEST_EXPECT(definition.directory.assign("."));
            SC_TEST_EXPECT(file.absolutePath.assign("plugin-source-that-does-not-exist.cpp"));
            SC_TEST_EXPECT(definition.files.push_back(file));

            PluginCompiler compiler;
#if SC_PLATFORM_WINDOWS
            compiler.type = PluginCompiler::Type::MicrosoftCompiler;
            SC_TEST_EXPECT(compiler.compilerPath.assign(L"Z:\\plugin-compiler-that-does-not-exist.exe"));
#else
            compiler.type = PluginCompiler::Type::GnuCompiler;
            SC_TEST_EXPECT(compiler.compilerPath.assign("/plugin-compiler-that-does-not-exist"));
#endif
            PluginSysroot             sysroot;
            PluginCompilerEnvironment environment;
            char                      logStorage[64];
            Span<char>                log             = {logStorage};
            const ResultPlugin        missingCompiler = compiler.compile(definition, sysroot, environment, log);
            SC_TEST_EXPECT(missingCompiler.toResult().isError(ProcessResultCategory, ProcessError::LaunchFailed));
            SC_TEST_EXPECT(missingCompiler.detail == PluginErrorDetail::None);
            SC_TEST_EXPECT(missingCompiler.contextKind == PluginErrorContextKind::None);
        }

#if SC_PLATFORM_APPLE || SC_PLATFORM_LINUX
        {
            PluginDefinition definition;
            PluginFile       file;
            SC_TEST_EXPECT(definition.directory.assign("."));
            SC_TEST_EXPECT(file.absolutePath.assign("plugin-source-that-does-not-exist.cpp"));
            SC_TEST_EXPECT(definition.files.push_back(file));

            PluginCompiler compiler;
            compiler.type = PluginCompiler::Type::GnuCompiler;
            SC_TEST_EXPECT(compiler.compilerPath.assign("/usr/bin/false"));
            PluginSysroot             sysroot;
            PluginCompilerEnvironment environment;
            char                      logStorage[64];
            Span<char>                log    = {logStorage};
            const ResultPlugin        exited = compiler.compile(definition, sysroot, environment, log);
            SC_TEST_EXPECT(exited.isError(PluginError::CompilerExitedWithFailure));
            SC_TEST_EXPECT(exited.detail == PluginErrorDetail::CompilerBuildArguments);
            SC_TEST_EXPECT(exited.contextKind == PluginErrorContextKind::ExitCode);
            SC_TEST_EXPECT(exited.context.exitCode == 1);
        }
#endif
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
