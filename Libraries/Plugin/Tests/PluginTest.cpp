// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "../Plugin.h"
#include "../../FileSystem/FileSystem.h"
#include "../../FileSystem/Path.h"
#include "../../FileSystemIterator/FileSystemIterator.h"
#include "../../Strings/StringBuilder.h"
#include "../../Testing/Testing.h"
#include "../../Threading/Threading.h"
#include "PluginTestDirectory/TestPluginChild/Interfaces.h"

namespace SC
{
struct PluginTest;
}

struct SC::PluginTest : public SC::TestCase
{
    SmallString<255> testPluginsPath;

    PluginTest(SC::TestReport& report) : TestCase(report, "PluginTest")
    {
        using namespace SC;
        if (test_section("PluginDefinition"))
        {
            StringView test =
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
            StringView       extracted;
            SC_TEST_EXPECT(PluginDefinition::find(test, extracted));
            SC_TEST_EXPECT(PluginDefinition::parse(extracted, definition));
            SC_TEST_EXPECT(definition.identity.name == "Test Plugin");
            SC_TEST_EXPECT(definition.identity.version == "1");
            SC_TEST_EXPECT(definition.description == "A Simple text plugin");
            SC_TEST_EXPECT(definition.category == "Generic");
            SC_TEST_EXPECT(definition.dependencies[0] == "TestPluginChild");
            SC_TEST_EXPECT(definition.dependencies[1] == "TestPlugin02");
            SC_TEST_EXPECT(definition.build[0] == "libc");
            SC_TEST_EXPECT(definition.build[1] == "libc++");
        }
        if (test_section("PluginScanner/PluginCompiler/PluginRegistry"))
        {
            SC_TEST_EXPECT(Path::join(
                testPluginsPath, {report.libraryRootDirectory, "Libraries", "Plugin", "Tests", "PluginTestDirectory"}));

            // Scan for definitions
            SmallVector<PluginDefinition, 5> definitions;
            SC_TEST_EXPECT(PluginScanner::scanDirectory(testPluginsPath.view(), definitions));
            SC_TEST_EXPECT(definitions.size() == 2);

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

            // Init compiler and sysroot
            PluginCompiler compiler;
            SC_TEST_EXPECT(PluginCompiler::findBestCompiler(compiler));
            PluginSysroot sysroot;
            SC_TEST_EXPECT(PluginSysroot::findBestSysroot(compiler.type, sysroot));
            SC_TEST_EXPECT(compiler.includePaths.push_back(report.libraryRootDirectory));

            // Setup registry
            PluginRegistry registry;
            SC_TEST_EXPECT(registry.replaceDefinitions(move(definitions)));
            SC_TEST_EXPECT(registry.loadPlugin(identifierChild, compiler, sysroot, report.executableFile));

            // Check that plugins have been compiled and are valid
            const PluginDynamicLibrary* pluginChild  = registry.findPlugin(identifierChild);
            const PluginDynamicLibrary* pluginParent = registry.findPlugin(identifierParent);
            SC_TEST_EXPECT(pluginChild->dynamicLibrary.isValid());
            SC_TEST_EXPECT(pluginParent->dynamicLibrary.isValid());

            // Query two interfaces from the child plugins and check their expected behaviour
            ITestInterface1* interface1 = nullptr;
            SC_TEST_EXPECT(pluginChild->queryInterface(interface1));
            SC_TEST_EXPECT(interface1 != nullptr);
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
            String     sourceContent;
            FileSystem fs;
            SC_TEST_EXPECT(fs.read(pluginScriptPath.view(), sourceContent, StringEncoding::Ascii));
            FileSystem::FileStat scriptFileStat;
            SC_TEST_EXPECT(fs.getFileStat(pluginScriptPath.view(), scriptFileStat));
            String sourceMod1;
            SC_TEST_EXPECT(StringBuilder(sourceMod1)
                               .appendReplaceAll(sourceContent.view(), //
                                                 "bool isPluginOriginal() { return true; }",
                                                 "bool isPluginOriginal() { return false; }"));
            String sourceMod2;
            SC_TEST_EXPECT(StringBuilder(sourceMod2).appendReplaceAll(sourceMod1.view(), "original", "MODIFIED"));
            SC_TEST_EXPECT(fs.writeString(pluginScriptPath.view(), sourceMod2.view()));

            // Reload child plugin
            SC_TEST_EXPECT(registry.loadPlugin(identifierChild, compiler, sysroot, report.executableFile,
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
            SC_TEST_EXPECT(fs.writeString(pluginScriptPath.view(), sourceContent.view()));
            SC_TEST_EXPECT(registry.removeAllBuildProducts(identifierChild));
            SC_TEST_EXPECT(registry.removeAllBuildProducts(identifierParent));

            // Restore last modified time to avoid triggering a rebuild as the file is included in the test project
            SC_TEST_EXPECT(fs.setLastModifiedTime(pluginScriptPath.view(), scriptFileStat.modifiedTime));
        }
    }
};

namespace SC
{
void runPluginTest(SC::TestReport& report) { PluginTest test(report); }
} // namespace SC
