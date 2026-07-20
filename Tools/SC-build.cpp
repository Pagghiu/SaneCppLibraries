// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#define SC_BUILD_SOURCE 1
#include "SC-build.h"
#include "../Libraries/FileSystemIterator/FileSystemIterator.h"
#include "../Libraries/Process/Process.h"
#include "SC-build/Build.inl"
#include "SC-build/BuildCLI.h"

#if !defined(SC_TOOLS_COMPILED_SEPARATELY)
#define SC_TOOLS_IMPORT
#include "SC-package.cpp"
#undef SC_TOOLS_IMPORT
#endif

#include "SC-build/BuildCLI.inl"

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

void addSaneCppDebugVisualizers(Project& project, const Parameters& parameters)
{
    if (parameters.generator == Generator::VisualStudio2022)
    {
        String debugVisualizersRoot = StringEncoding::Utf8;
        (void)Path::join(debugVisualizersRoot,
                         {parameters.directories.libraryDirectory.view(), "Support/DebugVisualizers/MSVC"});
        project.addFiles(debugVisualizersRoot.view(), "*.natvis");
    }
    else
    {
        String debugVisualizersRoot = StringEncoding::Utf8;
        (void)Path::join(debugVisualizersRoot,
                         {parameters.directories.libraryDirectory.view(), "Support/DebugVisualizers/LLDB"});
        project.addFiles(debugVisualizersRoot.view(), "*");
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

static constexpr StringView TEST_PROJECT_NAME                        = "SCTest";
static constexpr StringView AWAIT_TEST_PROJECT_NAME                  = "SCAwaitTest";
static constexpr StringView BUILD_TEST_PROJECT_NAME                  = "SCBuildTest";
static constexpr StringView AWAIT_COROUTINE_SHIM_TEST_PROJECT_NAME   = "SCAwaitCoroutineShimTest";
static constexpr StringView STD_CPP_HEADER_NO_LINK_TEST_PROJECT_NAME = "SCStdCppHeaderNoLinkTest";
static constexpr StringView ZLIB_FILC_PROJECT_NAME                   = "ZLibFilC";

static void configureSaneStrictNoStdCpp(Project& project)
{
    // These targets intentionally preserve the old pressure-test mode: SC code cannot include standard C++ headers
    // and SC provides the tiny runtime shims instead of linking the C++ standard-library runtime.
    project.files.compile.includeStdCpp    = false;
    project.link.linkStdCpp                = false;
    project.saneCpp.enabled                = true;
    project.saneCpp.provideCppRuntimeShims = true;
}

static void configureSaneStdHeadersNoRuntime(Project& project)
{
    // Used by coroutine/Await coverage: standard C++ headers are allowed, but linking libstdc++/libc++ is still
    // forbidden so we keep proving the explicit-runtime-shim path.
    project.files.compile.includeStdCpp    = true;
    project.link.linkStdCpp                = false;
    project.saneCpp.enabled                = true;
    project.saneCpp.provideCppRuntimeShims = true;
}

static Result configureZLibFilC(const Parameters& parameters, Workspace& workspace)
{
    ProcessEnvironment environment;
    StringSpan         sourceDirectory;
    StringSpan         outputDirectory;
    StringSpan         intermediateDirectory;
    if (not environment.get("SC_ZLIB_FILC_SOURCE_DIR", sourceDirectory) or sourceDirectory.isEmpty())
    {
        return Result(true);
    }
    SC_TRY_MSG(environment.get("SC_ZLIB_FILC_OUTPUT_DIR", outputDirectory) and not outputDirectory.isEmpty(),
               "SC_ZLIB_FILC_OUTPUT_DIR is required when SC_ZLIB_FILC_SOURCE_DIR is set");
    SC_TRY_MSG(environment.get("SC_ZLIB_FILC_INTERMEDIATE_DIR", intermediateDirectory) and
                   not intermediateDirectory.isEmpty(),
               "SC_ZLIB_FILC_INTERMEDIATE_DIR is required when SC_ZLIB_FILC_SOURCE_DIR is set");

    Project project = {ZLIB_FILC_PROJECT_NAME, TargetType::SharedLibrary};
    project.setRootDirectory(sourceDirectory);
    SC_TRY(project.targetName.assign("libz"));

    project.addPresetConfiguration(Configuration::Preset::Debug, parameters);
    project.addPresetConfiguration(Configuration::Preset::Release, parameters);
    for (Configuration& configuration : project.configurations)
    {
        SC_TRY(configuration.outputPath.assign(outputDirectory));
        SC_TRY(configuration.intermediatesPath.assign(intermediateDirectory));
        SC_TRY(configuration.compile.defines.push_back("ZEXPORT=__attribute__((visibility(\"default\")))"));
        SC_TRY(configuration.compile.defines.push_back("ZEXPORTVA=__attribute__((visibility(\"default\")))"));
        SC_TRY(configuration.compile.defines.push_back("HAVE_UNISTD_H"));
        SC_TRY(configuration.compile.defines.push_back("HAVE_HIDDEN"));
        SC_TRY(configuration.compile.disableWarnings({"implicit-fallthrough"}));
        configuration.link.enableDeadCodeStripping = false;
    }

    static constexpr StringView sources[] = {
        "adler32.c", "compress.c", "crc32.c",   "deflate.c",  "gzclose.c", "gzlib.c",   "gzread.c", "gzwrite.c",
        "infback.c", "inffast.c",  "inflate.c", "inftrees.c", "trees.c",   "uncompr.c", "zutil.c",
    };
    for (const StringView source : sources)
    {
        SC_TRY(project.addFile(source));
    }
    SC_TRY(project.addIncludePaths({"."}));
    SC_TRY(workspace.projects.push_back(move(project)));
    return Result(true);
}

Result configureTests(const Parameters& parameters, Workspace& workspace)
{
    Project project = {TEST_PROJECT_NAME};
    project.setRootDirectory(parameters.directories.projectDirectory.view());

    project.addPresetConfiguration(Configuration::Preset::Debug, parameters);
    project.addPresetConfiguration(Configuration::Preset::Release, parameters);
    project.addPresetConfiguration(Configuration::Preset::DebugCoverage, parameters);
    project.configurations.back().coverage.excludeRegex = ".*\\/Tools.*|"
                                                          ".*\\Test.(cpp|h|c)|"
                                                          ".*\\test.(c|h)|"
                                                          ".*\\/Tests/.*\\.*|"
                                                          ".*\\/LibC\\+\\+.inl|"
                                                          ".*\\/Assert.h|"
                                                          ".*\\/PluginMacros.h|"
                                                          ".*\\/ProcessPosixFork.inl|"
                                                          ".*\\/EnvironmentTable.h|"
                                                          ".*\\/InitializerList.h|"
                                                          ".*\\/Reflection/.*\\.*|"
                                                          ".*\\/ContainersReflection/.*\\.*|"
                                                          ".*\\/SerializationBinary/.*\\.*|"
                                                          ".*\\/Extra/Deprecated/.*\\.*";
    if (parameters.platform == Platform::Linux)
    {
        project.addPresetConfiguration(Configuration::Preset::Debug, parameters, "DebugValgrind");
        project.configurations.back().compile.enableASAN = false;
        project.configurations.back().link.enableASAN    = false;
    }

    configureSaneStrictNoStdCpp(project);

    project.addDefines({"SC_COMPILER_ENABLE_CONFIG=1", "SC_TOOLS_COMPILED_SEPARATELY=1"});
    SC_TRY(addCompiledLibraryRootDefine(project, parameters));
    project.addIncludePaths({
        ".",
        "Tests/SCTest",
    });

    SC_TRY(addSaneCppLibraries(project, parameters, Libraries::Multiple));
    SC_TRY(project.addIncludePaths({parameters.directories.libraryDirectory.view()}));
    addSaneCppDebugVisualizers(project, parameters);
    project.addFiles("Tests/SCTest", "*.cpp");
    project.addFiles("Tests/SCTest", "*.h");
    project.addFiles("Tests/Libraries", "**.c*");
    project.addFiles("Tests/Libraries", "**.inl");
    project.removeFiles("Tests/Libraries/Await", "AwaitTest.cpp");
    project.removeFiles("Tests/Libraries/Build", "BuildTest.cpp");
    project.addFiles("Tests/Support", "**.cpp");
    project.addFiles("Tests/Tools", "**.cpp");
    project.addFiles("Tools", "SC-*.cpp");
    project.addFiles("Tools", "*.h");

    if (not project.addExportLibraries({"Foundation", "Memory", "Strings", "Containers"}))
    {
        return Result::Error("Failed to configure exported SCTest libraries");
    }
    project.link.preserveExportedSymbols = true;

    project.addFiles("Extra/Deprecated/Tests", "**.cpp");
    project.addFiles("Extra/Deprecated/Libraries", "**.h");
    project.addFiles("Extra/Deprecated/Libraries", "**.cpp");

    SourceFiles specificFiles;
    specificFiles.addSelection("Tests/SCTest", "*.cpp");
    specificFiles.removeSelection("Tests/SCTest", "SCTest.cpp");
    specificFiles.compile.addDefines({"SC_SPACES_SPECIFIC_DEFINE=1"});
    specificFiles.compile.addIncludePaths({"../Directory With Spaces"});
    specificFiles.compile.disableWarnings({4100});
    specificFiles.compile.disableWarnings({"unused-parameter"});
    specificFiles.compile.disableClangWarnings({"reserved-user-defined-literal"});
    project.addSpecificFileFlags(specificFiles);

    SC_TRY(workspace.projects.push_back(move(project)));
    return Result(true);
}

Result configureSCBuildTest(const Parameters& parameters, Workspace& workspace)
{
    Project project = {BUILD_TEST_PROJECT_NAME};
    project.setRootDirectory(parameters.directories.projectDirectory.view());

    project.addPresetConfiguration(Configuration::Preset::Debug, parameters);
    project.addPresetConfiguration(Configuration::Preset::Release, parameters);

    configureSaneStrictNoStdCpp(project);

    project.addDefines({"SC_COMPILER_ENABLE_CONFIG=1", "SC_TOOLS_COMPILED_SEPARATELY=1"});
    SC_TRY(addCompiledLibraryRootDefine(project, parameters));
    project.addIncludePaths({
        ".",
        "Tests/SCBuildTest",
    });

    SC_TRY(addSaneCppLibraries(project, parameters, Libraries::Multiple));
    SC_TRY(project.addIncludePaths({parameters.directories.libraryDirectory.view()}));
    addSaneCppDebugVisualizers(project, parameters);
    project.addFiles("Tests/SCBuildTest", "*.cpp");
    project.addFiles("Tests/SCBuildTest", "*.h");
    project.addFiles("Tests/Libraries/Build", "BuildTest.cpp");
    project.addFiles("Tools", "SC-*.cpp");
    project.addFiles("Tools", "*.h");

    SC_TRY(workspace.projects.push_back(move(project)));
    return Result(true);
}

Result configureSCAwaitTest(const Parameters& parameters, Workspace& workspace)
{
    Project project = {AWAIT_TEST_PROJECT_NAME};
    project.setRootDirectory(parameters.directories.projectDirectory.view());

    project.addPresetConfiguration(Configuration::Preset::Debug, parameters);
    project.addPresetConfiguration(Configuration::Preset::Release, parameters);

    project.files.compile.cppStandard = CppStandard::CPP20;
    configureSaneStdHeadersNoRuntime(project);

    project.addDefines({"SC_COMPILER_ENABLE_CONFIG=1", "SC_TOOLS_COMPILED_SEPARATELY=1"});
    SC_TRY(addCompiledLibraryRootDefine(project, parameters));
    project.addIncludePaths({
        ".",
        "Tests/SCAwaitTest",
    });

    SC_TRY(addSaneCppLibraries(project, parameters, Libraries::Multiple));
    SC_TRY(project.addIncludePaths({parameters.directories.libraryDirectory.view()}));
    addSaneCppDebugVisualizers(project, parameters);
    project.addFiles("Tests/SCAwaitTest", "*.cpp");
    project.addFiles("Tests/SCAwaitTest", "*.h");
    project.addFiles("Tests/Libraries/Await", "**.cpp");

    SC_TRY(workspace.projects.push_back(move(project)));
    return Result(true);
}

Result configureStdCppHeaderNoLinkTest(const Parameters& parameters, Workspace& workspace)
{
    Project project = {STD_CPP_HEADER_NO_LINK_TEST_PROJECT_NAME};
    project.setRootDirectory(parameters.directories.projectDirectory.view());

    project.addPresetConfiguration(Configuration::Preset::Debug, parameters);
    project.addPresetConfiguration(Configuration::Preset::Release, parameters);

    project.files.compile.cppStandard = CppStandard::CPP20;
    project.link.linkStdCpp           = false; // This probe explicitly verifies standard headers without libstdc++.
    project.addFile("Support/CompileTests/StdCppHeaderNoLinkProbe.cpp");

    SC_TRY(workspace.projects.push_back(move(project)));
    return Result(true);
}

Result configureSCAwaitCoroutineShimTest(const Parameters& parameters, Workspace& workspace)
{
    Project project = {AWAIT_COROUTINE_SHIM_TEST_PROJECT_NAME};
    project.setRootDirectory(parameters.directories.projectDirectory.view());

    project.addPresetConfiguration(Configuration::Preset::Debug, parameters);
    project.addPresetConfiguration(Configuration::Preset::Release, parameters);

    configureSaneStrictNoStdCpp(project);
    project.files.compile.cppStandard = CppStandard::CPP20;
    project.addIncludePaths({"."});
    project.addDefines({"SC_AWAIT_ENABLE_NO_STDLIB_COROUTINE=1"});
    project.addFile("Support/CompileTests/AwaitCoroutineShimProbe.cpp");

    SC_TRY(workspace.projects.push_back(move(project)));
    return Result(true);
}

Result configureSCSharedLibrary(const Parameters& parameters, Workspace& workspace)
{
    Project project = {"SC", TargetType::SharedLibrary};

    project.setRootDirectory(parameters.directories.projectDirectory.view());
    project.addPresetConfiguration(Configuration::Preset::Debug, parameters);
    project.addPresetConfiguration(Configuration::Preset::Release, parameters);

    configureSaneStrictNoStdCpp(project);

    project.addIncludePaths({"."});
    SC_TRY(addSaneCppLibraries(project, parameters, Libraries::Multiple));
    SC_TRY(project.addIncludePaths({parameters.directories.libraryDirectory.view()}));
    addSaneCppDebugVisualizers(project, parameters);
    SC_TRY_MSG(project.addExportAllLibraries(), "Failed to configure exported Sane C++ libraries");

    SC_TRY(workspace.projects.push_back(move(project)));
    return Result(true);
}

Result configureTestSTLInterop(const Parameters& parameters, Workspace& workspace)
{
    Project project = {"InteropSTL"};

    project.setRootDirectory(parameters.directories.projectDirectory.view());
    project.addPresetConfiguration(Configuration::Preset::Debug, parameters);
    project.addPresetConfiguration(Configuration::Preset::Release, parameters);

    project.files.compile.enableExceptions = true;
    project.files.compile.enableRTTI       = true;
    project.files.compile.cppStandard      = CppStandard::CPP17;
    // InteropSTL is the explicit full-standard-C++ coverage target: headers and runtime link are both enabled.
    project.files.compile.includeStdCpp = true;
    project.link.linkStdCpp             = true;

    SC_TRY(addCompiledLibraryRootDefine(project, parameters));
    project.addIncludePaths({"."});
    SC_TRY(addSaneCppLibraries(project, parameters, Libraries::Multiple));
    SC_TRY(project.addIncludePaths({parameters.directories.libraryDirectory.view()}));
    addSaneCppDebugVisualizers(project, parameters);
    project.addFiles("Tests/InteropSTL", "*.cpp");
    project.addFiles("Tests/InteropSTL", "*.h");

    workspace.projects.push_back(move(project));
    return Result(true);
}

static constexpr StringView EXAMPLE_PROJECT_NAME = "SCExample";

namespace detail
{
Result findTaskflowBenchmarks(const Parameters& parameters, String& root, bool& found)
{
    found = false;

    Tools::Download download;
    download.packageName = "taskflow-benchmarks";

    String packageDirectory = StringEncoding::Utf8;
    SC_TRY(StringBuilder::format(packageDirectory, "{}_{}", download.packageName, download.packagePlatform));

    String packageRoot = StringEncoding::Utf8;
    SC_TRY(Path::join(packageRoot, {parameters.directories.packagesInstallDirectory.view(), packageDirectory.view()}));

    String receiptPath = StringEncoding::Utf8;
    SC_TRY(Path::join(receiptPath, {packageRoot.view(), Tools::PackageReceiptFileName}));

    FileSystem fs;
    SC_TRY(fs.init("."));
    if (not fs.existsAndIsFile(receiptPath.view()))
    {
        return Result(true);
    }

    SC_TRY(Tools::resolvePackageExportPath(packageRoot.view(), Tools::PackageExport::TaskflowBenchmarksRoot, root));
    found = true;
    return Result(true);
}
} // namespace detail

Result configureExamplesGUI(const Parameters& parameters, Workspace& workspace)
{
    Project    project            = {EXAMPLE_PROJECT_NAME, TargetType::GUIApplication};
    const bool isWindowsGNUTarget = parameters.platform == Platform::Windows and
                                    (parameters.targetMachine.environment == TargetEnvironment::WindowsGNU or
                                     parameters.toolchain.family == Toolchain::LLVMMingw);

    project.setRootDirectory(parameters.directories.projectDirectory.view());
    project.iconPath = "Documentation/Doxygen/SC.svg";

    Tools::Package sokol;
    SC_TRY(Tools::installSokol(parameters.directories, sokol));
    Tools::Package imgui;
    SC_TRY(Tools::installDearImGui(parameters.directories, imgui));

    project.addIncludePaths({".", sokol.packageLocalDirectory.view(), imgui.packageLocalDirectory.view()});
    project.addPresetConfiguration(Configuration::Preset::Debug, parameters);
    project.addPresetConfiguration(Configuration::Preset::Release, parameters);
    project.addPresetConfiguration(Configuration::Preset::DebugCoverage, parameters);

    configureSaneStrictNoStdCpp(project);

    SC_TRY(addSaneCppLibraries(project, parameters, Libraries::Multiple));
    SC_TRY(project.addIncludePaths({parameters.directories.libraryDirectory.view()}));
    addSaneCppDebugVisualizers(project, parameters);

    project.addFiles(imgui.packageLocalDirectory.view(), "*.cpp");
    project.addFiles(sokol.packageLocalDirectory.view(), "*.h");
    SC_TRY(addCompiledLibraryRootDefine(project, parameters));
    SC_TRY(addHotReloadIncludePathsDefine(project, parameters, imgui.packageLocalDirectory.view()));
    project.addExportAllLibraries();
    SC_TRY(project.addExportDirectories({imgui.packageLocalDirectory.view()}));
    project.link.preserveExportedSymbols = true;
    if (parameters.platform == Platform::Apple)
    {
        project.addFiles("Examples/SCExample", "*.m");
        project.addLinkFrameworks({"Metal", "MetalKit", "QuartzCore"});
        project.addLinkFrameworksMacOS({"Cocoa"});
        project.addLinkFrameworksIOS({"UIKit", "Foundation"});
    }
    else
    {
        project.addFiles("Examples/SCExample", "*.c");
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
    project.addFiles("Examples/SCExample", "**.h");
    project.addFiles("Examples/SCExample", "**.cpp");

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
    FileSystemIterator::FolderState entries[2];
    FileSystemIterator              fsi;

    String path;
    SC_TRY(Path::join(path, {parameters.directories.projectDirectory.view(), "Examples"}));

    fsi.init(path.view(), entries);

    while (fsi.enumerateNext())
    {
        FileSystemIterator::Entry entry = fsi.get();
        if (not entry.isDirectory() or entry.name == EXAMPLE_PROJECT_NAME)
            continue;

        StringView name, extension;
        SC_TRY(Path::parseNameExtension(entry.name, name, extension));

        String taskflowBenchmarksRoot = StringEncoding::Utf8;
        bool   hasTaskflowBenchmarks  = false;
        if (name == "FibersSkynetBenchmark")
        {
            SC_TRY(detail::findTaskflowBenchmarks(parameters, taskflowBenchmarksRoot, hasTaskflowBenchmarks));
            if (not hasTaskflowBenchmarks)
            {
                continue;
            }
        }

        Project project;
        project.targetType = TargetType::ConsoleExecutable;
        project.name       = name;
        project.targetName = name;
        project.setRootDirectory(parameters.directories.projectDirectory.view());
        project.addPresetConfiguration(Configuration::Preset::Debug, parameters);
        project.addPresetConfiguration(Configuration::Preset::Release, parameters);

        if (name == "FibersSkynetBenchmark")
        {
            project.files.compile.enableExceptions = true;
            project.files.compile.enableRTTI       = true;
            project.files.compile.cppStandard      = CppStandard::CPP20;
            project.files.compile.includeStdCpp    = true;
            project.files.compile.disableClangWarnings({"implicit-int-float-conversion"});
            project.link.linkStdCpp = true;
            SC_TRY(addSaneCppLibraries(project, parameters));

            String skynetRoot     = StringEncoding::Utf8;
            String taskflowSource = StringEncoding::Utf8;
            SC_TRY(Path::join(skynetRoot, {taskflowBenchmarksRoot.view(), "benchmarks", "skynet"}));
            SC_TRY(Path::join(taskflowSource, {skynetRoot.view(), "taskflow.cpp"}));
            SC_TRY(project.addIncludePaths({taskflowBenchmarksRoot.view(), skynetRoot.view()}));
            project.addFile(taskflowSource.view());
        }
        else if (name.startsWith("Await"))
        {
            project.files.compile.cppStandard = CppStandard::CPP20;
            configureSaneStdHeadersNoRuntime(project);
            SC_TRY(addSaneCppLibraries(project, parameters));
            project.addFiles("Libraries/Await", "**.cpp");
            project.addFiles("Libraries/Await", "**.h");
        }
        else
        {
            configureSaneStrictNoStdCpp(project);
            SC_TRY(addSaneCppLibraries(project, parameters));
        }
        SC_TRY(project.addIncludePaths({parameters.directories.libraryDirectory.view()}));
        project.addFiles(entry.path, "**.cpp");
        workspace.projects.push_back(move(project));
    }
    return Result(true);
}

Result configureSingleFileLibs(Definition& definition, const Parameters& parameters)
{
    Workspace workspace = {"SCSingleFileLibs"};

    FileSystemIterator::FolderState entries[1];
    FileSystemIterator              fsi;

    String path;
    SC_TRY(Path::join(path, {parameters.directories.projectDirectory.view(), "_Build", "_SingleFileLibrariesTest"}));

    SC_TRY_MSG(fsi.init(path.view(), entries), "Cannot access _Build/_SingleFileLibrariesTest");

    while (fsi.enumerateNext())
    {
        StringView name, extension;
        SC_TRY(Path::parseNameExtension(fsi.get().name, name, extension));
        if (extension != "cpp" or not name.startsWith("Test_"))
            continue;

        Project project;
        project.targetType = TargetType::ConsoleExecutable;
        project.name       = name;
        project.targetName = project.name;
        project.setRootDirectory(parameters.directories.projectDirectory.view());
        project.addPresetConfiguration(Configuration::Preset::Debug, parameters);
        project.addPresetConfiguration(Configuration::Preset::Release, parameters);

        if (name == "Test_SaneCppAwait" or name == "Test_SaneCppAwaitStandalone")
        {
            project.files.compile.cppStandard = CppStandard::CPP20;
            configureSaneStdHeadersNoRuntime(project);
        }
        else
        {
            configureSaneStrictNoStdCpp(project);
        }

        project.addIncludePaths({"_Build/_SingleFileLibraries"});
        project.addFile(fsi.get().path);

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

Result configure(Definition& definition, const Parameters& parameters)
{
    Workspace defaultWorkspace = {"SCWorkspace"};
    SC_TRY(configureTests(parameters, defaultWorkspace));
    SC_TRY(configureSCBuildTest(parameters, defaultWorkspace));
    SC_TRY(configureSCAwaitTest(parameters, defaultWorkspace));
    SC_TRY(configureStdCppHeaderNoLinkTest(parameters, defaultWorkspace));
    SC_TRY(configureSCAwaitCoroutineShimTest(parameters, defaultWorkspace));
    SC_TRY(configureSCSharedLibrary(parameters, defaultWorkspace));
    SC_TRY(configureTestSTLInterop(parameters, defaultWorkspace));
    SC_TRY(configureExamplesConsole(parameters, defaultWorkspace));
    SC_TRY(configureExamplesGUI(parameters, defaultWorkspace));
    SC_TRY(configureZLibFilC(parameters, defaultWorkspace));
    definition.workspaces.push_back(move(defaultWorkspace));

    (void)configureSingleFileLibs(definition, parameters);
    return Result(true);
}
SC_COMPILER_WARNING_POP_UNUSED_RESULT;
} // namespace Build
} // namespace SC
