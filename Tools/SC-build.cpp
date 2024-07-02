// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "SC-build.h"
#include "Libraries/Strings/StringBuilder.h"
#include "SC-package.h"

namespace SC
{
namespace Tools
{
[[nodiscard]] Result installSokol(const Build::Directories& directories, Package& package)
{
    Download download;
    download.packagesCacheDirectory   = directories.packagesCacheDirectory;
    download.packagesInstallDirectory = directories.packagesInstallDirectory;

    download.packageName    = "sokol";
    download.packageVersion = "7b5cfa7";
    download.url            = "https://github.com/floooh/sokol.git";
    download.isGitClone     = true;
    download.createLink     = false;
    package.packageBaseName = "sokol";

    CustomFunctions functions;
    functions.testFunction = &verifyGitCommitHash;

    SC_TRY(packageInstall(download, package, functions));
    return Result(true);
}

[[nodiscard]] Result installDearImGui(const Build::Directories& directories, Package& package)
{
    Download download;
    download.packagesCacheDirectory   = directories.packagesCacheDirectory;
    download.packagesInstallDirectory = directories.packagesInstallDirectory;

    download.packageName    = "dear-imgui";
    download.packageVersion = "00ad3c6";
    download.url            = "https://github.com/ocornut/imgui.git";
    download.isGitClone     = true;
    download.createLink     = false;
    package.packageBaseName = "dear-imgui";

    CustomFunctions functions;
    functions.testFunction = &verifyGitCommitHash;

    SC_TRY(packageInstall(download, package, functions));
    return Result(true);
}

} // namespace Tools
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
    if (parameters.platform == Platform::Apple)
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

Result buildTestProject(const Parameters& parameters, Project& project)
{
    project = {TargetType::Executable, TEST_PROJECT_NAME};

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
    return Result(true);
}

static constexpr StringView EXAMPLE_PROJECT_NAME = "SCExample";

Result buildExampleProject(const Parameters& parameters, Project& project)
{
    project = {TargetType::Executable, EXAMPLE_PROJECT_NAME};

    // All relative paths are evaluated from this project root directory.
    project.setRootDirectory(parameters.directories.libraryDirectory.view());

    // Project icon (currently used only by Xcode backend)
    project.iconPath = "Documentation/Doxygen/SC.svg";

    // Install dependencies
    Tools::Package sokol;
    SC_TRY(Tools::installSokol(parameters.directories, sokol));
    Tools::Package dearImGui;
    SC_TRY(Tools::installDearImGui(parameters.directories, dearImGui));

    // Add includes
    project.compile.addIncludes({".", sokol.packageLocalDirectory.view(), dearImGui.packageLocalDirectory.view()});

    // Project Configurations
    project.addPresetConfiguration(Configuration::Preset::Debug, parameters);
    project.addPresetConfiguration(Configuration::Preset::Release, parameters);
    project.addPresetConfiguration(Configuration::Preset::DebugCoverage, parameters);

    addSaneCppLibraries(project, parameters);            // add all SC Libraries
    project.removeFiles("Bindings/c", "*");              // remove all bindings
    project.removeFiles("Libraries", "**Test.cpp");      // remove all tests
    project.removeFiles("LibrariesExtra", "**Test.cpp"); // remove all tests
    project.removeFiles("Support", "**Test.cpp");        // remove all tests

    project.addDirectory(dearImGui.packageLocalDirectory.view(), "*.cpp");
    project.addDirectory(sokol.packageLocalDirectory.view(), "*.h");
    String imguiRelative, imguiDefine;
    SC_TRY(Path::relativeFromTo(project.rootDirectory.view(), dearImGui.packageLocalDirectory.view(), imguiRelative,
                                Path::AsNative));
    SC_TRY(StringBuilder(imguiDefine).format("SC_IMGUI_PATH=$(PROJECT_ROOT)/{}", imguiRelative));
    project.compile.addDefines({"SC_LIBRARY_PATH=$(PROJECT_ROOT)", imguiDefine.view()});
    project.link.set<Link::guiApplication>(true);
    if (parameters.platform == Platform::Apple)
    {
        project.addDirectory("Examples/SCExample", "*.m"); // add all .m from SCExample directory
        project.link.addFrameworks({"Metal", "MetalKit", "QuartzCore", "Cocoa"});
    }
    else
    {
        project.addDirectory("Examples/SCExample", "*.c"); // add all .c from SCExample directory
        if (parameters.platform == Platform::Linux)
        {
            project.link.addLibraries({"GL", "EGL", "X11", "Xi", "Xcursor"});
        }
    }
    if (parameters.platform == Platform::Windows)
    {
        project.compile.addDefines({"IMGUI_API=__declspec( dllexport )"});
    }
    else
    {
        project.compile.addDefines({"IMGUI_API=__attribute__((visibility(\"default\")))"});
    }
    project.addDirectory("Examples/SCExample", "**.h");   // add all .h from SCExample directory recursively
    project.addDirectory("Examples/SCExample", "**.cpp"); // add all .cpp from SCExample directory recursively
    return Result(true);
}

Result configure(Definition& definition, const Parameters& parameters)
{
    Workspace workspace = {"SCTest"};
    SC_TRY(workspace.projects.resize(2));
    SC_TRY(buildTestProject(parameters, workspace.projects[0]));
    SC_TRY(buildExampleProject(parameters, workspace.projects[1]));
    definition.workspaces.push_back(move(workspace));
    return Result(true);
}
SC_COMPILER_WARNING_POP;

Result executeAction(const Action& action) { return Build::Action::execute(action, configure, TEST_PROJECT_NAME); }
} // namespace Build
} // namespace SC
