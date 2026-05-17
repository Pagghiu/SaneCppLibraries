// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "Tools/SC-build/Build.h"
#include "Libraries/FileSystem/FileSystem.h"
#include "Libraries/Strings/Path.h"
#include "Libraries/Strings/StringBuilder.h"
#include "Libraries/Testing/Testing.h"
#include "Libraries/Time/Time.h"
#include "Tools/SC-build/BuildCLI.h"

namespace SC
{
struct BuildTest;

namespace
{
static constexpr StringView MakefileStaticLibraryWorkspaceName = "MakefileStaticLibraryWorkspace";
static constexpr StringView MakefileStaticLibraryProjectName   = "WriterStaticLibrary";

static Result configureMakefileStaticLibrary(Build::Definition& definition, const Build::Parameters& parameters)
{
    Build::Workspace workspace = {MakefileStaticLibraryWorkspaceName};
    Build::Project   project   = {MakefileStaticLibraryProjectName, Build::TargetType::StaticLibrary};

    SC_TRY(project.setRootDirectory(parameters.directories.projectDirectory.view()));
    SC_TRY(project.addPresetConfiguration(Build::Configuration::Preset::Debug, parameters));
    SC_TRY(project.addPresetConfiguration(Build::Configuration::Preset::Release, parameters));
    SC_TRY(project.addFiles("Tests/SCBuildTest/Fixture/TinyConsoleProgram", "main.cpp"));

    SC_TRY(workspace.projects.push_back(move(project)));
    SC_TRY(definition.workspaces.push_back(move(workspace)));
    return Result(true);
}

static Result computeGeneratedMakefilePath(const Build::Directories& directories, Build::Platform::Type platform,
                                           String& makefilePath)
{
    SC_TRY(
        Path::join(makefilePath, {directories.projectsDirectory.view(), "Make", MakefileStaticLibraryWorkspaceName}));
    if (platform == Build::Platform::Linux)
    {
        SC_TRY(Path::append(makefilePath, {"linux", "Makefile"}, Path::AsPosix));
    }
    else
    {
        SC_TRY(Path::append(makefilePath, {"apple", "Makefile"}, Path::AsPosix));
    }
    return Result(true);
}

static bool supportMatrixContains(Span<const Build::SupportMatrixEntry> matrix, Build::Platform::Type hostPlatform,
                                  Build::TargetEnvironment::Type targetEnvironment,
                                  Build::Architecture::Type targetArchitecture, Build::SupportStatus::Type buildSupport,
                                  Build::SupportStatus::Type runSupport, Build::SupportTier::Type tier,
                                  Build::RunnerSpec::Type runner)
{
    for (const Build::SupportMatrixEntry& entry : matrix)
    {
        if (entry.hostMachine.platform == hostPlatform and
            entry.hostMachine.environment == Build::TargetEnvironment::Native and
            entry.targetMachine.environment == targetEnvironment and
            entry.targetMachine.architecture == targetArchitecture and entry.buildSupport == buildSupport and
            entry.runSupport == runSupport and entry.tier == tier and entry.runner == runner)
        {
            return true;
        }
    }
    return false;
}

static constexpr StringView supportMatrixHostName(Build::Platform::Type platform)
{
    switch (platform)
    {
    case Build::Platform::Windows: return "Windows";
    case Build::Platform::Apple: return "macOS";
    case Build::Platform::Linux: return "Linux";
    case Build::Platform::Wasm: return "Wasm";
    case Build::Platform::Unknown: return "unknown";
    }
    Assert::unreachable();
}

static constexpr StringView supportMatrixArchitectureName(Build::Architecture::Type architecture)
{
    switch (architecture)
    {
    case Build::Architecture::Intel64: return "x86_64";
    case Build::Architecture::Arm64: return "arm64";
    case Build::Architecture::Intel32: return "intel32";
    case Build::Architecture::Wasm: return "wasm";
    case Build::Architecture::Any: return "any";
    }
    Assert::unreachable();
}

static Result prepareBuildCLIAction(TestReport& report, Span<const StringView> cliArguments,
                                    Build::Action::Type actionType, Build::Action& action,
                                    Tools::detail::BuildCLIStatus& status)
{
    Console    console;
    StringPath outputDirectory;
    SC_TRY(StringBuilder::format(outputDirectory, "{}/_Build", report.libraryRootDirectory));

    auto                                   arguments = Tools::Tool::Arguments{console,
                                            report.libraryRootDirectory,
                                            report.libraryRootDirectory,
                                            outputDirectory,
                                            report.libraryRootDirectory,
                                            "build",
                                            actionType == Build::Action::Run ? "run"_a8 : "compile"_a8,
                                            cliArguments};
    Tools::detail::BuildCLIResolvedStorage storage;
    return Tools::detail::prepareBuildAction(actionType, arguments, action, storage, status);
}

static bool writeBuildHelpAddendumToString(Build::Action::Type actionType, String& text)
{
    GrowableBuffer<String> growable(text);
    StringFormatOutput     output(StringEncoding::Utf8, growable);
    const bool             result = Tools::detail::appendBuildActionHelpAddendum(output, actionType);
    growable.finalize();
    return result;
}
} // namespace
} // namespace SC

struct SC::BuildTest : public SC::TestCase
{
    BuildTest(SC::TestReport& report) : TestCase(report, "BuildTest")
    {
        String buildDir;
        {
            String targetDirectory = report.applicationRootDirectory.view();
            SC_TRUST_RESULT(Path::append(targetDirectory, {"../..", "_Tests"}, Path::AsNative));
            SmallString<128> runDirectory;
            SC_TRUST_RESULT(StringBuilder::format(runDirectory, "run-{}-{}", Time::Realtime::now().milliseconds,
                                                  reinterpret_cast<size_t>(this)));
            SC_TRUST_RESULT(Path::append(targetDirectory, {runDirectory.view()}, Path::AsNative));
            // Normalizing is not strictly necessary but it helps when debugging the test
            SC_TRUST_RESULT(Path::normalize(buildDir, targetDirectory.view(), Path::AsNative));
        }
        Build::Action action;
        action.action = Build::Action::Configure;

        Build::Directories& directories = action.parameters.directories;
        SC_TRUST_RESULT(Path::join(directories.projectsDirectory, {buildDir.view(), "_Projects"}));
        SC_TRUST_RESULT(Path::join(directories.outputsDirectory, {buildDir.view(), "_Outputs"}));
        SC_TRUST_RESULT(Path::join(directories.intermediatesDirectory, {buildDir.view(), "_Intermediates"}));
        SC_TRUST_RESULT(Path::join(directories.buildCacheDirectory, {buildDir.view(), "_BuildCache"}));
        SC_TRUST_RESULT(Path::join(directories.packagesCacheDirectory, {buildDir.view(), "_PackagesCache"}));
        SC_TRUST_RESULT(Path::join(directories.packagesInstallDirectory, {buildDir.view(), "_Packages"}));

        directories.libraryDirectory = report.libraryRootDirectory.view();
        directories.projectDirectory = report.libraryRootDirectory.view();

        if (test_section("Native backend support matrix"))
        {
            Span<const Build::SupportMatrixEntry> matrix = Build::getNativeBackendSupportMatrix();
            SC_TEST_EXPECT(matrix.sizeInElements() >= 14);
            SC_TEST_EXPECT(Build::SupportStatus::toString(Build::SupportStatus::Supported) == "supported");
            SC_TEST_EXPECT(Build::SupportStatus::toString(Build::SupportStatus::NotYet) == "not-yet");
            SC_TEST_EXPECT(Build::SupportTier::toString(Build::SupportTier::Tier2) == "tier2");

            size_t buildSupportedRows = 0;
            for (const Build::SupportMatrixEntry& entry : matrix)
            {
                SC_TEST_EXPECT(entry.hostMachine.environment == Build::TargetEnvironment::Native);
                SC_TEST_EXPECT(entry.targetMachine.environment != Build::TargetEnvironment::Native);
                SC_TEST_EXPECT(not entry.validation.isEmpty());
                if (entry.buildSupport == Build::SupportStatus::Supported)
                {
                    buildSupportedRows += 1;
                }
            }
            SC_TEST_EXPECT(buildSupportedRows == matrix.sizeInElements());
            SC_TEST_EXPECT(supportMatrixContains(matrix, Build::Platform::Apple, Build::TargetEnvironment::WindowsGNU,
                                                 Build::Architecture::Intel64, Build::SupportStatus::Supported,
                                                 Build::SupportStatus::Supported, Build::SupportTier::Tier1,
                                                 Build::RunnerSpec::Wine));
            SC_TEST_EXPECT(supportMatrixContains(matrix, Build::Platform::Apple, Build::TargetEnvironment::WindowsGNU,
                                                 Build::Architecture::Arm64, Build::SupportStatus::Supported,
                                                 Build::SupportStatus::NotYet, Build::SupportTier::Tier1,
                                                 Build::RunnerSpec::Wine));
            SC_TEST_EXPECT(supportMatrixContains(matrix, Build::Platform::Apple, Build::TargetEnvironment::LinuxGlibc,
                                                 Build::Architecture::Arm64, Build::SupportStatus::Supported,
                                                 Build::SupportStatus::SmokeSupported, Build::SupportTier::Tier1,
                                                 Build::RunnerSpec::QEMU));
            SC_TEST_EXPECT(supportMatrixContains(matrix, Build::Platform::Apple, Build::TargetEnvironment::LinuxMusl,
                                                 Build::Architecture::Intel64, Build::SupportStatus::Supported,
                                                 Build::SupportStatus::SmokeSupported, Build::SupportTier::Tier1,
                                                 Build::RunnerSpec::QEMU));
            SC_TEST_EXPECT(supportMatrixContains(matrix, Build::Platform::Windows, Build::TargetEnvironment::LinuxGlibc,
                                                 Build::Architecture::Arm64, Build::SupportStatus::Supported,
                                                 Build::SupportStatus::NotYet, Build::SupportTier::Tier2,
                                                 Build::RunnerSpec::QEMU));
            SC_TEST_EXPECT(supportMatrixContains(matrix, Build::Platform::Windows, Build::TargetEnvironment::LinuxMusl,
                                                 Build::Architecture::Intel64, Build::SupportStatus::Supported,
                                                 Build::SupportStatus::NotYet, Build::SupportTier::Tier2,
                                                 Build::RunnerSpec::QEMU));
            SC_TEST_EXPECT(supportMatrixContains(matrix, Build::Platform::Linux, Build::TargetEnvironment::WindowsMSVC,
                                                 Build::Architecture::Arm64, Build::SupportStatus::Supported,
                                                 Build::SupportStatus::SmokeSupported, Build::SupportTier::Tier2,
                                                 Build::RunnerSpec::Wine));
        }

        if (test_section("Native backend support matrix documentation"))
        {
            FileSystem fs;
            SC_TRUST_RESULT(fs.init(report.libraryRootDirectory.view()));

            String buildDocument = StringEncoding::Utf8;
            SC_TRUST_RESULT(fs.read("Documentation/Pages/Build.md", buildDocument));
            const StringView documentView(buildDocument.view());

            for (const Build::SupportMatrixEntry& entry : Build::getNativeBackendSupportMatrix())
            {
                String expectedRow = StringEncoding::Utf8;
                SC_TRUST_RESULT(StringBuilder::format(
                    expectedRow, "| {} | {} | {} | {} | {} | {} | {} |",
                    supportMatrixHostName(entry.hostMachine.platform),
                    Build::TargetEnvironment::toString(entry.targetMachine.environment),
                    supportMatrixArchitectureName(entry.targetMachine.architecture),
                    Build::SupportStatus::toString(entry.buildSupport),
                    Build::SupportStatus::toString(entry.runSupport), Build::RunnerSpec::toString(entry.runner),
                    Build::SupportTier::toString(entry.tier)));
                SC_TEST_EXPECT(documentView.containsString(expectedRow.view()));
            }
        }

        if (test_section("Build CLI reserves ABI option"))
        {
            StringView cliArguments[] = {"SCTest", "--target", "linux-glibc-x86_64", "--abi", "gnu"};

            Build::Action                 cliAction;
            Tools::detail::BuildCLIStatus status = Tools::detail::BuildCLIStatus::Ready;
            SC_TEST_EXPECT(
                not prepareBuildCLIAction(report, {cliArguments, 5}, Build::Action::Compile, cliAction, status));
            SC_TEST_EXPECT(status == Tools::detail::BuildCLIStatus::Ready);
        }

        if (test_section("Build CLI runner diagnostics"))
        {
            auto expectRejectedRun = [&](Span<const StringView> cliArguments) -> Result
            {
                Build::Action                 cliAction;
                Tools::detail::BuildCLIStatus status = Tools::detail::BuildCLIStatus::Ready;
                SC_TEST_EXPECT(not prepareBuildCLIAction(report, cliArguments, Build::Action::Run, cliAction, status));
                SC_TEST_EXPECT(status == Tools::detail::BuildCLIStatus::Ready);
                return Result(true);
            };

            StringView runnerNone[]   = {"SCTest", "--target", "windows-gnu-x86_64", "--runner", "none"};
            StringView qemuWindows[]  = {"SCTest", "--target", "windows-gnu-x86_64", "--runner", "qemu"};
            StringView wineLinux[]    = {"SCTest", "--target", "linux-glibc-x86_64", "--runner", "wine"};
            StringView customNoPath[] = {"SCTest", "--target", "windows-gnu-x86_64", "--runner", "custom"};
            SC_TRUST_RESULT(expectRejectedRun({runnerNone, 5}));
            SC_TRUST_RESULT(expectRejectedRun({qemuWindows, 5}));
            SC_TRUST_RESULT(expectRejectedRun({wineLinux, 5}));
            SC_TRUST_RESULT(expectRejectedRun({customNoPath, 5}));
        }

        if (test_section("Build CLI keeps Fil-C toolchain-only"))
        {
            StringView filcTargetProfile[] = {"SCTest", "--target", "linux-filc-x86_64"};
            StringView filcCrossTarget[]   = {"SCTest", "--target", "linux-glibc-x86_64", "--toolchain", "filc"};

            Build::Action                 cliAction;
            Tools::detail::BuildCLIStatus status = Tools::detail::BuildCLIStatus::Ready;
            SC_TEST_EXPECT(
                not prepareBuildCLIAction(report, {filcTargetProfile, 3}, Build::Action::Compile, cliAction, status));
            SC_TEST_EXPECT(status == Tools::detail::BuildCLIStatus::Error);

            status = Tools::detail::BuildCLIStatus::Ready;
            SC_TEST_EXPECT(
                not prepareBuildCLIAction(report, {filcCrossTarget, 5}, Build::Action::Compile, cliAction, status));
            SC_TEST_EXPECT(status == Tools::detail::BuildCLIStatus::Ready);
        }

        if (test_section("Build CLI help reflects support matrix"))
        {
            String help = StringEncoding::Utf8;
            SC_TEST_EXPECT(writeBuildHelpAddendumToString(Build::Action::Run, help));
            SC_TEST_EXPECT(StringView(help.view()).containsString("Current native-backend support matrix"));
            SC_TEST_EXPECT(StringView(help.view())
                               .containsString("macOS -> linux-glibc-arm64: build=supported, run=smoke-supported"));
            SC_TEST_EXPECT(StringView(help.view())
                               .containsString("macOS -> linux-musl-x86_64: build=supported, run=smoke-supported"));
            SC_TEST_EXPECT(
                StringView(help.view()).containsString("Windows -> linux-musl-x86_64: build=supported, run=not-yet"));
            SC_TEST_EXPECT(
                StringView(help.view()).containsString("--abi is reserved for a future public ABI selector"));
            SC_TEST_EXPECT(StringView(help.view()).containsString("Fil-C is toolchain-only for now"));
        }

        if (test_section("Visual Studio 2022"))
        {
            action.parameters.generator = Build::Generator::VisualStudio2022;
            action.parameters.platform  = Build::Platform::Windows;
            SC_TEST_EXPECT(Build::executeAction(action));
        }
        if (test_section("XCode"))
        {
            action.parameters.generator = Build::Generator::XCode;
            action.parameters.platform  = Build::Platform::Apple;
            SC_TEST_EXPECT(Build::executeAction(action));
        }
        if (test_section("Makefile (macOS)"))
        {
            action.parameters.generator = Build::Generator::Make;
            action.parameters.platform  = Build::Platform::Apple;
            SC_TEST_EXPECT(Build::executeAction(action));
        }
        if (test_section("Makefile (Linux)"))
        {
            action.parameters.generator = Build::Generator::Make;
            action.parameters.platform  = Build::Platform::Linux;
            SC_TEST_EXPECT(Build::executeAction(action));
        }

        const bool makefileStaticLibraryMacOS = test_section("Makefile static library (macOS)");
        const bool makefileStaticLibraryLinux = test_section("Makefile static library (Linux)");
        if (makefileStaticLibraryMacOS or makefileStaticLibraryLinux)
        {
            FileSystem fileSystem;
            SC_TRUST_RESULT(fileSystem.init("."));

            Build::Action staticLibraryAction;
            staticLibraryAction.action                  = Build::Action::Configure;
            staticLibraryAction.configurationName       = "Debug";
            staticLibraryAction.workspaceName           = MakefileStaticLibraryWorkspaceName;
            staticLibraryAction.parameters.directories  = directories;
            staticLibraryAction.parameters.generator    = Build::Generator::Make;
            staticLibraryAction.parameters.architecture = Build::Architecture::Any;
            staticLibraryAction.parameters.execution    = action.parameters.execution;
            staticLibraryAction.parameters.toolchain    = action.parameters.toolchain;

            if (makefileStaticLibraryLinux)
            {
                staticLibraryAction.parameters.platform = Build::Platform::Linux;
            }
            else
            {
                staticLibraryAction.parameters.platform = Build::Platform::Apple;
            }

            SC_TEST_EXPECT(Build::Action::execute(staticLibraryAction, configureMakefileStaticLibrary));

            String makefilePath = StringEncoding::Utf8;
            SC_TRUST_RESULT(computeGeneratedMakefilePath(staticLibraryAction.parameters.directories,
                                                         staticLibraryAction.parameters.platform, makefilePath));
            SC_TEST_EXPECT(fileSystem.existsAndIsFile(makefilePath.view()));

            String makefileContents = StringEncoding::Utf8;
            SC_TRUST_RESULT(fileSystem.read(makefilePath.view(), makefileContents));

            SC_TEST_EXPECT(StringView(makefileContents.view()).containsString("libWriterStaticLibrary.a"));
            SC_TEST_EXPECT(
                StringView(makefileContents.view())
                    .containsString("ar rcs $(WriterStaticLibrary_TARGET_DIR)/$(WriterStaticLibrary_TARGET_NAME) "
                                    "$(WriterStaticLibrary_OBJECT_FILES)"));
            SC_TEST_EXPECT(not StringView(makefileContents.view())
                                   .containsString("$(CXX) -o $(WriterStaticLibrary_TARGET_DIR)/$(WriterStaticLibrary_"
                                                   "TARGET_NAME) $(WriterStaticLibrary_OBJECT_FILES) "
                                                   "$(WriterStaticLibrary_LDFLAGS)"));
            SC_TEST_EXPECT(StringView(makefileContents.view())
                               .containsString("Cannot run static library target 'WriterStaticLibrary'"));

            staticLibraryAction.action      = Build::Action::Run;
            staticLibraryAction.projectName = MakefileStaticLibraryProjectName;
            Result runResult = Build::Action::execute(staticLibraryAction, configureMakefileStaticLibrary);
            SC_TEST_EXPECT(not runResult);
        }
    }
};

namespace SC
{
void runBuildTest(SC::TestReport& report) { BuildTest test(report); }
} // namespace SC
