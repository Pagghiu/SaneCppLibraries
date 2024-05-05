// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "SC-build.h"
#include "Libraries/Strings/StringBuilder.h"

namespace SC
{
namespace Build
{
static constexpr StringView PROJECT_NAME = "SCTest";

Result configure(Build::Definition& definition, Build::Parameters& parameters)
{
    SC_COMPILER_WARNING_PUSH_UNUSED_RESULT; // Doing some optimistic coding here, ignoring all failures

    // Workspace
    Workspace workspace;
    workspace.name.assign(PROJECT_NAME);

    // Project
    Project project;
    project.targetType = TargetType::Executable;
    project.name.assign(PROJECT_NAME);
    project.targetName.assign(PROJECT_NAME);
    // All relative paths are evaluated from this project root directory.
    project.setRootDirectory(parameters.directories.libraryDirectory.view());

    // Project Configurations
    project.addPresetConfiguration(Configuration::Preset::Debug);
    project.addPresetConfiguration(Configuration::Preset::Release);
    if (parameters.generator == Build::Generator::VisualStudio2022)
    {
        project.addPresetConfiguration(Configuration::Preset::Debug, "Debug Clang");
        project.getConfiguration("Debug Clang")->visualStudio.platformToolset = "ClangCL";
    }
    else
    {
        project.addPresetConfiguration(Configuration::Preset::DebugCoverage);
    }

    // Project Configurations special flags
    for (Configuration& config : project.configurations)
    {
        config.compile.set<Compile::enableASAN>(config.preset == Configuration::Preset::Debug);
    }

    // Defines
    // $(PROJECT_ROOT) is the same directory set with Project::setRootDirectory
    project.compile.addDefines({"SC_LIBRARY_PATH=$(PROJECT_ROOT)", "SC_COMPILER_ENABLE_CONFIG=1"});

    // Includes
    project.compile.addIncludes({
        ".",            // Libraries path (for PluginTest)
        "Tests/SCTest", // SCConfig.h path (enabled by SC_COMPILER_ENABLE_CONFIG == 1)
    });

    // Libraries to link
    if (parameters.platforms.contains(Build::Platform::MacOS))
    {
        project.link.addFrameworks({"CoreFoundation", "CoreServices"});
    }

    // File overrides (order matters regarding to add / remove)
    project.addDirectory("Bindings/c", "**.cpp");              // add all cpp support files for c bindings
    project.addDirectory("Bindings/c", "**.c");                // add all c bindings
    project.addDirectory("Bindings/c", "**.h");                // add all c bindings
    project.addDirectory("Tests/SCTest", "*.cpp");             // add all .cpp from SCTest directory
    project.addDirectory("Tests/SCTest", "*.h");               // add all .h from SCTest directory
    project.addDirectory("Libraries", "**.cpp");               // recursively add all cpp files
    project.addDirectory("Libraries", "**.h");                 // recursively add all header files
    project.addDirectory("Libraries", "**.inl");               // recursively add all inline files
    project.addDirectory("LibrariesExtra", "**.h");            // recursively add all header files
    project.addDirectory("LibrariesExtra", "**.cpp");          // recursively add all cpp files
    project.addDirectory("Support/DebugVisualizers", "*.cpp"); // add debug visualizers
    project.addDirectory("Tools", "SC-*.cpp");                 // add all tools
    project.addDirectory("Tools", "*.h");                      // add tools headers
    project.addDirectory("Tools", "*Test.cpp");                // add tools tests

    // Debug visualization helpers
    if (parameters.generator == Build::Generator::VisualStudio2022)
    {
        project.addDirectory("Support/DebugVisualizers/MSVC", "*.natvis");
    }
    else
    {
        project.addDirectory("Support/DebugVisualizers/LLDB", "*");
    }

    // Adding to workspace and definition
    workspace.projects.push_back(move(project));
    definition.workspaces.push_back(move(workspace));

    SC_COMPILER_WARNING_POP;
    return Result(true);
}

Result executeAction(const Action& action) { return Build::Action::execute(action, configure, PROJECT_NAME); }
} // namespace Build
} // namespace SC
