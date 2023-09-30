// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../FileSystem/FileSystem.h"
#include "../FileSystem/Path.h"
#include "../Foundation/Containers/SmallVector.h"
#include "../Foundation/Strings/StringBuilder.h"
#include "../Testing/Test.h"
#include "Build.h"

namespace SC
{
struct BuildTest;
} // namespace SC

struct SC::BuildTest : public SC::TestCase
{
    [[nodiscard]] static Result testBuild(Build::Definition& definition, Build::Parameters& parameters,
                                          StringView rootDirectory)
    {
        using namespace Build;
        SC_COMPILER_WARNING_PUSH_UNUSED_RESULT; // Doing some optimistic coding here, ignoring all failures

        // Workspace overrides
        Workspace workspace;
        workspace.name.assign("SCUnitTest");

        // Project
        Project project;
        project.targetType = TargetType::Executable;
        project.name.assign("SCUnitTest");
        project.targetName.assign("SCUnitTest");
        project.setRootDirectory(rootDirectory);

        // Configurations
        project.addPresetConfiguration(Configuration::Preset::Debug);
        project.addPresetConfiguration(Configuration::Preset::Release, "Release");
        project.compile.addDefines({"SC_LIBRARY_PATH=$(PROJECT_DIR)/../../..", "SC_COMPILER_ENABLE_CONFIG=1"});
        project.getConfiguration("Debug")->compile.addDefines({"DEBUG=1"});
        // TODO: These includes must be relative to rootDirectory
        project.compile.addIncludes({
            "../../../..",           // Libraries
            "../../../../..",        // SC (for plugin
            "../../../Tests/SCTest", // For SCConfig.h (enabled by SC_COMPILER_ENABLE_CONFIG == 1)
        });
        if (parameters.platforms.contains(Build::Platform::MacOS))
        {
            project.link.addFrameworks({"CoreFoundation.framework", "CoreServices.framework"});
        }
        if (parameters.generator == Build::Generator::VisualStudio2022)
        {
            project.addPresetConfiguration(Configuration::Preset::Debug, "Debug Clang");
            project.getConfiguration("Debug Clang")->visualStudio.platformToolset = "ClangCL";
        }
        for (Configuration& config : project.configurations)
        {
            constexpr auto buildBase = "$(PROJECT_DIR)/../../../_Build";
            constexpr auto buildDir =
                "$(PLATFORM_DISPLAY_NAME)-$(MACOSX_DEPLOYMENT_TARGET)-$(ARCHS)-$(SC_GENERATOR)-$(CONFIGURATION)";
            StringBuilder(config.outputPath).format("{}/Output/{}", buildBase, buildDir);
            StringBuilder(config.intermediatesPath).format("{}/Intermediate/$(PROJECT_NAME)/{}", buildBase, buildDir);
            config.compile.set<Compile::enableASAN>(config.preset == Configuration::Preset::Debug);
        }

        // File overrides (order matters regarding to add / remove)
        project.addFiles("Tests/SCTest", "SCTest.cpp");      // add a single cpp files
        project.addFiles("Libraries", "**.cpp");             // recursively add all cpp files
        project.addFiles("Libraries", "**.h");               // recursively add all cpp files
        project.addFiles("Libraries", "**.inl");             // recursively add all cpp files
        project.removeFiles("Libraries/UserInterface", "*"); // Exclude anything in UserInterface
        project.addFiles("Support/DebugVisualizers", "*.h"); // add all header files
        if (parameters.generator == Build::Generator::VisualStudio2022)
        {
            project.addFiles("Support/DebugVisualizers/MSVC", "*.natvis");
        }
        else
        {
            project.addFiles("Support/DebugVisualizers/LLDB", ".lldbinit");
        }
        // Adding to workspace and definition
        workspace.projects.push_back(move(project));
        definition.workspaces.push_back(move(workspace));

        SC_COMPILER_WARNING_POP;
        return Result(true);
    }

    BuildTest(SC::TestReport& report) : TestCase(report, "BuildTest")
    {
        using namespace SC;
        String testPath;
        {
            FileSystem fs;
            SC_TRUST_RESULT(fs.init(report.applicationRootDirectory));
            // TODO: We really need mkdir recursive
            if (not fs.existsAndIsDirectory("../../SCUnitTest"))
            {
                SC_TRUST_RESULT(fs.makeDirectory({"../../SCUnitTest"}));
            }
            SC_TRUST_RESULT(Path::join(testPath, {report.applicationRootDirectory, "../../SCUnitTest"}));
        }
        if (test_section("VStudio"))
        {
            auto generatedProjectPath = testPath;
            SC_TRUST_RESULT(Path::append(generatedProjectPath, {"VisualStudio2022"}, Path::AsPosix));
            FileSystem fs;
            SC_TRUST_RESULT(fs.init(testPath.view()));
            if (not fs.existsAndIsDirectory("VisualStudio2022"))
            {
                SC_TRUST_RESULT(fs.makeDirectory({"VisualStudio2022"}));
            }
            Build::Definition definition;
            Build::Parameters parameters;
            parameters.generator     = Build::Generator::VisualStudio2022;
            parameters.platforms     = {Build::Platform::Windows};
            parameters.architectures = {Build::Architecture::Arm64, Build::Architecture::Intel64};
            SC_TEST_EXPECT(testBuild(definition, parameters, report.libraryRootDirectory));
            Build::DefinitionCompiler definitionCompiler(definition);
            SC_TEST_EXPECT(definitionCompiler.validate());
            SC_TEST_EXPECT(definitionCompiler.build());
            Build::ProjectWriter writer(definition, definitionCompiler, parameters);
            SC_TEST_EXPECT(writer.write(generatedProjectPath.view(), "SCUnitTest"));
        }
        if (test_section("XCode"))
        {
            auto generatedProjectPath = testPath;
            SC_TRUST_RESULT(Path::append(generatedProjectPath, {"MacOS"}, Path::AsPosix));
            FileSystem fs;
            SC_TRUST_RESULT(fs.init(testPath.view()));
            if (not fs.existsAndIsDirectory("MacOS"))
            {
                SC_TRUST_RESULT(fs.makeDirectory({"MacOS"}));
            }
            Build::Definition definition;
            Build::Parameters parameters;
            parameters.generator     = Build::Generator::XCode14;
            parameters.platforms     = {Build::Platform::MacOS};
            parameters.architectures = {Build::Architecture::Arm64, Build::Architecture::Intel64};
            SC_TEST_EXPECT(testBuild(definition, parameters, report.libraryRootDirectory));
            Build::DefinitionCompiler definitionCompiler(definition);
            SC_TEST_EXPECT(definitionCompiler.validate());
            SC_TEST_EXPECT(definitionCompiler.build());
#if 0
            for (auto& it : definitionCompiler.resolvedPaths)
            {
                report.console.print("{}\n", it.key);
                for (auto& path : it.value)
                {
                    report.console.print("\t - {}\n", path);
                }
            }
#endif
            Build::ProjectWriter writer(definition, definitionCompiler, parameters);
            SC_TEST_EXPECT(writer.write(generatedProjectPath.view(), "SCUnitTest"));
        }
    }
};
