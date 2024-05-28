// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "SC-build.h"
#include "Libraries/Strings/StringBuilder.h"

namespace SC
{
namespace Build
{
SC_COMPILER_WARNING_PUSH_UNUSED_RESULT; // Doing some optimistic coding here, ignoring all failures

void addSaneCppLibraries(Project& project, const Parameters& parameters)
{
    // Files
    project.addDirectory("Bindings/c", "**.cpp");              // add all cpp support files for c bindings
    project.addDirectory("Bindings/c", "**.c");                // add all c bindings
    project.addDirectory("Bindings/c", "**.h");                // add all c bindings
    project.addDirectory("Libraries", "**.cpp");               // recursively add all cpp files
    project.addDirectory("Libraries", "**.h");                 // recursively add all header files
    project.addDirectory("Libraries", "**.inl");               // recursively add all inline files
    project.addDirectory("LibrariesExtra", "**.h");            // recursively add all header files
    project.addDirectory("LibrariesExtra", "**.cpp");          // recursively add all cpp files
    project.addDirectory("Support/DebugVisualizers", "*.cpp"); // add debug visualizers

    // Libraries to link
    if (parameters.platform == Platform::MacOS)
    {
        project.link.addFrameworks({"CoreFoundation", "CoreServices"});
    }

    if (parameters.platform != Platform::Windows)
    {
        project.link.addLibraries({"dl", "pthread"});
    }

    // Debug visualization helpers
    if (parameters.generator == Generator::VisualStudio2022)
    {
        project.addDirectory("Support/DebugVisualizers/MSVC", "*.natvis");
    }
    else
    {
        project.addDirectory("Support/DebugVisualizers/LLDB", "*");
    }
}

static constexpr StringView TEST_PROJECT_NAME = "SCTest";

Project buildTestProject(const Parameters& parameters)
{
    Project project = {TargetType::Executable, TEST_PROJECT_NAME};

    // All relative paths are evaluated from this project root directory.
    project.setRootDirectory(parameters.directories.libraryDirectory.view());

    // Project Configurations
    project.addPresetConfiguration(Configuration::Preset::Debug, parameters);
    project.addPresetConfiguration(Configuration::Preset::Release, parameters);
    project.addPresetConfiguration(Configuration::Preset::DebugCoverage, parameters);

    // Defines
    // $(PROJECT_ROOT) expands to Project::setRootDirectory expressed relative to $(PROJECT_DIR)
    project.compile.addDefines({"SC_LIBRARY_PATH=$(PROJECT_ROOT)", "SC_COMPILER_ENABLE_CONFIG=1"});

    // Includes
    project.compile.addIncludes({
        ".",            // Libraries path (for PluginTest)
        "Tests/SCTest", // SCConfig.h path (enabled by SC_COMPILER_ENABLE_CONFIG == 1)
    });

    addSaneCppLibraries(project, parameters);
    project.addDirectory("Tests/SCTest", "*.cpp"); // add all .cpp from SCTest directory
    project.addDirectory("Tests/SCTest", "*.h");   // add all .h from SCTest directory
    project.addDirectory("Tools", "SC-*.cpp");     // add all tools
    project.addDirectory("Tools", "*.h");          // add tools headers
    project.addDirectory("Tools", "*Test.cpp");    // add tools tests
    return project;
}

Result configure(Definition& definition, const Parameters& parameters)
{
    Workspace workspace = {"SCTest"};
    workspace.projects.push_back(buildTestProject(parameters));
    definition.workspaces.push_back(move(workspace));
    return Result(true);
}
SC_COMPILER_WARNING_POP;

Result executeAction(const Action& action) { return Build::Action::execute(action, configure, TEST_PROJECT_NAME); }
} // namespace Build
} // namespace SC
