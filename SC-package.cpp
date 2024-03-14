// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT

#include "Tools/SC-package.h"

namespace SC
{
namespace Tools
{
// 7zr.exe is needed to extract 7zip installer on windows
[[nodiscard]] inline Result install7ZipR(StringView SC_PackagesDirectory, StringView SC_TOOLS_DIR, Package& package)
{
    Download download;
    download.packagesRootDirectory = SC_PackagesDirectory;
    download.hostToolsDirectory    = SC_TOOLS_DIR;

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

[[nodiscard]] inline Result install7Zip(StringView SC_PackagesDirectory, StringView SC_TOOLS_DIR, Package& package)
{
    Package         sevenZipRPackage;
    CustomFunctions functions;

    Download download;
    download.packagesRootDirectory = SC_PackagesDirectory;
    download.hostToolsDirectory    = SC_TOOLS_DIR;

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
        SC_TRY(install7ZipR(SC_PackagesDirectory, SC_TOOLS_DIR, sevenZipRPackage));
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

[[nodiscard]] inline Result findSystemClangFormat(StringView expectedVersion, String& foundPath)
{
    // Find the version
    {
        SmallString<255> versionBuffer;
        if (not Process().exec({"clang-format-15", "--version"}, versionBuffer))
        {
            SC_TRY(Process().exec({"clang-format", "--version"}, versionBuffer));
        }
        StringViewTokenizer tokenizer(versionBuffer.view());
        SC_TRY(tokenizer.tokenizeNext({' '}));
        SC_TRY(tokenizer.tokenizeNext({' '}));
        SC_TRY(tokenizer.tokenizeNext({' '}));
        StringView version = tokenizer.component.trimAnyOf({'\n', '\r'});
        SC_TRY_MSG(version.startsWith(expectedVersion), "clang-format was not at required version");
    }

    // Find the path
    switch (HostPlatform)
    {
    case Platform::Windows: // Windows
    {
        SC_TRY(Process().exec({"where", "clang-format"}, foundPath));
        StringViewTokenizer tokenizer(foundPath.view());
        SC_TRY(tokenizer.tokenizeNext({'\n'}));
        SC_TRY(foundPath.assign(tokenizer.component));
    }
    break;
    default: // Posix
    {
        SC_TRY(Process().exec({"which", "clang-format"}, foundPath));
    }
    break;
    }
    SC_TRY(foundPath.assign(foundPath.view().trimAnyOf({'\n', '\r'})));
    return Result(true);
}

[[nodiscard]] inline Result installClangBinaries(StringView SC_PackagesDirectory, StringView SC_TOOLS_DIR,
                                                 Package& package)
{
    Package         sevenZipPackage;
    CustomFunctions functions;
    functions.extractFunction = [](StringView sourceFile, StringView destinationDirectory)
    { return tarExpandTo(sourceFile, destinationDirectory, 1); };

    Download download;
    download.packagesRootDirectory = SC_PackagesDirectory;
    download.hostToolsDirectory    = SC_TOOLS_DIR;

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
        SC_TRY(install7Zip(SC_PackagesDirectory, SC_TOOLS_DIR, sevenZipPackage));
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

} // namespace Tools
constexpr StringView PackagesDirectory = "_Packages";
constexpr StringView ToolsDirectory    = "_Tools";

} // namespace SC

#if !defined(SC_TOOLS_IMPORT)
namespace SC
{
Result runPackagesCommand(ToolsArguments& arguments)
{
    Console& console = arguments.console;

    StringView action = "install"; // If no action is passed we assume "install"
    if (arguments.argc > 0)
    {
        action = StringView::fromNullTerminated(arguments.argv[0], StringEncoding::Ascii);
    }

    StringNative<256> packagesDirectory;
    StringNative<256> toolsDirectory;
    StringNative<256> buffer;
    StringBuilder     builder(buffer);
    SC_TRY(Path::join(packagesDirectory, {arguments.outputsDirectory, PackagesDirectory}));
    SC_TRY(Path::join(toolsDirectory, {arguments.outputsDirectory, ToolsDirectory}));
    SC_TRY(builder.append("sourcesDirectory  = \"{}\"\n", arguments.sourcesDirectory));
    SC_TRY(builder.append("packagesDirectory = \"{}\"\n", packagesDirectory.view()));
    SC_TRY(builder.append("toolsDirectory    = \"{}\"", toolsDirectory.view()));
    console.printLine(buffer.view());

    SC::Time::Absolute started = SC::Time::Absolute::now();
    SC_TRY(builder.format("SC-format \"{}\" started...", action));
    console.printLine(buffer.view());
    Tools::Package clangPackage;
    if (action == "install")
    {
        // Just install dependencies without formatting
        SC_TRY(Tools::installClangBinaries(packagesDirectory.view(), toolsDirectory.view(), clangPackage));
    }
    else
    {
        SC_TRY(builder.format("SC-format no action named \"{}\" exists", action));
        console.printLine(buffer.view());
        return Result::Error("SC-format error executing action");
    }

    Time::Relative elapsed = SC::Time::Absolute::now().subtract(started);
    SC_TRY(builder.format("SC-format \"{}\" finished (took {} ms)", action, elapsed.inRoundedUpperMilliseconds().ms));
    console.printLine(buffer.view());
    return Result(true);
}
#if !defined(SC_LIBRARY_PATH)
Result RunCommand(ToolsArguments& arguments) { return runPackagesCommand(arguments); }
#endif
} // namespace SC
#endif
