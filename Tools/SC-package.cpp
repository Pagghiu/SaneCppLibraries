// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT

#include "SC-package.h"
#include "../Libraries/Memory/String.h"
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
