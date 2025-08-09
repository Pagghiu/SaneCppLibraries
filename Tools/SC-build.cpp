// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "SC-build.h"
#include "../Libraries/FileSystemIterator/FileSystemIterator.h"

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
    download.packageVersion = "d5863cb";
    download.shallowClone   = "d5863cb78ea1552558c81d6db780dfcec49557ce";
    download.url            = "https://github.com/floooh/sokol.git";
    download.isGitClone     = true;
    download.createLink     = false;
    package.packageBaseName = "sokol";

    CustomFunctions functions;
    functions.testFunction = &verifyGitCommitHashCache;

    SC_TRY(packageInstall(download, package, functions));
    return Result(true);
}

[[nodiscard]] Result installDearImGui(const Build::Directories& directories, Package& package)
{
    Download download;
    download.packagesCacheDirectory   = directories.packagesCacheDirectory;
    download.packagesInstallDirectory = directories.packagesInstallDirectory;

    download.packageName    = "dear-imgui";
    download.packageVersion = "af987eb";
    download.url            = "https://github.com/ocornut/imgui.git";
    download.shallowClone   = "af987eb1176fb4c11a6f0a4f2550d9907d113df5";
    download.isGitClone     = true;
    download.createLink     = false;
    package.packageBaseName = "dear-imgui";

    CustomFunctions functions;
    functions.testFunction = &verifyGitCommitHashCache;

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
    project.addFiles("Libraries", "**.cpp");      // recursively add all cpp files
    project.addFiles("Libraries", "**.h");        // recursively add all header files
    project.addFiles("Libraries", "**.inl");      // recursively add all inline files
    project.addFiles("LibrariesExtra", "**.h");   // recursively add all header files
    project.addFiles("LibrariesExtra", "**.cpp"); // recursively add all cpp files

    // Libraries to link
    if (parameters.platform == Platform::Apple)
    {
        project.addLinkFrameworks({"CoreFoundation", "CoreServices"});
    }

    if (parameters.platform != Platform::Windows)
    {
        project.addLinkLibraries({"dl", "pthread"});
    }

    // Debug visualization helpers
    if (parameters.generator == Generator::VisualStudio2022)
    {
        project.addFiles("Support/DebugVisualizers/MSVC", "*.natvis");
    }
    else
    {
        project.addFiles("Support/DebugVisualizers/LLDB", "*");
    }
}

static constexpr StringView TEST_PROJECT_NAME = "SCTest";

Result buildTestProject(const Parameters& parameters, Project& project)
{
    project = {TargetType::ConsoleExecutable, TEST_PROJECT_NAME};

    // All relative paths are evaluated from this project root directory.
    project.setRootDirectory(parameters.directories.libraryDirectory.view());

    // Project Configurations
    project.addPresetConfiguration(Configuration::Preset::Debug, parameters);
    project.addPresetConfiguration(Configuration::Preset::Release, parameters);
    project.addPresetConfiguration(Configuration::Preset::DebugCoverage, parameters);

    // Defines
    // $(PROJECT_ROOT) expands to Project::setRootDirectory expressed relative to $(PROJECT_DIR)
    project.addDefines({"SC_LIBRARY_PATH=$(PROJECT_ROOT)", "SC_COMPILER_ENABLE_CONFIG=1"});

    // Includes
    project.addIncludePaths({
        ".",            // Libraries path (for PluginTest)
        "Tests/SCTest", // SCConfig.h path (enabled by SC_COMPILER_ENABLE_CONFIG == 1)
    });

    addSaneCppLibraries(project, parameters);
    project.addFiles("Tests/SCTest", "*.cpp");          // add all .cpp from SCTest directory
    project.addFiles("Tests/SCTest", "*.h");            // add all .h from SCTest directory
    project.addFiles("Tests/Libraries", "**.c*");       // add all tests from Libraries directory
    project.addFiles("Tests/Libraries", "**.inl");      // add all tests from Libraries directory
    project.addFiles("Tests/LibrariesExtra", "**.cpp"); // add all tests from LibrariesExtra directory
    project.addFiles("Tests/Support", "**.cpp");        // add all tests from Support directory
    project.addFiles("Tests/Tools", "**.cpp");          // add all tests from Tools directory
    project.addFiles("Tools", "SC-*.cpp");              // add all tools
    project.addFiles("Tools", "*.h");                   // add tools headers

    // This is a totally useless per-file define to test "per-file" flags SC::Build feature.
    SourceFiles specificFiles;
    // For testing purposes let's create a needlessly complex selection filter for "SC Spaces.cpp"
    specificFiles.addSelection("Tests/SCTest", "*.cpp");
    specificFiles.removeSelection("Tests/SCTest", "SCTest.cpp");
    // Add an useless define to be checked inside "SC Spaces.cpp" and "SCTest.cpp"
    specificFiles.compile.addDefines({"SC_SPACES_SPECIFIC_DEFINE=1"});
    specificFiles.compile.addIncludePaths({"../Directory With Spaces"});

    // For testing purposes disable some warnings caused in "SC Spaces.cpp"
    specificFiles.compile.disableWarnings({4100});                                 // MSVC only
    specificFiles.compile.disableWarnings({"unused-parameter"});                   // GCC and Clang
    specificFiles.compile.disableClangWarnings({"reserved-user-defined-literal"}); // Clang Only
    project.addSpecificFileFlags(specificFiles);

    return Result(true);
}

static constexpr StringView EXAMPLE_PROJECT_NAME = "SCExample";

Result buildExampleProject(const Parameters& parameters, Project& project)
{
    project = {TargetType::GUIApplication, EXAMPLE_PROJECT_NAME};

    // All relative paths are evaluated from this project root directory.
    project.setRootDirectory(parameters.directories.libraryDirectory.view());

    // Project icon (currently used only by Xcode backend)
    project.iconPath = "Documentation/Doxygen/SC.svg";

    // Install dependencies
    Tools::Package sokol;
    SC_TRY(Tools::installSokol(parameters.directories, sokol));
    Tools::Package imgui;
    SC_TRY(Tools::installDearImGui(parameters.directories, imgui));

    // Add includes
    project.addIncludePaths({".", sokol.packageLocalDirectory.view(), imgui.packageLocalDirectory.view()});

    // Project Configurations
    project.addPresetConfiguration(Configuration::Preset::Debug, parameters);
    project.addPresetConfiguration(Configuration::Preset::Release, parameters);
    project.addPresetConfiguration(Configuration::Preset::DebugCoverage, parameters);

    addSaneCppLibraries(project, parameters); // add all SC Libraries

    project.addFiles(imgui.packageLocalDirectory.view(), "*.cpp");
    project.addFiles(sokol.packageLocalDirectory.view(), "*.h");
    String imguiRelative, imguiDefine;
    SC_TRY(Path::relativeFromTo(project.rootDirectory.view(), imgui.packageLocalDirectory.view(), imguiRelative,
                                Path::AsNative));
    SC_TRY(StringBuilder(imguiDefine).format("SC_IMGUI_PATH=$(PROJECT_ROOT)/{}", imguiRelative));
    project.addDefines({"SC_LIBRARY_PATH=$(PROJECT_ROOT)", imguiDefine.view()});

    if (parameters.platform == Platform::Apple)
    {
        project.addFiles("Examples/SCExample", "*.m"); // add all .m from SCExample directory
        project.addLinkFrameworks({"Metal", "MetalKit", "QuartzCore"});
        project.addLinkFrameworksMacOS({"Cocoa"});
        project.addLinkFrameworksIOS({"UIKit", "Foundation"});
    }
    else
    {
        project.addFiles("Examples/SCExample", "*.c"); // add all .c from SCExample directory
        if (parameters.platform == Platform::Linux)
        {
            project.addLinkLibraries({"GL", "EGL", "X11", "Xi", "Xcursor"});
        }
    }
    if (parameters.platform == Platform::Windows)
    {
        project.addDefines({"IMGUI_API=__declspec( dllexport )"});
    }
    else
    {
        project.addDefines({"IMGUI_API=__attribute__((visibility(\"default\")))"});
    }
    project.addFiles("Examples/SCExample", "**.h");   // add all .h from SCExample directory recursively
    project.addFiles("Examples/SCExample", "**.cpp"); // add all .cpp from SCExample directory recursively
    return Result(true);
}

Result buildSingleFileLibs(Definition& definition, const Parameters& parameters)
{
    Workspace workspace = {"SCSingleFileLibs"};

    // Read all single file libraries from the _Build/_SingleFileLibrariesTest directory
    FileSystemIterator fsi;

    FileSystemIterator::FolderState entries[1];

    String path;
    SC_TRY(Path::join(path, {parameters.directories.libraryDirectory.view(), "_Build", "_SingleFileLibrariesTest"}));

    SC_TRY_MSG(fsi.init(path.view(), entries), "Cannot access _Build/_SingleFileLibrariesTest");

    // Create a project for each single file library
    while (fsi.enumerateNext())
    {
        StringView name, extension;
        SC_TRY(Path::parseNameExtension(fsi.get().name, name, extension));
        if (extension != "cpp" or not name.startsWith("Test_"))
            continue; // Only process .cpp files

        Project project;
        project.targetType = TargetType::ConsoleExecutable;
        project.name       = name;
        project.targetName = project.name;
        // All relative paths are evaluated from this project root directory.
        project.setRootDirectory(parameters.directories.libraryDirectory.view());
        project.addPresetConfiguration(Configuration::Preset::Debug, parameters);
        project.addPresetConfiguration(Configuration::Preset::Release, parameters);

        // Link C++ stdlib to avoid needing to link Memory library to define __cxa_guard_acquire etc.
        project.addDefines({"SC_COMPILER_ENABLE_STD_CPP=1"});
        project.configurations[0].compile.enableStdCpp = true;
        project.configurations[1].compile.enableStdCpp = true;

        project.addIncludePaths({"_Build/_SingleFileLibraries"});

        project.addFile(fsi.get().path);

        // Libraries to link
        if (parameters.platform == Platform::Apple)
        {
            project.addLinkFrameworks({"CoreFoundation", "CoreServices"});
        }

        if (parameters.platform != Platform::Windows)
        {
            project.addLinkLibraries({"dl", "pthread"});
        }

        workspace.projects.push_back(move(project));
    }
    definition.workspaces.push_back(move(workspace));
    return Result(true);
}
static constexpr StringView DEFAULT_WORKSPACE_NAME = "SCWorkspace";

Result configure(Definition& definition, const Parameters& parameters)
{
    Workspace workspace = {DEFAULT_WORKSPACE_NAME};
    SC_TRY(workspace.projects.resize(2));
    SC_TRY(buildTestProject(parameters, workspace.projects[0]));
    SC_TRY(buildExampleProject(parameters, workspace.projects[1]));
    definition.workspaces.push_back(move(workspace));

    // Ignore errors from building single file libraries
    (void)buildSingleFileLibs(definition, parameters);
    return Result(true);
}
SC_COMPILER_WARNING_POP;

Result executeAction(const Action& action) { return Build::Action::execute(action, configure, DEFAULT_WORKSPACE_NAME); }
} // namespace Build
} // namespace SC
