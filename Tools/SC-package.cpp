// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT

#include "SC-package.h"
#include "../Libraries/Strings/String.h"
namespace SC
{
namespace Tools
{
[[nodiscard]] Result installDoxygen(StringView packagesCacheDirectory, StringView packagesInstallDirectory,
                                    Package& package)
{
    switch (HostPlatform)
    {
    case Platform::Apple: break;
    default: return Result::Error("installDoxygen: Unsupported platform");
    }

    static constexpr StringView packageVersion = "1.9.2";
    static constexpr StringView testVersion    = "1.9.2 (caa4e3de211fbbef2c3adf58a6bd4c86d0eb7cb8*)\n";
    static constexpr StringView baseURL        = "https://master.dl.sourceforge.net/project/doxygen";

    Download download;
    download.packagesCacheDirectory   = packagesCacheDirectory;
    download.packagesInstallDirectory = packagesInstallDirectory;

    download.packageName     = "doxygen";
    download.packageVersion  = packageVersion;
    download.packagePlatform = "macOS";

    download.url     = format("{0}/rel-{1}/Doxygen-{1}.dmg?viasf=1", baseURL, download.packageVersion);
    download.fileMD5 = "dbf10cfda8f5128ce7d2b2fc1fa1ce1f";

    package.packageBaseName = format("Doxygen-{0}.dmg", download.packageVersion);

    CustomFunctions functions;
    functions.extractFunction = [](StringView fileName, StringView directory) -> Result
    {
        String mountPoint = format("/Volumes/Doxygen-{0}", packageVersion);
        SC_TRY(Process().exec({"hdiutil", "attach", "-nobrowse", "-readonly", "-noverify", "-noautoopen", "-mountpoint",
                               mountPoint.view(), fileName}));
        FileSystem fs;
        SC_TRY(fs.init(directory));
        String fileToCopy = format("/Volumes/Doxygen-{0}/Doxygen.app/Contents/Resources/doxygen", packageVersion);
        SC_TRY(fs.copyFile(fileToCopy.view(), "doxygen", FileSystem::CopyFlags().setOverwrite(true)));
        SC_TRY(Process().exec({"hdiutil", "detach", mountPoint.view()}));
        return Result(true);
    };
    functions.testFunction = [](const Download& download, const Package& package)
    {
        SC_COMPILER_UNUSED(download);
        String result;
        String path = format("{0}/doxygen", package.installDirectoryLink);
        SC_TRY(Process().exec({path.view(), "-v"}, result));
        return Result(result == testVersion);
    };
    SC_TRY(packageInstall(download, package, functions));
    return Result(true);
}

[[nodiscard]] Result installDoxygenAwesomeCss(StringView packagesCacheDirectory, StringView packagesInstallDirectory,
                                              Package& package)
{

    static constexpr StringView packageVersion = "v2.2.1";

    Download download;
    download.packagesCacheDirectory   = packagesCacheDirectory;
    download.packagesInstallDirectory = packagesInstallDirectory;

    download.packageName     = "doxygen-awesome-css";
    download.packageVersion  = packageVersion;
    download.packagePlatform = "all";
    download.url             = "https://github.com/jothepro/doxygen-awesome-css.git";
    download.isGitClone      = true;

    CustomFunctions functions;
    functions.testFunction = [](const Download& download, const Package& package)
    {
        String  result;
        Process process;
        SC_TRY(process.setWorkingDirectory(package.installDirectoryLink.view()));
        SC_TRY(process.exec(
            {
                "git",
                "describe",
                "--tags",
            },
            result));
        return Result(result.view().trimAnyOf({'\n'}) == download.packageVersion.view());
    };
    SC_TRY(packageInstall(download, package, functions));
    return Result(true);
}

// 7zr.exe is needed to extract 7zip installer on windows
[[nodiscard]] Result install7ZipR(StringView packagesCacheDirectory, StringView packagesInstallDirectory,
                                  Package& package)
{
    Download download;
    download.packagesCacheDirectory   = packagesCacheDirectory;
    download.packagesInstallDirectory = packagesInstallDirectory;

    download.packageName    = "7zip";
    download.packageVersion = "23.01";

    download.packagePlatform = "windows";
    download.url             = "https://www.7-zip.org/a/7zr.exe";
    download.fileMD5         = "58fc6de6c4e5d2fda63565d54feb9e75";
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

[[nodiscard]] Result install7Zip(StringView packagesCacheDirectory, StringView packagesInstallDirectory,
                                 Package& package)
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
        SC_TRY(install7ZipR(packagesCacheDirectory, packagesInstallDirectory, sevenZipRPackage));
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
            SC_TRY(StringBuilder(outputDirectory).format("-o\"{0}\"", directory));
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

[[nodiscard]] Result findSystemClangFormat(Console& console, StringView wantedMajorVersion, String& foundPath)
{
    StringView       clangFormatExecutable;
    SmallString<255> version;
    switch (HostPlatform)
    {
    case Platform::Apple: {
        SmallString<32> llvmVersion;
        (void)StringBuilder(llvmVersion).format("llvm@{}", wantedMajorVersion);
        if (Process().exec({"brew", "--prefix", llvmVersion.view()}, foundPath))
        {
            (void)foundPath.assign(foundPath.view().trimEndAnyOf('\n'));
            if (StringBuilder(foundPath, StringBuilder::DoNotClear).append("/bin/clang-format"))
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
        SC_TRY(StringBuilder(clangFormatVersion).format("clang-format-{}", wantedMajorVersion));
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
        SC_TRY(foundPath.assign(foundPath.view().trimAnyOf({'\n', '\r'})));
    }
    console.print("Found \"");
    console.print(foundPath.view());
    console.print("\" ");
    console.print(version.view());
    StringViewTokenizer tokenizer(version.view());
    SC_TRY(tokenizer.tokenizeNext({'-'})); // component = "clang-"
    SC_TRY(tokenizer.tokenizeNext({' '})); // component = "format"
    SC_TRY(tokenizer.tokenizeNext({' '})); // component = "version"
    SC_TRY(tokenizer.tokenizeNext({' '})); // component = "15.0.7\n"
    tokenizer = StringViewTokenizer(tokenizer.component.trimAnyOf({'\n', '\r'}));
    SC_TRY(tokenizer.tokenizeNext({'.'}));
    SC_TRY_MSG(tokenizer.component == wantedMajorVersion, "clang-format was not at required major version");
    return Result(true);
}

[[nodiscard]] Result installClangBinaries(StringView packagesCacheDirectory, StringView packagesInstallDirectory,
                                          Package& package)
{
    Package         sevenZipPackage;
    CustomFunctions functions;
    functions.extractFunction = [](StringView sourceFile, StringView destinationDirectory)
    { return tarExpandTo(sourceFile, destinationDirectory, 1); };

    Download download;
    download.packagesCacheDirectory   = packagesCacheDirectory;
    download.packagesInstallDirectory = packagesInstallDirectory;

    download.packageName    = "clang-binaries";
    download.packageVersion = "23.01";

    switch (HostPlatform)
    {
    case Platform::Apple: {
        switch (HostInstructionSet)
        {
        case InstructionSet::ARM64:

            download.packagePlatform = "macos_arm64";
            download.url             = "https://github.com/llvm/llvm-project/releases/download/llvmorg-15.0.7/"
                                       "clang+llvm-15.0.7-arm64-apple-darwin22.0.tar.xz";
            download.fileMD5         = "b822d9e4689bd8ed7f19eacec8143dc3";
            break;
        case InstructionSet::Intel64:
            download.packagePlatform = "macos_intel64";
            download.url             = "https://github.com/llvm/llvm-project/releases/download/llvmorg-15.0.7/"
                                       "clang+llvm-15.0.7-x86_64-apple-darwin21.0.tar.xz";
            download.fileMD5         = "a9ea8150a82f2627cac5b7719e7ba7ff";
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
            download.url             = "https://github.com/llvm/llvm-project/releases/download/llvmorg-15.0.6/"
                                       "clang+llvm-15.0.6-aarch64-linux-gnu.tar.xz";
            download.fileMD5         = "50a5bf00744ea7c4951fba14a381ad3e";
            break;
        case InstructionSet::Intel64:
            download.packagePlatform = "linux_intel64";
            download.url             = "https://github.com/llvm/llvm-project/releases/download/llvmorg-15.0.6/"
                                       "clang+llvm-15.0.6-x86_64-linux-gnu-ubuntu-18.04.tar.xz";
            download.fileMD5         = "a48464533ddabc180d830df7e13e82ae";
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
        functions.extractFunction = [&sevenZipPackage](StringView fileName, StringView directory) -> Result
        {
            Process          process;
            SmallString<255> outputDirectory;
            SC_TRY(StringBuilder(outputDirectory).format("-o\"{}\"", directory));
            SmallString<255> toolFile;
            SC_TRY(StringBuilder(toolFile).format("{}/7z.exe", sevenZipPackage.installDirectoryLink));
            SC_TRY(process.exec({toolFile.view(), "x", fileName, outputDirectory.view()}));
            return Result(process.getExitStatus() == 0);
        };

        switch (HostInstructionSet)
        {
        case InstructionSet::ARM64:
            download.packagePlatform = "windows_arm64";
            download.url =
                "https://github.com/llvm/llvm-project/releases/download/llvmorg-15.0.6/LLVM-15.0.6-woa64.exe";
            download.fileMD5 = "cb44a9d9646cdbfb42f2eec1c7dbe16b";
            break;
        case InstructionSet::Intel64:
            download.packagePlatform = "windows_intel64";
            download.url =
                "https://github.com/llvm/llvm-project/releases/download/llvmorg-15.0.6/LLVM-15.0.6-win64.exe";
            download.fileMD5 = "61cb3189c02e1582d1703ab1351adb0f";
            break;
        case InstructionSet::Intel32:
            download.packagePlatform = "windows_intel32";
            download.url =
                "https://github.com/llvm/llvm-project/releases/download/llvmorg-16.0.5/LLVM-16.0.5-win32.exe";
            download.fileMD5 = "c1a4c346c7c445c263554f954bba62dd";
            break;
        }
    }
    break;
    case Platform::Emscripten: {
        return Result::Error("Unsupported platform");
    }
    }

    // To verify the successful extraction we try to format some stdin with clang-format
    functions.testFunction = [](const Download&, const Package& package)
    {
        String  formatExecutable = format("{}/bin/clang-format", package.installDirectoryLink);
        Process process;
        String  result;
        SC_TRY(process.exec({formatExecutable.view()}, result, "int    asd=0;"));
        SC_TRY_MSG(result == "int asd = 0;", "clang-format doesn't work");
        return Result(process.getExitStatus() == 0);
    };
    SC_TRY(packageInstall(download, package, functions));
    return Result(true);
}

constexpr StringView PackagesCacheDirectory   = "_PackagesCache";
constexpr StringView PackagesInstallDirectory = "_Packages";

Result runPackageTool(Tool::Arguments& arguments, Tools::Package* package)
{
    Console& console = arguments.console;

    StringNative<256> packagesCacheDirectory;
    StringNative<256> packagesInstallDirectory;
    StringNative<256> buffer;
    StringBuilder     builder(buffer);
    SC_TRY(Path::join(packagesCacheDirectory, {arguments.outputsDirectory.view(), PackagesCacheDirectory}));
    SC_TRY(Path::join(packagesInstallDirectory, {arguments.outputsDirectory.view(), PackagesInstallDirectory}));
    SC_TRY(builder.append("packagesCache    = \"{}\"\n", packagesCacheDirectory.view()));
    SC_TRY(builder.append("packages         = \"{}\"", packagesInstallDirectory.view()));
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
        else
        {
            return Result::Error("Invalid package name");
        }
    }
    else
    {
        SC_TRY(builder.format("SC-format no action named \"{}\" exists", arguments.action));
        console.printLine(buffer.view());
        return Result::Error("SC-format error executing action");
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
