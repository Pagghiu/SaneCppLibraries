// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT

#include "SC-package.h"
#include "../Libraries/Memory/String.h"
namespace SC
{
namespace Tools
{
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
        download.fileMD5          = "354ee835cf03e8a0187460a1456eb108";
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
        download.fileMD5          = "fd96a5defa535dfe2e987b46540844a4";
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
        download.fileMD5         = "d014a212331693ffcf72ad99b2087ea0";
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

// 7zr.exe is needed to extract 7zip installer on windows
Result install7ZipR(StringView packagesCacheDirectory, StringView packagesInstallDirectory, Package& package)
{
    Download download;
    download.packagesCacheDirectory   = packagesCacheDirectory;
    download.packagesInstallDirectory = packagesInstallDirectory;

    download.packageName    = "7zip";
    download.packageVersion = "25.01";

    download.packagePlatform = "windows";
    download.url             = "https://www.7-zip.org/a/7zr.exe";
    download.fileMD5         = "890595b9f1fcbd6b627386335e96251b";
    download.createLink      = false;

    CustomFunctions functions;
    functions.extractFunction = [](StringView, StringView) -> Result { return Result(true); };
    functions.testFunction    = [](const Download& download, const Package& package)
    {
        String result;
        SC_TRY(Process().exec({package.packageLocalFile.view()}, result));
        StringViewTokenizer tokenizer(result.view());
        SC_TRY(tokenizer.tokenizeNext({':'}));
        tokenizer = StringViewTokenizer(tokenizer.component);
        SC_TRY(tokenizer.tokenizeNext({')'}));
        SC_TRY(tokenizer.tokenizeNext({'('}));
        StringView version = tokenizer.component.trimAnyOf({' '});
        SC_TRY_MSG(version == download.packageVersion.view(), "7zip doesn't work");
        return Result(true);
    };

    SC_TRY(packageInstall(download, package, functions));
    return Result(true);
}

Result install7Zip(StringView packagesCacheDirectory, StringView packagesInstallDirectory, Package& package)
{
    Package         sevenZipRPackage;
    CustomFunctions functions;

    Download download;
    download.packagesCacheDirectory   = packagesCacheDirectory;
    download.packagesInstallDirectory = packagesInstallDirectory;

    download.packageName    = "7zip";
    download.packageVersion = "23.01";

    switch (HostPlatform)
    {
    case Platform::Apple: {
        download.packagePlatform = "macos";
        download.url             = "https://www.7-zip.org/a/7z2301-mac.tar.xz";
        download.fileMD5         = "2a7461a5c41e5e3ee3138652ed2739b6";
    }
    break;
    case Platform::Windows: {
        Result res = install7ZipR(packagesCacheDirectory, packagesInstallDirectory, sevenZipRPackage);
        SC_TRY_MSG(res, "7zr install has failed (check if its hash must be updated)");
        switch (HostInstructionSet)
        {
        case InstructionSet::ARM64:
            download.packagePlatform = "windows_arm64";
            download.url             = "https://www.7-zip.org/a/7z2301-arm64.exe";
            download.fileMD5         = "3c5917f4da614ef892f055c697744b77";
            break;
        case InstructionSet::Intel64:
            download.packagePlatform = "windows_intel64";
            download.url             = "https://www.7-zip.org/a/7z2301-x64.exe";
            download.fileMD5         = "e5788b13546156281bf0a4b38bdd0901";
            break;
        case InstructionSet::Intel32:
            download.packagePlatform = "windows_intel32";
            download.url             = "https://www.7-zip.org/a/7z2301.exe";
            download.fileMD5         = "1cfb215a6fb373ac33a38b1db320c178";
            break;
        }

        functions.extractFunction = [&sevenZipRPackage](StringView fileName, StringView directory) -> Result
        {
            Process          process;
            SmallString<255> outputDirectory;
            SC_TRY(StringBuilder::format(outputDirectory, "-o\"{0}\"", directory));
            SC_TRY(process.exec({sevenZipRPackage.packageLocalFile.view(), "e", fileName, outputDirectory.view()}));
            SC_TRY_MSG(process.getExitStatus() == 0, "Extracting 7Zip with 7ZipR failed");
            return Result(true);
        };
    }
    break;
    case Platform::Linux: {

        switch (HostInstructionSet)
        {
        case InstructionSet::ARM64:
            download.packagePlatform = "linux_arm64";
            download.url             = "https://www.7-zip.org/a/7z2301-linux-arm64.tar.xz";
            download.fileMD5         = "c95bea5eed5f35327fa0e24d90808250";
            break;
        case InstructionSet::Intel64:
            download.packagePlatform = "linux_intel64";
            download.url             = "https://www.7-zip.org/a/7z2301-linux-x64.tar.xz";
            download.fileMD5         = "e6ec894ac83a6f9d203a295d5a9079e7";
            break;
        case InstructionSet::Intel32:
            download.packagePlatform = "linux_intel32";
            download.url             = "https://www.7-zip.org/a/7z2301-linux-x86.tar.xz";
            download.fileMD5         = "b97fc1f37eb3f514794c35df683e9f18";
            break;
        }
    }
    break;
    case Platform::Emscripten: {
        return Result::Error("Unsupported platform");
    }
    }

    functions.testFunction = [](const Download& download, const Package& package)
    {
        String formatExecutable = HostPlatform == Platform::Windows ? format("{}/7z.exe", package.installDirectoryLink)
                                                                    : format("{}/7zz", package.installDirectoryLink);
        String result;
        SC_TRY(Process().exec({formatExecutable.view()}, result));
        StringViewTokenizer tokenizer(result.view());
        switch (HostPlatform)
        {
        case Platform::Windows: {
            SC_TRY(tokenizer.tokenizeNext({' '}));
            SC_TRY(tokenizer.tokenizeNext({' '}));
        }
        break;
        case Platform::Apple:
        case Platform::Linux: {
            SC_TRY(tokenizer.tokenizeNext({':'}));
            tokenizer = StringViewTokenizer(tokenizer.component);
            SC_TRY(tokenizer.tokenizeNext({')'}));
            SC_TRY(tokenizer.tokenizeNext({'('}));
        }
        break;
        case Platform::Emscripten: {
            return Result::Error("Unsupported platform");
        }
        }
        StringView version = tokenizer.component.trimAnyOf({' '});
        SC_TRY_MSG(version == download.packageVersion.view(), "7zip doesn't work");
        return Result(true);
    };
    SC_TRY(packageInstall(download, package, functions));
    return Result(true);
}

Result clangFormatMatchesVersion(StringView versionString, StringView wantedMajorVersion)
{
    StringViewTokenizer tokenizer(versionString);
    SC_TRY_MSG(tokenizer.tokenizeNext({'-'}), "clang-format tokenize error"); // component = "clang-"
    SC_TRY_MSG(tokenizer.tokenizeNext({' '}), "clang-format tokenize error"); // component = "format"
    SC_TRY_MSG(tokenizer.tokenizeNext({' '}), "clang-format tokenize error"); // component = "version"
    SC_TRY_MSG(tokenizer.tokenizeNext({' '}), "clang-format tokenize error"); // component = "x.y.z\n"
    tokenizer = StringViewTokenizer(tokenizer.component.trimAnyOf({'\n', '\r'}));
    SC_TRY_MSG(tokenizer.tokenizeNext({'.'}), "clang-format tokenize error");
    SC_TRY_MSG(tokenizer.component == wantedMajorVersion, "clang-format major version doesn't match wanted one");
    return Result(true);
}

Result findSystemClangFormat(Console& console, StringView wantedMajorVersion, String& foundPath)
{
    StringView       clangFormatExecutable;
    SmallString<255> version;
    switch (HostPlatform)
    {
    case Platform::Apple: {
        SmallString<32> llvmVersion;
        (void)StringBuilder::format(llvmVersion, "llvm@{}", wantedMajorVersion);
        if (Process().exec({"brew", "--prefix", llvmVersion.view()}, foundPath))
        {
            (void)foundPath.assign(StringView(foundPath.view()).trimEndAnyOf('\n'));
            auto sb  = StringBuilder::createForAppendingTo(foundPath);
            bool res = sb.append("/bin/clang-format");
            sb.finalize();
            if (res)
            {
                if (Process().exec({foundPath.view(), "--version"}, version))
                {
                    clangFormatExecutable = foundPath.view();
                }
            }
        }
    }
    break;
    default: break;
    }

    if (clangFormatExecutable.isEmpty())
    {
        SmallString<32> clangFormatVersion;
        SC_TRY(StringBuilder::format(clangFormatVersion, "clang-format-{}", wantedMajorVersion));
        if (Process().exec({clangFormatVersion.view(), "--version"}, version))
        {
            clangFormatExecutable = clangFormatVersion.view();
        }
        else
        {
            SC_TRY(Process().exec({"clang-format", "--version"}, version));
            clangFormatExecutable = "clang-format";
        }
        // Find the path
        switch (HostPlatform)
        {
        case Platform::Windows: // Windows
        {
            SC_TRY(Process().exec({"where", clangFormatExecutable}, foundPath));
            StringViewTokenizer tokenizer(foundPath.view());
            SC_TRY(tokenizer.tokenizeNext({'\n'}));
            SC_TRY(foundPath.assign(tokenizer.component));
        }
        break;
        default: // Posix
        {
            SC_TRY(Process().exec({"which", clangFormatExecutable}, foundPath));
        }
        break;
        }
        SC_TRY(foundPath.assign(StringView(foundPath.view()).trimAnyOf({'\n', '\r'})));
    }
    console.print("Found \"");
    console.print(foundPath.view());
    console.print("\" ");
    console.print(version.view());
    return clangFormatMatchesVersion(version.view(), wantedMajorVersion);
}

Result installClangBinaries(StringView packagesCacheDirectory, StringView packagesInstallDirectory, Package& package)
{
    Package         sevenZipPackage;
    CustomFunctions functions;

    Download download;
    download.packagesCacheDirectory   = packagesCacheDirectory;
    download.packagesInstallDirectory = packagesInstallDirectory;

    download.packageName    = "clang-binaries";
    download.packageVersion = "25.04";

    StringView wantedVersion = "19";
    switch (HostPlatform)
    {
    case Platform::Apple: {
        switch (HostInstructionSet)
        {
        case InstructionSet::ARM64:
            download.packagePlatform = "macos_arm64";
            download.url             = "https://github.com/llvm/llvm-project/releases/download/llvmorg-19.1.7/"
                                       "LLVM-19.1.7-macOS-ARM64.tar.xz";
            download.fileMD5         = "6d28d32e6b74dfbc138483c145acf791";
            break;
        case InstructionSet::Intel64:
            download.packagePlatform = "macos_intel64";
            download.url             = "https://github.com/llvm/llvm-project/releases/download/llvmorg-19.1.7/"
                                       "LLVM-19.1.7-macOS-X64.tar.xz";
            download.fileMD5         = "a07342bacdaf5ec9964798ca1d8c6315";
            break;
        case InstructionSet::Intel32: {
            return Result::Error("Unsupported platform");
        }
        }
        break;
    }

    case Platform::Linux: {
        switch (HostInstructionSet)
        {
        case InstructionSet::ARM64:
            download.packagePlatform = "linux_arm64";
            download.url             = "https://github.com/llvm/llvm-project/releases/download/llvmorg-19.1.7/"
                                       "clang+llvm-19.1.7-aarch64-linux-gnu.tar.xz";
            download.fileMD5         = "f1996d9754e1e29b655475c44517401d";
            break;
        case InstructionSet::Intel64:
            download.packagePlatform = "linux_intel64";
            download.url             = "https://github.com/llvm/llvm-project/releases/download/llvmorg-19.1.7/"
                                       "LLVM-19.1.7-Linux-X64.tar.xz";
            download.fileMD5         = "1d50ec07e8b02b3edd798ae8cfded860";
            break;
        case InstructionSet::Intel32: {
            return Result::Error("Unsupported platform");
        }
        break;
        }
    }
    break;

    case Platform::Windows: {
        SC_TRY(install7Zip(packagesCacheDirectory, packagesInstallDirectory, sevenZipPackage));

        switch (HostInstructionSet)
        {
        case InstructionSet::ARM64:
            download.packagePlatform = "windows_arm64";
            download.url             = "https://github.com/llvm/llvm-project/releases/download/llvmorg-19.1.7/"
                                       "LLVM-19.1.7-woa64.exe";
            download.fileMD5         = "780795d36a58ccfee79ea74252d7741e";
            break;
        case InstructionSet::Intel64:
            download.packagePlatform = "windows_intel64";
            download.url             = "https://github.com/llvm/llvm-project/releases/download/llvmorg-19.1.7/"
                                       "LLVM-19.1.7-win64.exe";
            download.fileMD5         = "d4c4bed41b38c1427888e070f651908b";
            break;
        case InstructionSet::Intel32:
            download.packagePlatform = "windows_intel32";
            download.url             = "https://github.com/llvm/llvm-project/releases/download/llvmorg-19.1.7/"
                                       "LLVM-19.1.7-win32.exe";
            download.fileMD5         = "a710a064915752191366b3c79c71ff57";
            break;
        }
    }
    break;
    case Platform::Emscripten: {
        return Result::Error("Unsupported platform");
    }
    }
    const bool isWindows = HostPlatform == Platform::Windows; // avoids MSVC 'conditional expression is constant'
    if (isWindows)
    {
        functions.extractFunction = [&sevenZipPackage](StringView fileName, StringView directory) -> Result
        {
            Process          process;
            SmallString<255> outputDirectory;
            SC_TRY(StringBuilder::format(outputDirectory, "-o\"{}\"", directory));
            SmallString<255> toolFile;
            SC_TRY(StringBuilder::format(toolFile, "{}/7z.exe", sevenZipPackage.installDirectoryLink));
            SC_TRY(process.exec({toolFile.view(), "x", fileName, outputDirectory.view(), "bin/clang-format.exe"}));
            return Result(process.getExitStatus() == 0);
        };
    }
    else
    {
        StringView tarballFile    = Path::basename(Path::basename(download.url.view(), Path::Type::AsPosix), ".tar.xz");
        functions.extractFunction = [&tarballFile](StringView sourceFile, StringView destinationDirectory)
        {
            String clangFile = format("{}/bin/clang-format", tarballFile);
            return tarExpandSingleFileTo(sourceFile, destinationDirectory, clangFile.view(), 1);
        };
    }

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
        else if (packageName == "7zip")
        {
            SC_TRY(Tools::install7Zip(packagesCacheDirectory.view(), packagesInstallDirectory.view(), *package));
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
#if !defined(SC_LIBRARY_PATH) && !defined(SC_TOOLS_IMPORT)
StringView Tool::getToolName() { return "SC-package"; }
StringView Tool::getDefaultAction() { return "install"; }
Result     Tool::runTool(Tool::Arguments& arguments) { return runPackageTool(arguments); }
#endif
} // namespace Tools
} // namespace SC
