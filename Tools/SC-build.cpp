// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "SC-build.h"
#include "../Libraries/FileSystemIterator/FileSystemIterator.h"
#include "SC-build/Build.inl"

namespace SC
{
namespace Tools
{
Result installSokol(const Build::Directories& directories, Package& package)
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

Result installDearImGui(const Build::Directories& directories, Package& package)
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
    project.addFiles("Libraries", "**.cpp"); // recursively add all cpp files
    project.addFiles("Libraries", "**.h");   // recursively add all header files
    project.addFiles("Libraries", "**.inl"); // recursively add all inline files

    // Libraries to link
    if (parameters.platform == Platform::Apple)
    {
        project.addLinkFrameworks({"CoreFoundation", "CoreServices", "CFNetwork", "Foundation"});
    }

    if (parameters.platform == Platform::Windows)
    {
        project.addLinkLibraries({"Advapi32", "Dbghelp", "Mswsock", "ntdll", "Rstrtmgr", "Winhttp", "Ws2_32"});
    }
    else
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

static constexpr StringView buildPlatformName(Platform::Type platform)
{
    switch (platform)
    {
    case Platform::Apple: return "macOS";
    case Platform::Linux: return "linux";
    case Platform::Windows: return "windows";
    case Platform::Wasm: return "wasm";
    case Platform::Unknown: return "unknown";
    }
    Assert::unreachable();
}

static constexpr Architecture::Type hostArchitecture()
{
    switch (HostInstructionSet)
    {
    case InstructionSet::Intel32: return Architecture::Intel32;
    case InstructionSet::Intel64: return Architecture::Intel64;
    case InstructionSet::ARM64: return Architecture::Arm64;
    }
    Assert::unreachable();
}

static constexpr StringView buildArchitectureName(const Parameters& parameters, const Configuration& configuration)
{
    Architecture::Type architecture = configuration.architecture;
    if (architecture == Architecture::Any)
    {
        architecture = parameters.architecture;
    }

    if (parameters.generator == Generator::XCode)
    {
        switch (architecture)
        {
        case Architecture::Intel64: return "x86_64";
        case Architecture::Arm64: return "arm64";
        case Architecture::Any: return "arm64 x86_64";
        case Architecture::Intel32:
        case Architecture::Wasm: return "unsupported";
        }
    }
    else if (parameters.generator == Generator::VisualStudio2019 or parameters.generator == Generator::VisualStudio2022)
    {
        if (architecture == Architecture::Any)
        {
            architecture = hostArchitecture();
        }
        switch (architecture)
        {
        case Architecture::Intel32: return "x86";
        case Architecture::Intel64: return "x64";
        case Architecture::Arm64: return "ARM64";
        case Architecture::Any:
        case Architecture::Wasm: return "unsupported";
        }
    }
    else
    {
        if (architecture == Architecture::Any)
        {
            architecture = hostArchitecture();
        }
        switch (architecture)
        {
        case Architecture::Intel32: return "x86";
        case Architecture::Intel64: return "x86_64";
        case Architecture::Arm64: return "arm64";
        case Architecture::Any:
        case Architecture::Wasm: return "unsupported";
        }
    }
    Assert::unreachable();
}

static constexpr StringView buildSystemName(Generator::Type generator)
{
    switch (generator)
    {
    case Generator::Native: return "Native";
    case Generator::Make: return "make";
    case Generator::XCode: return "xcode";
    case Generator::VisualStudio2019:
    case Generator::VisualStudio2022: return "msbuild";
    }
    Assert::unreachable();
}

static constexpr StringView compilerName(const Parameters& parameters)
{
    switch (parameters.toolchain.family)
    {
    case Toolchain::Clang: return "clang";
    case Toolchain::FilC: return "filc";
    case Toolchain::GCC: return "gcc";
    case Toolchain::MSVC: return "msvc";
    case Toolchain::ClangCL: return "clang-cl";
    case Toolchain::LLVMMingw: return "llvm-mingw";
    case Toolchain::CustomDriver: return "custom-driver";
    case Toolchain::HostDefault:
        if (parameters.platform == Platform::Windows)
        {
            return "msvc";
        }
        if (parameters.generator == Generator::XCode or parameters.platform == Platform::Apple)
        {
            return "clang";
        }
        return "gcc";
    }
    Assert::unreachable();
}

static Result expandBuildDirectoryVariables(StringView source, const Parameters& parameters,
                                            const Configuration& configuration, String& output)
{
    const ProjectWriter::ReplacePair substitutions[] = {
        {"$(TARGET_OS)", buildPlatformName(parameters.platform)},
        {"$(TARGET_ARCHITECTURES)", buildArchitectureName(parameters, configuration)},
        {"$(BUILD_SYSTEM)", buildSystemName(parameters.generator)},
        {"$(COMPILER)", compilerName(parameters)},
        {"$(CONFIGURATION)", configuration.name.view()},
    };
    auto builder = StringBuilder::create(output);
    SC_TRY(ProjectWriter::appendReplaceMultiple(builder, source, substitutions));
    builder.finalize();
    return Result(true);
}

static Result computeExecutableDirectory(const Project& project, const Parameters& parameters,
                                         const Configuration& configuration, String& executableDirectory)
{
    String outputDirectory = StringEncoding::Utf8;
    SC_TRY(expandBuildDirectoryVariables(configuration.outputPath.view(), parameters, configuration, outputDirectory));

    if (Path::isAbsolute(outputDirectory.view(), Path::AsNative))
    {
        SC_TRY(executableDirectory.assign(outputDirectory.view()));
    }
    else
    {
        SC_TRY(
            Path::join(executableDirectory, {parameters.directories.outputsDirectory.view(), outputDirectory.view()}));
    }

    if (project.targetType == TargetType::GUIApplication and parameters.generator == Generator::XCode)
    {
        String bundleDirectory = StringEncoding::Utf8;
        SC_TRY(StringBuilder::format(bundleDirectory, "{}.app", project.targetName.view()));
        String fullDirectory = StringEncoding::Utf8;
        SC_TRY(Path::join(fullDirectory, {executableDirectory.view(), bundleDirectory.view(), "Contents", "MacOS"}));
        executableDirectory = move(fullDirectory);
    }
    return Result(true);
}

static Result appendEscapedCString(StringBuilder& builder, StringView text)
{
    for (size_t idx = 0; idx < text.sizeInBytes(); ++idx)
    {
        const char ch = text.bytesWithoutTerminator()[idx];
        if (ch == '\\' or ch == '"')
        {
            SC_TRY(builder.append("\\"));
        }
        const char character[] = {ch, 0};
        SC_TRY(builder.append(StringView::fromNullTerminated(character, StringEncoding::Utf8)));
    }
    return Result(true);
}

static Result normalizeRelativePathForCompileDefine(String& path)
{
    const StringView pathView   = path.view();
    String           normalized = StringEncoding::Utf8;
    auto             builder    = StringBuilder::create(normalized);
    for (size_t idx = 0; idx < pathView.sizeInBytes(); ++idx)
    {
        char ch = pathView.bytesWithoutTerminator()[idx];
        if (ch == '\\')
        {
            ch = '/';
        }
        const char character[] = {ch, 0};
        SC_TRY(builder.append(StringView::fromNullTerminated(character, StringEncoding::Utf8)));
    }
    builder.finalize();
    path = move(normalized);
    return Result(true);
}

static Result addCompiledLibraryRootDefine(Project& project, const Parameters& parameters)
{
    for (Configuration& configuration : project.configurations)
    {
        String executableDirectory = StringEncoding::Utf8;
        SC_TRY(computeExecutableDirectory(project, parameters, configuration, executableDirectory));

        String relativeRoot = StringEncoding::Utf8;
        SC_TRY(Path::relativeFromTo(relativeRoot, executableDirectory.view(), project.rootDirectory.view(),
                                    Path::AsNative, Path::AsNative));
        SC_TRY(normalizeRelativePathForCompileDefine(relativeRoot));

        String define = StringEncoding::Utf8;
        SC_TRY(StringBuilder::format(define, "SC_LIBRARY_ROOT={}", relativeRoot.view()));
        SC_TRY(configuration.compile.defines.push_back(move(define)));
    }
    return Result(true);
}

static Result addHotReloadIncludePathsDefine(Project& project, const Parameters& parameters, StringView imguiDirectory)
{
    for (Configuration& configuration : project.configurations)
    {
        String executableDirectory = StringEncoding::Utf8;
        SC_TRY(computeExecutableDirectory(project, parameters, configuration, executableDirectory));

        String relativeImgui = StringEncoding::Utf8;
        SC_TRY(Path::relativeFromTo(relativeImgui, executableDirectory.view(), imguiDirectory, Path::AsNative,
                                    Path::AsNative));
        SC_TRY(normalizeRelativePathForCompileDefine(relativeImgui));

        String define  = StringEncoding::Utf8;
        auto   builder = StringBuilder::create(define);
        SC_TRY(builder.append("SC_HOT_RELOAD_INCLUDE_PATHS=\""));
        SC_TRY(appendEscapedCString(builder, relativeImgui.view()));
        SC_TRY(builder.append("\""));
        builder.finalize();
        SC_TRY(configuration.compile.defines.push_back(move(define)));
    }
    return Result(true);
}

static constexpr StringView TEST_PROJECT_NAME       = "SCTest";
static constexpr StringView BUILD_TEST_PROJECT_NAME = "SCBuildTest";

Result configureTests(const Parameters& parameters, Workspace& workspace)
{
    Project project = {TargetType::ConsoleExecutable, TEST_PROJECT_NAME};

    // All relative paths are evaluated from this project root directory.
    project.setRootDirectory(parameters.directories.libraryDirectory.view());

    // Project Configurations
    project.addPresetConfiguration(Configuration::Preset::Debug, parameters);
    project.addPresetConfiguration(Configuration::Preset::Release, parameters);
    project.addPresetConfiguration(Configuration::Preset::DebugCoverage, parameters);
    project.configurations.back().coverage.excludeRegex =
        ".*\\/Tools.*|"
        ".*\\Test.(cpp|h|c)|"
        ".*\\test.(c|h)|"
        ".*\\/Tests/.*\\.*|"
        ".*\\/LibC\\+\\+.inl|"              // new / delete overloads
        ".*\\/Assert.h|"                    // Can't test Assert::unreachable
        ".*\\/PluginMacros.h|"              // macros for client plugins
        ".*\\/ProcessPosixFork.inl|"        // Can't compute coverage for fork
        ".*\\/EnvironmentTable.h|"          // Can't compute coverage for fork
        ".*\\/InitializerList.h|"           // C++ Language Support
        ".*\\/Reflection/.*\\.*|"           // constexpr and templates
        ".*\\/ContainersReflection/.*\\.*|" // constexpr and templates
        ".*\\/SerializationBinary/.*\\.*|"  // constexpr and templates
        ".*\\/Extra/Deprecated/.*\\.*";
    if (parameters.platform == Platform::Linux)
    {
        project.addPresetConfiguration(Configuration::Preset::Debug, parameters, "DebugValgrind");
        project.configurations.back().compile.enableASAN = false; // ASAN and Valgrind don't mix
        project.configurations.back().link.enableASAN    = false; // ASAN and Valgrind don't mix
    }

    // Defines
    // $(PROJECT_ROOT) expands to Project::setRootDirectory expressed relative to $(PROJECT_DIR)
    project.addDefines({"SC_COMPILER_ENABLE_CONFIG=1", "SC_TOOLS_COMPILED_SEPARATELY=1"});
    SC_TRY(addCompiledLibraryRootDefine(project, parameters));

    // Includes
    project.addIncludePaths({
        ".",            // Libraries path (for PluginTest)
        "Tests/SCTest", // SCConfig.h path (enabled by SC_COMPILER_ENABLE_CONFIG == 1)
    });

    addSaneCppLibraries(project, parameters);
    project.addFiles("Tests/SCTest", "*.cpp");     // add all .cpp from SCTest directory
    project.addFiles("Tests/SCTest", "*.h");       // add all .h from SCTest directory
    project.addFiles("Tests/Libraries", "**.c*");  // add all tests from Libraries directory
    project.addFiles("Tests/Libraries", "**.inl"); // add all tests from Libraries directory
    project.removeFiles("Tests/Libraries/Build", "BuildTest.cpp");
    project.addFiles("Tests/Support", "**.cpp"); // add all tests from Support directory
    project.addFiles("Tests/Tools", "**.cpp");   // add all tests from Tools directory
    project.addFiles("Tools", "SC-*.cpp");       // add all tools
    project.addFiles("Tools", "*.h");            // add tools headers

    if (not project.addExportLibraries({"Foundation", "Memory", "Strings", "Containers"}))
    {
        return Result::Error("Failed to configure exported SCTest libraries");
    }
    project.link.preserveExportedSymbols = true;

    // Deprecated code tests and libraries (to be removed when deprecated code will be removed)
    project.addFiles("Extra/Deprecated/Tests", "**.cpp");     // add all deprecated tests
    project.addFiles("Extra/Deprecated/Libraries", "**.h");   // add all deprecated libraries header files
    project.addFiles("Extra/Deprecated/Libraries", "**.cpp"); // add all deprecated libraries cpp files

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

    SC_TRY(workspace.projects.push_back(move(project)));
    return Result(true);
}

Result configureSCBuildTest(const Parameters& parameters, Workspace& workspace)
{
    Project project = {TargetType::ConsoleExecutable, BUILD_TEST_PROJECT_NAME};

    project.setRootDirectory(parameters.directories.libraryDirectory.view());

    project.addPresetConfiguration(Configuration::Preset::Debug, parameters);
    project.addPresetConfiguration(Configuration::Preset::Release, parameters);

    project.addDefines({"SC_COMPILER_ENABLE_CONFIG=1", "SC_TOOLS_COMPILED_SEPARATELY=1"});
    SC_TRY(addCompiledLibraryRootDefine(project, parameters));
    project.addIncludePaths({
        ".",
        "Tests/SCBuildTest",
    });

    addSaneCppLibraries(project, parameters);
    project.addFiles("Tests/SCBuildTest", "*.cpp");
    project.addFiles("Tests/SCBuildTest", "*.h");
    project.addFiles("Tests/Libraries/Build", "BuildTest.cpp");
    project.addFiles("Tools", "SC-*.cpp");
    project.addFiles("Tools", "*.h");

    SC_TRY(workspace.projects.push_back(move(project)));
    return Result(true);
}

Result configureSCSharedLibrary(const Parameters& parameters, Workspace& workspace)
{
    Project project = {TargetType::SharedLibrary, "SC"};

    project.setRootDirectory(parameters.directories.libraryDirectory.view());

    project.addPresetConfiguration(Configuration::Preset::Debug, parameters);
    project.addPresetConfiguration(Configuration::Preset::Release, parameters);

    project.addIncludePaths({"."});
    addSaneCppLibraries(project, parameters);
    SC_TRY_MSG(project.addExportAllLibraries(), "Failed to configure exported Sane C++ libraries");

    SC_TRY(workspace.projects.push_back(move(project)));
    return Result(true);
}

Result configureTestSTLInterop(const Parameters& parameters, Workspace& workspace)
{
    Project project = {TargetType::ConsoleExecutable, "InteropSTL"};

    // All relative paths are evaluated from this project root directory.
    project.setRootDirectory(parameters.directories.libraryDirectory.view());

    // Project Configurations
    project.addPresetConfiguration(Configuration::Preset::Debug, parameters);
    project.addPresetConfiguration(Configuration::Preset::Release, parameters);

    // Enable C++ STL, exceptions and RTTI
    project.files.compile.enableStdCpp     = true;
    project.files.compile.enableExceptions = true;
    project.files.compile.enableRTTI       = true;
    project.files.compile.cppStandard      = CppStandard::CPP17; // string_view requires C++17

    // $(PROJECT_ROOT) expands to Project::setRootDirectory expressed relative to $(PROJECT_DIR)
    project.addDefines({"SC_COMPILER_ENABLE_STD_CPP=1"});
    SC_TRY(addCompiledLibraryRootDefine(project, parameters));
    project.addIncludePaths({"."}); // Libraries path
    addSaneCppLibraries(project, parameters);
    project.addFiles("Tests/InteropSTL", "*.cpp");
    project.addFiles("Tests/InteropSTL", "*.h");

    workspace.projects.push_back(move(project));
    return Result(true);
}

static constexpr StringView EXAMPLE_PROJECT_NAME = "SCExample";

Result configureExamplesGUI(const Parameters& parameters, Workspace& workspace)
{
    Project    project            = {TargetType::GUIApplication, EXAMPLE_PROJECT_NAME};
    const bool isWindowsGNUTarget = parameters.platform == Platform::Windows and
                                    (parameters.targetMachine.environment == TargetEnvironment::WindowsGNU or
                                     parameters.toolchain.family == Toolchain::LLVMMingw);

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
    SC_TRY(addCompiledLibraryRootDefine(project, parameters));
    SC_TRY(addHotReloadIncludePathsDefine(project, parameters, imgui.packageLocalDirectory.view()));
    project.addExportAllLibraries(); // Export all SC libraries for plugins
    SC_TRY(project.addExportDirectories({imgui.packageLocalDirectory.view()}));
    project.link.preserveExportedSymbols = true;
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
        project.addLinkLibraries({"d3d11", "dxgi", "gdi32", "kernel32", "shell32", "user32"});
    }
    else
    {
        project.addDefines({"IMGUI_API=__attribute__((visibility(\"default\")))"});
    }
    project.addFiles("Examples/SCExample", "**.h");   // add all .h from SCExample directory recursively
    project.addFiles("Examples/SCExample", "**.cpp"); // add all .cpp from SCExample directory recursively

    if (not project.addExportLibraries({"Async", "Containers", "ContainersReflection", "File", "FileSystem",
                                        "Foundation", "Http", "Memory", "Plugin", "Process", "Reflection",
                                        "SerializationBinary", "SerializationText", "Socket", "Strings", "Threading"}))
    {
        return Result::Error("Failed to configure exported SCExample libraries");
    }

    if (isWindowsGNUTarget)
    {
        SourceFiles sokolWarnings;
        sokolWarnings.addSelection("Examples/SCExample", "SCExampleSokol.c");
        sokolWarnings.compile.disableClangWarnings({"unknown-pragmas"});
        project.addSpecificFileFlags(sokolWarnings);

        SourceFiles imguiWarnings;
        imguiWarnings.addSelection(imgui.packageLocalDirectory.view(), "*.cpp");
        imguiWarnings.compile.disableClangWarnings({"uninitialized-const-pointer"});
        project.addSpecificFileFlags(imguiWarnings);
    }

    SC_TRY(workspace.projects.push_back(move(project)));
    return Result(true);
}

Result configureExamplesConsole(const Parameters& parameters, Workspace& workspace)
{
    // Read all projects from Examples directory
    FileSystemIterator::FolderState entries[2];

    FileSystemIterator fsi;

    String path;
    SC_TRY(Path::join(path, {parameters.directories.libraryDirectory.view(), "Examples"}));

    fsi.init(path.view(), entries);

    // Create a project for folder containing a .cpp file
    while (fsi.enumerateNext())
    {
        FileSystemIterator::Entry entry = fsi.get();
        if (not entry.isDirectory() or entry.name == EXAMPLE_PROJECT_NAME)
            continue;

        StringView name, extension;
        SC_TRY(Path::parseNameExtension(entry.name, name, extension));

        Project project;
        project.targetType = TargetType::ConsoleExecutable;
        project.name       = name;
        project.targetName = name;
        // All relative paths are evaluated from this project root directory.
        project.setRootDirectory(parameters.directories.libraryDirectory.view());
        project.addPresetConfiguration(Configuration::Preset::Debug, parameters);
        project.addPresetConfiguration(Configuration::Preset::Release, parameters);

#if 0 // Flip this ifdef to add all Sane C++ Libraries instead of using the SC.cpp unity build
        addSaneCppLibraries(project, parameters);
#else
        project.addFile("SC.cpp"); // Unity build file including all Sane C++ Libraries
        if (parameters.platform == Platform::Apple)
        {
            project.addLinkFrameworks({"CoreFoundation", "CoreServices"});
        }

        if (parameters.platform != Platform::Windows)
        {
            project.addLinkLibraries({"dl", "pthread"});
        }
#endif
        project.addFiles(entry.path, "**.cpp");
        workspace.projects.push_back(move(project));
    }
    return Result(true);
}

Result configureSingleFileLibs(Definition& definition, const Parameters& parameters)
{
    Workspace workspace = {"SCSingleFileLibs"};

    // Read all single file libraries from the _Build/_SingleFileLibrariesTest directory
    FileSystemIterator::FolderState entries[1];

    FileSystemIterator fsi;

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
static constexpr StringView DEFAULT_WORKSPACE = "SCWorkspace";

Result configure(Definition& definition, const Parameters& parameters)
{
    Workspace defaultWorkspace = {DEFAULT_WORKSPACE};
    SC_TRY(configureTests(parameters, defaultWorkspace));
    SC_TRY(configureSCBuildTest(parameters, defaultWorkspace));
    SC_TRY(configureSCSharedLibrary(parameters, defaultWorkspace));
    SC_TRY(configureTestSTLInterop(parameters, defaultWorkspace));
    SC_TRY(configureExamplesConsole(parameters, defaultWorkspace));
    SC_TRY(configureExamplesGUI(parameters, defaultWorkspace));
    definition.workspaces.push_back(move(defaultWorkspace));

    // Ignore errors from configuring single file libraries
    (void)configureSingleFileLibs(definition, parameters);
    return Result(true);
}
SC_COMPILER_WARNING_POP;

Result executeAction(const Action& action) { return Build::Action::execute(action, configure, DEFAULT_WORKSPACE); }
} // namespace Build

#if !defined(SC_TOOLS_COMPILED_SEPARATELY) && !defined(SC_TOOLS_IMPORT)
StringView Tools::Tool::getToolName() { return "SC-build"; }
StringView Tools::Tool::getDefaultAction() { return "configure"; }
Result     Tools::Tool::runTool(Tools::Tool::Arguments& arguments) { return Tools::runBuildTool(arguments); }
#endif
} // namespace SC
