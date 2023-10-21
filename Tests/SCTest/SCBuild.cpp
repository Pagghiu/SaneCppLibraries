// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#include "../../Libraries/Foundation/Strings/StringBuilder.h"
#include "SCBuild.h"
SC::Result SC::SCBuild::configure(Build::Definition& definition, Build::Parameters& parameters,
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
        "../../../../..",        // SC (for PluginTest)
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
    project.addFiles("Tests/SCTest", "SCTest.cpp");        // add a single cpp files
    project.addFiles("Libraries", "**.cpp");               // recursively add all cpp files
    project.addFiles("Libraries", "**.h");                 // recursively add all header files
    project.addFiles("Libraries", "**.inl");               // recursively add all inline files
    project.addFiles("Support/DebugVisualizers", "*.cpp"); // add debug visualizers
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

SC::Result SC::SCBuild::generate(Build::Generator::Type generator, StringView targetDirectory,
                                 StringView sourcesDirectory)
{
    return SC::Build::ConfigurePresets::generateAllPlatforms(configure, "SCUnitTest", generator, targetDirectory, sourcesDirectory);
}

#include "../../Support/Build/BuildBootstrap.cpp"
