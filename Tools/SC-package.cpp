// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT

#include "SC-package.h"
#include "../Libraries/FileSystemIterator/FileSystemIterator.h"
#include "../Libraries/Memory/String.h"
#include <stdlib.h>
namespace SC
{
namespace Tools
{
static Result extractTarArchiveFlatteningRoot(StringView sourceFile, StringView destinationDirectory)
{
    String  archiveListing;
    Process listProcess;
    SC_TRY(listProcess.exec({"tar", "-tf", sourceFile}, archiveListing));
    SC_TRY_MSG(listProcess.getExitStatus() == 0, "tar listing failed");

    StringViewTokenizer tokenizer(archiveListing.view());
    SC_TRY_MSG(tokenizer.tokenizeNext({'\n'}), "tar archive is empty");

    StringView rootDirectory = tokenizer.component.trimAnyOf({'\r', '\n', '/'});
    StringView nestedPath;
    if (rootDirectory.splitBefore("/", nestedPath))
    {
        rootDirectory = nestedPath;
    }
    SC_TRY_MSG(not rootDirectory.isEmpty(), "tar archive root directory is empty");

    String     tempDirectory = format("{}-extracting", destinationDirectory);
    String     extractedRoot = format("{}/{}", tempDirectory.view(), rootDirectory);
    FileSystem fs;
    SC_TRY(fs.init("."));
    if (fs.existsAndIsDirectory(tempDirectory.view()))
    {
        SC_TRY(fs.removeDirectoryRecursive(tempDirectory.view()));
    }
    SC_TRY(fs.makeDirectoryRecursive(tempDirectory.view()));

    Process extractProcess;
    SC_TRY(extractProcess.exec({"tar", "-xf", sourceFile, "-C", tempDirectory.view()}));
    SC_TRY_MSG(extractProcess.getExitStatus() == 0, "tar extraction failed");
    SC_TRY_MSG(fs.existsAndIsDirectory(extractedRoot.view()), "tar extraction root directory missing");

    SC_TRY(fs.removeDirectoryRecursive(destinationDirectory));
    SC_TRY(fs.rename(extractedRoot.view(), destinationDirectory));
    SC_TRY(fs.removeDirectoryRecursive(tempDirectory.view()));
    return Result(true);
}

static Result resolveToolSupportPath(StringView fileName, String& output)
{
    String sourcePath = StringEncoding::Utf8;
    SC_TRY(sourcePath.assign(__FILE__));
    String toolsDirectory = StringEncoding::Utf8;
    SC_TRY(toolsDirectory.assign(Path::dirname(sourcePath.view(), Path::AsNative)));
    SC_TRY(Path::join(output, {toolsDirectory.view(), "Support", fileName}));
    return Result(true);
}

static Result findFirstSubdirectory(StringView directory, String& output)
{
    FileSystemIterator::FolderState entries[1];
    FileSystemIterator              iterator;
    SC_TRY(iterator.init(directory, entries));
    while (iterator.enumerateNext())
    {
        const FileSystemIterator::Entry entry = iterator.get();
        if (entry.isDirectory())
        {
            SC_TRY(output.assign(entry.name));
            return Result(true);
        }
    }
    return Result::Error("Missing package directory");
}

static Result resolveMSVCVersions(StringView packageRoot, String& msvcVersion, String& sdkVersion)
{
    String msvcDirectory = StringEncoding::Utf8;
    String sdkDirectory  = StringEncoding::Utf8;
    SC_TRY(Path::join(msvcDirectory, {packageRoot, "VC", "Tools", "MSVC"}));
    SC_TRY(Path::join(sdkDirectory, {packageRoot, "Windows Kits", "10", "bin"}));
    SC_TRY(findFirstSubdirectory(msvcDirectory.view(), msvcVersion));
    SC_TRY(findFirstSubdirectory(sdkDirectory.view(), sdkVersion));
    return Result(true);
}

static Result resolveHostCommandPath(StringView executable, String& output)
{
    if (StringView(executable).containsString("/") or StringView(executable).containsString("\\"))
    {
        SC_TRY(output.assign(executable));
        return Result(true);
    }

    Process process;
    String  commandPath = StringEncoding::Utf8;
    SC_TRY(process.exec({"which", executable}, commandPath));
    SC_TRY_MSG(process.getExitStatus() == 0, "Cannot resolve host command path");
    SC_TRY(output.assign(StringView(commandPath.view()).trimWhiteSpaces()));
    return Result(true);
}

static bool probeCommandVersion(StringView executable)
{
    Process process;
    String  version = StringEncoding::Utf8;
    return process.exec({executable, "--version"}, version) and process.getExitStatus() == 0;
}

static bool resolveRunnableHostCommand(StringView executable, String& output)
{
    if (not resolveHostCommandPath(executable, output))
    {
        return false;
    }
    return probeCommandVersion(output.view());
}

static Result writeLinuxWineWrapper(StringView packagesCacheDirectory, StringView wrapperName, StringView firstStage,
                                    StringView secondStage, String& output)
{
    FileSystem fs;
    SC_TRY(fs.init("."));

    String wrapperDirectory = StringEncoding::Utf8;
    SC_TRY(Path::join(wrapperDirectory, {packagesCacheDirectory, "msvc"}));
    SC_TRY(fs.makeDirectoryRecursive(wrapperDirectory.view()));

    SC_TRY(Path::join(output, {wrapperDirectory.view(), wrapperName}));

    String scriptContents = StringEncoding::Utf8;
    auto   builder        = StringBuilder::create(scriptContents);
    SC_TRY(builder.append("#!/bin/sh\n"));
    SC_TRY(builder.append("exec \"{}\" \"{}\" \"$@\"\n", firstStage, secondStage));
    builder.finalize();

    SC_TRY(fs.writeString(output.view(), scriptContents.view()));
    SC_TRY(fs.chmod(output.view(), 0755u));
    return Result(true);
}

static Result resolveLinuxWineExecutable(StringView packagesCacheDirectory, String& output)
{
    String wine64Path = StringEncoding::Utf8;
    String winePath   = StringEncoding::Utf8;
    String box64Path  = StringEncoding::Utf8;

    const bool hasWine64 = resolveRunnableHostCommand("wine64", wine64Path);
    const bool hasWine   = resolveRunnableHostCommand("wine", winePath);
    const bool hasBox64  = resolveRunnableHostCommand("box64", box64Path);

    if (HostInstructionSet == InstructionSet::ARM64 and hasBox64)
    {
        if (hasWine64)
        {
            return writeLinuxWineWrapper(packagesCacheDirectory, "box64-wine64-wrapper.sh", box64Path.view(),
                                         wine64Path.view(), output);
        }
        if (hasWine)
        {
            return writeLinuxWineWrapper(packagesCacheDirectory, "box64-wine-wrapper.sh", box64Path.view(),
                                         winePath.view(), output);
        }
    }

    if (hasWine64)
    {
        SC_TRY(output.assign(wine64Path.view()));
        return Result(true);
    }
    if (hasWine)
    {
        SC_TRY(output.assign(winePath.view()));
        return Result(true);
    }

    if (HostInstructionSet == InstructionSet::ARM64)
    {
        return Result::Error("Cannot find a usable Wine runner. Install wine64/wine, or install box64 plus "
                             "wine64/wine, or pass --wine/SC_MSVC_WINE with a wrapper path. Linux arm64 hosts need "
                             "a runner that can launch the Windows x64 MSVC tools.");
    }
    return Result::Error(
        "Cannot find wine executable. Install wine64/wine or pass --wine/SC_MSVC_WINE with a Wine wrapper path.");
}

static Result readEnvironmentVariable(StringView name, String& value, bool& found)
{
    found = false;
#if SC_PLATFORM_WINDOWS
    if (not name.isNullTerminated())
    {
        return Result::Error("Environment variable name must be null terminated");
    }

    char*   variableValue = nullptr;
    size_t  variableSize  = 0;
    errno_t result        = _dupenv_s(&variableValue, &variableSize, name.bytesWithoutTerminator());
    if (result != 0 or variableValue == nullptr or variableSize == 0)
    {
        free(variableValue);
        return Result(true);
    }

    found = true;
    SC_TRY(value.assign(StringView::fromNullTerminated(variableValue, StringEncoding::Native)));
    free(variableValue);
    return Result(true);
#else
    if (not name.isNullTerminated())
    {
        return Result::Error("Environment variable name must be null terminated");
    }

    if (const char* variableValue = ::getenv(name.bytesWithoutTerminator()))
    {
        found = true;
        SC_TRY(value.assign(StringView::fromNullTerminated(variableValue, StringEncoding::Native)));
    }
    return Result(true);
#endif
}

static Result resolveMSVCWineExecutable(StringView packagesCacheDirectory, StringView packagesInstallDirectory,
                                        StringView overrideWineExecutable, String& wineExecutable)
{
    if (not overrideWineExecutable.isEmpty())
    {
        SC_TRY(resolveHostCommandPath(overrideWineExecutable, wineExecutable));
        return Result(true);
    }

    bool hasOverrideWine = false;
    SC_TRY(readEnvironmentVariable("SC_MSVC_WINE", wineExecutable, hasOverrideWine));
    if (hasOverrideWine)
    {
        String resolvedWine = StringEncoding::Utf8;
        SC_TRY(resolveHostCommandPath(wineExecutable.view(), resolvedWine));
        SC_TRY(wineExecutable.assign(resolvedWine.view()));
        return Result(true);
    }

    switch (HostPlatform)
    {
    case Platform::Apple: {
        Package winePackage;
        SC_TRY(installWineStableRunner(packagesCacheDirectory, packagesInstallDirectory, winePackage));
        SC_TRY(StringBuilder::format(wineExecutable, "{}/Wine Stable.app/Contents/Resources/wine/bin/wine",
                                     winePackage.installDirectoryLink));
        return Result(true);
    }
    case Platform::Linux: return resolveLinuxWineExecutable(packagesCacheDirectory, wineExecutable);
    case Platform::Windows:
    case Platform::Emscripten: return Result::Error("Portable MSVC is only supported on macOS and Linux hosts");
    }
    Assert::unreachable();
}

struct MSVCPackageInstallOptions
{
    StringView importDirectory;
    StringView wineExecutable;
};

static Result parseMSVCPackageInstallOptions(Span<const StringView> arguments, MSVCPackageInstallOptions& options)
{
    options = {};
    if (arguments.sizeInElements() <= 1)
    {
        return Result(true);
    }

    for (size_t idx = 1; idx < arguments.sizeInElements(); ++idx)
    {
        const StringView argument = arguments[idx];
        if (argument == "--import-directory")
        {
            SC_TRY_MSG(idx + 1 < arguments.sizeInElements(), "Missing value for --import-directory");
            options.importDirectory = arguments[++idx];
        }
        else if (argument == "--wine")
        {
            SC_TRY_MSG(idx + 1 < arguments.sizeInElements(), "Missing value for --wine");
            options.wineExecutable = arguments[++idx];
        }
        else if (argument.startsWith("--"))
        {
            return Result::Error("Unknown option for SC-package install msvc");
        }
        else if (options.importDirectory.isEmpty())
        {
            // Legacy compatibility: keep accepting a positional import directory after "msvc".
            options.importDirectory = argument;
        }
        else
        {
            return Result::Error("Unexpected extra argument for SC-package install msvc");
        }
    }
    return Result(true);
}

static Result writeMSVCPackageMetadata(StringView packageRoot, StringView wineExecutable)
{
    String msvcVersion = StringEncoding::Utf8;
    String sdkVersion  = StringEncoding::Utf8;
    SC_TRY(resolveMSVCVersions(packageRoot, msvcVersion, sdkVersion));

    String metadata = StringEncoding::Utf8;
    auto   builder  = StringBuilder::create(metadata);
    SC_TRY(builder.append("{\n"));
    SC_TRY(builder.append("  \"toolchain\": \"msvc\",\n"));
    SC_TRY(builder.append("  \"host\": \"x64\",\n"));
    SC_TRY(builder.append("  \"target\": \"multi\",\n"));
    SC_TRY(builder.append("  \"targets\": [\"x64\", \"arm64\"],\n"));
    SC_TRY(builder.append("  \"msvcVersion\": \"{}\",\n", msvcVersion.view()));
    SC_TRY(builder.append("  \"sdkVersion\": \"{}\",\n", sdkVersion.view()));
    SC_TRY(builder.append("  \"wine\": \"{}\"\n", wineExecutable));
    SC_TRY(builder.append("}\n"));
    builder.finalize();

    FileSystem fs;
    SC_TRY(fs.init("."));
    String metadataPath = StringEncoding::Utf8;
    SC_TRY(Path::join(metadataPath, {packageRoot, "sc-msvc-package.json"}));
    SC_TRY(fs.writeString(metadataPath.view(), metadata.view()));
    return Result(true);
}

static Result writeMSVCWrapperScripts(StringView packageRoot)
{
    FileSystem fs;
    SC_TRY(fs.init("."));

    String sourceWrapper = StringEncoding::Utf8;
    SC_TRY(resolveToolSupportPath("PortableMSVCWrapper.py", sourceWrapper));
    String wrapperContents = StringEncoding::Utf8;
    SC_TRY(readFileIntoString(sourceWrapper.view(), wrapperContents));

    static constexpr StringView targetArchitectures[] = {"x64", "arm64"};
    static constexpr StringView toolNames[]           = {"cl", "link", "lib"};
    for (const StringView targetArchitecture : targetArchitectures)
    {
        String binDirectory = StringEncoding::Utf8;
        SC_TRY(Path::join(binDirectory, {packageRoot, "bin", targetArchitecture}));
        SC_TRY(fs.makeDirectoryRecursive(binDirectory.view()));

        String destinationWrapper = StringEncoding::Utf8;
        SC_TRY(Path::join(destinationWrapper, {binDirectory.view(), "msvc-wrapper.py"}));
        SC_TRY(fs.writeString(destinationWrapper.view(), wrapperContents.view()));
        SC_TRY(fs.chmod(destinationWrapper.view(), 0755u));

        for (const StringView toolName : toolNames)
        {
            String scriptPath = StringEncoding::Utf8;
            SC_TRY(Path::join(scriptPath, {binDirectory.view(), toolName}));

            String scriptContents = StringEncoding::Utf8;
            auto   builder        = StringBuilder::create(scriptContents);
            SC_TRY(builder.append("#!/bin/sh\n"));
            SC_TRY(builder.append("SCRIPT_DIR=$(CDPATH= cd -- \"$(dirname \"$0\")\" && pwd)\n"));
            SC_TRY(builder.append("exec \"$SCRIPT_DIR/msvc-wrapper.py\" {} \"$@\"\n", toolName));
            builder.finalize();

            SC_TRY(fs.writeString(scriptPath.view(), scriptContents.view()));
            SC_TRY(fs.chmod(scriptPath.view(), 0755u));
        }
    }
    return Result(true);
}

static Result finalizeInstalledPackage(Package& package)
{
    FileSystem fs;
    SC_TRY(fs.init("."));
    SC_TRY(fs.removeLinkIfExists(package.installDirectoryLink.view()));
    if (not createLink(package.packageLocalDirectory.view(), package.installDirectoryLink.view()))
    {
        if (fs.existsAndIsDirectory(package.installDirectoryLink.view()))
        {
            SC_TRY(fs.removeDirectoriesRecursive(package.installDirectoryLink.view()));
        }
        SC_TRY(fs.copyDirectory(package.packageLocalDirectory.view(), package.installDirectoryLink.view()));
    }
    return Result(true);
}

static Result testMSVCToolchain(const Package& package)
{
    FileSystem fs;
    SC_TRY(fs.init("."));

    static constexpr StringView targetArchitectures[] = {"x64", "arm64"};
    for (const StringView targetArchitecture : targetArchitectures)
    {
        String testDirectory = StringEncoding::Utf8;
        SC_TRY(Path::join(testDirectory, {package.packageLocalDirectory.view(), "tmp", targetArchitecture}));
        SC_TRY(fs.makeDirectoryRecursive(testDirectory.view()));

        String sourcePath = StringEncoding::Utf8;
        String objectPath = StringEncoding::Utf8;
        String outputPath = StringEncoding::Utf8;
        SC_TRY(Path::join(sourcePath, {testDirectory.view(), "toolchain-smoke.cpp"}));
        SC_TRY(Path::join(objectPath, {testDirectory.view(), "toolchain-smoke.obj"}));
        SC_TRY(Path::join(outputPath, {testDirectory.view(), "toolchain-smoke.exe"}));

        SC_TRY(fs.writeString(sourcePath.view(), "#define WIN32_LEAN_AND_MEAN\n"
                                                 "#include <WinSock2.h>\n"
                                                 "int main()\n"
                                                 "{\n"
                                                 "    WSADATA data = {};\n"
                                                 "    return WSAStartup(MAKEWORD(2, 2), &data);\n"
                                                 "}\n"));

        String compilerWrapper = StringEncoding::Utf8;
        String linkerWrapper   = StringEncoding::Utf8;
        SC_TRY(Path::join(compilerWrapper, {package.installDirectoryLink.view(), "bin", targetArchitecture, "cl"}));
        SC_TRY(Path::join(linkerWrapper, {package.installDirectoryLink.view(), "bin", targetArchitecture, "link"}));

        {
            String  output = StringEncoding::Utf8;
            String  errors = StringEncoding::Utf8;
            Process process;
            SC_TRY(process.exec({compilerWrapper.view(), "/nologo", "/EHsc", "/MTd", "/Z7", "/c", sourcePath.view(),
                                 format("/Fo{}", objectPath.view()).view()},
                                output, Process::StdIn(), errors));
            SC_TRY_MSG(process.getExitStatus() == 0, "Portable MSVC compile smoke test failed");
        }
        SC_TRY_MSG(fs.existsAndIsFile(objectPath.view()), "Portable MSVC compile smoke output is missing");

        {
            String  output = StringEncoding::Utf8;
            String  errors = StringEncoding::Utf8;
            Process process;
            SC_TRY(process.exec({linkerWrapper.view(), "/nologo", objectPath.view(), "Ws2_32.lib",
                                 format("/OUT:{}", outputPath.view()).view()},
                                output, Process::StdIn(), errors));
            SC_TRY_MSG(process.getExitStatus() == 0, "Portable MSVC link smoke test failed");
        }
        SC_TRY_MSG(fs.existsAndIsFile(outputPath.view()), "Portable MSVC link smoke output is missing");
    }
    return Result(true);
}

static Result prepareMSVCWinePrefixHeadless(StringView wineExecutable, StringView prefixDirectory)
{
    auto configureProcess = [&](Process& process) -> Result
    {
        SC_TRY(process.setEnvironment("WINEPREFIX", prefixDirectory));
        SC_TRY(process.setEnvironment("WINEARCH", "win64"));
        SC_TRY(process.setEnvironment("WINEDLLOVERRIDES", "winemenubuilder.exe=d;winedbg.exe=d;vctip.exe=d"));
        SC_TRY(process.setEnvironment("WINEDEBUG", "-all"));
        SC_TRY(process.setEnvironment("MVK_CONFIG_LOG_LEVEL", "0"));
        return Result(true);
    };

    auto runRegCommand = [&](Span<const StringSpan> arguments, const char* errorMessage,
                             int allowedExitStatus = 0) -> Result
    {
        Process process;
        String  stdOut = StringEncoding::Utf8;
        String  stdErr = StringEncoding::Utf8;
        SC_TRY(configureProcess(process));
        SC_TRY(process.exec(arguments, stdOut, {}, stdErr));
        if (not(process.getExitStatus() == allowedExitStatus or process.getExitStatus() == 0))
        {
            return Result::FromStableCharPointer(errorMessage);
        }
        return Result(true);
    };

    const StringSpan showCrashDialogArguments[] = {
        wineExecutable, "reg",
        "add",          "HKEY_CURRENT_USER\\Software\\Wine\\WineDbg",
        "/v",           "ShowCrashDialog",
        "/t",           "REG_DWORD",
        "/d",           "0",
        "/f",
    };
    SC_TRY(runRegCommand(showCrashDialogArguments, "Failed configuring MSVC Wine crash dialog"));

    const StringSpan breakOnFirstChanceArguments[] = {
        wineExecutable, "reg",
        "add",          "HKEY_CURRENT_USER\\Software\\Wine\\WineDbg",
        "/v",           "BreakOnFirstChance",
        "/t",           "REG_DWORD",
        "/d",           "0",
        "/f",
    };
    SC_TRY(runRegCommand(breakOnFirstChanceArguments, "Failed configuring MSVC Wine first-chance exceptions"));

    const StringSpan removeWinemenubuilderArguments[] = {
        wineExecutable, "reg",
        "delete",       "HKEY_LOCAL_MACHINE\\Software\\Microsoft\\Windows\\CurrentVersion\\RunServices",
        "/v",           "winemenubuilder",
        "/f",
    };
    SC_TRY(runRegCommand(removeWinemenubuilderArguments, "Failed disabling MSVC Wine menu builder", 1));

    const StringSpan removeWow64WinemenubuilderArguments[] = {
        wineExecutable, "reg",
        "delete",       "HKEY_LOCAL_MACHINE\\Software\\Wow6432Node\\Microsoft\\Windows\\CurrentVersion\\RunServices",
        "/v",           "winemenubuilder",
        "/f",
    };
    SC_TRY(runRegCommand(removeWow64WinemenubuilderArguments, "Failed disabling MSVC Wine menu builder", 1));
    return Result(true);
}

Result installDoxygen(StringView packagesCacheDirectory, StringView packagesInstallDirectory, Package& package)
{
    // https://github.com/doxygen/doxygen/releases/download/Release_1_12_0/Doxygen-1.12.0.dmg
    static constexpr StringView packageVersion     = "1.12.0";
    static constexpr StringView packageVersionDash = "1_12_0";
    static constexpr StringView testVersion        = "1.12.0 (c73f5d30f9e8b1df5ba15a1d064ff2067cbb8267";
    static constexpr StringView baseURL            = "https://github.com/doxygen/doxygen/releases/download";

    Download download;
    download.packagesCacheDirectory   = packagesCacheDirectory;
    download.packagesInstallDirectory = packagesInstallDirectory;

    download.packageName    = "doxygen";
    download.packageVersion = packageVersion;

    SC_TRY(StringBuilder::format(download.url, "{0}/Release_{1}/", baseURL, packageVersionDash));

    CustomFunctions functions;
    switch (HostPlatform)
    {
    case Platform::Apple: {
        auto sb = StringBuilder::createForAppendingTo(download.url);
        SC_TRY(sb.append("Doxygen-{0}.dmg", download.packageVersion));
        sb.finalize();
        download.packagePlatform  = "macos";
        download.expectedHash     = "354ee835cf03e8a0187460a1456eb108";
        package.packageBaseName   = format("Doxygen-{0}.dmg", download.packageVersion);
        functions.extractFunction = [](StringView fileName, StringView directory) -> Result
        {
            String mountPoint = format("/Volumes/Doxygen-{0}", packageVersion);
            SC_TRY(Process().exec({"hdiutil", "attach", "-nobrowse", "-readonly", "-noverify", "-noautoopen",
                                   "-mountpoint", mountPoint.view(), fileName}));
            FileSystem fs;
            SC_TRY(fs.init(directory));
            String fileToCopy = format("/Volumes/Doxygen-{0}/Doxygen.app/Contents/Resources/doxygen", packageVersion);
            SC_TRY(fs.copyFile(fileToCopy.view(), "doxygen", FileSystem::CopyFlags().setOverwrite(true)));
            SC_TRY(Process().exec({"hdiutil", "detach", mountPoint.view()}));
            return Result(true);
        };
    }
    break;

    case Platform::Linux: {
        switch (HostInstructionSet)
        {
        case InstructionSet::ARM64: return Result::Error("Doxygen: Unsupported architecture ARM64");
        default: break;
        }
        auto sb = StringBuilder::createForAppendingTo(download.url);
        SC_TRY(sb.append("doxygen-{0}.linux.bin.tar.gz", download.packageVersion));
        sb.finalize();
        download.packagePlatform  = "linux";
        download.expectedHash     = "fd96a5defa535dfe2e987b46540844a4";
        package.packageBaseName   = format("doxygen-{0}.linux.bin.tar.gz", download.packageVersion);
        functions.extractFunction = [](StringView fileName, StringView directory) -> Result
        { return tarExpandSingleFileTo(fileName, directory, "doxygen-1.12.0/bin/doxygen", 2); };
    }
    break;
    case Platform::Windows: {
        auto sb = StringBuilder::createForAppendingTo(download.url);
        SC_TRY(sb.append("doxygen-{0}.windows.x64.bin.zip", download.packageVersion));
        sb.finalize();
        download.packagePlatform = "windows";
        download.expectedHash    = "d014a212331693ffcf72ad99b2087ea0";
        package.packageBaseName  = format("doxygen-{0}.windows.x64.bin.zip", download.packageVersion);
    }
    break;
    case Platform::Emscripten: return Result::Error("Unsupported platform");
    }

    functions.testFunction = [](const Download& download, const Package& package)
    {
        SC_COMPILER_UNUSED(download);
        String result;
        String path;
        switch (HostPlatform)
        {
        case Platform::Linux: //
        case Platform::Apple: //
            path = format("{0}/doxygen", package.installDirectoryLink);
            break;
        case Platform::Windows: path = format("{0}/doxygen.exe", package.installDirectoryLink); break;
        case Platform::Emscripten: return Result::Error("Unsupported platform");
        }
        SC_TRY_MSG(Process().exec({path.view(), "-v"}, result), "Cannot run doxygen executable");
        return Result(StringView(result.view()).startsWith(testVersion));
    };
    SC_TRY(packageInstall(download, package, functions));
    return Result(true);
}

Result installDoxygenAwesomeCss(StringView packagesCacheDirectory, StringView packagesInstallDirectory,
                                Package& package)
{
    Download download;
    download.packagesCacheDirectory   = packagesCacheDirectory;
    download.packagesInstallDirectory = packagesInstallDirectory;

    download.packageName    = "doxygen-awesome-css";
    download.packageVersion = "568f56c"; // corresponds to "v2.3.4";
    download.url            = "https://github.com/jothepro/doxygen-awesome-css.git";
    download.isGitClone     = true;
    download.shallowClone   = "568f56cde6ac78b6dfcc14acd380b2e745c301ea";
    package.packageBaseName = format("doxygen-awesome-css-{0}", download.packagePlatform);

    CustomFunctions functions;
    functions.testFunction = &verifyGitCommitHashInstall;
    SC_TRY(packageInstall(download, package, functions));
    return Result(true);
}

Result clangFormatMatchesVersion(StringView versionString, StringView wantedVersion)
{
    StringViewTokenizer tokenizer(versionString);
    SC_TRY_MSG(tokenizer.tokenizeNext({'-'}), "clang-format tokenize error"); // component = "clang-"
    SC_TRY_MSG(tokenizer.tokenizeNext({' '}), "clang-format tokenize error"); // component = "format"
    SC_TRY_MSG(tokenizer.tokenizeNext({' '}), "clang-format tokenize error"); // component = "version"
    SC_TRY_MSG(tokenizer.tokenizeNext({' '}), "clang-format tokenize error"); // component = "x.y.z"
    SC_TRY_MSG(tokenizer.component.trimAnyOf({'\n', '\r'}) == wantedVersion, "clang-format version doesn't match");
    return Result(true);
}

Result findExecutablePath(StringView executableName, String& foundPath)
{
    switch (HostPlatform)
    {
    case Platform::Windows: {
        SC_TRY(Process().exec({"where", executableName}, foundPath));
        StringViewTokenizer tokenizer(foundPath.view());
        SC_TRY(tokenizer.tokenizeNext({'\n'}));
        SC_TRY(foundPath.assign(tokenizer.component));
    }
    break;
    case Platform::Apple:
    case Platform::Linux: SC_TRY(Process().exec({"which", executableName}, foundPath)); break;
    case Platform::Emscripten: return Result::Error("Unsupported platform");
    }
    SC_TRY(foundPath.assign(StringView(foundPath.view()).trimAnyOf({'\n', '\r'})));
    return Result(true);
}

Result findSystemClangFormat(Console& console, StringView wantedVersion, String& foundPath)
{
    StringView       majorVersion = wantedVersion;
    SmallString<32>  versionedExecutable;
    SmallString<255> version;

    (void)wantedVersion.splitBefore(".", majorVersion);

    switch (HostPlatform)
    {
    case Platform::Apple: {
        SmallString<32> llvmFormula;
        SC_TRY(StringBuilder::format(llvmFormula, "llvm@{}", majorVersion));
        if (Process().exec({"brew", "--prefix", llvmFormula.view()}, foundPath))
        {
            (void)foundPath.assign(StringView(foundPath.view()).trimAnyOf({'\n', '\r'}));
            auto sb  = StringBuilder::createForAppendingTo(foundPath);
            bool res = sb.append("/bin/clang-format");
            sb.finalize();
            if (res and Process().exec({foundPath.view(), "--version"}, version) and
                clangFormatMatchesVersion(version.view(), wantedVersion))
            {
                console.print("Found \"");
                console.print(foundPath.view());
                console.print("\" ");
                console.print(version.view());
                return Result(true);
            }
        }
    }
    break;
    default: break;
    }

    SC_TRY(StringBuilder::format(versionedExecutable, "clang-format-{}", majorVersion));
    if (Process().exec({versionedExecutable.view(), "--version"}, version) and
        clangFormatMatchesVersion(version.view(), wantedVersion))
    {
        SC_TRY(findExecutablePath(versionedExecutable.view(), foundPath));
        console.print("Found \"");
        console.print(foundPath.view());
        console.print("\" ");
        console.print(version.view());
        return Result(true);
    }

    if (Process().exec({"clang-format", "--version"}, version) and
        clangFormatMatchesVersion(version.view(), wantedVersion))
    {
        SC_TRY(findExecutablePath("clang-format", foundPath));
        console.print("Found \"");
        console.print(foundPath.view());
        console.print("\" ");
        console.print(version.view());
        return Result(true);
    }

    return Result::Error("No matching system clang-format found");
}

Result installClangBinaries(StringView packagesCacheDirectory, StringView packagesInstallDirectory, Package& package)
{
    CustomFunctions functions;

    Download download;
    download.packagesCacheDirectory   = packagesCacheDirectory;
    download.packagesInstallDirectory = packagesInstallDirectory;
    download.packageName              = "clang-binaries";
    download.packageVersion           = "20.1.8";
    download.hashType                 = Hashing::TypeSHA256;

    StringView wantedVersion            = "20.1.8";
    StringView clangFormatPathInArchive = {};
    switch (HostPlatform)
    {
    case Platform::Apple: {
        switch (HostInstructionSet)
        {
        case InstructionSet::ARM64:
            download.packagePlatform = "macos_arm64";
            download.url             = "https://github.com/llvm/llvm-project/releases/download/llvmorg-20.1.8/"
                                       "LLVM-20.1.8-macOS-ARM64.tar.xz";
            download.expectedHash    = "a9a22f450d35f1f73cd61ab6a17c6f27d8f6051d56197395c1eb397f0c9bbec4";
            package.packageBaseName  = "LLVM-20.1.8-macOS-ARM64.tar.xz";
            clangFormatPathInArchive = "LLVM-20.1.8-macOS-ARM64/bin/clang-format";
            break;
        case InstructionSet::Intel64:
            return Result::Error("Automatic clang-format install is unavailable on Intel macOS because recent official "
                                 "LLVM releases no longer ship Intel macOS archives. Install llvm@20 with Homebrew "
                                 "and use $(brew --prefix llvm@20)/bin/clang-format");
        case InstructionSet::Intel32: return Result::Error("Unsupported platform");
        }
        break;
    }

    case Platform::Linux: {
        switch (HostInstructionSet)
        {
        case InstructionSet::ARM64:
            download.packagePlatform = "linux_arm64";
            download.url             = "https://github.com/llvm/llvm-project/releases/download/llvmorg-20.1.8/"
                                       "LLVM-20.1.8-Linux-ARM64.tar.xz";
            download.expectedHash    = "b855cc17d935fdd83da82206b7a7cfc680095efd1e9e8182c4a05e761958bef8";
            package.packageBaseName  = "LLVM-20.1.8-Linux-ARM64.tar.xz";
            clangFormatPathInArchive = "LLVM-20.1.8-Linux-ARM64/bin/clang-format";
            break;
        case InstructionSet::Intel64:
            download.packagePlatform = "linux_intel64";
            download.url             = "https://github.com/llvm/llvm-project/releases/download/llvmorg-20.1.8/"
                                       "LLVM-20.1.8-Linux-X64.tar.xz";
            download.expectedHash    = "1ead36b3dfcb774b57be530df42bec70ab2d239fbce9889447c7a29a4ddc1ae6";
            package.packageBaseName  = "LLVM-20.1.8-Linux-X64.tar.xz";
            clangFormatPathInArchive = "LLVM-20.1.8-Linux-X64/bin/clang-format";
            break;
        case InstructionSet::Intel32: return Result::Error("Unsupported platform");
        }
        break;
    }

    case Platform::Windows: {
        switch (HostInstructionSet)
        {
        case InstructionSet::ARM64:
            download.packagePlatform = "windows_arm64";
            download.url             = "https://github.com/llvm/llvm-project/releases/download/llvmorg-20.1.8/"
                                       "clang%2Bllvm-20.1.8-aarch64-pc-windows-msvc.tar.xz";
            download.expectedHash    = "0df3e81e8fe26370dd2b60b9e009d81cd130d3fdc41b257434aa663c5d9f0c13";
            package.packageBaseName  = "clang+llvm-20.1.8-aarch64-pc-windows-msvc.tar.xz";
            clangFormatPathInArchive = "clang+llvm-20.1.8-aarch64-pc-windows-msvc/bin/clang-format.exe";
            break;
        case InstructionSet::Intel64:
            download.packagePlatform = "windows_intel64";
            download.url             = "https://github.com/llvm/llvm-project/releases/download/llvmorg-20.1.8/"
                                       "clang%2Bllvm-20.1.8-x86_64-pc-windows-msvc.tar.xz";
            download.expectedHash    = "f229769f11d6a6edc8ada599c0cda964b7dee6ab1a08c6cf9dd7f513e85b107f";
            package.packageBaseName  = "clang+llvm-20.1.8-x86_64-pc-windows-msvc.tar.xz";
            clangFormatPathInArchive = "clang+llvm-20.1.8-x86_64-pc-windows-msvc/bin/clang-format.exe";
            break;
        case InstructionSet::Intel32: return Result::Error("Unsupported platform");
        }
        break;
    }
    case Platform::Emscripten: return Result::Error("Unsupported platform");
    }

    functions.extractFunction = [&clangFormatPathInArchive](StringView sourceFile, StringView destinationDirectory)
    { return tarExpandSingleFileTo(sourceFile, destinationDirectory, clangFormatPathInArchive, 1); };

    // To verify the successful extraction we try to format some stdin with clang-format
    functions.testFunction = [&wantedVersion](const Download&, const Package& package)
    {
        String  result;
        String  formatExecutable;
        Process process;
        formatExecutable = format("{}/bin/clang-format", package.installDirectoryLink);
        SC_TRY(process.exec({formatExecutable.view(), "--version"}, result));
        SC_TRY_MSG(process.getExitStatus() == 0, "clang-format returned error");
        return clangFormatMatchesVersion(result.view(), wantedVersion);
    };
    SC_TRY(packageInstall(download, package, functions));
    return Result(true);
}

Result installLLVMMingwToolchain(StringView packagesCacheDirectory, StringView packagesInstallDirectory,
                                 Package& package)
{
    static constexpr StringView packageVersion = "20260324";
    static constexpr StringView llvmVersion    = "22.1.2";

    CustomFunctions functions;

    Download download;
    download.packagesCacheDirectory   = packagesCacheDirectory;
    download.packagesInstallDirectory = packagesInstallDirectory;
    download.packageName              = "llvm-mingw";
    download.packageVersion           = packageVersion;
    download.hashType                 = Hashing::TypeSHA256;

    switch (HostPlatform)
    {
    case Platform::Apple:
        download.packagePlatform = "macos_universal";
        download.url             = "https://github.com/mstorsjo/llvm-mingw/releases/download/20260324/"
                                   "llvm-mingw-20260324-ucrt-macos-universal.tar.xz";
        download.expectedHash    = "1834ad45eb1a26c8bf3aa6137bc79db12fa1ef368af3ed0bbfba7c60adbe2fa6";
        package.packageBaseName  = "llvm-mingw-20260324-ucrt-macos-universal.tar.xz";
        break;
    case Platform::Linux:
        switch (HostInstructionSet)
        {
        case InstructionSet::ARM64:
            download.packagePlatform = "linux_arm64";
            download.url             = "https://github.com/mstorsjo/llvm-mingw/releases/download/20260324/"
                                       "llvm-mingw-20260324-ucrt-ubuntu-22.04-aarch64.tar.xz";
            download.expectedHash    = "d28db713552e9d92699081b573a5b7c543d1d8095ed0d1c15dba184bf6e51440";
            package.packageBaseName  = "llvm-mingw-20260324-ucrt-ubuntu-22.04-aarch64.tar.xz";
            break;
        case InstructionSet::Intel64:
            download.packagePlatform = "linux_intel64";
            download.url             = "https://github.com/mstorsjo/llvm-mingw/releases/download/20260324/"
                                       "llvm-mingw-20260324-ucrt-ubuntu-22.04-x86_64.tar.xz";
            download.expectedHash    = "f92b02c4f835470deb5ac5fb92ddb458239e80ddff9ce8867155679ee5f57ffc";
            package.packageBaseName  = "llvm-mingw-20260324-ucrt-ubuntu-22.04-x86_64.tar.xz";
            break;
        case InstructionSet::Intel32: return Result::Error("Unsupported platform");
        }
        break;
    case Platform::Windows: return Result::Error("Automatic llvm-mingw install is not supported on Windows hosts yet");
    case Platform::Emscripten: return Result::Error("Unsupported platform");
    }

    functions.extractFunction = [](StringView sourceFile, StringView destinationDirectory)
    { return extractTarArchiveFlatteningRoot(sourceFile, destinationDirectory); };

    functions.testFunction = [](const Download&, const Package& package)
    {
        String  result;
        String  compilerExecutable;
        Process process;
        compilerExecutable = format("{}/bin/x86_64-w64-mingw32-clang++", package.installDirectoryLink);
        SC_TRY(process.exec({compilerExecutable.view(), "--version"}, result));
        SC_TRY_MSG(process.getExitStatus() == 0, "llvm-mingw compiler returned error");
        SC_TRY_MSG(StringView(result.view()).containsString("clang version"), "llvm-mingw compiler version missing");
        SC_TRY_MSG(StringView(result.view()).containsString(llvmVersion), "llvm-mingw compiler version doesn't match");
        return Result(true);
    };

    SC_TRY(packageInstall(download, package, functions));
    return Result(true);
}

Result installWineStableRunner(StringView packagesCacheDirectory, StringView packagesInstallDirectory, Package& package)
{
    static constexpr StringView packageVersion = "11.0";
    static constexpr StringView packageURL =
        "https://github.com/Gcenx/macOS_Wine_builds/releases/download/11.0/wine-stable-11.0-osx64.tar.xz";
    static constexpr StringView packageHash = "573d43fc4618521148d98ad9c74e63387831827395c014925fdfdc52fe55cb5a";

    switch (HostPlatform)
    {
    case Platform::Apple: break;
    case Platform::Linux:
    case Platform::Windows:
    case Platform::Emscripten: return Result::Error("Automatic Wine install is only supported on macOS hosts yet");
    }

    Download download;
    download.packagesCacheDirectory   = packagesCacheDirectory;
    download.packagesInstallDirectory = packagesInstallDirectory;
    download.packageName              = "wine-stable";
    download.packageVersion           = packageVersion;
    download.packagePlatform          = "macos_universal";
    download.url                      = packageURL;
    download.expectedHash             = packageHash;
    download.hashType                 = Hashing::TypeSHA256;
    package.packageBaseName           = "wine-stable-11.0-osx64.tar.xz";

    CustomFunctions functions;
    functions.testFunction = [](const Download&, const Package& package)
    {
        String executable = StringEncoding::Utf8;
        String version    = StringEncoding::Utf8;
        SC_TRY(StringBuilder::format(executable, "{}/Wine Stable.app/Contents/Resources/wine/bin/wine",
                                     package.installDirectoryLink));

        Process process;
        SC_TRY(process.exec({executable.view(), "--version"}, version));
        SC_TRY_MSG(process.getExitStatus() == 0, "Wine runner returned error");
        SC_TRY_MSG(StringView(version.view()).containsString("wine-11.0"), "Wine runner version doesn't match");
        return Result(true);
    };

    SC_TRY(packageInstall(download, package, functions));
    return Result(true);
}

Result installMSVCToolchain(StringView packagesCacheDirectory, StringView packagesInstallDirectory, Package& package,
                            StringView importDirectory, StringView wineExecutableOverride)
{
    package.packageFullName       = format("msvc-portable-{}", HostPlatform == Platform::Apple   ? "macos"
                                                               : HostPlatform == Platform::Linux ? "linux"
                                                                                                 : "host");
    package.packageLocalDirectory = format("{}/msvc/portable-x64", packagesCacheDirectory);
    package.packageLocalTxt       = format("{}/msvc/portable-x64.txt", packagesCacheDirectory);
    package.installDirectoryLink  = format("{}/msvc_{}", packagesInstallDirectory,
                                          HostPlatform == Platform::Apple   ? "macos"
                                           : HostPlatform == Platform::Linux ? "linux"
                                                                             : "host");

    String resolvedImportDirectory = StringEncoding::Utf8;
    if (not importDirectory.isEmpty())
    {
        SC_TRY(resolvedImportDirectory.assign(importDirectory));
    }
    else
    {
        bool hasImportedDirectory = false;
        SC_TRY(readEnvironmentVariable("SC_MSVC_IMPORT_DIRECTORY", resolvedImportDirectory, hasImportedDirectory));
    }

    String resolvedWine = StringEncoding::Utf8;
    SC_TRY(resolveMSVCWineExecutable(packagesCacheDirectory, packagesInstallDirectory, wineExecutableOverride,
                                     resolvedWine));

    FileSystem fs;
    SC_TRY(fs.init("."));
    SC_TRY(fs.makeDirectoryRecursive(packagesCacheDirectory));
    SC_TRY(fs.makeDirectoryRecursive(packagesInstallDirectory));

    bool needsInstall = true;
    if (fs.existsAndIsDirectory(package.packageLocalDirectory.view()))
    {
        SC_TRY(finalizeInstalledPackage(package));
        String packageWinePrefix = StringEncoding::Utf8;
        SC_TRY(Path::join(packageWinePrefix, {package.installDirectoryLink.view(), ".wine-prefix"}));
        if (fs.existsAndIsDirectory(packageWinePrefix.view()))
        {
            SC_TRY(prepareMSVCWinePrefixHeadless(resolvedWine.view(), packageWinePrefix.view()));
        }
        if (testMSVCToolchain(package))
        {
            needsInstall = false;
        }
    }

    if (needsInstall)
    {
        if (fs.existsAndIsDirectory(package.packageLocalDirectory.view()))
        {
            SC_TRY(fs.removeDirectoriesRecursive(package.packageLocalDirectory.view()));
        }

        if (not resolvedImportDirectory.isEmpty())
        {
            SC_TRY(fs.makeDirectoryRecursive(Path::dirname(package.packageLocalDirectory.view(), Path::AsNative)));
            Process copyProcess;
            SC_TRY(
                copyProcess.exec({"cp", "-R", resolvedImportDirectory.view(), package.packageLocalDirectory.view()}));
            SC_TRY_MSG(copyProcess.getExitStatus() == 0, "Failed copying imported MSVC toolchain");
            SC_TRY(writeMSVCPackageMetadata(package.packageLocalDirectory.view(), resolvedWine.view()));
        }
        else
        {
            String downloaderScript = StringEncoding::Utf8;
            SC_TRY(resolveToolSupportPath("PortableMSVCDownloader.py", downloaderScript));

            String downloadCache = StringEncoding::Utf8;
            String winePrefix    = StringEncoding::Utf8;
            SC_TRY(Path::join(downloadCache, {packagesCacheDirectory, "msvc", "downloads"}));
            SC_TRY(Path::join(winePrefix, {packagesCacheDirectory, "msvc", "wine-prefix"}));
            SC_TRY(fs.makeDirectoryRecursive(downloadCache.view()));
            SC_TRY(fs.makeDirectoryRecursive(winePrefix.view()));

            Process process;
            SC_TRY(process.exec({"python3", downloaderScript.view(), "--dest", package.packageLocalDirectory.view(),
                                 "--cache-dir", downloadCache.view(), "--wine", resolvedWine.view(), "--wine-prefix",
                                 winePrefix.view(), "--accept-license"}));
            SC_TRY_MSG(process.getExitStatus() == 0, "Portable MSVC download failed");

            String packageWinePrefix = StringEncoding::Utf8;
            SC_TRY(Path::join(packageWinePrefix, {package.packageLocalDirectory.view(), ".wine-prefix"}));
            if (fs.existsAndIsDirectory(packageWinePrefix.view()))
            {
                SC_TRY(fs.removeDirectoriesRecursive(packageWinePrefix.view()));
            }
            Process copyPrefixProcess;
            SC_TRY(copyPrefixProcess.exec({"cp", "-R", winePrefix.view(), packageWinePrefix.view()}));
            SC_TRY_MSG(copyPrefixProcess.getExitStatus() == 0, "Failed seeding portable MSVC Wine prefix");
        }

        SC_TRY(writeMSVCWrapperScripts(package.packageLocalDirectory.view()));
        SC_TRY(finalizeInstalledPackage(package));
        String packageWinePrefix = StringEncoding::Utf8;
        SC_TRY(Path::join(packageWinePrefix, {package.installDirectoryLink.view(), ".wine-prefix"}));
        if (fs.existsAndIsDirectory(packageWinePrefix.view()))
        {
            SC_TRY(prepareMSVCWinePrefixHeadless(resolvedWine.view(), packageWinePrefix.view()));
        }
        SC_TRY(testMSVCToolchain(package));

        String packageTxt = StringEncoding::Utf8;
        auto   builder    = StringBuilder::create(packageTxt);
        if (not resolvedImportDirectory.isEmpty())
        {
            SC_TRY(builder.append("SC_PACKAGE_URL=import:{}\n", resolvedImportDirectory.view()));
        }
        else
        {
            SC_TRY(builder.append("SC_PACKAGE_URL=https://aka.ms/vs/17/release/channel\n"));
        }
        SC_TRY(builder.append("SC_PACKAGE_WINE={}\n", resolvedWine.view()));
        builder.finalize();
        SC_TRY(fs.makeDirectoryRecursive(Path::dirname(package.packageLocalTxt.view(), Path::AsNative)));
        SC_TRY(fs.writeString(package.packageLocalTxt.view(), packageTxt.view()));
    }

    return Result(true);
}

Result runPackageTool(Tool::Arguments& arguments, Tools::Package* package)
{
    Console& console = arguments.console;

    SmallStringNative<256> packagesCacheDirectory;
    SmallStringNative<256> packagesInstallDirectory;
    SmallStringNative<256> buffer;

    auto builder = StringBuilder::create(buffer);
    SC_TRY(Path::join(packagesCacheDirectory, {arguments.toolDestination.view(), PackagesCacheDirectory}));
    SC_TRY(Path::join(packagesInstallDirectory, {arguments.toolDestination.view(), PackagesInstallDirectory}));
    SC_TRY(builder.append("packagesCache    = \"{}\"\n", packagesCacheDirectory.view()));
    SC_TRY(builder.append("packages         = \"{}\"", packagesInstallDirectory.view()));
    builder.finalize();
    console.printLine(buffer.view());

    Tools::Package clangPackage;
    if (package == nullptr)
    {
        package = &clangPackage;
    }
    if (arguments.action == "install")
    {
        StringView packageName = arguments.arguments.sizeInElements() > 0 ? arguments.arguments[0] : "clang";
        if (packageName == "doxygen")
        {
            SC_TRY(Tools::installDoxygen(packagesCacheDirectory.view(), packagesInstallDirectory.view(), *package));
        }
        else if (packageName == "doxygen-awesome-css")
        {
            SC_TRY(Tools::installDoxygenAwesomeCss(packagesCacheDirectory.view(), packagesInstallDirectory.view(),
                                                   *package));
        }
        else if (packageName == "clang")
        {
            SC_TRY(
                Tools::installClangBinaries(packagesCacheDirectory.view(), packagesInstallDirectory.view(), *package));
        }
        else if (packageName == "llvm-mingw")
        {
            SC_TRY(Tools::installLLVMMingwToolchain(packagesCacheDirectory.view(), packagesInstallDirectory.view(),
                                                    *package));
        }
        else if (packageName == "wine")
        {
            SC_TRY(Tools::installWineStableRunner(packagesCacheDirectory.view(), packagesInstallDirectory.view(),
                                                  *package));
        }
        else if (packageName == "msvc")
        {
            MSVCPackageInstallOptions options;
            SC_TRY(parseMSVCPackageInstallOptions(arguments.arguments, options));
            SC_TRY(Tools::installMSVCToolchain(packagesCacheDirectory.view(), packagesInstallDirectory.view(), *package,
                                               options.importDirectory, options.wineExecutable));
        }
        else
        {
            return Result::Error("Invalid package name");
        }
    }
    else
    {
        SC_TRY(StringBuilder::format(buffer, "SC-package no action named \"{}\" exists", arguments.action));
        console.printLine(buffer.view());
        return Result::Error("SC-package error executing action");
    }
    return Result(true);
}
#if !defined(SC_TOOLS_COMPILED_SEPARATELY) && !defined(SC_TOOLS_IMPORT)
StringView Tool::getToolName() { return "SC-package"; }
StringView Tool::getDefaultAction() { return "install"; }
Result     Tool::runTool(Tool::Arguments& arguments) { return runPackageTool(arguments); }
#endif
} // namespace Tools
} // namespace SC
