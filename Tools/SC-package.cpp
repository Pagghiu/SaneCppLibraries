// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT

#include "SC-package.h"
#include "../Libraries/Containers/Algorithms/AlgorithmBubbleSort.h"
#include "../Libraries/ContainersReflection/ContainersSerialization.h"
#include "../Libraries/ContainersReflection/MemorySerialization.h"
#include "../Libraries/FileSystemIterator/FileSystemIterator.h"
#include "../Libraries/Memory/String.h"
#include "../Libraries/SerializationText/SerializationJson.h"
#include "../Libraries/Time/Time.h"
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

    if (fs.existsAndIsDirectory(destinationDirectory))
    {
        SC_TRY(fs.removeDirectoryRecursive(destinationDirectory));
    }
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

static Result tryFindFirstSubdirectory(StringView directory, String& output, bool& found)
{
    found = false;

    FileSystem fs;
    SC_TRY(fs.init("."));
    if (not fs.existsAndIsDirectory(directory))
    {
        return Result(true);
    }

    SC_TRY(findFirstSubdirectory(directory, output));
    found = true;
    return Result(true);
}

static Result resolveMSVCVersions(StringView packageRoot, String& msvcVersion, String& sdkVersion)
{
    String msvcDirectory       = StringEncoding::Utf8;
    String sdkBinDirectory     = StringEncoding::Utf8;
    String sdkIncludeDirectory = StringEncoding::Utf8;
    String sdkLibDirectory     = StringEncoding::Utf8;
    SC_TRY(Path::join(msvcDirectory, {packageRoot, "VC", "Tools", "MSVC"}));
    SC_TRY(Path::join(sdkBinDirectory, {packageRoot, "Windows Kits", "10", "bin"}));
    SC_TRY(Path::join(sdkIncludeDirectory, {packageRoot, "Windows Kits", "10", "Include"}));
    SC_TRY(Path::join(sdkLibDirectory, {packageRoot, "Windows Kits", "10", "Lib"}));
    SC_TRY(findFirstSubdirectory(msvcDirectory.view(), msvcVersion));

    bool hasSDKVersion = false;
    SC_TRY(tryFindFirstSubdirectory(sdkBinDirectory.view(), sdkVersion, hasSDKVersion));
    if (not hasSDKVersion)
    {
        SC_TRY(tryFindFirstSubdirectory(sdkIncludeDirectory.view(), sdkVersion, hasSDKVersion));
    }
    if (not hasSDKVersion)
    {
        SC_TRY(tryFindFirstSubdirectory(sdkLibDirectory.view(), sdkVersion, hasSDKVersion));
    }
    SC_TRY_MSG(hasSDKVersion, "Missing Windows SDK directory");
    return Result(true);
}

static constexpr StringView portableMSVCCacheLeafName()
{
    return HostPlatform == Platform::Apple   ? StringView::fromNullTerminated("portable-macos", StringEncoding::Ascii)
           : HostPlatform == Platform::Linux ? StringView::fromNullTerminated("portable-linux", StringEncoding::Ascii)
                                             : StringView::fromNullTerminated("portable-host", StringEncoding::Ascii);
}

static constexpr StringView hostPlatformName()
{
    return HostPlatform == Platform::Apple        ? StringView::fromNullTerminated("apple", StringEncoding::Ascii)
           : HostPlatform == Platform::Linux      ? StringView::fromNullTerminated("linux", StringEncoding::Ascii)
           : HostPlatform == Platform::Windows    ? StringView::fromNullTerminated("windows", StringEncoding::Ascii)
           : HostPlatform == Platform::Emscripten ? StringView::fromNullTerminated("emscripten", StringEncoding::Ascii)
                                                  : StringView::fromNullTerminated("unknown", StringEncoding::Ascii);
}

static constexpr StringView hostInstructionSetName()
{
    return HostInstructionSet == InstructionSet::ARM64 ? StringView::fromNullTerminated("arm64", StringEncoding::Ascii)
           : HostInstructionSet == InstructionSet::Intel64
               ? StringView::fromNullTerminated("x86_64", StringEncoding::Ascii)
               : StringView::fromNullTerminated("x86", StringEncoding::Ascii);
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
    switch (HostPlatform)
    {
    case Platform::Windows: SC_TRY(process.exec({"where", executable}, commandPath)); break;
    case Platform::Apple:
    case Platform::Linux: SC_TRY(process.exec({"which", executable}, commandPath)); break;
    case Platform::Emscripten: return Result::Error("Cannot resolve host command path");
    }
    SC_TRY_MSG(process.getExitStatus() == 0, "Cannot resolve host command path");
    StringView resolvedCommand = StringView(commandPath.view()).trimWhiteSpaces();
#if SC_PLATFORM_WINDOWS
    StringViewTokenizer tokenizer(resolvedCommand);
    SC_TRY_MSG(tokenizer.tokenizeNext({'\n'}), "Cannot resolve host command path");
    resolvedCommand = tokenizer.component.trimWhiteSpaces();
#endif
    SC_TRY(output.assign(resolvedCommand));
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

static bool isArm64HostInstructionSet() { return HostInstructionSet == InstructionSet::ARM64; }
static bool supportsAutomaticLinuxSysrootInstall()
{
    return HostPlatform == Platform::Apple or HostPlatform == Platform::Linux or HostPlatform == Platform::Windows;
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

static Result extractDebArchive(StringView sourceFile, StringView destinationDirectory)
{
    FileSystem fs;
    SC_TRY(fs.init("."));

    String extractDirectory = format("{}-deb-extract", destinationDirectory);
    if (fs.existsAndIsDirectory(extractDirectory.view()))
    {
        SC_TRY(fs.removeDirectoriesRecursive(extractDirectory.view()));
    }
    SC_TRY(fs.makeDirectoryRecursive(extractDirectory.view()));

    Process process;
    SC_TRY(process.setWorkingDirectory(extractDirectory.view()));
    SC_TRY(process.exec({"ar", "-x", sourceFile}));
    SC_TRY_MSG(process.getExitStatus() == 0, "Failed extracting .deb ar archive");

    String archivePath   = StringEncoding::Utf8;
    auto   expandArchive = [&](StringView archiveName, bool& expanded) -> Result
    {
        expanded = false;
        SC_TRY(Path::join(archivePath, {extractDirectory.view(), archiveName}));
        if (not fs.existsAndIsFile(archivePath.view()))
        {
            return Result(true);
        }

        Process tarProcess;
        SC_TRY(tarProcess.exec({"tar", "-xf", archivePath.view(), "-C", destinationDirectory}));
        SC_TRY_MSG(tarProcess.getExitStatus() == 0, "Failed extracting .deb payload");
        expanded = true;
        return Result(true);
    };

    bool expanded = false;
    SC_TRY(expandArchive("data.tar.xz", expanded));
    if (not expanded)
    {
        SC_TRY(expandArchive("data.tar.zst", expanded));
    }
    if (not expanded)
    {
        SC_TRY(expandArchive("data.tar.gz", expanded));
    }
    if (not expanded)
    {
        SC_TRY(expandArchive("data.tar", expanded));
    }
    SC_TRY_MSG(expanded, "Unsupported .deb payload archive");
    SC_TRY(fs.removeDirectoriesRecursive(extractDirectory.view()));
    return Result(true);
}

static Result extractApkArchive(StringView sourceFile, StringView destinationDirectory)
{
    Process process;
    SC_TRY(process.exec({"tar", "-xzf", sourceFile, "-C", destinationDirectory}));
    SC_TRY_MSG(process.getExitStatus() == 0, "Failed extracting .apk archive");
    return Result(true);
}

static Result downloadTextFile(StringView url, StringView destinationFile)
{
    Process process;
    String  stdErr = StringEncoding::Utf8;
    SC_TRY(process.exec({"curl", "-L", "-o", destinationFile, url}, {}, {}, stdErr));
    SC_TRY_MSG(process.getExitStatus() == 0, "Failed downloading text file");
    return Result(true);
}

static Result downloadGzipTextFile(StringView url, StringView destinationFile)
{
    String compressedPath = format("{}.gz", destinationFile);
    SC_TRY(downloadTextFile(url, compressedPath.view()));

    Process process;
    String  output = StringEncoding::Utf8;
    SC_TRY(process.exec({"gzip", "-dc", compressedPath.view()}, output));
    SC_TRY_MSG(process.getExitStatus() == 0, "Failed decompressing gzip metadata");

    FileSystem fs;
    SC_TRY(fs.init("."));
    SC_TRY(fs.writeString(destinationFile, output.view()));
    return Result(true);
}

#if SC_PLATFORM_LINUX
static bool isLexicographicallyAfter(StringView candidate, StringView current)
{
    const size_t minSize =
        candidate.sizeInBytes() < current.sizeInBytes() ? candidate.sizeInBytes() : current.sizeInBytes();
    for (size_t idx = 0; idx < minSize; ++idx)
    {
        const auto lhs = candidate.bytesWithoutTerminator()[idx];
        const auto rhs = current.bytesWithoutTerminator()[idx];
        if (lhs == rhs)
        {
            continue;
        }
        return lhs > rhs;
    }
    return candidate.sizeInBytes() > current.sizeInBytes();
}
#endif

static Result extractPackageIndexField(StringView stanza, StringView fieldName, String& output)
{
    size_t searchIndex = 0;
    while (searchIndex < stanza.sizeInBytes())
    {
        size_t lineEnd = searchIndex;
        while (lineEnd < stanza.sizeInBytes() and stanza.bytesWithoutTerminator()[lineEnd] != '\n')
        {
            lineEnd++;
        }
        const StringView line = {
            {stanza.bytesWithoutTerminator() + searchIndex, lineEnd - searchIndex}, false, StringEncoding::Ascii};
        if (line.startsWith(fieldName))
        {
            SC_TRY(output.assign(line.sliceStart(fieldName.sizeInBytes()).trimWhiteSpaces()));
            return Result(true);
        }
        searchIndex = lineEnd + 1;
    }
    return Result::Error("Missing package index field");
}

struct UbuntuPackageMetadata
{
    String version  = StringEncoding::Utf8;
    String filename = StringEncoding::Utf8;
    String sha256   = StringEncoding::Utf8;
};

static Result resolveUbuntuPackageMetadata(StringView packageIndex, StringView packageName,
                                           UbuntuPackageMetadata& metadata)
{
    const StringView indexView = packageIndex;
    size_t           stanzaPos = 0;
    while (stanzaPos < indexView.sizeInBytes())
    {
        size_t stanzaEnd = stanzaPos;
        while (stanzaEnd + 1 < indexView.sizeInBytes())
        {
            if (indexView.bytesWithoutTerminator()[stanzaEnd] == '\n' and
                indexView.bytesWithoutTerminator()[stanzaEnd + 1] == '\n')
            {
                break;
            }
            stanzaEnd++;
        }

        const StringView stanza = {
            {indexView.bytesWithoutTerminator() + stanzaPos, stanzaEnd - stanzaPos}, false, StringEncoding::Ascii};
        String stanzaPackageName = StringEncoding::Utf8;
        if (extractPackageIndexField(stanza, "Package:", stanzaPackageName) and stanzaPackageName.view() == packageName)
        {
            SC_TRY(extractPackageIndexField(stanza, "Version:", metadata.version));
            SC_TRY(extractPackageIndexField(stanza, "Filename:", metadata.filename));
            SC_TRY(extractPackageIndexField(stanza, "SHA256:", metadata.sha256));
            return Result(true);
        }
        stanzaPos = stanzaEnd + 2;
    }
    return Result::Error("Cannot resolve Ubuntu package metadata");
}

struct AlpinePackageMetadata
{
    String version = StringEncoding::Utf8;
};

static Result resolveAlpinePackageMetadata(StringView packageIndex, StringView packageName,
                                           AlpinePackageMetadata& metadata)
{
    const StringView indexView = packageIndex;
    size_t           recordPos = 0;
    while (recordPos < indexView.sizeInBytes())
    {
        size_t recordEnd = recordPos;
        while (recordEnd + 1 < indexView.sizeInBytes())
        {
            if (indexView.bytesWithoutTerminator()[recordEnd] == '\n' and
                indexView.bytesWithoutTerminator()[recordEnd + 1] == '\n')
            {
                break;
            }
            recordEnd++;
        }

        const StringView record = {
            {indexView.bytesWithoutTerminator() + recordPos, recordEnd - recordPos}, false, StringEncoding::Ascii};
        StringView currentPackageName;
        StringView currentVersion;

        size_t lineStart = 0;
        while (lineStart < record.sizeInBytes())
        {
            size_t lineEnd = lineStart;
            while (lineEnd < record.sizeInBytes() and record.bytesWithoutTerminator()[lineEnd] != '\n')
            {
                lineEnd++;
            }
            const StringView line = StringView({record.bytesWithoutTerminator() + lineStart, lineEnd - lineStart},
                                               false, StringEncoding::Ascii)
                                        .trimWhiteSpaces();
            if (line.startsWith("P:"))
            {
                currentPackageName = line.sliceStart(2);
            }
            else if (line.startsWith("V:"))
            {
                currentVersion = line.sliceStart(2);
            }
            lineStart = lineEnd + 1;
        }

        if (currentPackageName == packageName)
        {
            SC_TRY(metadata.version.assign(currentVersion));
            return Result(true);
        }

        recordPos = recordEnd + 2;
    }
    return Result::Error("Cannot resolve Alpine package metadata");
}

#if SC_PLATFORM_LINUX
struct LinuxBox64PackageMetadata
{
    String version = StringEncoding::Utf8;
    String url     = StringEncoding::Utf8;
    String sha256  = StringEncoding::Utf8;
};

static Result resolveLinuxBox64PackageMetadata(StringView downloadsDirectory, LinuxBox64PackageMetadata& metadata)
{
    static constexpr StringView packageIndexURL =
        "https://raw.githubusercontent.com/Pi-Apps-Coders/box64-debs/master/debian/Packages";
    static constexpr StringView packageBaseURL =
        "https://raw.githubusercontent.com/Pi-Apps-Coders/box64-debs/master/debian";

    FileSystem fs;
    SC_TRY(fs.init("."));

    String packageIndexPath = StringEncoding::Utf8;
    SC_TRY(Path::join(packageIndexPath, {downloadsDirectory, "box64-packages-index.txt"}));
    SC_TRY(downloadTextFile(packageIndexURL, packageIndexPath.view()));

    String packageIndex = StringEncoding::Utf8;
    SC_TRY(fs.read(packageIndexPath.view(), packageIndex));

    const StringView indexView = packageIndex.view();
    size_t           stanzaPos = 0;
    while (stanzaPos < indexView.sizeInBytes())
    {
        size_t stanzaEnd = stanzaPos;
        while (stanzaEnd + 1 < indexView.sizeInBytes())
        {
            if (indexView.bytesWithoutTerminator()[stanzaEnd] == '\n' and
                indexView.bytesWithoutTerminator()[stanzaEnd + 1] == '\n')
            {
                break;
            }
            stanzaEnd++;
        }

        const StringView stanza      = indexView.sliceStartLength(stanzaPos, stanzaEnd - stanzaPos);
        String           packageName = StringEncoding::Utf8;
        if (extractPackageIndexField(stanza, "Package:", packageName) and packageName.view() == "box64-generic-arm")
        {
            String version  = StringEncoding::Utf8;
            String filename = StringEncoding::Utf8;
            String sha256   = StringEncoding::Utf8;
            SC_TRY(extractPackageIndexField(stanza, "Version:", version));
            SC_TRY(extractPackageIndexField(stanza, "Filename:", filename));
            SC_TRY(extractPackageIndexField(stanza, "SHA256:", sha256));

            if (metadata.version.isEmpty() or isLexicographicallyAfter(version.view(), metadata.version.view()))
            {
                String           normalizedFilename = StringEncoding::Utf8;
                const StringView filenameView       = filename.view();
                if (filenameView.startsWith("./"))
                {
                    SC_TRY(normalizedFilename.assign(filenameView.sliceStartLength(2, filenameView.sizeInBytes() - 2)));
                }
                else
                {
                    SC_TRY(normalizedFilename.assign(filenameView));
                }

                SC_TRY(metadata.version.assign(version.view()));
                SC_TRY(metadata.sha256.assign(sha256.view()));
                SC_TRY(Path::join(metadata.url, {packageBaseURL, normalizedFilename.view()}));
            }
        }

        stanzaPos = stanzaEnd + 2;
    }

    SC_TRY_MSG(not metadata.url.isEmpty(), "Cannot resolve Linux ARM64 Box64 package metadata");
    SC_TRY_MSG(not metadata.sha256.isEmpty(), "Cannot resolve Linux ARM64 Box64 package hash");
    return Result(true);
}

static Result writeLinuxBox64WineScript(StringView packageRoot, StringView executableName, StringView wineCommand)
{
    FileSystem fs;
    SC_TRY(fs.init("."));

    String binDirectory = StringEncoding::Utf8;
    SC_TRY(Path::join(binDirectory, {packageRoot, "bin"}));
    SC_TRY(fs.makeDirectoryRecursive(binDirectory.view()));

    String scriptPath = StringEncoding::Utf8;
    SC_TRY(Path::join(scriptPath, {binDirectory.view(), executableName}));

    String scriptContents = StringEncoding::Utf8;
    auto   builder        = StringBuilder::create(scriptContents);
    SC_TRY(builder.append("#!/bin/sh\n"));
    SC_TRY(builder.append("case \"$0\" in\n"));
    SC_TRY(builder.append("  */*) SCRIPT_DIR=${0%/*} ;;\n"));
    SC_TRY(builder.append("  *) SCRIPT_DIR=. ;;\n"));
    SC_TRY(builder.append("esac\n"));
    SC_TRY(builder.append("SCRIPT_DIR=$(CDPATH= cd -- \"$SCRIPT_DIR\" && pwd)\n"));
    SC_TRY(builder.append("RUNNER_ROOT=$(CDPATH= cd -- \"$SCRIPT_DIR/..\" && pwd)\n"));
    SC_TRY(builder.append("BOX64_BIN=\"$RUNNER_ROOT/box64/usr/bin/box64\"\n"));
    SC_TRY(builder.append("WINE_BIN=\"$RUNNER_ROOT/wine/opt/wine-stable/bin/{}\"\n", wineCommand));
    SC_TRY(builder.append("LIB_PATH=\"$RUNNER_ROOT/amd64-libs/usr/lib/x86_64-linux-gnu\"\n"));
    SC_TRY(builder.append("export BOX64_NOBANNER=1\n"));
    SC_TRY(builder.append("if [ -n \"$BOX64_LD_LIBRARY_PATH\" ]; then\n"));
    SC_TRY(builder.append("  export BOX64_LD_LIBRARY_PATH=\"$LIB_PATH:$BOX64_LD_LIBRARY_PATH\"\n"));
    SC_TRY(builder.append("else\n"));
    SC_TRY(builder.append("  export BOX64_LD_LIBRARY_PATH=\"$LIB_PATH\"\n"));
    SC_TRY(builder.append("fi\n"));
    SC_TRY(builder.append("exec \"$BOX64_BIN\" \"$WINE_BIN\" \"$@\"\n"));
    builder.finalize();

    SC_TRY(fs.writeString(scriptPath.view(), scriptContents.view()));
    SC_TRY(fs.chmod(scriptPath.view(), 0755u));
    return Result(true);
}

static Result writeLinuxNativeWineScript(StringView packageRoot, StringView executableName)
{
    FileSystem fs;
    SC_TRY(fs.init("."));

    String binDirectory = StringEncoding::Utf8;
    SC_TRY(Path::join(binDirectory, {packageRoot, "bin"}));
    SC_TRY(fs.makeDirectoryRecursive(binDirectory.view()));

    String scriptPath = StringEncoding::Utf8;
    SC_TRY(Path::join(scriptPath, {binDirectory.view(), executableName}));

    String scriptContents = StringEncoding::Utf8;
    auto   builder        = StringBuilder::create(scriptContents);
    SC_TRY(builder.append("#!/bin/sh\n"));
    SC_TRY(builder.append("case \"$0\" in\n"));
    SC_TRY(builder.append("  */*) SCRIPT_DIR=${0%/*} ;;\n"));
    SC_TRY(builder.append("  *) SCRIPT_DIR=. ;;\n"));
    SC_TRY(builder.append("esac\n"));
    SC_TRY(builder.append("SCRIPT_DIR=$(CDPATH= cd -- \"$SCRIPT_DIR\" && pwd)\n"));
    SC_TRY(builder.append("RUNNER_ROOT=$(CDPATH= cd -- \"$SCRIPT_DIR/..\" && pwd)\n"));
    SC_TRY(builder.append("WINE_ROOT=\"$RUNNER_ROOT/root\"\n"));
    SC_TRY(builder.append("WINE_BIN=\"$WINE_ROOT/usr/lib/wine/wine64\"\n"));
    SC_TRY(builder.append("WINE_LIB_ROOT=\"$WINE_ROOT/usr/lib/aarch64-linux-gnu\"\n"));
    SC_TRY(builder.append("export LD_LIBRARY_PATH=\"$WINE_LIB_ROOT${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}\"\n"));
    SC_TRY(builder.append("export PATH=\"$WINE_ROOT/usr/lib/wine:$WINE_ROOT/usr/bin${PATH:+:$PATH}\"\n"));
    SC_TRY(builder.append("export WINEDLLPATH=\"$WINE_LIB_ROOT/wine\"\n"));
    SC_TRY(builder.append("exec \"$WINE_BIN\" \"$@\"\n"));
    builder.finalize();

    SC_TRY(fs.writeString(scriptPath.view(), scriptContents.view()));
    SC_TRY(fs.chmod(scriptPath.view(), 0755u));
    return Result(true);
}

static Result patchLinuxNativeWineserverScript(StringView packageRoot)
{
    FileSystem fs;
    SC_TRY(fs.init("."));

    String wineserverPath = StringEncoding::Utf8;
    SC_TRY(Path::join(wineserverPath, {packageRoot, "root", "usr", "lib", "wine", "wineserver"}));
    SC_TRY_MSG(fs.existsAndIsFile(wineserverPath.view()), "Missing native Wine wineserver script");

    String scriptContents = StringEncoding::Utf8;
    auto   builder        = StringBuilder::create(scriptContents);
    SC_TRY(builder.append("#!/bin/sh -e\n"));
    SC_TRY(builder.append("case \"$0\" in\n"));
    SC_TRY(builder.append("  */*) SCRIPT_DIR=${0%/*} ;;\n"));
    SC_TRY(builder.append("  *) SCRIPT_DIR=. ;;\n"));
    SC_TRY(builder.append("esac\n"));
    SC_TRY(builder.append("SCRIPT_DIR=$(CDPATH= cd -- \"$SCRIPT_DIR\" && pwd)\n"));
    SC_TRY(builder.append("exec \"$SCRIPT_DIR/wineserver64\" -p0 \"$@\"\n"));
    builder.finalize();

    SC_TRY(fs.writeString(wineserverPath.view(), scriptContents.view()));
    SC_TRY(fs.chmod(wineserverPath.view(), 0755u));
    return Result(true);
}

static Result ensureLinuxNativeWineLoaderAlias(StringView packageRoot, StringView aliasName)
{
    FileSystem fs;
    SC_TRY(fs.init("."));

    String aliasesRoot = StringEncoding::Utf8;
    String targetRoot  = StringEncoding::Utf8;
    String aliasPath   = StringEncoding::Utf8;
    SC_TRY(Path::join(aliasesRoot, {packageRoot, "lib", "wine"}));
    SC_TRY(Path::join(targetRoot, {packageRoot, "root", "usr", "lib", "aarch64-linux-gnu", "wine"}));
    SC_TRY(Path::join(aliasPath, {aliasesRoot.view(), aliasName}));
    SC_TRY(fs.makeDirectoryRecursive(aliasesRoot.view()));
    SC_TRY(fs.removeLinkIfExists(aliasPath.view()));
    if (fs.existsAndIsDirectory(aliasPath.view()))
    {
        SC_TRY(fs.removeDirectoriesRecursive(aliasPath.view()));
    }
    if (not createLink(targetRoot.view(), aliasPath.view()))
    {
        SC_TRY(fs.makeDirectoryRecursive(aliasPath.view()));
    }
    return Result(true);
}

static Result repairLinuxNativeWineRunnerLayout(StringView packageRoot)
{
    SC_TRY(writeLinuxNativeWineScript(packageRoot, "wine"));
    SC_TRY(patchLinuxNativeWineserverScript(packageRoot));
    SC_TRY(ensureLinuxNativeWineLoaderAlias(packageRoot, "arm64-windows"));
    SC_TRY(ensureLinuxNativeWineLoaderAlias(packageRoot, "aarch64-windows"));
    return Result(true);
}
#endif

static Result resolveLinuxWineExecutable(StringView packagesCacheDirectory, StringView packagesInstallDirectory,
                                         String& output)
{
    String wine64Path = StringEncoding::Utf8;
    String winePath   = StringEncoding::Utf8;
    String box64Path  = StringEncoding::Utf8;

    const bool hasWine64 = resolveRunnableHostCommand("wine64", wine64Path);
    const bool hasWine   = resolveRunnableHostCommand("wine", winePath);
    const bool hasBox64  = resolveRunnableHostCommand("box64", box64Path);

    if (isArm64HostInstructionSet() and hasBox64)
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

    if (isArm64HostInstructionSet())
    {
        Package winePackage;
        if (installLinuxWineRunner(packagesCacheDirectory, packagesInstallDirectory, winePackage))
        {
            SC_TRY(Path::join(output, {winePackage.installDirectoryLink.view(), "bin", "wine"}));
            return Result(true);
        }
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

static Result readMSVCPackageMetadataWine(StringView packageRoot, String& wineExecutable, bool& found)
{
    found = false;

    FileSystem fs;
    SC_TRY(fs.init("."));

    String metadataPath = StringEncoding::Utf8;
    SC_TRY(Path::join(metadataPath, {packageRoot, "sc-msvc-package.json"}));
    if (not fs.existsAndIsFile(metadataPath.view()))
    {
        return Result(true);
    }

    String metadata = StringEncoding::Utf8;
    SC_TRY(fs.read(metadataPath.view(), metadata));

    StringView remainder;
    if (not StringView(metadata.view()).splitAfter("\"wine\":", remainder))
    {
        return Result(true);
    }

    remainder = remainder.trimWhiteSpaces();

    StringView afterQuote;
    if (not remainder.splitAfter("\"", afterQuote))
    {
        return Result(true);
    }

    StringView parsedWine;
    if (not afterQuote.splitBefore("\"", parsedWine))
    {
        return Result(true);
    }

    String resolvedWine = StringEncoding::Utf8;
    SC_TRY(resolveHostCommandPath(parsedWine, resolvedWine));
    SC_TRY(wineExecutable.assign(resolvedWine.view()));
    found = true;
    return Result(true);
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

    String metadataWine         = StringEncoding::Utf8;
    bool   hasMetadataWine      = false;
    String installedPackageRoot = format("{}/msvc_{}", packagesInstallDirectory,
                                         HostPlatform == Platform::Apple   ? "macos"
                                         : HostPlatform == Platform::Linux ? "linux"
                                                                           : "host");
    SC_TRY(readMSVCPackageMetadataWine(installedPackageRoot.view(), metadataWine, hasMetadataWine));
    if (not hasMetadataWine)
    {
        String cachedPackageRoot = format("{}/msvc/{}", packagesCacheDirectory, portableMSVCCacheLeafName());
        SC_TRY(readMSVCPackageMetadataWine(cachedPackageRoot.view(), metadataWine, hasMetadataWine));
        if (not hasMetadataWine)
        {
            String legacyCachedPackageRoot = format("{}/msvc/portable-x64", packagesCacheDirectory);
            SC_TRY(readMSVCPackageMetadataWine(legacyCachedPackageRoot.view(), metadataWine, hasMetadataWine));
        }
    }
    if (hasMetadataWine)
    {
        SC_TRY(wineExecutable.assign(metadataWine.view()));
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
    case Platform::Linux:
        return resolveLinuxWineExecutable(packagesCacheDirectory, packagesInstallDirectory, wineExecutable);
    case Platform::Windows:
    case Platform::Emscripten: return Result::Error("Portable MSVC is only supported on macOS and Linux hosts");
    }
    Assert::unreachable();
}

static constexpr StringView qemuRunnerCacheLeafName()
{
    switch (HostPlatform)
    {
    case Platform::Apple:
        switch (HostInstructionSet)
        {
        case InstructionSet::ARM64: return "macos-arm64";
        case InstructionSet::Intel64: return "macos-intel64";
        case InstructionSet::Intel32: return {};
        }
        break;
    case Platform::Linux:
        switch (HostInstructionSet)
        {
        case InstructionSet::ARM64: return "linux-arm64";
        case InstructionSet::Intel64: return "linux-intel64";
        case InstructionSet::Intel32: return {};
        }
        break;
    case Platform::Windows:
        switch (HostInstructionSet)
        {
        case InstructionSet::ARM64: return "windows-arm64";
        case InstructionSet::Intel64: return "windows-intel64";
        case InstructionSet::Intel32: return {};
        }
        break;
    case Platform::Emscripten: return {};
    }
    Assert::unreachable();
}

static constexpr StringView qemuRunnerInstallLeafName()
{
    switch (HostPlatform)
    {
    case Platform::Apple:
        switch (HostInstructionSet)
        {
        case InstructionSet::ARM64: return "macos_arm64";
        case InstructionSet::Intel64: return "macos_intel64";
        case InstructionSet::Intel32: return {};
        }
        break;
    case Platform::Linux:
        switch (HostInstructionSet)
        {
        case InstructionSet::ARM64: return "linux_arm64";
        case InstructionSet::Intel64: return "linux_intel64";
        case InstructionSet::Intel32: return {};
        }
        break;
    case Platform::Windows:
        switch (HostInstructionSet)
        {
        case InstructionSet::ARM64: return "windows_arm64";
        case InstructionSet::Intel64: return "windows_intel64";
        case InstructionSet::Intel32: return {};
        }
        break;
    case Platform::Emscripten: return {};
    }
    Assert::unreachable();
}

static Result resolveImportedQEMURootFromMetadata(StringView metadataPath, String& root, bool& found)
{
    found = false;

    FileSystem fs;
    SC_TRY(fs.init("."));
    if (not fs.existsAndIsFile(metadataPath))
    {
        return Result(true);
    }

    String metadata = StringEncoding::Utf8;
    SC_TRY(fs.read(metadataPath, metadata));

    StringView          sourcePath;
    StringViewTokenizer lines(metadata.view());
    while (lines.tokenizeNextLine())
    {
        StringView line = lines.component.trimWhiteSpaces();
        if (line.startsWith("SC_PACKAGE_URL=import:"))
        {
            SC_TRY(line.splitAfter("SC_PACKAGE_URL=import:", sourcePath));
            break;
        }
        if (line.startsWith("SC_PACKAGE_URL=/"))
        {
            SC_TRY(line.splitAfter("SC_PACKAGE_URL=", sourcePath));
            break;
        }
#if SC_PLATFORM_WINDOWS
        if (line.startsWith("SC_PACKAGE_URL=") and line.containsString(":\\"_a8))
        {
            SC_TRY(line.splitAfter("SC_PACKAGE_URL=", sourcePath));
            break;
        }
#endif
    }

    if (sourcePath.isEmpty())
    {
        return Result(true);
    }

    SC_TRY(root.assign(sourcePath));
    found = true;
    return Result(true);
}

static Result resolveImportedQEMURootFromPATH(String& root)
{
    static constexpr StringView candidates[] = {"qemu-x86_64", "qemu-x86_64-static", "qemu-aarch64",
                                                "qemu-aarch64-static"};
    for (const StringView candidate : candidates)
    {
        String executablePath = StringEncoding::Utf8;
        if (resolveRunnableHostCommand(candidate, executablePath))
        {
            SC_TRY(root.assign(Path::dirname(executablePath.view(), Path::AsNative)));
            return Result(true);
        }
    }
    return Result::Error("Cannot find QEMU runner executable");
}

static Result qemuRunnerExecutableCandidates(InstructionSet architecture, Span<const StringView>& candidates)
{
#if SC_PLATFORM_WINDOWS
    static constexpr StringView x86_64Candidates[] = {"qemu-x86_64.exe", "qemu-x86_64-static.exe"};
    static constexpr StringView arm64Candidates[]  = {"qemu-aarch64.exe", "qemu-aarch64-static.exe"};
#else
    static constexpr StringView x86_64Candidates[] = {"qemu-x86_64", "qemu-x86_64-static"};
    static constexpr StringView arm64Candidates[]  = {"qemu-aarch64", "qemu-aarch64-static"};
#endif
    switch (architecture)
    {
    case InstructionSet::Intel64:
        candidates = {x86_64Candidates, sizeof(x86_64Candidates) / sizeof(x86_64Candidates[0])};
        return Result(true);
    case InstructionSet::ARM64:
        candidates = {arm64Candidates, sizeof(arm64Candidates) / sizeof(arm64Candidates[0])};
        return Result(true);
    case InstructionSet::Intel32: return Result::Error("Unsupported QEMU runner architecture");
    }
    Assert::unreachable();
}

Result resolveQEMURunnerExecutable(StringView packageRoot, InstructionSet architecture, String& output)
{
    Span<const StringView> candidates;
    SC_TRY(qemuRunnerExecutableCandidates(architecture, candidates));

    FileSystem fs;
    SC_TRY(fs.init("."));

    for (const StringView candidate : candidates)
    {
        String executable = StringEncoding::Utf8;
        SC_TRY(Path::join(executable, {packageRoot, "bin", candidate}));
        if (fs.existsAndIsFile(executable.view()))
        {
            SC_TRY(output.assign(executable.view()));
            return Result(true);
        }

        SC_TRY(executable.assign({}));
        SC_TRY(Path::join(executable, {packageRoot, candidate}));
        if (fs.existsAndIsFile(executable.view()))
        {
            SC_TRY(output.assign(executable.view()));
            return Result(true);
        }
    }

    switch (architecture)
    {
    case InstructionSet::Intel64: return Result::Error("QEMU package is missing qemu-x86_64 runner executable");
    case InstructionSet::ARM64: return Result::Error("QEMU package is missing qemu-aarch64 runner executable");
    case InstructionSet::Intel32: return Result::Error("Unsupported QEMU runner architecture");
    }
    Assert::unreachable();
}

static Result resolveQEMURunnerExecutableExport(StringView packageRoot, InstructionSet architecture, String& output)
{
    Span<const StringView> candidates;
    SC_TRY(qemuRunnerExecutableCandidates(architecture, candidates));

    FileSystem fs;
    SC_TRY(fs.init("."));

    for (const StringView candidate : candidates)
    {
        String executable = StringEncoding::Utf8;
        SC_TRY(Path::join(executable, {packageRoot, "bin", candidate}));
        if (fs.existsAndIsFile(executable.view()))
        {
            SC_TRY(StringBuilder::format(output, "bin/{}", candidate));
            return Result(true);
        }

        SC_TRY(executable.assign({}));
        SC_TRY(Path::join(executable, {packageRoot, candidate}));
        if (fs.existsAndIsFile(executable.view()))
        {
            SC_TRY(output.assign(candidate));
            return Result(true);
        }
    }

    switch (architecture)
    {
    case InstructionSet::Intel64: return Result::Error("QEMU package is missing qemu-x86_64 runner executable");
    case InstructionSet::ARM64: return Result::Error("QEMU package is missing qemu-aarch64 runner executable");
    case InstructionSet::Intel32: return Result::Error("Unsupported QEMU runner architecture");
    }
    Assert::unreachable();
}

static Result testQEMURunnerExecutable(StringView executable, String* versionLine = nullptr)
{
    Process process;
    String  output = StringEncoding::Utf8;
    SC_TRY(process.exec({executable, "--version"}, output));
    SC_TRY_MSG(process.getExitStatus() == 0, "QEMU runner returned error");

    StringView          firstLine = StringView(output.view()).trimWhiteSpaces();
    StringViewTokenizer tokenizer(firstLine);
    if (tokenizer.tokenizeNextLine())
    {
        firstLine = tokenizer.component.trimWhiteSpaces();
    }

    SC_TRY_MSG(firstLine.containsString("qemu") or firstLine.containsString("QEMU"),
               "QEMU runner version output is missing the qemu banner");
    if (versionLine != nullptr)
    {
        SC_TRY(versionLine->assign(firstLine));
    }
    return Result(true);
}

static Result testQEMUPackageRoot(StringView packageRoot, String* detectedTargets = nullptr)
{
    bool   foundX86_64 = false;
    bool   foundArm64  = false;
    String targets     = StringEncoding::Utf8;
    for (const InstructionSet architecture : {InstructionSet::Intel64, InstructionSet::ARM64})
    {
        String executable = StringEncoding::Utf8;
        if (resolveQEMURunnerExecutable(packageRoot, architecture, executable))
        {
            SC_TRY(testQEMURunnerExecutable(executable.view()));
            if (architecture == InstructionSet::Intel64)
            {
                foundX86_64 = true;
            }
            else
            {
                foundArm64 = true;
            }
            if (detectedTargets != nullptr and not targets.isEmpty())
            {
                SC_TRY(StringBuilder::createForAppendingTo(targets).append(","));
            }
            SC_TRY(StringBuilder::createForAppendingTo(targets).append(
                architecture == InstructionSet::Intel64 ? "x86_64"_a8 : "arm64"_a8));
        }
    }

    if (not foundX86_64 and not foundArm64)
    {
        return Result::Error("QEMU package must provide qemu-x86_64 and qemu-aarch64");
    }
    if (not foundX86_64)
    {
        return Result::Error("QEMU package is missing qemu-x86_64");
    }
    if (not foundArm64)
    {
        return Result::Error("QEMU package is missing qemu-aarch64");
    }
    if (detectedTargets != nullptr)
    {
        SC_TRY(detectedTargets->assign(targets.view()));
    }
    return Result(true);
}

struct QEMUPackageInstallOptions
{
    StringView importDirectory;
};

struct MSVCPackageInstallOptions
{
    StringView importDirectory;
    StringView wineExecutable;
};

struct FilCPackageInstallOptions
{
    StringView importDirectory;
};

static Result parseQEMUPackageInstallOptions(Span<const StringView> arguments, QEMUPackageInstallOptions& options)
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
        else if (argument.startsWith("--"))
        {
            return Result::Error("Unknown option for SC-package install qemu");
        }
        else if (options.importDirectory.isEmpty())
        {
            options.importDirectory = argument;
        }
        else
        {
            return Result::Error("Unexpected extra argument for SC-package install qemu");
        }
    }
    return Result(true);
}

static Result parseFilCPackageInstallOptions(Span<const StringView> arguments, FilCPackageInstallOptions& options)
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
        else if (argument.startsWith("--"))
        {
            return Result::Error("Unknown option for SC-package install filc");
        }
        else if (options.importDirectory.isEmpty())
        {
            options.importDirectory = argument;
        }
        else
        {
            return Result::Error("Unexpected extra argument for SC-package install filc");
        }
    }
    return Result(true);
}

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

static Result writeMSVCWrapperScripts(StringView packageRoot);

static Result repairMSVCPackageLayout(StringView packageRoot, StringView wineExecutable)
{
    FileSystem fs;
    SC_TRY(fs.init("."));

    String metadataPath = StringEncoding::Utf8;
    SC_TRY(Path::join(metadataPath, {packageRoot, "sc-msvc-package.json"}));
    if (not fs.existsAndIsFile(metadataPath.view()))
    {
        SC_TRY(writeMSVCPackageMetadata(packageRoot, wineExecutable));
    }

    SC_TRY(writeMSVCWrapperScripts(packageRoot));

    return Result(true);
}

static Result msvcPackagePathExists(FileSystem& fs, StringView packageRoot, Span<const StringView> components,
                                    const char* missingMessage)
{
    String path = StringEncoding::Utf8;
    SC_TRY(Path::join(path, {packageRoot}));
    SC_TRY(Path::append(path, components, Path::AsNative));
    if (not fs.exists(path.view()))
    {
        return Result::FromStableCharPointer(missingMessage);
    }
    return Result(true);
}

static Result validateMSVCPackageLayout(FileSystem& fs, StringView packageRoot)
{
    String msvcVersion = StringEncoding::Utf8;
    String sdkVersion  = StringEncoding::Utf8;
    SC_TRY(resolveMSVCVersions(packageRoot, msvcVersion, sdkVersion));

    SC_TRY(msvcPackagePathExists(fs, packageRoot, {"VC", "Tools", "MSVC", msvcVersion.view(), "include"},
                                 "Portable MSVC package is missing the MSVC include directory"));
    SC_TRY(msvcPackagePathExists(fs, packageRoot, {"Windows Kits", "10", "Include", sdkVersion.view(), "um"},
                                 "Portable MSVC package is missing the Windows SDK um include directory"));
    SC_TRY(msvcPackagePathExists(fs, packageRoot, {"Windows Kits", "10", "Include", sdkVersion.view(), "shared"},
                                 "Portable MSVC package is missing the Windows SDK shared include directory"));
    SC_TRY(msvcPackagePathExists(fs, packageRoot, {"Windows Kits", "10", "Include", sdkVersion.view(), "ucrt"},
                                 "Portable MSVC package is missing the Windows SDK ucrt include directory"));
    SC_TRY(msvcPackagePathExists(fs, packageRoot, {"Windows Kits", "10", "Include", sdkVersion.view(), "winrt"},
                                 "Portable MSVC package is missing the Windows SDK winrt include directory"));
    SC_TRY(msvcPackagePathExists(fs, packageRoot, {"Windows Kits", "10", "Include", sdkVersion.view(), "cppwinrt"},
                                 "Portable MSVC package is missing the Windows SDK cppwinrt include directory"));

    SC_TRY(msvcPackagePathExists(fs, packageRoot, {"VC", "Tools", "MSVC", msvcVersion.view(), "lib", "x64"},
                                 "Portable MSVC package is missing the x64 MSVC library directory"));
    SC_TRY(msvcPackagePathExists(fs, packageRoot, {"Windows Kits", "10", "Lib", sdkVersion.view(), "um", "x64"},
                                 "Portable MSVC package is missing the x64 Windows SDK um library directory"));
    SC_TRY(msvcPackagePathExists(fs, packageRoot, {"Windows Kits", "10", "Lib", sdkVersion.view(), "ucrt", "x64"},
                                 "Portable MSVC package is missing the x64 Windows SDK ucrt library directory"));
    SC_TRY(msvcPackagePathExists(fs, packageRoot,
                                 {"VC", "Tools", "MSVC", msvcVersion.view(), "bin", "Hostx64", "x64", "cl.exe"},
                                 "Portable MSVC package is missing the x64 cl.exe tool"));
    SC_TRY(msvcPackagePathExists(fs, packageRoot,
                                 {"VC", "Tools", "MSVC", msvcVersion.view(), "bin", "Hostx64", "x64", "link.exe"},
                                 "Portable MSVC package is missing the x64 link.exe tool"));
    SC_TRY(msvcPackagePathExists(fs, packageRoot,
                                 {"VC", "Tools", "MSVC", msvcVersion.view(), "bin", "Hostx64", "x64", "lib.exe"},
                                 "Portable MSVC package is missing the x64 lib.exe tool"));

    SC_TRY(msvcPackagePathExists(fs, packageRoot, {"VC", "Tools", "MSVC", msvcVersion.view(), "lib", "arm64"},
                                 "Portable MSVC package is missing the arm64 MSVC library directory"));
    SC_TRY(msvcPackagePathExists(fs, packageRoot, {"Windows Kits", "10", "Lib", sdkVersion.view(), "um", "arm64"},
                                 "Portable MSVC package is missing the arm64 Windows SDK um library directory"));
    SC_TRY(msvcPackagePathExists(fs, packageRoot, {"Windows Kits", "10", "Lib", sdkVersion.view(), "ucrt", "arm64"},
                                 "Portable MSVC package is missing the arm64 Windows SDK ucrt library directory"));
    SC_TRY(msvcPackagePathExists(fs, packageRoot,
                                 {"VC", "Tools", "MSVC", msvcVersion.view(), "bin", "Hostx64", "arm64", "cl.exe"},
                                 "Portable MSVC package is missing the arm64 cl.exe tool"));
    SC_TRY(msvcPackagePathExists(fs, packageRoot,
                                 {"VC", "Tools", "MSVC", msvcVersion.view(), "bin", "Hostx64", "arm64", "link.exe"},
                                 "Portable MSVC package is missing the arm64 link.exe tool"));
    SC_TRY(msvcPackagePathExists(fs, packageRoot,
                                 {"VC", "Tools", "MSVC", msvcVersion.view(), "bin", "Hostx64", "arm64", "lib.exe"},
                                 "Portable MSVC package is missing the arm64 lib.exe tool"));
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
            SC_TRY(builder.append("case \"$0\" in\n"));
            SC_TRY(builder.append("  */*) SCRIPT_DIR=${0%/*} ;;\n"));
            SC_TRY(builder.append("  *) SCRIPT_DIR=. ;;\n"));
            SC_TRY(builder.append("esac\n"));
            SC_TRY(builder.append("SCRIPT_DIR=$(CDPATH= cd -- \"$SCRIPT_DIR\" && pwd)\n"));
            SC_TRY(builder.append("exec \"$SCRIPT_DIR/msvc-wrapper.py\" {} \"$@\"\n", toolName));
            builder.finalize();

            SC_TRY(fs.writeString(scriptPath.view(), scriptContents.view()));
            SC_TRY(fs.chmod(scriptPath.view(), 0755u));
        }
    }
    return Result(true);
}

static Result finalizeInstalledPackageFromRoot(StringView packageRoot, Package& package)
{
    FileSystem fs;
    SC_TRY(fs.init("."));
    SC_TRY(fs.removeLinkIfExists(package.installDirectoryLink.view()));
    if (not createLink(packageRoot, package.installDirectoryLink.view()))
    {
        if (fs.existsAndIsDirectory(package.installDirectoryLink.view()))
        {
            SC_TRY(fs.removeDirectoriesRecursive(package.installDirectoryLink.view()));
        }
        SC_TRY(fs.copyDirectory(packageRoot, package.installDirectoryLink.view()));
    }
    return Result(true);
}

static Result finalizeInstalledPackage(Package& package)
{
    return finalizeInstalledPackageFromRoot(package.packageLocalDirectory.view(), package);
}

static Result removePackageInstallLink(FileSystem& fs, const Package& package)
{
    SC_TRY(fs.removeLinkIfExists(package.installDirectoryLink.view()));
    if (fs.existsAndIsDirectory(package.installDirectoryLink.view()))
    {
        SC_TRY(fs.removeDirectoriesRecursive(package.installDirectoryLink.view()));
    }
    return Result(true);
}

static Result writeManualPackageReceipt(const Package& package, StringView name, StringView version, StringView variant,
                                        StringView source, StringView sourceHash,
                                        Span<const PackageReceiptExport> exports = {},
                                        Span<const StringView>           phases  = {});

Result installQEMURunner(StringView packagesCacheDirectory, StringView packagesInstallDirectory, Package& package,
                         StringView importDirectory)
{
    const StringView cacheLeaf   = qemuRunnerCacheLeafName();
    const StringView installLeaf = qemuRunnerInstallLeafName();
    SC_TRY_MSG(not cacheLeaf.isEmpty() and not installLeaf.isEmpty(),
               "QEMU package install is not supported on this host");

    package.packageFullName       = format("qemu-{}", cacheLeaf);
    package.packageLocalDirectory = format("{}/qemu/{}", packagesCacheDirectory, cacheLeaf);
    package.packageLocalTxt       = format("{}/qemu/{}.txt", packagesCacheDirectory, cacheLeaf);
    package.installDirectoryLink  = format("{}/qemu_{}", packagesInstallDirectory, installLeaf);

    FileSystem fs;
    SC_TRY(fs.init("."));
    SC_TRY(fs.makeDirectoryRecursive(packagesCacheDirectory));
    SC_TRY(fs.makeDirectoryRecursive(packagesInstallDirectory));
    SC_TRY(fs.makeDirectoryRecursive(Path::dirname(package.packageLocalTxt.view(), Path::AsNative)));

    if (fs.existsAndIsDirectory(package.installDirectoryLink.view()) and
        testQEMUPackageRoot(package.installDirectoryLink.view()))
    {
        if (importDirectory.isEmpty())
        {
            String qemuX86_64Export = StringEncoding::Utf8;
            String qemuArm64Export  = StringEncoding::Utf8;
            SC_TRY(resolveQEMURunnerExecutableExport(package.installDirectoryLink.view(), InstructionSet::Intel64,
                                                     qemuX86_64Export));
            SC_TRY(resolveQEMURunnerExecutableExport(package.installDirectoryLink.view(), InstructionSet::ARM64,
                                                     qemuArm64Export));
            const PackageReceiptExport exports[] = {
                {"runner", PackageExport::RunnerQEMU, "."},
                {"capability", PackageCapability::RunnerQEMUX86_64, qemuX86_64Export.view()},
                {"capability", PackageCapability::RunnerQEMUArm64, qemuArm64Export.view()},
            };
            static constexpr StringView phases[] = {
                "resolveImportedQEMU",
                "validateQEMUTargets",
                "writeReceipt",
            };
            SC_TRY(writeManualPackageReceipt(package, "qemu", "imported", installLeaf, "cached", {}, exports, phases));
            return Result(true);
        }
    }

    String sourceRoot     = StringEncoding::Utf8;
    bool   hasSourceRoot  = false;
    bool   explicitImport = not importDirectory.isEmpty();
    if (explicitImport)
    {
        SC_TRY(sourceRoot.assign(importDirectory));
        hasSourceRoot = true;
    }
    else
    {
        SC_TRY(resolveImportedQEMURootFromMetadata(package.packageLocalTxt.view(), sourceRoot, hasSourceRoot));
        if (not hasSourceRoot and resolveImportedQEMURootFromPATH(sourceRoot))
        {
            hasSourceRoot = true;
        }
    }

    SC_TRY_MSG(hasSourceRoot, "Cannot find a reusable QEMU runner. Install qemu on PATH or run SC-package install qemu "
                              "--import-directory <path>.");
    SC_TRY_MSG(fs.existsAndIsDirectory(sourceRoot.view()), "Imported QEMU runner directory does not exist");

    SC_TRY(finalizeInstalledPackageFromRoot(sourceRoot.view(), package));

    String       detectedTargets = StringEncoding::Utf8;
    const Result qemuValidation  = testQEMUPackageRoot(package.installDirectoryLink.view(), &detectedTargets);
    if (not qemuValidation)
    {
        SC_TRY(removePackageInstallLink(fs, package));
        return qemuValidation;
    }

    String metadata = StringEncoding::Utf8;
    auto   builder  = StringBuilder::create(metadata);
    SC_TRY(builder.append("SC_PACKAGE_URL=import:{}\n", sourceRoot.view()));
    SC_TRY(builder.append("SC_PACKAGE_TARGETS={}\n", detectedTargets.view()));
    builder.finalize();
    SC_TRY(fs.writeString(package.packageLocalTxt.view(), metadata.view()));
    String qemuX86_64Export = StringEncoding::Utf8;
    String qemuArm64Export  = StringEncoding::Utf8;
    SC_TRY(resolveQEMURunnerExecutableExport(package.installDirectoryLink.view(), InstructionSet::Intel64,
                                             qemuX86_64Export));
    SC_TRY(
        resolveQEMURunnerExecutableExport(package.installDirectoryLink.view(), InstructionSet::ARM64, qemuArm64Export));
    const PackageReceiptExport exports[] = {
        {"runner", PackageExport::RunnerQEMU, "."},
        {"capability", PackageCapability::RunnerQEMUX86_64, qemuX86_64Export.view()},
        {"capability", PackageCapability::RunnerQEMUArm64, qemuArm64Export.view()},
    };
    static constexpr StringView phases[] = {
        "resolveImportedQEMU",
        "validateQEMUTargets",
        "writeReceipt",
    };
    SC_TRY(writeManualPackageReceipt(package, "qemu", "imported", installLeaf, sourceRoot.view(), {}, exports, phases));
    return Result(true);
}

static Result testMSVCToolchain(const Package& package)
{
    FileSystem fs;
    SC_TRY(fs.init("."));
    SC_TRY(validateMSVCPackageLayout(fs, package.installDirectoryLink.view()));

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

struct PackageReceiptExportJSON
{
    String kind = StringEncoding::Utf8;
    String name = StringEncoding::Utf8;
    String path = StringEncoding::Utf8;
};

struct PackageReceiptJSON
{
    int    schema        = 1;
    String name          = StringEncoding::Utf8;
    String version       = StringEncoding::Utf8;
    String recipeVersion = StringEncoding::Utf8;
    String hostPlatform  = StringEncoding::Utf8;
    String variant       = StringEncoding::Utf8;
    String source        = StringEncoding::Utf8;
    String sourceHash    = StringEncoding::Utf8;
    String installRoot   = StringEncoding::Utf8;
    String validation    = StringEncoding::Utf8;

    Vector<String>                   phases;
    Vector<PackageReceiptExportJSON> exports;
};

struct PackageLockEntryJSON
{
    String name          = StringEncoding::Utf8;
    String version       = StringEncoding::Utf8;
    String recipeVersion = StringEncoding::Utf8;
    String hostPlatform  = StringEncoding::Utf8;
    String variant       = StringEncoding::Utf8;
    String source        = StringEncoding::Utf8;
    String sourceHash    = StringEncoding::Utf8;
    String installRoot   = StringEncoding::Utf8;
    String receipt       = StringEncoding::Utf8;

    Vector<PackageReceiptExportJSON> exports;
};

struct PackageLockJSON
{
    int    schema       = 2;
    String tool         = StringEncoding::Utf8;
    String toolVersion  = StringEncoding::Utf8;
    String generatedAt  = StringEncoding::Utf8;
    String hostPlatform = StringEncoding::Utf8;
    String hostArch     = StringEncoding::Utf8;
    int    packageCount = 0;

    Vector<PackageLockEntryJSON> packages;
};

} // namespace Tools
} // namespace SC

SC_REFLECT_STRUCT_VISIT(SC::Tools::PackageReceiptExportJSON)
SC_REFLECT_STRUCT_FIELD(0, kind)
SC_REFLECT_STRUCT_FIELD(1, name)
SC_REFLECT_STRUCT_FIELD(2, path)
SC_REFLECT_STRUCT_LEAVE()

SC_REFLECT_STRUCT_VISIT(SC::Tools::PackageReceiptJSON)
SC_REFLECT_STRUCT_FIELD(0, schema)
SC_REFLECT_STRUCT_FIELD(1, name)
SC_REFLECT_STRUCT_FIELD(2, version)
SC_REFLECT_STRUCT_FIELD(3, recipeVersion)
SC_REFLECT_STRUCT_FIELD(4, hostPlatform)
SC_REFLECT_STRUCT_FIELD(5, variant)
SC_REFLECT_STRUCT_FIELD(6, source)
SC_REFLECT_STRUCT_FIELD(7, sourceHash)
SC_REFLECT_STRUCT_FIELD(8, installRoot)
SC_REFLECT_STRUCT_FIELD(9, validation)
SC_REFLECT_STRUCT_FIELD(10, phases)
SC_REFLECT_STRUCT_FIELD(11, exports)
SC_REFLECT_STRUCT_LEAVE()

SC_REFLECT_STRUCT_VISIT(SC::Tools::PackageLockEntryJSON)
SC_REFLECT_STRUCT_FIELD(0, name)
SC_REFLECT_STRUCT_FIELD(1, version)
SC_REFLECT_STRUCT_FIELD(2, recipeVersion)
SC_REFLECT_STRUCT_FIELD(3, hostPlatform)
SC_REFLECT_STRUCT_FIELD(4, variant)
SC_REFLECT_STRUCT_FIELD(5, source)
SC_REFLECT_STRUCT_FIELD(6, sourceHash)
SC_REFLECT_STRUCT_FIELD(7, installRoot)
SC_REFLECT_STRUCT_FIELD(8, receipt)
SC_REFLECT_STRUCT_FIELD(9, exports)
SC_REFLECT_STRUCT_LEAVE()

SC_REFLECT_STRUCT_VISIT(SC::Tools::PackageLockJSON)
SC_REFLECT_STRUCT_FIELD(0, schema)
SC_REFLECT_STRUCT_FIELD(1, tool)
SC_REFLECT_STRUCT_FIELD(2, toolVersion)
SC_REFLECT_STRUCT_FIELD(3, generatedAt)
SC_REFLECT_STRUCT_FIELD(4, hostPlatform)
SC_REFLECT_STRUCT_FIELD(5, hostArch)
SC_REFLECT_STRUCT_FIELD(6, packageCount)
SC_REFLECT_STRUCT_FIELD(7, packages)
SC_REFLECT_STRUCT_LEAVE()

namespace SC
{
namespace Tools
{

static Result assignJSONField(String& output, StringView value)
{
    SC_TRY(output.assign(value));
    return Result(true);
}

static Result appendJSONString(Vector<String>& output, StringView value)
{
    String item = StringEncoding::Utf8;
    SC_TRY(item.assign(value));
    SC_TRY(output.push_back(move(item)));
    return Result(true);
}

static Result appendJSONExport(Vector<PackageReceiptExportJSON>& output, StringView kind, StringView name,
                               StringView path)
{
    PackageReceiptExportJSON item;
    SC_TRY(assignJSONField(item.kind, kind));
    SC_TRY(assignJSONField(item.name, name));
    SC_TRY(assignJSONField(item.path, path));
    SC_TRY(output.push_back(move(item)));
    return Result(true);
}

static Result validatePackageReceiptExportPath(StringView path)
{
    SC_TRY_MSG(not path.isEmpty(), "Package receipt export is missing path");
    if (path == "."_a8)
    {
        return Result(true);
    }
    SC_TRY_MSG(not Path::isAbsolute(path, Path::AsPosix) and not Path::isAbsolute(path, Path::AsWindows),
               "Package receipt export path must be relative");

    StringViewTokenizer tokenizer(path);
    while (tokenizer.tokenizeNext({'/', '\\'}, StringViewTokenizer::SkipEmpty))
    {
        SC_TRY_MSG(tokenizer.component != ".."_a8, "Package receipt export path cannot escape package root");
    }
    return Result(true);
}

static Result resolvePackageReceiptExportNativePath(StringView packageRoot, StringView exportPath, String& output)
{
    if (exportPath == "."_a8)
    {
        SC_TRY(output.assign(packageRoot));
        return Result(true);
    }

    StringView          components[64];
    size_t              numComponents = 0;
    StringViewTokenizer tokenizer(exportPath);
    while (tokenizer.tokenizeNext({'/', '\\'}, StringViewTokenizer::SkipEmpty))
    {
        SC_TRY_MSG(numComponents < sizeof(components) / sizeof(components[0]),
                   "Package receipt export path has too many components");
        components[numComponents] = tokenizer.component;
        numComponents += 1;
    }

    SC_TRY(output.assign(packageRoot));
    SC_TRY_MSG(Path::append(output, {components, numComponents}, Path::AsNative),
               "Failed resolving package receipt export path");
    return Result(true);
}

static Result validatePackageReceiptSourceHash(StringView sourceHash)
{
    if (sourceHash.isEmpty())
    {
        return Result(true);
    }
    StringView algorithm;
    StringView digest;
    SC_TRY_MSG(sourceHash.splitBefore(":"_a8, algorithm) and sourceHash.splitAfter(":"_a8, digest),
               "Package receipt source hash is missing algorithm");
    SC_TRY_MSG(algorithm == "md5"_a8 or algorithm == "sha1"_a8 or algorithm == "sha256"_a8,
               "Package receipt source hash has an unsupported algorithm");
    SC_TRY_MSG(not digest.isEmpty(), "Package receipt source hash is missing digest");
    return Result(true);
}

static constexpr StringView hostPackagePlatformName()
{
    switch (HostPlatform)
    {
    case Platform::Apple: return "macos"_a8;
    case Platform::Linux: return "linux"_a8;
    case Platform::Windows: return "windows"_a8;
    case Platform::Emscripten: return "emscripten"_a8;
    }
    Assert::unreachable();
}

static Result packageReceiptPath(StringView packageRoot, String& output)
{
    SC_TRY(Path::join(output, {packageRoot, PackageReceiptFileName}));
    return Result(true);
}

Result writePackageReceipt(const Package& package, const PackageReceiptInfo& info,
                           Span<const PackageReceiptExport> exports)
{
    SC_TRY_MSG(not package.installDirectoryLink.isEmpty(), "Cannot write package receipt without install root");

    String receiptPath = StringEncoding::Utf8;
    SC_TRY(packageReceiptPath(package.installDirectoryLink.view(), receiptPath));

    String sourceHash = StringEncoding::Utf8;
    if (not info.sourceHash.isEmpty())
    {
        SC_TRY(validatePackageReceiptSourceHash(info.sourceHash));
        SC_TRY(sourceHash.assign(info.sourceHash));
    }

    PackageReceiptJSON receiptJSON;
    SC_TRY(assignJSONField(receiptJSON.name, info.packageName));
    SC_TRY(assignJSONField(receiptJSON.version, info.packageVersion));
    SC_TRY(assignJSONField(receiptJSON.recipeVersion, info.recipeVersion.isEmpty() ? "1"_a8 : info.recipeVersion));
    SC_TRY(assignJSONField(receiptJSON.hostPlatform,
                           info.hostPlatform.isEmpty() ? hostPackagePlatformName() : info.hostPlatform));
    SC_TRY(assignJSONField(receiptJSON.variant, info.packageVariant));
    SC_TRY(assignJSONField(receiptJSON.source, info.source));
    SC_TRY(assignJSONField(receiptJSON.sourceHash, sourceHash.view()));
    SC_TRY(assignJSONField(receiptJSON.installRoot, package.installDirectoryLink.view()));
    SC_TRY(assignJSONField(receiptJSON.validation, info.validation));
    for (size_t idx = 0; idx < info.phases.sizeInElements(); ++idx)
    {
        SC_TRY(appendJSONString(receiptJSON.phases, info.phases[idx]));
    }
    for (size_t idx = 0; idx < exports.sizeInElements(); ++idx)
    {
        SC_TRY(appendJSONExport(receiptJSON.exports, exports[idx].kind, exports[idx].name, exports[idx].relativePath));
    }

    String receipt = StringEncoding::Utf8;
    SC_TRY_MSG(SerializationJson::write(receiptJSON, receipt), "Failed writing package receipt JSON");

    FileSystem fs;
    SC_TRY(fs.init("."));
    return fs.writeString(receiptPath.view(), receipt.view());
}

static Result downloadReceiptInfo(const Download& download, PackageReceiptInfo& info, String& sourceHash)
{
    SC_TRY(sourceHash.assign({}));
    if (not download.expectedHash.isEmpty())
    {
        SC_TRY(StringBuilder::format(sourceHash, "{}:{}", packageHashName(download.hashType), download.expectedHash));
    }
    info.packageName    = download.packageName.view();
    info.packageVersion = download.packageVersion.view();
    info.recipeVersion  = "1";
    info.hostPlatform   = hostPackagePlatformName();
    info.packageVariant = download.packagePlatform.view();
    info.source         = download.url.view();
    info.sourceHash     = sourceHash.view();
    info.validation     = "passed";
    return Result(true);
}

static Result writeDownloadPackageReceipt(const Download& download, const Package& package,
                                          Span<const PackageReceiptExport> exports = {},
                                          Span<const StringView>           phases  = {})
{
    PackageReceiptInfo info;
    String             sourceHash = StringEncoding::Utf8;
    SC_TRY(downloadReceiptInfo(download, info, sourceHash));
    info.phases = phases;
    return writePackageReceipt(package, info, exports);
}

static Result writeManualPackageReceipt(const Package& package, StringView name, StringView version, StringView variant,
                                        StringView source, StringView sourceHash,
                                        Span<const PackageReceiptExport> exports, Span<const StringView> phases)
{
    PackageReceiptInfo info;
    info.packageName    = name;
    info.packageVersion = version;
    info.recipeVersion  = "1";
    info.hostPlatform   = hostPackagePlatformName();
    info.packageVariant = variant;
    info.source         = source;
    info.sourceHash     = sourceHash;
    info.validation     = "passed";
    info.phases         = phases;
    return writePackageReceipt(package, info, exports);
}

static Result readPackageReceiptJSON(StringView receipt, PackageReceiptJSON& output)
{
    SC_TRY_MSG(SerializationJson::loadVersioned(output, receipt), "Malformed package receipt JSON");
    return Result(true);
}

static Result validatePackageReceiptHeader(const PackageReceiptJSON& receiptJSON)
{
    SC_TRY_MSG(receiptJSON.schema == 1, "Package receipt schema is unsupported");
    SC_TRY_MSG(not receiptJSON.name.isEmpty(), "Package receipt is missing package name");
    return Result(true);
}

template <typename Callback>
static Result forEachReceiptExport(StringView receipt, Callback&& callback)
{
    PackageReceiptJSON receiptJSON;
    SC_TRY(readPackageReceiptJSON(receipt, receiptJSON));
    SC_TRY(validatePackageReceiptHeader(receiptJSON));
    for (auto& exportView : receiptJSON.exports)
    {
        SC_TRY(callback(exportView));
    }
    return Result(true);
}

static Result resolvePackageReceiptExportPath(StringView packageRoot, StringView exportKind, StringView exportName,
                                              String& output)
{
    String receiptPath = StringEncoding::Utf8;
    SC_TRY(packageReceiptPath(packageRoot, receiptPath));

    String receipt = StringEncoding::Utf8;
    SC_TRY(readFileIntoString(receiptPath.view(), receipt));

    bool found = false;
    SC_TRY(forEachReceiptExport(
        receipt.view(),
        [&](const PackageReceiptExportJSON& exportView) -> Result
        {
            if ((exportKind.isEmpty() or exportView.kind.view() == exportKind) and exportView.name.view() == exportName)
            {
                SC_TRY_MSG(not found, "Package receipt export is duplicated");
                SC_TRY(validatePackageReceiptExportPath(exportView.path.view()));
                SC_TRY(resolvePackageReceiptExportNativePath(packageRoot, exportView.path.view(), output));
                found = true;
            }
            return Result(true);
        }));
    return found ? Result(true) : Result::Error("Package export not found");
}

Result resolvePackageExportPath(StringView packageRoot, StringView exportName, String& output)
{
    return resolvePackageReceiptExportPath(packageRoot, {}, exportName, output);
}

Result resolvePackageCapabilityPath(StringView packageRoot, StringView capabilityName, String& output)
{
    return resolvePackageReceiptExportPath(packageRoot, "capability"_a8, capabilityName, output);
}

Result installPackageRecipe(const PackageRecipe& recipe, Package& package)
{
    package = recipe.package;
    if (recipe.kind == PackageRecipeKind::CopyDirectory)
    {
        SC_TRY_MSG(not recipe.copySourceDirectory.isEmpty(), "Package copy recipe is missing source directory");
        SC_TRY_MSG(not package.installDirectoryLink.isEmpty(), "Package copy recipe is missing install directory");

        FileSystem fs;
        SC_TRY(fs.init("."));
        if (fs.existsAndIsDirectory(package.installDirectoryLink.view()))
        {
            SC_TRY(fs.removeDirectoriesRecursive(package.installDirectoryLink.view()));
        }
        else
        {
            SC_TRY(fs.removeFileIfExists(package.installDirectoryLink.view()));
        }
        SC_TRY(fs.copyDirectory(recipe.copySourceDirectory, package.installDirectoryLink.view()));

        PackageReceiptInfo info;
        info.packageName = recipe.download.packageName.view();
        info.packageVersion =
            recipe.download.packageVersion.isEmpty() ? "local"_a8 : recipe.download.packageVersion.view();
        info.recipeVersion  = "1";
        info.hostPlatform   = recipe.download.packagePlatform.view();
        info.packageVariant = recipe.download.packagePlatform.view();
        info.source         = recipe.copySourceDirectory;
        info.sourceHash     = {};
        info.validation     = "passed";
        info.phases         = recipe.phases;
        return writePackageReceipt(package, info, recipe.exports);
    }
    SC_TRY(packageInstall(recipe.download, package, recipe.functions));
    return writeDownloadPackageReceipt(recipe.download, package, recipe.exports, recipe.phases);
}

Result installDoxygen(StringView packagesCacheDirectory, StringView packagesInstallDirectory, Package& package)
{
    // https://github.com/doxygen/doxygen/releases/download/Release_1_12_0/Doxygen-1.12.0.dmg
    static constexpr StringView packageVersion     = "1.12.0";
    static constexpr StringView packageVersionDash = "1_12_0";
    static constexpr StringView testVersion        = "1.12.0 (c73f5d30f9e8b1df5ba15a1d064ff2067cbb8267";
    static constexpr StringView baseURL            = "https://github.com/doxygen/doxygen/releases/download";

    PackageRecipe recipe;
    recipe.download.packagesCacheDirectory   = packagesCacheDirectory;
    recipe.download.packagesInstallDirectory = packagesInstallDirectory;
    recipe.download.packageName              = "doxygen";
    recipe.download.packageVersion           = packageVersion;

    SC_TRY(StringBuilder::format(recipe.download.url, "{0}/Release_{1}/", baseURL, packageVersionDash));

    recipe.functions.keepDownloadedArchive = false;
    switch (HostPlatform)
    {
    case Platform::Apple: {
        auto sb = StringBuilder::createForAppendingTo(recipe.download.url);
        SC_TRY(sb.append("Doxygen-{0}.dmg", recipe.download.packageVersion));
        sb.finalize();
        recipe.download.packagePlatform  = "macos";
        recipe.download.expectedHash     = "354ee835cf03e8a0187460a1456eb108";
        recipe.package.packageBaseName   = format("Doxygen-{0}.dmg", recipe.download.packageVersion);
        recipe.functions.extractFunction = [](StringView fileName, StringView directory) -> Result
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
        auto sb = StringBuilder::createForAppendingTo(recipe.download.url);
        SC_TRY(sb.append("doxygen-{0}.linux.bin.tar.gz", recipe.download.packageVersion));
        sb.finalize();
        recipe.download.packagePlatform  = "linux";
        recipe.download.expectedHash     = "fd96a5defa535dfe2e987b46540844a4";
        recipe.package.packageBaseName   = format("doxygen-{0}.linux.bin.tar.gz", recipe.download.packageVersion);
        recipe.functions.extractFunction = [](StringView fileName, StringView directory) -> Result
        { return tarExpandSingleFileTo(fileName, directory, "doxygen-1.12.0/bin/doxygen", 2); };
    }
    break;
    case Platform::Windows: {
        auto sb = StringBuilder::createForAppendingTo(recipe.download.url);
        SC_TRY(sb.append("doxygen-{0}.windows.x64.bin.zip", recipe.download.packageVersion));
        sb.finalize();
        recipe.download.packagePlatform = "windows";
        recipe.download.expectedHash    = "d014a212331693ffcf72ad99b2087ea0";
        recipe.package.packageBaseName  = format("doxygen-{0}.windows.x64.bin.zip", recipe.download.packageVersion);
    }
    break;
    case Platform::Emscripten: return Result::Error("Unsupported platform");
    }

    recipe.functions.testFunction = [](const Download& download, const Package& package)
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
    const PackageReceiptExport exports[] = {
        {"tool", "doxygen", HostPlatform == Platform::Windows ? "doxygen.exe"_a8 : "doxygen"_a8},
    };
    static constexpr StringView phases[] = {
        "resolveDoxygenRelease",
        "extractDoxygenExecutable",
        "validateDoxygenVersion",
        "writeReceipt",
    };
    recipe.exports = exports;
    recipe.phases  = phases;
    return installPackageRecipe(recipe, package);
}

Result installDoxygenAwesomeCss(StringView packagesCacheDirectory, StringView packagesInstallDirectory,
                                Package& package)
{
    const PackageReceiptExport exports[] = {
        {"asset", "doxygen-awesome-css", "."},
    };

    PackageRecipe recipe;
    recipe.download.packagesCacheDirectory   = packagesCacheDirectory;
    recipe.download.packagesInstallDirectory = packagesInstallDirectory;
    recipe.download.packageName              = "doxygen-awesome-css";
    recipe.download.packageVersion           = "568f56c"; // corresponds to "v2.3.4";
    recipe.download.url                      = "https://github.com/jothepro/doxygen-awesome-css.git";
    recipe.download.isGitClone               = true;
    recipe.download.shallowClone             = "568f56cde6ac78b6dfcc14acd380b2e745c301ea";
    recipe.package.packageBaseName           = format("doxygen-awesome-css-{0}", recipe.download.packagePlatform);
    recipe.functions.testFunction            = &verifyGitCommitHashInstall;
    static constexpr StringView phases[]     = {
        "fetchGitRevision",
        "validateGitCommit",
        "writeReceipt",
    };
    recipe.exports = exports;
    recipe.phases  = phases;
    return installPackageRecipe(recipe, package);
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

static constexpr StringView hostLLVMExecutableName(StringView baseName);
static Result               resolveHostLLVMArchive(StringView packageName, StringView packagesCacheDirectory,
                                                   StringView packagesInstallDirectory, Download& download, Package& package,
                                                   StringView& archiveRoot);

Result installClangBinaries(StringView packagesCacheDirectory, StringView packagesInstallDirectory, Package& package)
{
    PackageRecipe recipe;
    recipe.functions.keepDownloadedArchive = false;
    StringView archiveRoot                 = {};
    SC_TRY(resolveHostLLVMArchive("clang-binaries"_a8, packagesCacheDirectory, packagesInstallDirectory,
                                  recipe.download, recipe.package, archiveRoot));

    static constexpr StringView wantedVersion            = "20.1.8";
    String                      clangFormatPathInArchive = StringEncoding::Utf8;
    SC_TRY(Path::join(clangFormatPathInArchive, {archiveRoot, "bin", hostLLVMExecutableName("clang-format"_a8)}));

    recipe.functions.extractFunction =
        [&clangFormatPathInArchive](StringView sourceFile, StringView destinationDirectory)
    { return tarExpandSingleFileTo(sourceFile, destinationDirectory, clangFormatPathInArchive.view(), 1); };

    recipe.functions.testFunction = [](const Download&, const Package& package)
    {
        String  result;
        String  formatExecutable;
        Process process;
        SC_TRY(Path::join(formatExecutable,
                          {package.installDirectoryLink.view(), "bin", hostLLVMExecutableName("clang-format"_a8)}));
        SC_TRY(process.exec({formatExecutable.view(), "--version"}, result));
        SC_TRY_MSG(process.getExitStatus() == 0, "clang-format returned error");
        return clangFormatMatchesVersion(result.view(), wantedVersion);
    };
    String clangFormat = StringEncoding::Utf8;
    SC_TRY(Path::join(clangFormat, {"bin", hostLLVMExecutableName("clang-format"_a8)}));
    const PackageReceiptExport exports[] = {
        {"tool", "clang-format", clangFormat.view()},
    };
    static constexpr StringView phases[] = {
        "resolveHostLLVMArchive",
        "extractClangFormat",
        "validateClangFormatVersion",
        "writeReceipt",
    };
    recipe.exports = exports;
    recipe.phases  = phases;
    return installPackageRecipe(recipe, package);
}

static constexpr StringView hostLLVMExecutableName(StringView baseName)
{
#if SC_PLATFORM_WINDOWS
    if (baseName == "clang"_a8)
    {
        return "clang.exe"_a8;
    }
    if (baseName == "clang++"_a8)
    {
        return "clang++.exe"_a8;
    }
    if (baseName == "clang-format"_a8)
    {
        return "clang-format.exe"_a8;
    }
    if (baseName == "lld"_a8)
    {
        return "lld.exe"_a8;
    }
    if (baseName == "llvm-ar"_a8)
    {
        return "llvm-ar.exe"_a8;
    }
#endif
    return baseName;
}

static Result resolveHostLLVMArchive(StringView packageName, StringView packagesCacheDirectory,
                                     StringView packagesInstallDirectory, Download& download, Package& package,
                                     StringView& archiveRoot)
{
    download                          = {};
    download.packagesCacheDirectory   = packagesCacheDirectory;
    download.packagesInstallDirectory = packagesInstallDirectory;
    download.packageName              = packageName;
    download.packageVersion           = "20.1.8";
    download.hashType                 = Hashing::TypeSHA256;

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
            archiveRoot              = "LLVM-20.1.8-macOS-ARM64";
            return Result(true);
        case InstructionSet::Intel64:
            return Result::Error("Automatic LLVM install is unavailable on Intel macOS because recent official LLVM "
                                 "releases no longer ship Intel macOS archives. Install llvm@20 with Homebrew.");
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
            archiveRoot              = "LLVM-20.1.8-Linux-ARM64";
            return Result(true);
        case InstructionSet::Intel64:
            download.packagePlatform = "linux_intel64";
            download.url             = "https://github.com/llvm/llvm-project/releases/download/llvmorg-20.1.8/"
                                       "LLVM-20.1.8-Linux-X64.tar.xz";
            download.expectedHash    = "1ead36b3dfcb774b57be530df42bec70ab2d239fbce9889447c7a29a4ddc1ae6";
            package.packageBaseName  = "LLVM-20.1.8-Linux-X64.tar.xz";
            archiveRoot              = "LLVM-20.1.8-Linux-X64";
            return Result(true);
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
            archiveRoot              = "clang+llvm-20.1.8-aarch64-pc-windows-msvc";
            return Result(true);
        case InstructionSet::Intel64:
            download.packagePlatform = "windows_intel64";
            download.url             = "https://github.com/llvm/llvm-project/releases/download/llvmorg-20.1.8/"
                                       "clang%2Bllvm-20.1.8-x86_64-pc-windows-msvc.tar.xz";
            download.expectedHash    = "f229769f11d6a6edc8ada599c0cda964b7dee6ab1a08c6cf9dd7f513e85b107f";
            package.packageBaseName  = "clang+llvm-20.1.8-x86_64-pc-windows-msvc.tar.xz";
            archiveRoot              = "clang+llvm-20.1.8-x86_64-pc-windows-msvc";
            return Result(true);
        case InstructionSet::Intel32: return Result::Error("Unsupported platform");
        }
        break;
    }
    case Platform::Emscripten: return Result::Error("Unsupported platform");
    }
    Assert::unreachable();
}

Result installLLVMToolchain(StringView packagesCacheDirectory, StringView packagesInstallDirectory, Package& package)
{
    PackageRecipe recipe;
    recipe.functions.keepDownloadedArchive = false;
    StringView archiveRoot                 = {};
    SC_TRY(resolveHostLLVMArchive("llvm"_a8, packagesCacheDirectory, packagesInstallDirectory, recipe.download,
                                  recipe.package, archiveRoot));

    static constexpr StringView wantedVersion = "20.1.8";
    recipe.functions.extractFunction          = [](StringView sourceFile, StringView destinationDirectory)
    { return extractTarArchiveFlatteningRoot(sourceFile, destinationDirectory); };

    recipe.functions.testFunction = [](const Download&, const Package& package)
    {
        String  clangExecutable  = StringEncoding::Utf8;
        String  llvmArExecutable = StringEncoding::Utf8;
        String  result           = StringEncoding::Utf8;
        Process process;
        SC_TRY(Path::join(clangExecutable,
                          {package.installDirectoryLink.view(), "bin", hostLLVMExecutableName("clang"_a8)}));
        SC_TRY(process.exec({clangExecutable.view(), "--version"}, result));
        SC_TRY_MSG(process.getExitStatus() == 0, "LLVM clang returned error");
        SC_TRY_MSG(StringView(result.view()).containsString("clang version"), "LLVM clang version missing");
        SC_TRY_MSG(StringView(result.view()).containsString(wantedVersion), "LLVM clang version doesn't match");

        result = "";
        Process process2;
        SC_TRY(Path::join(llvmArExecutable,
                          {package.installDirectoryLink.view(), "bin", hostLLVMExecutableName("llvm-ar"_a8)}));
        SC_TRY(process2.exec({llvmArExecutable.view(), "--version"}, result));
        SC_TRY_MSG(process2.getExitStatus() == 0, "LLVM archiver returned error");
        return Result(true);
    };
    String clang   = StringEncoding::Utf8;
    String clangxx = StringEncoding::Utf8;
    String llvmAr  = StringEncoding::Utf8;
    String lld     = StringEncoding::Utf8;
    SC_TRY(Path::join(clang, {"bin", hostLLVMExecutableName("clang"_a8)}));
    SC_TRY(Path::join(clangxx, {"bin", hostLLVMExecutableName("clang++"_a8)}));
    SC_TRY(Path::join(llvmAr, {"bin", hostLLVMExecutableName("llvm-ar"_a8)}));
    SC_TRY(Path::join(lld, {"bin", HostPlatform == Platform::Windows ? "lld.exe"_a8 : "ld.lld"_a8}));
    const PackageReceiptExport exports[] = {
        {"tool", PackageExport::Clang, clang.view()},
        {"tool", PackageExport::ClangXX, clangxx.view()},
        {"tool", PackageExport::LLVMAr, llvmAr.view()},
        {"tool", PackageExport::LLVMLinker, lld.view()},
        {"capability", PackageCapability::ToolCCompiler, clang.view()},
        {"capability", PackageCapability::ToolCXXCompiler, clangxx.view()},
        {"capability", PackageCapability::ToolArchiver, llvmAr.view()},
        {"capability", PackageCapability::ToolLinker, lld.view()},
    };
    static constexpr StringView phases[] = {
        "resolveHostLLVMArchive",
        "extractLLVMToolchain",
        "validateLLVMToolchain",
        "writeReceipt",
    };
    recipe.exports = exports;
    recipe.phases  = phases;
    return installPackageRecipe(recipe, package);
}

#if SC_PLATFORM_LINUX
static Result resolveFilCRawCompilerPath(StringView packageRoot, StringView executableName, String& output)
{
    SC_TRY(Path::join(output, {packageRoot, "build", "bin", executableName}));
    return Result(true);
}

static Result resolveFilCCompilerPath(StringView packageRoot, StringView executableName, String& output)
{
    FileSystem fs;
    SC_TRY(fs.init("."));

    String wrapperPath = StringEncoding::Utf8;
    SC_TRY(Path::join(wrapperPath, {packageRoot, "sc-filc", "bin", executableName}));
    if (fs.existsAndIsFile(wrapperPath.view()))
    {
        SC_TRY(output.assign(wrapperPath.view()));
        return Result(true);
    }

    return resolveFilCRawCompilerPath(packageRoot, executableName, output);
}

static Result probeFilCCompiler(StringView compilerPath, String& versionOutput)
{
    String compilerDirectory = StringEncoding::Utf8;
    SC_TRY(compilerDirectory.assign(Path::dirname(compilerPath, Path::AsNative)));

    Process process;
    SC_TRY(process.setWorkingDirectory(compilerDirectory.view()));
    SC_TRY(process.exec({compilerPath, "--version"}, versionOutput));
    SC_TRY_MSG(process.getExitStatus() == 0, "Fil-C compiler returned error");
    SC_TRY_MSG(StringView(versionOutput.view()).containsString("Fil-C"), "Fil-C compiler marker missing");
    SC_TRY_MSG(StringView(versionOutput.view()).containsString("Target:"), "Fil-C target triple missing");
    return Result(true);
}

static Result extractVersionLineSuffix(StringView versionOutput, StringView prefix, String& value)
{
    StringViewTokenizer lines(versionOutput);
    while (lines.tokenizeNextLine())
    {
        StringView line = lines.component.trimWhiteSpaces();
        if (line.startsWith(prefix))
        {
            SC_TRY(value.assign(line.sliceStart(prefix.sizeInBytes()).trimWhiteSpaces()));
            return Result(true);
        }
    }
    return Result::Error("Missing Fil-C metadata line");
}

static Result writeFilCPackageMetadata(StringView packageRoot, StringView version, StringView flavor,
                                       StringView compilerC, StringView compilerCpp, StringView linker,
                                       StringView archiver, StringView targetTriple)
{
    String metadataPath = StringEncoding::Utf8;
    SC_TRY(Path::join(metadataPath, {packageRoot, "sc-filc-package.json"}));

    String metadata = StringEncoding::Utf8;
    auto   builder  = StringBuilder::create(metadata);
    SC_TRY(builder.append("{\n"));
    SC_TRY(builder.append("  \"version\": \"{}\",\n", version));
    SC_TRY(builder.append("  \"flavor\": \"{}\",\n", flavor));
    SC_TRY(builder.append("  \"compilerC\": \"{}\",\n", compilerC));
    SC_TRY(builder.append("  \"compilerCpp\": \"{}\",\n", compilerCpp));
    SC_TRY(builder.append("  \"linker\": \"{}\",\n", linker));
    SC_TRY(builder.append("  \"archiver\": \"{}\",\n", archiver));
    SC_TRY(builder.append("  \"targetTriple\": \"{}\"\n", targetTriple));
    SC_TRY(builder.append("}\n"));
    builder.finalize();

    FileSystem fs;
    SC_TRY(fs.init("."));
    SC_TRY(fs.writeString(metadataPath.view(), metadata.view()));
    return Result(true);
}

static Result writeFilCCompilerWrapperScript(FileSystem& fs, StringView scriptPath, StringView launcherPath,
                                             StringView compilerPath)
{
    String script  = StringEncoding::Utf8;
    auto   builder = StringBuilder::create(script);
    SC_TRY(builder.append("#!/bin/sh\n"));
    SC_TRY(builder.append("COMPILER_PATH=\"{}\"\n", compilerPath));
    SC_TRY(builder.append("COMPILER_DIR=$(dirname \"$COMPILER_PATH\")\n"));
    SC_TRY(builder.append("cd \"$COMPILER_DIR\" || exit 1\n"));
    SC_TRY(builder.append("exec \"{}\" \"$COMPILER_PATH\" \"$@\"\n", launcherPath));
    builder.finalize();
    SC_TRY(fs.writeString(scriptPath, script.view()));
    SC_TRY(fs.chmod(scriptPath, 0755));
    return Result(true);
}

static Result prepareFilCCompilerLaunchers(StringView packageRoot)
{
    FileSystem fs;
    SC_TRY(fs.init("."));

    if (HostPlatform != Platform::Linux or HostInstructionSet != InstructionSet::ARM64)
    {
        return Result(true);
    }

    static constexpr StringView rosettaPath = "/media/psf/RosettaLinux/rosetta";
    if (not fs.existsAndIsFile(rosettaPath))
    {
        return Result(true);
    }

    String rawCompilerC   = StringEncoding::Utf8;
    String rawCompilerCpp = StringEncoding::Utf8;
    String wrapperRoot    = StringEncoding::Utf8;
    String wrapperC       = StringEncoding::Utf8;
    String wrapperCpp     = StringEncoding::Utf8;
    SC_TRY(resolveFilCRawCompilerPath(packageRoot, "clang", rawCompilerC));
    SC_TRY(resolveFilCRawCompilerPath(packageRoot, "clang++", rawCompilerCpp));
    SC_TRY(Path::join(wrapperRoot, {packageRoot, "sc-filc", "bin"}));
    SC_TRY(Path::join(wrapperC, {wrapperRoot.view(), "clang"}));
    SC_TRY(Path::join(wrapperCpp, {wrapperRoot.view(), "clang++"}));
    SC_TRY(fs.makeDirectoryRecursive(wrapperRoot.view()));
    SC_TRY(writeFilCCompilerWrapperScript(fs, wrapperC.view(), rosettaPath, rawCompilerC.view()));
    SC_TRY(writeFilCCompilerWrapperScript(fs, wrapperCpp.view(), rosettaPath, rawCompilerCpp.view()));
    return Result(true);
}

static Result ensureFilCPackagePrepared(StringView packageRoot)
{
    FileSystem fs;
    SC_TRY(fs.init("."));

    String compilerCpp = StringEncoding::Utf8;
    SC_TRY(resolveFilCRawCompilerPath(packageRoot, "clang++", compilerCpp));
    if (fs.existsAndIsFile(compilerCpp.view()))
    {
        return Result(true);
    }

    String setupPath = StringEncoding::Utf8;
    SC_TRY(Path::join(setupPath, {packageRoot, "setup.sh"}));
    SC_TRY_MSG(fs.existsAndIsFile(setupPath.view()), "Fil-C package is missing setup.sh");

    Process process;
    SC_TRY(process.setWorkingDirectory(packageRoot));
    SC_TRY(process.exec({"sh", "./setup.sh"}));
    SC_TRY_MSG(process.getExitStatus() == 0, "Fil-C setup.sh failed");
    SC_TRY_MSG(fs.existsAndIsFile(compilerCpp.view()), "Fil-C setup did not produce build/bin/clang++");
    return Result(true);
}

static Result testFilCToolchain(const Package& package, String* detectedVersion = nullptr,
                                String* detectedTarget = nullptr)
{
    String compilerC    = StringEncoding::Utf8;
    String compilerCpp  = StringEncoding::Utf8;
    String versionOut   = StringEncoding::Utf8;
    String version      = StringEncoding::Utf8;
    String targetTriple = StringEncoding::Utf8;
    SC_TRY(resolveFilCCompilerPath(package.installDirectoryLink.view(), "clang", compilerC));
    SC_TRY(resolveFilCCompilerPath(package.installDirectoryLink.view(), "clang++", compilerCpp));
    SC_TRY(probeFilCCompiler(compilerCpp.view(), versionOut));
    SC_TRY(extractVersionLineSuffix(versionOut.view(), "Fil-C "_a8, version));
    SC_TRY(extractVersionLineSuffix(versionOut.view(), "Target:"_a8, targetTriple));
    SC_TRY_MSG(targetTriple == "x86_64-unknown-linux-gnu",
               "Fil-C package target triple is unsupported; expected x86_64-unknown-linux-gnu");

    Process process;
    String  cVersionOut   = StringEncoding::Utf8;
    String  cTargetTriple = StringEncoding::Utf8;
    SC_TRY(process.exec({compilerC.view(), "--version"}, cVersionOut));
    SC_TRY_MSG(process.getExitStatus() == 0, "Fil-C C compiler returned error");
    SC_TRY_MSG(StringView(cVersionOut.view()).containsString("Fil-C"), "Fil-C C compiler marker missing");
    SC_TRY(extractVersionLineSuffix(cVersionOut.view(), "Target:"_a8, cTargetTriple));
    SC_TRY_MSG(cTargetTriple == targetTriple, "Fil-C C and C++ compiler target triples do not match");

    if (detectedVersion)
    {
        SC_TRY(detectedVersion->assign(version.view()));
    }
    if (detectedTarget)
    {
        SC_TRY(detectedTarget->assign(targetTriple.view()));
    }
    return Result(true);
}

Result installFilCToolchain(StringView packagesCacheDirectory, StringView packagesInstallDirectory, Package& package,
                            StringView importDirectory)
{

    static constexpr StringView packageVersion = "0.678";
    static constexpr StringView packageFlavor  = "pizfix";
    static constexpr StringView packageURL =
        "https://github.com/pizlonator/fil-c/releases/download/v0.678/filc-0.678-linux-x86_64.tar.xz";
    static constexpr StringView packageHash = "8c515f704b3ba524566847d78a8c324708a64d0eefadabb40094bc5130aa8995";

    package.packageFullName       = "filc-0.678-linux-x86_64-pizfix";
    package.packageBaseName       = "filc-0.678-linux-x86_64.tar.xz";
    package.packageLocalFile      = format("{}/filc/{}", packagesCacheDirectory, package.packageBaseName.view());
    package.packageLocalDirectory = format("{}/filc/pizfix-{}-linux-x86_64", packagesCacheDirectory, packageVersion);
    package.packageLocalTxt      = format("{}/filc/pizfix-{}-linux-x86_64.txt", packagesCacheDirectory, packageVersion);
    package.installDirectoryLink = format("{}/filc_linux_x86_64", packagesInstallDirectory);

    FileSystem fs;
    SC_TRY(fs.init("."));
    SC_TRY(fs.makeDirectoryRecursive(packagesCacheDirectory));
    SC_TRY(fs.makeDirectoryRecursive(packagesInstallDirectory));
    SC_TRY(fs.makeDirectoryRecursive(Path::dirname(package.packageLocalFile.view(), Path::AsNative)));

    String resolvedImportDirectory = StringEncoding::Utf8;
    if (not importDirectory.isEmpty())
    {
        SC_TRY(resolvedImportDirectory.assign(importDirectory));
    }
    else if (fs.existsAndIsFile(package.packageLocalTxt.view()))
    {
        String metadata = StringEncoding::Utf8;
        SC_TRY(fs.read(package.packageLocalTxt.view(), metadata));

        StringView          urlValue;
        StringViewTokenizer lines(metadata.view());
        while (lines.tokenizeNextLine())
        {
            StringView line = lines.component.trimWhiteSpaces();
            if (line.startsWith("SC_PACKAGE_URL=import:"))
            {
                SC_TRY(line.splitAfter("SC_PACKAGE_URL=import:", urlValue));
                SC_TRY(resolvedImportDirectory.assign(urlValue));
                break;
            }
            if (line.startsWith("SC_PACKAGE_URL=/"))
            {
                SC_TRY(line.splitAfter("SC_PACKAGE_URL=", urlValue));
                SC_TRY(resolvedImportDirectory.assign(urlValue));
                break;
            }
        }
    }

    const bool importing        = not resolvedImportDirectory.isEmpty();
    String     sourceIdentifier = StringEncoding::Utf8;
    if (importing)
    {
        SC_TRY(StringBuilder::format(sourceIdentifier, "import:{}", resolvedImportDirectory.view()));
    }
    else
    {
        SC_TRY(sourceIdentifier.assign(packageURL));
    }

    String activePackageRoot = StringEncoding::Utf8;
    SC_TRY(activePackageRoot.assign(importing ? resolvedImportDirectory.view() : package.packageLocalDirectory.view()));

    auto metadataMatches = [&](bool& matches) -> Result
    {
        matches = false;
        if (not fs.existsAndIsFile(package.packageLocalTxt.view()))
        {
            return Result(true);
        }

        String metadata = StringEncoding::Utf8;
        SC_TRY(fs.read(package.packageLocalTxt.view(), metadata));

        StringView          urlValue;
        StringView          flavorValue;
        StringViewTokenizer lines(metadata.view());
        while (lines.tokenizeNextLine())
        {
            StringView line = lines.component.trimWhiteSpaces();
            if (line.startsWith("SC_PACKAGE_URL="))
            {
                SC_TRY(line.splitAfter("SC_PACKAGE_URL=", urlValue));
            }
            else if (line.startsWith("SC_PACKAGE_FLAVOR="))
            {
                SC_TRY(line.splitAfter("SC_PACKAGE_FLAVOR=", flavorValue));
            }
        }
        matches = flavorValue == packageFlavor and
                  (urlValue == sourceIdentifier.view() or (importing and urlValue == resolvedImportDirectory.view()));
        return Result(true);
    };

    bool needsInstall = true;
    if (fs.existsAndIsDirectory(activePackageRoot.view()))
    {
        SC_TRY(prepareFilCCompilerLaunchers(activePackageRoot.view()));
        SC_TRY(finalizeInstalledPackageFromRoot(activePackageRoot.view(), package));
        bool matches = false;
        SC_TRY(metadataMatches(matches));
        if (matches and testFilCToolchain(package))
        {
            needsInstall = false;
        }
    }

    if (needsInstall)
    {
        if (not importing and fs.existsAndIsDirectory(package.packageLocalDirectory.view()))
        {
            SC_TRY(fs.removeDirectoriesRecursive(package.packageLocalDirectory.view()));
        }

        if (importing)
        {
            SC_TRY_MSG(fs.existsAndIsDirectory(resolvedImportDirectory.view()),
                       "Imported Fil-C toolchain directory does not exist");
        }
        else
        {
            SC_TRY(downloadFileHash(packageURL, package.packageLocalFile.view(), Hashing::TypeSHA256, packageHash));
            SC_TRY(
                extractTarArchiveFlatteningRoot(package.packageLocalFile.view(), package.packageLocalDirectory.view()));
        }

        SC_TRY(ensureFilCPackagePrepared(activePackageRoot.view()));
        SC_TRY(prepareFilCCompilerLaunchers(activePackageRoot.view()));
        SC_TRY(finalizeInstalledPackageFromRoot(activePackageRoot.view(), package));

        String compilerC       = StringEncoding::Utf8;
        String compilerCpp     = StringEncoding::Utf8;
        String archiver        = StringEncoding::Utf8;
        String detectedVersion = StringEncoding::Utf8;
        String detectedTarget  = StringEncoding::Utf8;
        SC_TRY(resolveFilCCompilerPath(package.installDirectoryLink.view(), "clang", compilerC));
        SC_TRY(resolveFilCCompilerPath(package.installDirectoryLink.view(), "clang++", compilerCpp));
        SC_TRY(resolveHostCommandPath("ar", archiver));
        const Result filcValidation = testFilCToolchain(package, &detectedVersion, &detectedTarget);
        if (not filcValidation)
        {
            SC_TRY(removePackageInstallLink(fs, package));
            if (not importing and fs.existsAndIsDirectory(package.packageLocalDirectory.view()))
            {
                SC_TRY(fs.removeDirectoriesRecursive(package.packageLocalDirectory.view()));
            }
            return filcValidation;
        }
        SC_TRY(writeFilCPackageMetadata(activePackageRoot.view(), detectedVersion.view(), packageFlavor,
                                        compilerC.view(), compilerCpp.view(), compilerCpp.view(), archiver.view(),
                                        detectedTarget.view()));

        String metadata = StringEncoding::Utf8;
        auto   builder  = StringBuilder::create(metadata);
        SC_TRY(builder.append("SC_PACKAGE_URL={}\n", sourceIdentifier.view()));
        if (not importing)
        {
            SC_TRY(builder.append("SC_PACKAGE_HASH=sha256:{}\n", packageHash));
        }
        SC_TRY(builder.append("SC_PACKAGE_FLAVOR={}\n", packageFlavor));
        SC_TRY(builder.append("SC_PACKAGE_VERSION={}\n", detectedVersion.view()));
        SC_TRY(builder.append("SC_PACKAGE_TARGET={}\n", detectedTarget.view()));
        builder.finalize();
        SC_TRY(fs.makeDirectoryRecursive(Path::dirname(package.packageLocalTxt.view(), Path::AsNative)));
        SC_TRY(fs.writeString(package.packageLocalTxt.view(), metadata.view()));
    }

    const PackageReceiptExport exports[] = {
        {"tool", PackageExport::Clang, "sc-filc/bin/clang"},
        {"tool", PackageExport::ClangXX, "sc-filc/bin/clang++"},
        {"capability", PackageCapability::ToolchainFilCX86_64, "sc-filc/bin/clang"},
    };
    static constexpr StringView phases[] = {
        "resolveFilCSource",
        "prepareFilCCompilerLaunchers",
        "validateFilCToolchain",
        "writeReceipt",
    };
    SC_TRY(writeManualPackageReceipt(
        package, "filc", packageVersion, packageFlavor, sourceIdentifier.view(),
        importing ? StringView() : "sha256:8c515f704b3ba524566847d78a8c324708a64d0eefadabb40094bc5130aa8995"_a8,
        exports, phases));
    return Result(true);
}
#else
Result installFilCToolchain(StringView, StringView, Package&, StringView)
{
    return Result::Error("Fil-C package install is only supported on Linux hosts");
}
#endif

Result installLLVMMingwToolchain(StringView packagesCacheDirectory, StringView packagesInstallDirectory,
                                 Package& package)
{
    static constexpr StringView packageVersion = "20260324";
    static constexpr StringView llvmVersion    = "22.1.2";

    PackageRecipe recipe;
    recipe.functions.keepDownloadedArchive   = false;
    recipe.download.packagesCacheDirectory   = packagesCacheDirectory;
    recipe.download.packagesInstallDirectory = packagesInstallDirectory;
    recipe.download.packageName              = "llvm-mingw";
    recipe.download.packageVersion           = packageVersion;
    recipe.download.hashType                 = Hashing::TypeSHA256;

    switch (HostPlatform)
    {
    case Platform::Apple:
        recipe.download.packagePlatform = "macos_universal";
        recipe.download.url             = "https://github.com/mstorsjo/llvm-mingw/releases/download/20260324/"
                                          "llvm-mingw-20260324-ucrt-macos-universal.tar.xz";
        recipe.download.expectedHash    = "1834ad45eb1a26c8bf3aa6137bc79db12fa1ef368af3ed0bbfba7c60adbe2fa6";
        recipe.package.packageBaseName  = "llvm-mingw-20260324-ucrt-macos-universal.tar.xz";
        break;
    case Platform::Linux:
        switch (HostInstructionSet)
        {
        case InstructionSet::ARM64:
            recipe.download.packagePlatform = "linux_arm64";
            recipe.download.url             = "https://github.com/mstorsjo/llvm-mingw/releases/download/20260324/"
                                              "llvm-mingw-20260324-ucrt-ubuntu-22.04-aarch64.tar.xz";
            recipe.download.expectedHash    = "d28db713552e9d92699081b573a5b7c543d1d8095ed0d1c15dba184bf6e51440";
            recipe.package.packageBaseName  = "llvm-mingw-20260324-ucrt-ubuntu-22.04-aarch64.tar.xz";
            break;
        case InstructionSet::Intel64:
            recipe.download.packagePlatform = "linux_intel64";
            recipe.download.url             = "https://github.com/mstorsjo/llvm-mingw/releases/download/20260324/"
                                              "llvm-mingw-20260324-ucrt-ubuntu-22.04-x86_64.tar.xz";
            recipe.download.expectedHash    = "f92b02c4f835470deb5ac5fb92ddb458239e80ddff9ce8867155679ee5f57ffc";
            recipe.package.packageBaseName  = "llvm-mingw-20260324-ucrt-ubuntu-22.04-x86_64.tar.xz";
            break;
        case InstructionSet::Intel32: return Result::Error("Unsupported platform");
        }
        break;
    case Platform::Windows: return Result::Error("Automatic llvm-mingw install is not supported on Windows hosts yet");
    case Platform::Emscripten: return Result::Error("Unsupported platform");
    }

    recipe.functions.extractFunction = [](StringView sourceFile, StringView destinationDirectory)
    { return extractTarArchiveFlatteningRoot(sourceFile, destinationDirectory); };

    recipe.functions.testFunction = [](const Download&, const Package& package)
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

    String x64Compiler      = StringEncoding::Utf8;
    String x64CompilerCpp   = StringEncoding::Utf8;
    String arm64Compiler    = StringEncoding::Utf8;
    String arm64CompilerCpp = StringEncoding::Utf8;
    String archiver         = StringEncoding::Utf8;
    SC_TRY(Path::join(x64Compiler, {"bin", "x86_64-w64-mingw32-clang"}));
    SC_TRY(Path::join(x64CompilerCpp, {"bin", "x86_64-w64-mingw32-clang++"}));
    SC_TRY(Path::join(arm64Compiler, {"bin", "aarch64-w64-mingw32-clang"}));
    SC_TRY(Path::join(arm64CompilerCpp, {"bin", "aarch64-w64-mingw32-clang++"}));
    SC_TRY(Path::join(archiver, {"bin", "llvm-ar"}));
    const PackageReceiptExport exports[] = {
        {"tool", PackageExport::LLVMMinGWClang_X86_64, x64Compiler.view()},
        {"tool", PackageExport::LLVMMinGWClangXX_X86_64, x64CompilerCpp.view()},
        {"tool", PackageExport::LLVMMinGWClangArm64, arm64Compiler.view()},
        {"tool", PackageExport::LLVMMinGWClangXXArm64, arm64CompilerCpp.view()},
        {"tool", PackageExport::LLVMAr, archiver.view()},
        {"capability", PackageCapability::ToolchainWindowsGNUX86_64, x64Compiler.view()},
        {"capability", PackageCapability::ToolchainWindowsGNUArm64, arm64Compiler.view()},
    };
    static constexpr StringView phases[] = {
        "resolveLLVMMingwArchive",
        "extractLLVMMingwToolchain",
        "validateLLVMMingwToolchain",
        "writeReceipt",
    };
    recipe.exports = exports;
    recipe.phases  = phases;
    return installPackageRecipe(recipe, package);
}

static constexpr StringView linuxSysrootArchitectureName(InstructionSet architecture)
{
    switch (architecture)
    {
    case InstructionSet::Intel64: return "x86_64"_a8;
    case InstructionSet::ARM64: return "arm64"_a8;
    case InstructionSet::Intel32: return "intel32"_a8;
    }
    Assert::unreachable();
}

static constexpr StringView linuxSysrootAlpineArchitectureName(InstructionSet architecture)
{
    switch (architecture)
    {
    case InstructionSet::Intel64: return "x86_64"_a8;
    case InstructionSet::ARM64: return "aarch64"_a8;
    case InstructionSet::Intel32: return {};
    }
    Assert::unreachable();
}

static Result linuxSysrootPackagePathExists(StringView packageRoot, StringView relativePath)
{
    FileSystem fs;
    SC_TRY(fs.init("."));
    String absolutePath = StringEncoding::Utf8;
    SC_TRY(Path::join(absolutePath, {packageRoot, relativePath}));
    SC_TRY_MSG(fs.exists(absolutePath.view()), "Linux sysroot package is incomplete");
    return Result(true);
}

static Result repairLinuxGlibcSysrootLayout(StringView packageRoot, InstructionSet architecture)
{
    if (architecture != InstructionSet::Intel64)
    {
        return Result(true);
    }

    FileSystem fs;
    SC_TRY(fs.init("."));

    String loaderDirectory = StringEncoding::Utf8;
    String loaderAlias     = StringEncoding::Utf8;
    SC_TRY(Path::join(loaderDirectory, {packageRoot, "lib64"}));
    SC_TRY(Path::join(loaderAlias, {loaderDirectory.view(), "ld-linux-x86-64.so.2"}));
    SC_TRY(fs.makeDirectoryRecursive(loaderDirectory.view()));
    SC_TRY(fs.removeLinkIfExists(loaderAlias.view()));
    if (not createLink("../lib/x86_64-linux-gnu/ld-linux-x86-64.so.2", loaderAlias.view()))
    {
        String targetLoader = StringEncoding::Utf8;
        SC_TRY(Path::join(targetLoader, {packageRoot, "lib", "x86_64-linux-gnu", "ld-linux-x86-64.so.2"}));
        if (fs.existsAndIsFile(targetLoader.view()))
        {
            SC_TRY(fs.copyFile(targetLoader.view(), loaderAlias.view(), FileSystem::CopyFlags().setOverwrite(true)));
        }
    }
    return Result(true);
}

static Result repairLinuxSysrootLayout(StringView packageRoot, const Tools::LinuxSysrootSpec& spec)
{
    if (spec.environment == Tools::LinuxSysrootSpec::Glibc)
    {
        SC_TRY(repairLinuxGlibcSysrootLayout(packageRoot, spec.architecture));
    }
    return Result(true);
}

static Result validateLinuxSysrootPackage(StringView packageRoot, const Tools::LinuxSysrootSpec& spec)
{
    switch (spec.environment)
    {
    case Tools::LinuxSysrootSpec::Glibc:
        if (spec.architecture == InstructionSet::Intel64)
        {
            SC_TRY(linuxSysrootPackagePathExists(packageRoot, "usr/include/stdio.h"));
            SC_TRY(linuxSysrootPackagePathExists(packageRoot, "usr/lib/x86_64-linux-gnu/Scrt1.o"));
            SC_TRY(linuxSysrootPackagePathExists(packageRoot, "lib64/ld-linux-x86-64.so.2"));
            SC_TRY(linuxSysrootPackagePathExists(packageRoot, "usr/lib/gcc/x86_64-linux-gnu/11/crtbeginS.o"));
            SC_TRY(linuxSysrootPackagePathExists(packageRoot, "usr/lib/gcc/x86_64-linux-gnu/11/libgcc.a"));
        }
        else if (spec.architecture == InstructionSet::ARM64)
        {
            SC_TRY(linuxSysrootPackagePathExists(packageRoot, "usr/aarch64-linux-gnu/include/stdio.h"));
            SC_TRY(linuxSysrootPackagePathExists(packageRoot, "usr/aarch64-linux-gnu/lib/ld-linux-aarch64.so.1"));
            SC_TRY(linuxSysrootPackagePathExists(packageRoot, "usr/lib/gcc-cross/aarch64-linux-gnu/11/crtbeginS.o"));
            SC_TRY(linuxSysrootPackagePathExists(packageRoot, "usr/lib/gcc-cross/aarch64-linux-gnu/11/libgcc.a"));
        }
        else
        {
            return Result::Error("Unsupported Linux glibc sysroot architecture");
        }
        break;
    case Tools::LinuxSysrootSpec::Musl:
        if (spec.architecture == InstructionSet::Intel64)
        {
            SC_TRY(linuxSysrootPackagePathExists(packageRoot, "lib/ld-musl-x86_64.so.1"));
            SC_TRY(linuxSysrootPackagePathExists(packageRoot, "usr/include/stdio.h"));
            SC_TRY(linuxSysrootPackagePathExists(packageRoot, "usr/include/linux/io_uring.h"));
            SC_TRY(linuxSysrootPackagePathExists(packageRoot, "usr/lib/crt1.o"));
            SC_TRY(
                linuxSysrootPackagePathExists(packageRoot, "usr/lib/gcc/x86_64-alpine-linux-musl/15.2.0/crtbeginS.o"));
            SC_TRY(linuxSysrootPackagePathExists(packageRoot, "usr/lib/gcc/x86_64-alpine-linux-musl/15.2.0/libgcc.a"));
        }
        else if (spec.architecture == InstructionSet::ARM64)
        {
            SC_TRY(linuxSysrootPackagePathExists(packageRoot, "lib/ld-musl-aarch64.so.1"));
            SC_TRY(linuxSysrootPackagePathExists(packageRoot, "usr/include/stdio.h"));
            SC_TRY(linuxSysrootPackagePathExists(packageRoot, "usr/include/linux/io_uring.h"));
            SC_TRY(linuxSysrootPackagePathExists(packageRoot, "usr/lib/crt1.o"));
            SC_TRY(
                linuxSysrootPackagePathExists(packageRoot, "usr/lib/gcc/aarch64-alpine-linux-musl/15.2.0/crtbeginS.o"));
            SC_TRY(linuxSysrootPackagePathExists(packageRoot, "usr/lib/gcc/aarch64-alpine-linux-musl/15.2.0/libgcc.a"));
        }
        else
        {
            return Result::Error("Unsupported Linux musl sysroot architecture");
        }
        break;
    }
    return Result(true);
}

static Result writeLinuxSysrootReceipt(const Package& package, const Tools::LinuxSysrootSpec& spec, StringView source)
{
    const StringView environment  = spec.environment == Tools::LinuxSysrootSpec::Glibc ? "glibc"_a8 : "musl"_a8;
    const StringView architecture = linuxSysrootArchitectureName(spec.architecture);
    String           packageName  = StringEncoding::Utf8;
    String           capability   = StringEncoding::Utf8;
    String           includeDir   = StringEncoding::Utf8;
    String           libraryDir   = StringEncoding::Utf8;
    SC_TRY(StringBuilder::format(packageName, "linux-sysroot-{}-{}", environment, architecture));
    SC_TRY(StringBuilder::format(capability, "sysroot.linux.{}.{}", environment, architecture));
    if (spec.environment == Tools::LinuxSysrootSpec::Glibc and spec.architecture == InstructionSet::ARM64)
    {
        SC_TRY(includeDir.assign("usr/aarch64-linux-gnu/include"));
        SC_TRY(libraryDir.assign("usr/aarch64-linux-gnu/lib"));
    }
    else
    {
        SC_TRY(includeDir.assign("usr/include"));
        SC_TRY(libraryDir.assign("usr/lib"));
    }
    const PackageReceiptExport exports[] = {
        {"sysroot", PackageExport::Sysroot, "."},
        {"include-dir", "sysroot.include", includeDir.view()},
        {"library-dir", "sysroot.lib", libraryDir.view()},
        {"capability", capability.view(), "."},
    };
    static constexpr StringView phases[] = {
        "resolveLinuxSysrootPackages",
        "extractLinuxSysrootPackages",
        "repairLinuxSysrootLayout",
        "validateLinuxSysroot",
        "writeReceipt",
    };
    return writeManualPackageReceipt(package, packageName.view(), "system", architecture, source, StringView(), exports,
                                     phases);
}

static Result resolveUbuntuPackagesIndex(StringView downloadsDirectory, StringView indexURL, String& output)
{
    String packageIndexPath = StringEncoding::Utf8;
    SC_TRY(Path::join(packageIndexPath, {downloadsDirectory, "ubuntu-packages.txt"}));
    SC_TRY(downloadGzipTextFile(indexURL, packageIndexPath.view()));

    FileSystem fs;
    SC_TRY(fs.init("."));
    SC_TRY(fs.read(packageIndexPath.view(), output));
    return Result(true);
}

static Result resolveAlpinePackagesIndex(StringView downloadsDirectory, StringView architecture, String& output)
{
    String indexPath = StringEncoding::Utf8;
    String indexURL  = format("https://dl-cdn.alpinelinux.org/alpine/v3.23/main/{}/APKINDEX.tar.gz", architecture);
    SC_TRY(Path::join(indexPath, {downloadsDirectory, "APKINDEX.tar.gz"}));
    SC_TRY(downloadTextFile(indexURL.view(), indexPath.view()));

    Process process;
    String  indexContents = StringEncoding::Utf8;
    SC_TRY(process.exec({"tar", "-xOzf", indexPath.view(), "APKINDEX"}, indexContents));
    SC_TRY_MSG(process.getExitStatus() == 0, "Failed extracting Alpine APKINDEX");
    SC_TRY(output.assign(indexContents.view()));
    return Result(true);
}

static Result installLinuxGlibcSysroot(StringView packagesCacheDirectory, StringView packagesInstallDirectory,
                                       const Tools::LinuxSysrootSpec& spec, Package& package)
{
    SC_TRY_MSG(spec.architecture == InstructionSet::Intel64 or spec.architecture == InstructionSet::ARM64,
               "Linux glibc sysroots only support x86_64 and arm64");
    SC_TRY_MSG(supportsAutomaticLinuxSysrootInstall(),
               "Automatic Linux glibc sysroot install is only supported on macOS and Linux hosts");

    const StringView targetArchitecture = linuxSysrootArchitectureName(spec.architecture);
    package.packageFullName             = format("linux-sysroot-glibc-{}", targetArchitecture);
    package.packageLocalDirectory = format("{}/linux-sysroot/glibc-{}", packagesCacheDirectory, targetArchitecture);
    package.packageLocalTxt       = format("{}/linux-sysroot/glibc-{}.txt", packagesCacheDirectory, targetArchitecture);
    package.installDirectoryLink  = format("{}/linux-sysroot_glibc_{}", packagesInstallDirectory, targetArchitecture);

    auto finalizePackage = [&](FileSystem& fs) -> Result
    {
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
    };

    auto testPackage = [&](const Package& installedPackage) -> Result
    {
        SC_TRY(repairLinuxSysrootLayout(installedPackage.installDirectoryLink.view(), spec));
        return validateLinuxSysrootPackage(installedPackage.installDirectoryLink.view(), spec);
    };

    FileSystem fs;
    SC_TRY(fs.init("."));
    SC_TRY(fs.makeDirectoryRecursive(packagesCacheDirectory));
    SC_TRY(fs.makeDirectoryRecursive(packagesInstallDirectory));

    if (fs.existsAndIsDirectory(package.packageLocalDirectory.view()))
    {
        if (repairLinuxSysrootLayout(package.packageLocalDirectory.view(), spec) and finalizePackage(fs) and
            testPackage(package))
        {
            SC_TRY(writeLinuxSysrootReceipt(package, spec, "cached"));
            return Result(true);
        }
    }

    if (fs.existsAndIsDirectory(package.packageLocalDirectory.view()))
    {
        SC_TRY(fs.removeDirectoriesRecursive(package.packageLocalDirectory.view()));
    }
    SC_TRY(fs.makeDirectoryRecursive(package.packageLocalDirectory.view()));

    String downloadsDirectory = StringEncoding::Utf8;
    String downloadsLeaf      = format("glibc-{}", targetArchitecture);
    SC_TRY(
        Path::join(downloadsDirectory, {packagesCacheDirectory, "linux-sysroot", "downloads", downloadsLeaf.view()}));
    SC_TRY(fs.makeDirectoryRecursive(downloadsDirectory.view()));

    const StringView packageBaseURL = spec.architecture == InstructionSet::Intel64
                                          ? "https://archive.ubuntu.com/ubuntu"_a8
                                          : "https://ports.ubuntu.com/ubuntu-ports"_a8;
    const StringView packageIndexURL =
        spec.architecture == InstructionSet::Intel64
            ? "https://archive.ubuntu.com/ubuntu/dists/jammy/main/binary-amd64/Packages.gz"_a8
            : "https://ports.ubuntu.com/ubuntu-ports/dists/jammy/main/binary-arm64/Packages.gz"_a8;

    String packagesIndex = StringEncoding::Utf8;
    SC_TRY(resolveUbuntuPackagesIndex(downloadsDirectory.view(), packageIndexURL, packagesIndex));

    static constexpr StringView x64PackageNames[] = {
        "libc6"_a8,         "libc6-dev"_a8,  "linux-libc-dev"_a8,   "libgcc-s1"_a8,
        "libgcc-11-dev"_a8, "libstdc++6"_a8, "libstdc++-11-dev"_a8,
    };
    static constexpr StringView arm64PackageNames[] = {
        "libc6-arm64-cross"_a8,
        "libc6-dev-arm64-cross"_a8,
        "linux-libc-dev-arm64-cross"_a8,
        "libgcc-s1-arm64-cross"_a8,
        "libgcc-11-dev-arm64-cross"_a8,
        "libstdc++6-arm64-cross"_a8,
        "libstdc++-11-dev-arm64-cross"_a8,
    };

    const Span<const StringView> packageNames = spec.architecture == InstructionSet::Intel64
                                                    ? Span<const StringView>(x64PackageNames)
                                                    : Span<const StringView>(arm64PackageNames);

    String packageTxt      = StringEncoding::Utf8;
    auto   metadataBuilder = StringBuilder::create(packageTxt);
    for (size_t idx = 0; idx < packageNames.sizeInElements(); ++idx)
    {
        UbuntuPackageMetadata metadata;
        SC_TRY(resolveUbuntuPackageMetadata(packagesIndex.view(), packageNames[idx], metadata));

        String packageURL  = StringEncoding::Utf8;
        String packageFile = StringEncoding::Utf8;
        SC_TRY(StringBuilder::format(packageURL, "{}/{}", packageBaseURL, metadata.filename.view()));
        SC_TRY(Path::join(packageFile,
                          {downloadsDirectory.view(), Path::basename(metadata.filename.view(), Path::AsPosix)}));
        SC_TRY(downloadFileHash(packageURL.view(), packageFile.view(), Hashing::TypeSHA256, metadata.sha256.view()));
        SC_TRY(extractDebArchive(packageFile.view(), package.packageLocalDirectory.view()));
        SC_TRY(metadataBuilder.append("SC_PACKAGE_URL_{}={}\n", idx, packageURL.view()));
    }
    metadataBuilder.finalize();

    SC_TRY(repairLinuxSysrootLayout(package.packageLocalDirectory.view(), spec));
    SC_TRY(finalizePackage(fs));
    SC_TRY(testPackage(package));
    SC_TRY(fs.writeString(package.packageLocalTxt.view(), packageTxt.view()));
    SC_TRY(writeLinuxSysrootReceipt(package, spec, packageIndexURL));
    return Result(true);
}

static Result installLinuxMuslSysroot(StringView packagesCacheDirectory, StringView packagesInstallDirectory,
                                      const Tools::LinuxSysrootSpec& spec, Package& package)
{
    SC_TRY_MSG(spec.architecture == InstructionSet::Intel64 or spec.architecture == InstructionSet::ARM64,
               "Linux musl sysroots only support x86_64 and arm64");
    SC_TRY_MSG(supportsAutomaticLinuxSysrootInstall(),
               "Automatic Linux musl sysroot install is only supported on macOS and Linux hosts");

    const StringView targetArchitecture = linuxSysrootArchitectureName(spec.architecture);
    const StringView alpineArchitecture = linuxSysrootAlpineArchitectureName(spec.architecture);
    package.packageFullName             = format("linux-sysroot-musl-{}", targetArchitecture);
    package.packageLocalDirectory = format("{}/linux-sysroot/musl-{}", packagesCacheDirectory, targetArchitecture);
    package.packageLocalTxt       = format("{}/linux-sysroot/musl-{}.txt", packagesCacheDirectory, targetArchitecture);
    package.installDirectoryLink  = format("{}/linux-sysroot_musl_{}", packagesInstallDirectory, targetArchitecture);

    auto finalizePackage = [&](FileSystem& fs) -> Result
    {
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
    };

    auto testPackage = [&](const Package& installedPackage) -> Result
    {
        SC_TRY(repairLinuxSysrootLayout(installedPackage.installDirectoryLink.view(), spec));
        return validateLinuxSysrootPackage(installedPackage.installDirectoryLink.view(), spec);
    };

    FileSystem fs;
    SC_TRY(fs.init("."));
    SC_TRY(fs.makeDirectoryRecursive(packagesCacheDirectory));
    SC_TRY(fs.makeDirectoryRecursive(packagesInstallDirectory));

    if (fs.existsAndIsDirectory(package.packageLocalDirectory.view()))
    {
        if (repairLinuxSysrootLayout(package.packageLocalDirectory.view(), spec) and finalizePackage(fs) and
            testPackage(package))
        {
            SC_TRY(writeLinuxSysrootReceipt(package, spec, "cached"));
            return Result(true);
        }
    }

    if (fs.existsAndIsDirectory(package.packageLocalDirectory.view()))
    {
        SC_TRY(fs.removeDirectoriesRecursive(package.packageLocalDirectory.view()));
    }
    SC_TRY(fs.makeDirectoryRecursive(package.packageLocalDirectory.view()));

    String downloadsDirectory = StringEncoding::Utf8;
    String downloadsLeaf      = format("musl-{}", targetArchitecture);
    SC_TRY(
        Path::join(downloadsDirectory, {packagesCacheDirectory, "linux-sysroot", "downloads", downloadsLeaf.view()}));
    SC_TRY(fs.makeDirectoryRecursive(downloadsDirectory.view()));

    String packagesIndex = StringEncoding::Utf8;
    SC_TRY(resolveAlpinePackagesIndex(downloadsDirectory.view(), alpineArchitecture, packagesIndex));

    static constexpr StringView packageNames[] = {
        "musl"_a8, "musl-dev"_a8, "linux-headers"_a8, "libgcc"_a8, "gcc"_a8, "libstdc++"_a8, "libstdc++-dev"_a8,
    };

    String packageBaseURL = format("https://dl-cdn.alpinelinux.org/alpine/v3.23/main/{}", alpineArchitecture);

    String packageTxt      = StringEncoding::Utf8;
    auto   metadataBuilder = StringBuilder::create(packageTxt);
    for (size_t idx = 0; idx < Span<const StringView>(packageNames).sizeInElements(); ++idx)
    {
        AlpinePackageMetadata metadata;
        SC_TRY(resolveAlpinePackageMetadata(packagesIndex.view(), packageNames[idx], metadata));

        String packageURL  = StringEncoding::Utf8;
        String packageFile = StringEncoding::Utf8;
        SC_TRY(StringBuilder::format(packageURL, "{}/{}-{}.apk", packageBaseURL, packageNames[idx],
                                     metadata.version.view()));
        SC_TRY(Path::join(packageFile, {downloadsDirectory.view(), Path::basename(packageURL.view(), Path::AsPosix)}));
        SC_TRY(downloadTextFile(packageURL.view(), packageFile.view()));
        SC_TRY(extractApkArchive(packageFile.view(), package.packageLocalDirectory.view()));
        SC_TRY(metadataBuilder.append("SC_PACKAGE_URL_{}={}\n", idx, packageURL.view()));
    }
    metadataBuilder.finalize();

    SC_TRY(repairLinuxSysrootLayout(package.packageLocalDirectory.view(), spec));
    SC_TRY(finalizePackage(fs));
    SC_TRY(testPackage(package));
    SC_TRY(fs.writeString(package.packageLocalTxt.view(), packageTxt.view()));
    SC_TRY(writeLinuxSysrootReceipt(package, spec, packageBaseURL.view()));
    return Result(true);
}

Result installLinuxSysroot(StringView packagesCacheDirectory, StringView packagesInstallDirectory,
                           const Tools::LinuxSysrootSpec& spec, Package& package)
{
    switch (spec.environment)
    {
    case Tools::LinuxSysrootSpec::Glibc:
        return installLinuxGlibcSysroot(packagesCacheDirectory, packagesInstallDirectory, spec, package);
    case Tools::LinuxSysrootSpec::Musl:
        return installLinuxMuslSysroot(packagesCacheDirectory, packagesInstallDirectory, spec, package);
    }
    Assert::unreachable();
}

Result installLinuxWineRunner(StringView packagesCacheDirectory, StringView packagesInstallDirectory, Package& package)
{
#if SC_PLATFORM_LINUX
    static constexpr StringView libunwindURL =
        "https://archive.ubuntu.com/ubuntu/pool/main/libu/libunwind/libunwind8_1.6.2-3build1_amd64.deb";
    static constexpr StringView libunwindHash = "658977d18976149b75391850ba0ccacaf7bde3201f0284189da50cd634334d17";
    static constexpr StringView wineBinaryURL =
        "https://dl.winehq.org/wine-builds/ubuntu/pool/main/w/wine/wine-stable-amd64_11.0.0.0~noble-1_amd64.deb";
    static constexpr StringView wineBinaryHash = "6cb835e2171b5572b17f1c06729735c2c7e40178239d7fa6c29ef14bd9b40d16";
    static constexpr StringView wineSupportURL =
        "https://dl.winehq.org/wine-builds/ubuntu/pool/main/w/wine/wine-stable_11.0.0.0~noble-1_amd64.deb";
    static constexpr StringView wineSupportHash = "04e7b4b995262c734019099d93277d8f219f7d180ebb65b5ccb3df7f97be1078";

    switch (HostPlatform)
    {
    case Platform::Linux: break;
    case Platform::Apple:
    case Platform::Windows:
    case Platform::Emscripten: return Result::Error("Automatic Linux Wine install is only supported on Linux hosts");
    }
    SC_TRY_MSG(isArm64HostInstructionSet(), "Automatic Linux Wine install is only supported on Linux ARM64 hosts");

    package.packageFullName       = "wine-stable-linux-arm64-box64";
    package.packageLocalDirectory = format("{}/wine-stable/linux-arm64-box64", packagesCacheDirectory);
    package.packageLocalTxt       = format("{}/wine-stable/linux-arm64-box64.txt", packagesCacheDirectory);
    package.installDirectoryLink  = format("{}/wine-stable_linux_arm64_box64", packagesInstallDirectory);

    auto finalizePackage = [&](FileSystem& fs) -> Result
    {
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
    };

    auto testPackage = [&](const Package& installedPackage) -> Result
    {
        String executable = StringEncoding::Utf8;
        String version    = StringEncoding::Utf8;
        SC_TRY(Path::join(executable, {installedPackage.installDirectoryLink.view(), "bin", "wine"}));
        Process process;
        SC_TRY(process.exec({executable.view(), "--version"}, version));
        SC_TRY_MSG(process.getExitStatus() == 0, "Linux Wine runner returned error");
        SC_TRY_MSG(StringView(version.view()).containsString("wine-11.0"), "Linux Wine runner version doesn't match");
        return Result(true);
    };

    FileSystem fs;
    SC_TRY(fs.init("."));
    SC_TRY(fs.makeDirectoryRecursive(packagesCacheDirectory));
    SC_TRY(fs.makeDirectoryRecursive(packagesInstallDirectory));

    if (fs.existsAndIsDirectory(package.packageLocalDirectory.view()))
    {
        SC_TRY(writeLinuxBox64WineScript(package.packageLocalDirectory.view(), "wine", "wine"));
        SC_TRY(writeLinuxBox64WineScript(package.packageLocalDirectory.view(), "wineconsole", "wineconsole"));
        SC_TRY(finalizePackage(fs));
        if (testPackage(package))
        {
            return Result(true);
        }
    }

    if (fs.existsAndIsDirectory(package.packageLocalDirectory.view()))
    {
        SC_TRY(fs.removeDirectoriesRecursive(package.packageLocalDirectory.view()));
    }
    SC_TRY(fs.makeDirectoryRecursive(package.packageLocalDirectory.view()));

    String downloadsDirectory = StringEncoding::Utf8;
    SC_TRY(Path::join(downloadsDirectory, {packagesCacheDirectory, "wine-stable", "downloads"}));
    SC_TRY(fs.makeDirectoryRecursive(downloadsDirectory.view()));

    LinuxBox64PackageMetadata box64Package;
    SC_TRY(resolveLinuxBox64PackageMetadata(downloadsDirectory.view(), box64Package));

    String box64Deb       = StringEncoding::Utf8;
    String libunwindDeb   = StringEncoding::Utf8;
    String wineBinaryDeb  = StringEncoding::Utf8;
    String wineSupportDeb = StringEncoding::Utf8;
    String box64Filename  = StringEncoding::Utf8;
    SC_TRY(StringBuilder::format(box64Filename, "box64-generic-arm_{}_arm64.deb", box64Package.version.view()));
    SC_TRY(Path::join(box64Deb, {downloadsDirectory.view(), box64Filename.view()}));
    SC_TRY(Path::join(libunwindDeb, {downloadsDirectory.view(), "libunwind8_1.6.2-3build1_amd64.deb"}));
    SC_TRY(Path::join(wineBinaryDeb, {downloadsDirectory.view(), "wine-stable-amd64_11.0.0.0~noble-1_amd64.deb"}));
    SC_TRY(Path::join(wineSupportDeb, {downloadsDirectory.view(), "wine-stable_11.0.0.0~noble-1_amd64.deb"}));

    SC_TRY(downloadFileHash(box64Package.url.view(), box64Deb.view(), Hashing::TypeSHA256, box64Package.sha256.view()));
    SC_TRY(downloadFileHash(libunwindURL, libunwindDeb.view(), Hashing::TypeSHA256, libunwindHash));
    SC_TRY(downloadFileHash(wineBinaryURL, wineBinaryDeb.view(), Hashing::TypeSHA256, wineBinaryHash));
    SC_TRY(downloadFileHash(wineSupportURL, wineSupportDeb.view(), Hashing::TypeSHA256, wineSupportHash));

    String box64Root    = StringEncoding::Utf8;
    String amd64LibRoot = StringEncoding::Utf8;
    String wineRoot     = StringEncoding::Utf8;
    SC_TRY(Path::join(box64Root, {package.packageLocalDirectory.view(), "box64"}));
    SC_TRY(Path::join(amd64LibRoot, {package.packageLocalDirectory.view(), "amd64-libs"}));
    SC_TRY(Path::join(wineRoot, {package.packageLocalDirectory.view(), "wine"}));
    SC_TRY(fs.makeDirectoryRecursive(box64Root.view()));
    SC_TRY(fs.makeDirectoryRecursive(amd64LibRoot.view()));
    SC_TRY(fs.makeDirectoryRecursive(wineRoot.view()));

    SC_TRY(extractDebArchive(box64Deb.view(), box64Root.view()));
    SC_TRY(extractDebArchive(libunwindDeb.view(), amd64LibRoot.view()));
    SC_TRY(extractDebArchive(wineBinaryDeb.view(), wineRoot.view()));
    SC_TRY(extractDebArchive(wineSupportDeb.view(), wineRoot.view()));

    SC_TRY(writeLinuxBox64WineScript(package.packageLocalDirectory.view(), "wine", "wine"));
    SC_TRY(writeLinuxBox64WineScript(package.packageLocalDirectory.view(), "wineconsole", "wineconsole"));

    SC_TRY(finalizePackage(fs));
    SC_TRY(testPackage(package));

    String packageTxt = StringEncoding::Utf8;
    auto   builder    = StringBuilder::create(packageTxt);
    SC_TRY(builder.append("SC_PACKAGE_URL={}\n", wineBinaryURL));
    SC_TRY(builder.append("SC_PACKAGE_URL_EXTRA={}\n", wineSupportURL));
    SC_TRY(builder.append("SC_PACKAGE_URL_BOX64={}\n", box64Package.url.view()));
    SC_TRY(builder.append("SC_PACKAGE_URL_LIBUNWIND={}\n", libunwindURL));
    builder.finalize();
    SC_TRY(fs.writeString(package.packageLocalTxt.view(), packageTxt.view()));
    const PackageReceiptExport exports[] = {
        {"runner", PackageExport::RunnerWine, "bin/wine"},
        {"capability", PackageCapability::RunnerWine, "bin/wine"},
    };
    static constexpr StringView phases[] = {
        "resolveLinuxWinePackages",
        "repairLinuxWineRunner",
        "validateWineRunner",
        "writeReceipt",
    };
    SC_TRY(writeManualPackageReceipt(package, "wine-stable", "system", "linux", wineBinaryURL, {}, exports, phases));

    return Result(true);
#else
    (void)packagesCacheDirectory;
    (void)packagesInstallDirectory;
    (void)package;
    return Result::Error("Automatic Linux Wine install is only supported on Linux hosts");
#endif
}

Result installLinuxNativeArm64WineRunner(StringView packagesCacheDirectory, StringView packagesInstallDirectory,
                                         Package& package)
{
#if SC_PLATFORM_LINUX
    static constexpr StringView wineURL    = "http://launchpadlibrarian.net/590592808/wine_6.0.3~repack-1_all.deb";
    static constexpr StringView wineHash   = "1d940593266e346700a0951b71808b2cc2a7a472c4bf00641fae26a39bb756cb";
    static constexpr StringView wine64URL  = "http://launchpadlibrarian.net/590597908/wine64_6.0.3~repack-1_arm64.deb";
    static constexpr StringView wine64Hash = "f064a30541673cf2f98e45cdc13dce56ca3c253267177af7e5aa5d4b724c6164";
    static constexpr StringView wine64PreloaderURL =
        "http://launchpadlibrarian.net/590597906/wine64-preloader_6.0.3~repack-1_arm64.deb";
    static constexpr StringView wine64PreloaderHash =
        "6c9feb4f657d1758e851708865ecbcef1cd171bf20c6f5165614b003cc1330bb";
    static constexpr StringView libwineURL = "http://launchpadlibrarian.net/590597905/libwine_6.0.3~repack-1_arm64.deb";
    static constexpr StringView libwineHash = "f66d74708c57f6f168400932e50ab3699161aa92d17e437154c755d775ad76c1";
    static constexpr StringView fontsWineURL =
        "http://launchpadlibrarian.net/590592798/fonts-wine_6.0.3~repack-1_all.deb";
    static constexpr StringView fontsWineHash = "9c14ea5e3cdaf1253165cfad3293c217620bd179c783b05426878c22cb221873";

    switch (HostPlatform)
    {
    case Platform::Linux: break;
    case Platform::Apple:
    case Platform::Windows:
    case Platform::Emscripten:
        return Result::Error("Automatic Linux ARM64 Wine install is only supported on Linux hosts");
    }
    SC_TRY_MSG(isArm64HostInstructionSet(),
               "Automatic Linux ARM64 Wine install is only supported on Linux ARM64 hosts");

    package.packageFullName       = "wine-stable-linux-arm64-native";
    package.packageLocalDirectory = format("{}/wine-stable/linux-arm64-native", packagesCacheDirectory);
    package.packageLocalTxt       = format("{}/wine-stable/linux-arm64-native.txt", packagesCacheDirectory);
    package.installDirectoryLink  = format("{}/wine-stable_linux_arm64_native", packagesInstallDirectory);

    auto finalizePackage = [&](FileSystem& fs) -> Result
    {
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
    };

    auto testPackage = [&](const Package& installedPackage) -> Result
    {
        String executable = StringEncoding::Utf8;
        String version    = StringEncoding::Utf8;
        SC_TRY(Path::join(executable, {installedPackage.installDirectoryLink.view(), "bin", "wine"}));
        Process process;
        SC_TRY(process.exec({executable.view(), "--version"}, version));
        SC_TRY_MSG(process.getExitStatus() == 0, "Linux ARM64 Wine runner returned error");
        SC_TRY_MSG(StringView(version.view()).containsString("wine-6.0"),
                   "Linux ARM64 Wine runner version doesn't match");
        return Result(true);
    };

    FileSystem fs;
    SC_TRY(fs.init("."));
    SC_TRY(fs.makeDirectoryRecursive(packagesCacheDirectory));
    SC_TRY(fs.makeDirectoryRecursive(packagesInstallDirectory));

    if (fs.existsAndIsDirectory(package.packageLocalDirectory.view()))
    {
        if (repairLinuxNativeWineRunnerLayout(package.packageLocalDirectory.view()) and finalizePackage(fs) and
            testPackage(package))
        {
            return Result(true);
        }
    }

    if (fs.existsAndIsDirectory(package.packageLocalDirectory.view()))
    {
        SC_TRY(fs.removeDirectoriesRecursive(package.packageLocalDirectory.view()));
    }
    SC_TRY(fs.makeDirectoryRecursive(package.packageLocalDirectory.view()));

    String downloadsDirectory = StringEncoding::Utf8;
    String extractRoot        = StringEncoding::Utf8;
    SC_TRY(Path::join(downloadsDirectory, {packagesCacheDirectory, "wine-stable", "downloads"}));
    SC_TRY(Path::join(extractRoot, {package.packageLocalDirectory.view(), "root"}));
    SC_TRY(fs.makeDirectoryRecursive(downloadsDirectory.view()));
    SC_TRY(fs.makeDirectoryRecursive(extractRoot.view()));

    String wineDeb            = StringEncoding::Utf8;
    String wine64Deb          = StringEncoding::Utf8;
    String wine64PreloaderDeb = StringEncoding::Utf8;
    String libwineDeb         = StringEncoding::Utf8;
    String fontsWineDeb       = StringEncoding::Utf8;
    SC_TRY(Path::join(wineDeb, {downloadsDirectory.view(), "wine_6.0.3~repack-1_all.deb"}));
    SC_TRY(Path::join(wine64Deb, {downloadsDirectory.view(), "wine64_6.0.3~repack-1_arm64.deb"}));
    SC_TRY(Path::join(wine64PreloaderDeb, {downloadsDirectory.view(), "wine64-preloader_6.0.3~repack-1_arm64.deb"}));
    SC_TRY(Path::join(libwineDeb, {downloadsDirectory.view(), "libwine_6.0.3~repack-1_arm64.deb"}));
    SC_TRY(Path::join(fontsWineDeb, {downloadsDirectory.view(), "fonts-wine_6.0.3~repack-1_all.deb"}));

    SC_TRY(downloadFileHash(wineURL, wineDeb.view(), Hashing::TypeSHA256, wineHash));
    SC_TRY(downloadFileHash(wine64URL, wine64Deb.view(), Hashing::TypeSHA256, wine64Hash));
    SC_TRY(downloadFileHash(wine64PreloaderURL, wine64PreloaderDeb.view(), Hashing::TypeSHA256, wine64PreloaderHash));
    SC_TRY(downloadFileHash(libwineURL, libwineDeb.view(), Hashing::TypeSHA256, libwineHash));
    SC_TRY(downloadFileHash(fontsWineURL, fontsWineDeb.view(), Hashing::TypeSHA256, fontsWineHash));

    SC_TRY(extractDebArchive(wineDeb.view(), extractRoot.view()));
    SC_TRY(extractDebArchive(wine64Deb.view(), extractRoot.view()));
    SC_TRY(extractDebArchive(wine64PreloaderDeb.view(), extractRoot.view()));
    SC_TRY(extractDebArchive(libwineDeb.view(), extractRoot.view()));
    SC_TRY(extractDebArchive(fontsWineDeb.view(), extractRoot.view()));
    SC_TRY(repairLinuxNativeWineRunnerLayout(package.packageLocalDirectory.view()));

    SC_TRY(finalizePackage(fs));
    SC_TRY(testPackage(package));

    String packageTxt = StringEncoding::Utf8;
    auto   builder    = StringBuilder::create(packageTxt);
    SC_TRY(builder.append("SC_PACKAGE_URL={}\n", wine64URL));
    SC_TRY(builder.append("SC_PACKAGE_URL_EXTRA={}\n", wineURL));
    SC_TRY(builder.append("SC_PACKAGE_URL_EXTRA2={}\n", wine64PreloaderURL));
    SC_TRY(builder.append("SC_PACKAGE_URL_EXTRA3={}\n", libwineURL));
    SC_TRY(builder.append("SC_PACKAGE_URL_EXTRA4={}\n", fontsWineURL));
    builder.finalize();
    SC_TRY(fs.writeString(package.packageLocalTxt.view(), packageTxt.view()));
    const PackageReceiptExport exports[] = {
        {"runner", PackageExport::RunnerWine, "bin/wine"},
        {"capability", PackageCapability::RunnerWine, "bin/wine"},
    };
    static constexpr StringView phases[] = {
        "resolveLinuxArm64WinePackages",
        "repairLinuxWineRunner",
        "validateWineRunner",
        "writeReceipt",
    };
    SC_TRY(writeManualPackageReceipt(package, "wine-stable", "6.0.3", "linux_arm64_native", wine64URL, {}, exports,
                                     phases));

    return Result(true);
#else
    (void)packagesCacheDirectory;
    (void)packagesInstallDirectory;
    (void)package;
    return Result::Error("Automatic Linux ARM64 Wine install is only supported on Linux hosts");
#endif
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
    case Platform::Linux: return installLinuxWineRunner(packagesCacheDirectory, packagesInstallDirectory, package);
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
    const PackageReceiptExport exports[] = {
        {"runner", PackageExport::RunnerWine, "Wine Stable.app/Contents/Resources/wine/bin/wine"},
        {"capability", PackageCapability::RunnerWine, "Wine Stable.app/Contents/Resources/wine/bin/wine"},
    };
    static constexpr StringView phases[] = {
        "extractWineStableRunner",
        "validateWineRunner",
        "writeReceipt",
    };
    SC_TRY(writeDownloadPackageReceipt(download, package, exports, phases));
    return Result(true);
}

Result installMSVCToolchain(StringView packagesCacheDirectory, StringView packagesInstallDirectory, Package& package,
                            StringView importDirectory, StringView wineExecutableOverride)
{
    package.packageFullName       = format("msvc-portable-{}", HostPlatform == Platform::Apple   ? "macos"
                                                               : HostPlatform == Platform::Linux ? "linux"
                                                                                                 : "host");
    package.packageLocalDirectory = format("{}/msvc/{}", packagesCacheDirectory, portableMSVCCacheLeafName());
    package.packageLocalTxt       = format("{}/msvc/{}.txt", packagesCacheDirectory, portableMSVCCacheLeafName());
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
        if (repairMSVCPackageLayout(package.packageLocalDirectory.view(), resolvedWine.view()))
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
    }

    if (needsInstall)
    {
        if (fs.existsAndIsDirectory(package.packageLocalDirectory.view()))
        {
            SC_TRY(fs.removeDirectoriesRecursive(package.packageLocalDirectory.view()));
        }

        auto installPortableMSVC = [&]() -> Result
        {
            if (not resolvedImportDirectory.isEmpty())
            {
                SC_TRY(fs.makeDirectoryRecursive(Path::dirname(package.packageLocalDirectory.view(), Path::AsNative)));
                Process copyProcess;
                SC_TRY(copyProcess.exec(
                    {"cp", "-R", resolvedImportDirectory.view(), package.packageLocalDirectory.view()}));
                SC_TRY_MSG(copyProcess.getExitStatus() == 0, "Failed copying imported MSVC toolchain");
                SC_TRY(repairMSVCPackageLayout(package.packageLocalDirectory.view(), resolvedWine.view()));
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
                                     "--cache-dir", downloadCache.view(), "--wine", resolvedWine.view(),
                                     "--wine-prefix", winePrefix.view(), "--accept-license"}));
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
            return Result(true);
        };

        const Result installResult = installPortableMSVC();
        if (not installResult)
        {
            SC_TRY(removePackageInstallLink(fs, package));
            if (fs.existsAndIsDirectory(package.packageLocalDirectory.view()))
            {
                SC_TRY(fs.removeDirectoriesRecursive(package.packageLocalDirectory.view()));
            }
            return installResult;
        }

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

    const PackageReceiptExport exports[] = {
        {"tool", PackageExport::MSVCClX64, "bin/x64/cl"},
        {"tool", PackageExport::MSVCLinkX64, "bin/x64/link"},
        {"tool", PackageExport::MSVCLibX64, "bin/x64/lib"},
        {"tool", PackageExport::MSVCClArm64, "bin/arm64/cl"},
        {"tool", PackageExport::MSVCLinkArm64, "bin/arm64/link"},
        {"tool", PackageExport::MSVCLibArm64, "bin/arm64/lib"},
        {"capability", PackageCapability::ToolchainWindowsMSVCX64, "bin/x64/cl"},
        {"capability", PackageCapability::ToolchainWindowsMSVCArm64, "bin/arm64/cl"},
    };
    static constexpr StringView phases[] = {
        "fetchPortableMSVC", "repairMSVCLayout", "prepareMSVCWinePrefix", "validateMSVCLayout", "writeReceipt",
    };
    String source = StringEncoding::Utf8;
    if (not resolvedImportDirectory.isEmpty())
    {
        SC_TRY(StringBuilder::format(source, "import:{}", resolvedImportDirectory.view()));
    }
    else
    {
        SC_TRY(source.assign("https://aka.ms/vs/17/release/channel"));
    }
    SC_TRY(writeManualPackageReceipt(package, "msvc", "portable", package.packageFullName.view(), source.view(),
                                     StringView(), exports, phases));
    return Result(true);
}

static Result installDoxygenEntry(StringView cache, StringView install, Package& package, Span<const StringView>)
{
    return installDoxygen(cache, install, package);
}

static Result installDoxygenAwesomeCssEntry(StringView cache, StringView install, Package& package,
                                            Span<const StringView>)
{
    return installDoxygenAwesomeCss(cache, install, package);
}

static Result installClangEntry(StringView cache, StringView install, Package& package, Span<const StringView>)
{
    return installClangBinaries(cache, install, package);
}

static Result installLLVMEntry(StringView cache, StringView install, Package& package, Span<const StringView>)
{
    return installLLVMToolchain(cache, install, package);
}

static Result installQEMUEntry(StringView cache, StringView install, Package& package, Span<const StringView> arguments)
{
    QEMUPackageInstallOptions options;
    SC_TRY(parseQEMUPackageInstallOptions(arguments, options));
    return installQEMURunner(cache, install, package, options.importDirectory);
}

static Result installFilCEntry(StringView cache, StringView install, Package& package, Span<const StringView> arguments)
{
    FilCPackageInstallOptions options;
    SC_TRY(parseFilCPackageInstallOptions(arguments, options));
    return installFilCToolchain(cache, install, package, options.importDirectory);
}

static Result installLLVMMingwEntry(StringView cache, StringView install, Package& package, Span<const StringView>)
{
    return installLLVMMingwToolchain(cache, install, package);
}

static Result installLinuxSysrootGlibcX64Entry(StringView cache, StringView install, Package& package,
                                               Span<const StringView>)
{
    LinuxSysrootSpec spec;
    spec.environment  = LinuxSysrootSpec::Glibc;
    spec.architecture = InstructionSet::Intel64;
    return installLinuxSysroot(cache, install, spec, package);
}

static Result installLinuxSysrootGlibcArm64Entry(StringView cache, StringView install, Package& package,
                                                 Span<const StringView>)
{
    LinuxSysrootSpec spec;
    spec.environment  = LinuxSysrootSpec::Glibc;
    spec.architecture = InstructionSet::ARM64;
    return installLinuxSysroot(cache, install, spec, package);
}

static Result installLinuxSysrootMuslX64Entry(StringView cache, StringView install, Package& package,
                                              Span<const StringView>)
{
    LinuxSysrootSpec spec;
    spec.environment  = LinuxSysrootSpec::Musl;
    spec.architecture = InstructionSet::Intel64;
    return installLinuxSysroot(cache, install, spec, package);
}

static Result installLinuxSysrootMuslArm64Entry(StringView cache, StringView install, Package& package,
                                                Span<const StringView>)
{
    LinuxSysrootSpec spec;
    spec.environment  = LinuxSysrootSpec::Musl;
    spec.architecture = InstructionSet::ARM64;
    return installLinuxSysroot(cache, install, spec, package);
}

static Result installWineEntry(StringView cache, StringView install, Package& package, Span<const StringView>)
{
    return installWineStableRunner(cache, install, package);
}

static Result installMSVCEntry(StringView cache, StringView install, Package& package, Span<const StringView> arguments)
{
    MSVCPackageInstallOptions options;
    SC_TRY(parseMSVCPackageInstallOptions(arguments, options));
    return installMSVCToolchain(cache, install, package, options.importDirectory, options.wineExecutable);
}

static constexpr StringView DoxygenExports[] = {
    "tool:doxygen",
};
static constexpr StringView DoxygenPhases[] = {
    "resolveDoxygenRelease",
    "extractDoxygenExecutable",
    "validateDoxygenVersion",
    "writeReceipt",
};
static constexpr StringView DoxygenAwesomeExports[] = {
    "asset:doxygen-awesome-css",
};
static constexpr StringView DoxygenAwesomePhases[] = {
    "fetchGitRevision",
    "validateGitCommit",
    "writeReceipt",
};
static constexpr StringView ClangExports[] = {
    "tool:clang-format",
};
static constexpr StringView ClangPhases[] = {
    "resolveHostLLVMArchive",
    "extractClangFormat",
    "validateClangFormatVersion",
    "writeReceipt",
};
static constexpr StringView LLVMExports[] = {
    "tool:clang",
    "tool:clang++",
    "tool:llvm-ar",
    "tool:ld.lld",
    "capability:tool.c-compiler",
    "capability:tool.cxx-compiler",
    "capability:tool.archiver",
    "capability:tool.linker",
};
static constexpr StringView LLVMPhases[] = {
    "resolveHostLLVMArchive",
    "extractLLVMToolchain",
    "validateLLVMToolchain",
    "writeReceipt",
};
static constexpr StringView QEMUExports[] = {
    "runner:qemu",
    "capability:runner.qemu.x86_64",
    "capability:runner.qemu.arm64",
};
static constexpr StringView QEMUPhases[] = {
    "resolveImportedQEMU",
    "validateQEMUTargets",
    "writeReceipt",
};
static constexpr StringView FilCExports[] = {
    "tool:clang",
    "tool:clang++",
    "capability:toolchain.filc.x86_64",
};
static constexpr StringView FilCPhases[] = {
    "resolveFilCSource",
    "prepareFilCCompilerLaunchers",
    "validateFilCToolchain",
    "writeReceipt",
};
static constexpr StringView LLVMMingwExports[] = {
    "tool:x86_64-w64-mingw32-clang",
    "tool:x86_64-w64-mingw32-clang++",
    "tool:aarch64-w64-mingw32-clang",
    "tool:aarch64-w64-mingw32-clang++",
    "tool:llvm-ar",
    "capability:toolchain.windows-gnu.x86_64",
    "capability:toolchain.windows-gnu.arm64",
};
static constexpr StringView LLVMMingwPhases[] = {
    "resolveLLVMMingwArchive",
    "extractLLVMMingwToolchain",
    "validateLLVMMingwToolchain",
    "writeReceipt",
};
static constexpr StringView LinuxSysrootExports[] = {
    "sysroot:sysroot",
    "include-dir:sysroot.include",
    "library-dir:sysroot.lib",
    "capability:sysroot.linux.<environment>.<architecture>",
};
static constexpr StringView LinuxSysrootPhases[] = {
    "resolveLinuxSysrootPackages",
    "extractLinuxSysrootPackages",
    "repairLinuxSysrootLayout",
    "validateLinuxSysroot",
    "writeReceipt",
};
static constexpr StringView WineExports[] = {
    "runner:wine",
    "capability:runner.wine",
};
static constexpr StringView WinePhases[] = {
    "resolveWinePackages",
    "repairLinuxWineRunner",
    "validateWineRunner",
    "writeReceipt",
};
static constexpr StringView MSVCExports[] = {
    "tool:cl.x64",
    "tool:link.x64",
    "tool:lib.x64",
    "tool:cl.arm64",
    "tool:link.arm64",
    "tool:lib.arm64",
    "capability:toolchain.windows-msvc.x64",
    "capability:toolchain.windows-msvc.arm64",
};
static constexpr StringView MSVCPhases[] = {
    "fetchPortableMSVC", "repairMSVCLayout", "prepareMSVCWinePrefix", "validateMSVCLayout", "writeReceipt",
};

static constexpr PackageRegistryEntry BuiltinPackageRegistryEntries[] = {
    {"doxygen", "doxygen", "tool", "Documentation generator binary", "host", "GitHub release archive", false,
     DoxygenExports, DoxygenPhases, installDoxygenEntry},
    {"doxygen-awesome-css", "doxygen-awesome-css", "asset", "Doxygen Awesome CSS theme checkout", "host",
     "Pinned Git revision", false, DoxygenAwesomeExports, DoxygenAwesomePhases, installDoxygenAwesomeCssEntry},
    {"clang", "clang-binaries", "tool", "Pinned host clang-format binary", "host", "Official LLVM release archive",
     false, ClangExports, ClangPhases, installClangEntry},
    {"llvm", "llvm", "toolchain", "Pinned host LLVM toolchain", "host", "Official LLVM release archive", false,
     LLVMExports, LLVMPhases, installLLVMEntry},
    {"qemu", "qemu", "runner", "Imported QEMU user-mode runner registration", "host", "Imported directory or PATH",
     true, QEMUExports, QEMUPhases, installQEMUEntry},
    {"filc", "filc", "toolchain", "Experimental Fil-C compiler toolchain", "linux-x86_64", "Pinned archive or import",
     true, FilCExports, FilCPhases, installFilCEntry},
    {"llvm-mingw", "llvm-mingw", "toolchain", "LLVM MinGW Windows GNU toolchain", "host", "llvm-mingw release archive",
     false, LLVMMingwExports, LLVMMingwPhases, installLLVMMingwEntry},
    {"linux-sysroot-glibc-x86_64", "linux-sysroot-glibc-x86_64", "sysroot", "Linux glibc x86_64 sysroot",
     "linux-glibc-x86_64", "Ubuntu Jammy package index", false, LinuxSysrootExports, LinuxSysrootPhases,
     installLinuxSysrootGlibcX64Entry},
    {"linux-sysroot-glibc-arm64", "linux-sysroot-glibc-arm64", "sysroot", "Linux glibc arm64 sysroot",
     "linux-glibc-arm64", "Ubuntu Jammy package index", false, LinuxSysrootExports, LinuxSysrootPhases,
     installLinuxSysrootGlibcArm64Entry},
    {"linux-sysroot-musl-x86_64", "linux-sysroot-musl-x86_64", "sysroot", "Linux musl x86_64 sysroot",
     "linux-musl-x86_64", "Alpine APK index", false, LinuxSysrootExports, LinuxSysrootPhases,
     installLinuxSysrootMuslX64Entry},
    {"linux-sysroot-musl-arm64", "linux-sysroot-musl-arm64", "sysroot", "Linux musl arm64 sysroot", "linux-musl-arm64",
     "Alpine APK index", false, LinuxSysrootExports, LinuxSysrootPhases, installLinuxSysrootMuslArm64Entry},
    {"wine", "wine-stable", "runner", "Wine runner for Windows targets", "host", "Wine release archive or Linux DEBs",
     false, WineExports, WinePhases, installWineEntry},
    {"msvc", "msvc", "toolchain", "Portable MSVC and Windows SDK toolchain", "windows-msvc-x64/windows-msvc-arm64",
     "Portable MSVC vendor fetch or import", true, MSVCExports, MSVCPhases, installMSVCEntry},
};

PackageRegistry builtinPackageRegistry() { return {BuiltinPackageRegistryEntries}; }

Result addBuiltinPackages(PackageRegistryBuilder& registry) { return registry.add(builtinPackageRegistry()); }

static Result printKnownPackages(Console& console, PackageRegistry registry)
{
    for (const PackageRegistryEntry& entry : registry.entries)
    {
        console.print(entry.name);
        console.print(" [");
        console.print(entry.kind);
        console.print("] ");
        console.printLine(entry.description);
    }
    return Result(true);
}

static Result printPackageHelp(Console& console)
{
    console.printLine("Usage: ./SC.sh package <action> [package] [options]");
    console.printLine("");
    console.printLine("Actions:");
    console.printLine("  install [package]       Install a package (default package: clang)");
    console.printLine("  list                    List known packages");
    console.printLine("  info <package>          Show package registry metadata");
    console.printLine("  status [package]        Show installed receipt status");
    console.printLine("  verify [package]        Verify installed package receipts and exports");
    console.printLine("  doctor [package]        Explain package receipt and export health");
    console.printLine("  receipt <package>       Print the installed receipt JSON");
    console.printLine("  exports <package>       Print resolved receipt exports");
    console.printLine("  lock                    Write _Build/SC-package.lock");
    console.printLine("");
    console.printLine("Import-capable packages:");
    console.printLine("  qemu --import-directory <path>");
    console.printLine("  filc --import-directory <path>");
    console.printLine("  msvc --import-directory <path> --wine <path>");
    return Result(true);
}

static Result printStringViewList(Console& console, StringView label, Span<const StringView> values)
{
    console.print(label);
    console.print(" = ");
    if (values.empty())
    {
        console.printLine("-");
        return Result(true);
    }
    for (size_t idx = 0; idx < values.sizeInElements(); ++idx)
    {
        if (idx > 0)
        {
            console.print(", ");
        }
        console.print(values[idx]);
    }
    console.printLine(""_a8);
    return Result(true);
}

static Result printUnknownPackageError(Console& console, PackageRegistry registry, StringView packageName)
{
    console.print("Unknown package: ");
    console.printLine(packageName);
    console.print("Known packages: ");
    for (size_t idx = 0; idx < registry.entries.sizeInElements(); ++idx)
    {
        if (idx > 0)
        {
            console.print(", ");
        }
        console.print(registry.entries[idx].name);
    }
    console.printLine(""_a8);
    return Result::Error("Invalid package name");
}

static Result findInstalledPackageReceipt(StringView packagesInstallDirectory, StringView installedName,
                                          String& receiptPath, String& packageRoot, bool& found)
{
    found = false;
    FileSystem fs;
    SC_TRY(fs.init("."));
    if (not fs.existsAndIsDirectory(packagesInstallDirectory))
    {
        return Result(true);
    }

    FileSystemIterator::FolderState entries[8];
    FileSystemIterator              iterator;
    SC_TRY(iterator.init(packagesInstallDirectory, entries));

    auto checkReceipt = [&](StringView candidateReceiptPath, StringView candidatePackageRoot) -> Result
    {
        if (not fs.existsAndIsFile(candidateReceiptPath))
        {
            return Result(true);
        }
        String receipt = StringEncoding::Utf8;
        SC_TRY(readFileIntoString(candidateReceiptPath, receipt));
        PackageReceiptJSON receiptJSON;
        SC_TRY(readPackageReceiptJSON(receipt.view(), receiptJSON));
        if (receiptJSON.name.view() == installedName)
        {
            SC_TRY(receiptPath.assign(candidateReceiptPath));
            SC_TRY(packageRoot.assign(candidatePackageRoot));
            found = true;
        }
        return Result(true);
    };

    while (iterator.enumerateNext())
    {
        const FileSystemIterator::Entry entry            = iterator.get();
        String                          candidateReceipt = StringEncoding::Utf8;
        SC_TRY(Path::join(candidateReceipt, {entry.path, PackageReceiptFileName}));
        SC_TRY(checkReceipt(candidateReceipt.view(), entry.path));
        if (found)
        {
            return Result(true);
        }

        if (entry.isDirectory() or StringView(entry.name) != PackageReceiptFileName)
        {
            continue;
        }
        SC_TRY(checkReceipt(entry.path, Path::dirname(entry.path, Path::AsNative)));
        if (found)
        {
            return Result(true);
        }
    }
    SC_TRY(iterator.checkErrors());
    return Result(true);
}

static Result verifyPackageReceipt(StringView receiptPath, StringView packageRoot)
{
    String receipt = StringEncoding::Utf8;
    SC_TRY(readFileIntoString(receiptPath, receipt));

    PackageReceiptJSON receiptJSON;
    SC_TRY(readPackageReceiptJSON(receipt.view(), receiptJSON));

    SC_TRY(validatePackageReceiptHeader(receiptJSON));
    SC_TRY_MSG(not receiptJSON.version.isEmpty(), "Package receipt is missing package version");
    SC_TRY_MSG(not receiptJSON.source.isEmpty(), "Package receipt is missing source");
    SC_TRY_MSG(not receiptJSON.installRoot.isEmpty(), "Package receipt is missing install root");
    SC_TRY_MSG(receiptJSON.validation.view() == "passed"_a8, "Package receipt validation did not pass");
    SC_TRY(validatePackageReceiptSourceHash(receiptJSON.sourceHash.view()));

    FileSystem fs;
    SC_TRY(fs.init("."));
    for (PackageReceiptExportJSON& exportView : receiptJSON.exports)
    {
        SC_TRY_MSG(not exportView.kind.isEmpty(), "Package receipt export is missing kind");
        SC_TRY_MSG(not exportView.name.isEmpty(), "Package receipt export is missing name");
        SC_TRY(validatePackageReceiptExportPath(exportView.path.view()));
        if (exportView.path.view() == "."_a8)
        {
            continue;
        }
        String exportedPath = StringEncoding::Utf8;
        SC_TRY(Path::join(exportedPath, {packageRoot, exportView.path.view()}));
        SC_TRY_MSG(fs.exists(exportedPath.view()), "Package receipt export is missing");
    }
    return Result(true);
}

static Result verifyPackageReceiptMatchesRegistryExports(const PackageRegistryEntry& entry,
                                                         const PackageReceiptJSON&   receiptJSON)
{
    for (StringView expectedExport : entry.exports)
    {
        if (expectedExport.containsString("<"))
        {
            continue;
        }

        StringView expectedKind;
        StringView expectedName;
        SC_TRY_MSG(expectedExport.splitBefore(":", expectedKind) and expectedExport.splitAfter(":", expectedName),
                   "Malformed package registry export contract");

        bool found = false;
        for (const PackageReceiptExportJSON& exportView : receiptJSON.exports)
        {
            if (exportView.kind.view() == expectedKind and exportView.name.view() == expectedName)
            {
                found = true;
                break;
            }
        }
        SC_TRY_MSG(found, "Package receipt is missing registry export");
    }
    return Result(true);
}

static Result verifyPackageReceiptForEntry(const PackageRegistryEntry& entry, StringView receiptPath,
                                           StringView packageRoot)
{
    SC_TRY(verifyPackageReceipt(receiptPath, packageRoot));
    String receipt = StringEncoding::Utf8;
    SC_TRY(readFileIntoString(receiptPath, receipt));
    PackageReceiptJSON receiptJSON;
    SC_TRY(readPackageReceiptJSON(receipt.view(), receiptJSON));
    SC_TRY_MSG(receiptJSON.name.view() == entry.installedName,
               "Package receipt identity does not match registry entry");
    SC_TRY(verifyPackageReceiptMatchesRegistryExports(entry, receiptJSON));
    return Result(true);
}

static Result printAllPackageStatuses(Console& console, PackageRegistry registry, StringView packagesInstallDirectory,
                                      bool verify)
{
    size_t verifiedCount = 0;
    for (const PackageRegistryEntry& entry : registry.entries)
    {
        String receiptPath = StringEncoding::Utf8;
        String packageRoot = StringEncoding::Utf8;
        bool   found       = false;
        SC_TRY(findInstalledPackageReceipt(packagesInstallDirectory, entry.installedName, receiptPath, packageRoot,
                                           found));
        if (not found)
        {
            if (not verify)
            {
                console.print("not installed: ");
                console.printLine(entry.name);
            }
            continue;
        }
        if (verify)
        {
            SC_TRY(verifyPackageReceiptForEntry(entry, receiptPath.view(), packageRoot.view()));
            console.print("verified: ");
            verifiedCount += 1;
        }
        else
        {
            console.print("installed: ");
        }
        console.print(entry.name);
        console.print(" at ");
        console.print(packageRoot.view());
        if (not verify)
        {
            const Result validation = verifyPackageReceiptForEntry(entry, receiptPath.view(), packageRoot.view());
            console.print(validation ? " (receipt valid)"_a8 : " (receipt invalid)"_a8);
        }
        console.printLine(""_a8);
    }
    if (verify and verifiedCount == 0)
    {
        console.printLine("no installed package receipts found");
    }
    return Result(true);
}

static Result printPackageDoctorForEntry(Console& console, StringView packagesInstallDirectory,
                                         const PackageRegistryEntry& entry, bool& hasIssues)
{
    String receiptPath = StringEncoding::Utf8;
    String packageRoot = StringEncoding::Utf8;
    bool   found       = false;
    SC_TRY(findInstalledPackageReceipt(packagesInstallDirectory, entry.installedName, receiptPath, packageRoot, found));
    if (not found)
    {
        hasIssues = true;
        console.print("missing: ");
        console.printLine(entry.name);
        console.print("  expected receipt for installedName: ");
        console.printLine(entry.installedName);
        console.print("  suggested action: ./SC.sh package install ");
        console.printLine(entry.name);
        return Result(true);
    }

    const Result validation = verifyPackageReceiptForEntry(entry, receiptPath.view(), packageRoot.view());
    if (validation)
    {
        console.print("healthy: ");
        console.print(entry.name);
        console.print(" at ");
        console.printLine(packageRoot.view());
        return Result(true);
    }

    hasIssues = true;
    console.print("problem: ");
    console.print(entry.name);
    console.print(" at ");
    console.printLine(packageRoot.view());
    console.print("  receipt: ");
    console.printLine(receiptPath.view());
    console.print("  reason: ");
    console.printLine(StringView::fromNullTerminated(validation.message, StringEncoding::Ascii));
    console.printLine("  suggested action: re-run install or remove the stale package directory before reinstalling");
    return Result(true);
}

static Result printLegacyPackageSidecars(Console& console, StringView packagesInstallDirectory, bool& hasIssues)
{
    FileSystem fs;
    SC_TRY(fs.init("."));
    if (not fs.existsAndIsDirectory(packagesInstallDirectory))
    {
        return Result(true);
    }

    FileSystemIterator::FolderState entries[8];
    FileSystemIterator              iterator;
    SC_TRY(iterator.init(packagesInstallDirectory, entries));
    while (iterator.enumerateNext())
    {
        const FileSystemIterator::Entry entry = iterator.get();
        if (not entry.isDirectory() and StringView(entry.name).endsWith(".txt"))
        {
            hasIssues = true;
            console.print("legacy sidecar: ");
            console.printLine(entry.path);
            console.printLine("  suggested action: reinstall the package so a structured receipt is written");
        }
    }
    SC_TRY(iterator.checkErrors());
    return Result(true);
}

static Result printPackageDoctor(Console& console, PackageRegistry registry, StringView packagesInstallDirectory,
                                 const PackageRegistryEntry* singleEntry)
{
    bool hasIssues = false;
    if (singleEntry != nullptr)
    {
        SC_TRY(printPackageDoctorForEntry(console, packagesInstallDirectory, *singleEntry, hasIssues));
    }
    else
    {
        for (const PackageRegistryEntry& entry : registry.entries)
        {
            SC_TRY(printPackageDoctorForEntry(console, packagesInstallDirectory, entry, hasIssues));
        }
        SC_TRY(printLegacyPackageSidecars(console, packagesInstallDirectory, hasIssues));
    }

    console.printLine(hasIssues ? "doctor: issues found"_a8 : "doctor: ok"_a8);
    return Result(true);
}

static Result printPackageReceipt(Console& console, StringView packagesInstallDirectory,
                                  const PackageRegistryEntry& entry)
{
    String receiptPath = StringEncoding::Utf8;
    String packageRoot = StringEncoding::Utf8;
    bool   found       = false;
    SC_TRY(findInstalledPackageReceipt(packagesInstallDirectory, entry.installedName, receiptPath, packageRoot, found));
    if (not found)
    {
        console.print("not installed: ");
        console.printLine(entry.name);
        return Result::Error("Package receipt not found");
    }

    String receipt = StringEncoding::Utf8;
    SC_TRY(readFileIntoString(receiptPath.view(), receipt));

    console.print("packageRoot     = ");
    console.printLine(packageRoot.view());
    console.print("receipt         = ");
    console.printLine(receiptPath.view());
    console.printLine(receipt.view());
    return Result(true);
}

static Result printPackageExports(Console& console, StringView packagesInstallDirectory,
                                  const PackageRegistryEntry& entry)
{
    String receiptPath = StringEncoding::Utf8;
    String packageRoot = StringEncoding::Utf8;
    bool   found       = false;
    SC_TRY(findInstalledPackageReceipt(packagesInstallDirectory, entry.installedName, receiptPath, packageRoot, found));
    if (not found)
    {
        console.print("not installed: ");
        console.printLine(entry.name);
        return Result::Error("Package receipt not found");
    }

    String receipt = StringEncoding::Utf8;
    SC_TRY(readFileIntoString(receiptPath.view(), receipt));
    console.print("packageRoot     = ");
    console.printLine(packageRoot.view());
    console.print("receipt         = ");
    console.printLine(receiptPath.view());

    bool hasExports = false;
    SC_TRY(forEachReceiptExport(receipt.view(),
                                [&](const PackageReceiptExportJSON& exportView) -> Result
                                {
                                    String exportedPath = StringEncoding::Utf8;
                                    SC_TRY(Path::join(exportedPath, {packageRoot.view(), exportView.path.view()}));
                                    console.print(exportView.kind.view());
                                    console.print(":");
                                    console.print(exportView.name.view());
                                    console.print(" = ");
                                    console.printLine(exportedPath.view());
                                    hasExports = true;
                                    return Result(true);
                                }));
    if (not hasExports)
    {
        console.printLine("no exports");
    }
    return Result(true);
}

static bool isSmaller(StringView left, StringView right)
{
    return left.compare(right) == StringView::Comparison::Smaller;
}

static bool isPackageLockExportSmaller(const PackageReceiptExportJSON& left, const PackageReceiptExportJSON& right)
{
    if (left.kind.view() != right.kind.view())
    {
        return isSmaller(left.kind.view(), right.kind.view());
    }
    if (left.name.view() != right.name.view())
    {
        return isSmaller(left.name.view(), right.name.view());
    }
    return isSmaller(left.path.view(), right.path.view());
}

static bool isPackageLockEntrySmaller(const PackageLockEntryJSON& left, const PackageLockEntryJSON& right)
{
    if (left.name.view() != right.name.view())
    {
        return isSmaller(left.name.view(), right.name.view());
    }
    if (left.variant.view() != right.variant.view())
    {
        return isSmaller(left.variant.view(), right.variant.view());
    }
    return isSmaller(left.installRoot.view(), right.installRoot.view());
}

static void sortPackageLock(PackageLockJSON& lockJSON)
{
    for (PackageLockEntryJSON& entry : lockJSON.packages)
    {
        Algorithms::bubbleSort(entry.exports.begin(), entry.exports.end(), isPackageLockExportSmaller);
    }
    Algorithms::bubbleSort(lockJSON.packages.begin(), lockJSON.packages.end(), isPackageLockEntrySmaller);
}

static Result lockInstalledPackages(StringView packagesInstallDirectory, StringView lockPath)
{
    FileSystem fs;
    SC_TRY(fs.init("."));

    PackageLockJSON lockJSON;
    SC_TRY(assignJSONField(lockJSON.tool, "SC-package"));
    SC_TRY(assignJSONField(lockJSON.toolVersion, "1"));
    SC_TRY(StringBuilder::format(lockJSON.generatedAt, "{}", Time::Realtime::now().milliseconds));
    SC_TRY(assignJSONField(lockJSON.hostPlatform, hostPlatformName()));
    SC_TRY(assignJSONField(lockJSON.hostArch, hostInstructionSetName()));
    auto appendReceiptObject = [&](StringView receiptPath) -> Result
    {
        if (not fs.existsAndIsFile(receiptPath))
        {
            return Result(true);
        }
        String receipt = StringEncoding::Utf8;
        SC_TRY(readFileIntoString(receiptPath, receipt));
        SC_TRY(verifyPackageReceipt(receiptPath, Path::dirname(receiptPath, Path::AsNative)));

        PackageReceiptJSON receiptJSON;
        SC_TRY(readPackageReceiptJSON(receipt.view(), receiptJSON));

        PackageLockEntryJSON entry;
        SC_TRY(assignJSONField(entry.name, receiptJSON.name.view()));
        SC_TRY(assignJSONField(entry.version, receiptJSON.version.view()));
        SC_TRY(assignJSONField(entry.recipeVersion, receiptJSON.recipeVersion.view()));
        SC_TRY(assignJSONField(entry.hostPlatform, receiptJSON.hostPlatform.view()));
        SC_TRY(assignJSONField(entry.variant, receiptJSON.variant.view()));
        SC_TRY(assignJSONField(entry.source, receiptJSON.source.view()));
        SC_TRY(assignJSONField(entry.sourceHash, receiptJSON.sourceHash.view()));
        SC_TRY(assignJSONField(entry.installRoot, receiptJSON.installRoot.view()));
        SC_TRY(assignJSONField(entry.receipt, receiptPath));
        for (PackageReceiptExportJSON& exportView : receiptJSON.exports)
        {
            SC_TRY(appendJSONExport(entry.exports, exportView.kind.view(), exportView.name.view(),
                                    exportView.path.view()));
        }
        SC_TRY(lockJSON.packages.push_back(move(entry)));
        lockJSON.packageCount = static_cast<int>(lockJSON.packages.size());
        return Result(true);
    };

    if (fs.existsAndIsDirectory(packagesInstallDirectory))
    {
        FileSystemIterator::FolderState entries[8];
        FileSystemIterator              iterator;
        SC_TRY(iterator.init(packagesInstallDirectory, entries));
        while (iterator.enumerateNext())
        {
            const FileSystemIterator::Entry entry            = iterator.get();
            String                          candidateReceipt = StringEncoding::Utf8;
            SC_TRY(Path::join(candidateReceipt, {entry.path, PackageReceiptFileName}));
            SC_TRY(appendReceiptObject(candidateReceipt.view()));

            if (entry.isDirectory() or StringView(entry.name) != PackageReceiptFileName)
            {
                continue;
            }
            SC_TRY(appendReceiptObject(entry.path));
        }
        SC_TRY(iterator.checkErrors());
    }

    sortPackageLock(lockJSON);
    String lock = StringEncoding::Utf8;
    SC_TRY_MSG(SerializationJson::write(lockJSON, lock), "Failed writing package lock JSON");
    return fs.writeString(lockPath, lock.view());
}

Result runPackageTool(Tool::Arguments& arguments, PackageRegistry registry, Tools::Package* package)
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
    auto packageNameFromArguments = [&]() -> StringView
    { return arguments.arguments.sizeInElements() > 0 ? arguments.arguments[0] : "clang"_a8; };

    if (arguments.action == "help" or arguments.action == "--help" or
        (arguments.action == "install" and arguments.arguments.sizeInElements() > 0 and
         (arguments.arguments[0] == "--help"_a8 or arguments.arguments[0] == "-h"_a8)))
    {
        SC_TRY(printPackageHelp(console));
    }
    else if (arguments.action == "install")
    {
        const StringView            packageName = packageNameFromArguments();
        const PackageRegistryEntry* entry       = registry.find(packageName);
        if (entry == nullptr)
        {
            return printUnknownPackageError(console, registry, packageName);
        }
        if (entry->install != nullptr)
        {
            SC_TRY(entry->install(packagesCacheDirectory.view(), packagesInstallDirectory.view(), *package,
                                  arguments.arguments));
        }
        else if (entry->recipe != nullptr)
        {
            PackageRecipe recipe = *entry->recipe;
            if (recipe.download.packagesCacheDirectory.isEmpty())
            {
                SC_TRY(recipe.download.packagesCacheDirectory.assign(packagesCacheDirectory.view()));
            }
            if (recipe.download.packagesInstallDirectory.isEmpty())
            {
                SC_TRY(recipe.download.packagesInstallDirectory.assign(packagesInstallDirectory.view()));
            }
            if (recipe.download.packageName.isEmpty())
            {
                SC_TRY(recipe.download.packageName.assign(entry->installedName));
            }
            if (recipe.download.packageVersion.isEmpty())
            {
                SC_TRY(recipe.download.packageVersion.assign("local"));
            }
            if (recipe.package.installDirectoryLink.isEmpty())
            {
                SC_TRY(Path::join(recipe.package.installDirectoryLink,
                                  {packagesInstallDirectory.view(), entry->installedName}));
            }
            SC_TRY(installPackageRecipe(recipe, *package));
        }
        else
        {
            return Result::Error("Package registry entry is missing install handler or recipe");
        }
    }
    else if (arguments.action == "list")
    {
        SC_TRY(printKnownPackages(console, registry));
    }
    else if (arguments.action == "info")
    {
        const StringView            packageName = packageNameFromArguments();
        const PackageRegistryEntry* entry       = registry.find(packageName);
        if (entry == nullptr)
        {
            return printUnknownPackageError(console, registry, packageName);
        }
        console.print("name            = ");
        console.printLine(entry->name);
        console.print("kind            = ");
        console.printLine(entry->kind);
        console.print("installedName   = ");
        console.printLine(entry->installedName);
        console.print("variants        = ");
        console.printLine(entry->variants);
        console.print("source          = ");
        console.printLine(entry->source);
        console.print("supportsImport  = ");
        console.printLine(entry->supportsImport ? "true"_a8 : "false"_a8);
        console.print("description     = ");
        console.printLine(entry->description);
        SC_TRY(printStringViewList(console, "exports        "_a8, entry->exports));
        SC_TRY(printStringViewList(console, "phases         "_a8, entry->phases));
    }
    else if (arguments.action == "status" or arguments.action == "verify")
    {
        if (arguments.arguments.empty())
        {
            SC_TRY(printAllPackageStatuses(console, registry, packagesInstallDirectory.view(),
                                           arguments.action == "verify"));
            return Result(true);
        }
        const StringView            packageName = packageNameFromArguments();
        const PackageRegistryEntry* entry       = registry.find(packageName);
        if (entry == nullptr)
        {
            return printUnknownPackageError(console, registry, packageName);
        }
        String receiptPath = StringEncoding::Utf8;
        String packageRoot = StringEncoding::Utf8;
        bool   found       = false;
        SC_TRY(findInstalledPackageReceipt(packagesInstallDirectory.view(), entry->installedName, receiptPath,
                                           packageRoot, found));
        if (not found)
        {
            console.print("not installed: ");
            console.printLine(entry->name);
            return arguments.action == "verify" ? Result::Error("Package receipt not found") : Result(true);
        }
        if (arguments.action == "verify")
        {
            SC_TRY(verifyPackageReceiptForEntry(*entry, receiptPath.view(), packageRoot.view()));
            console.print("verified: ");
        }
        else
        {
            console.print("installed: ");
        }
        console.print(entry->name);
        console.print(" at ");
        console.print(packageRoot.view());
        if (arguments.action == "status")
        {
            const Result validation = verifyPackageReceiptForEntry(*entry, receiptPath.view(), packageRoot.view());
            console.print(validation ? " (receipt valid)"_a8 : " (receipt invalid)"_a8);
        }
        console.printLine(""_a8);
    }
    else if (arguments.action == "doctor")
    {
        if (arguments.arguments.empty())
        {
            SC_TRY(printPackageDoctor(console, registry, packagesInstallDirectory.view(), nullptr));
            return Result(true);
        }
        const StringView            packageName = packageNameFromArguments();
        const PackageRegistryEntry* entry       = registry.find(packageName);
        if (entry == nullptr)
        {
            return printUnknownPackageError(console, registry, packageName);
        }
        SC_TRY(printPackageDoctor(console, registry, packagesInstallDirectory.view(), entry));
    }
    else if (arguments.action == "receipt")
    {
        const StringView            packageName = packageNameFromArguments();
        const PackageRegistryEntry* entry       = registry.find(packageName);
        if (entry == nullptr)
        {
            return printUnknownPackageError(console, registry, packageName);
        }
        SC_TRY(printPackageReceipt(console, packagesInstallDirectory.view(), *entry));
    }
    else if (arguments.action == "exports")
    {
        const StringView            packageName = packageNameFromArguments();
        const PackageRegistryEntry* entry       = registry.find(packageName);
        if (entry == nullptr)
        {
            return printUnknownPackageError(console, registry, packageName);
        }
        SC_TRY(printPackageExports(console, packagesInstallDirectory.view(), *entry));
    }
    else if (arguments.action == "lock")
    {
        String lockPath = StringEncoding::Utf8;
        SC_TRY(Path::join(lockPath, {arguments.toolDestination.view(), "SC-package.lock"}));
        SC_TRY(lockInstalledPackages(packagesInstallDirectory.view(), lockPath.view()));
        console.print("lock            = ");
        console.printLine(lockPath.view());
    }
    else
    {
        SC_TRY(StringBuilder::format(buffer, "SC-package no action named \"{}\" exists", arguments.action));
        console.printLine(buffer.view());
        return Result::Error("SC-package error executing action");
    }
    return Result(true);
}

Result runPackageTool(Tool::Arguments& arguments, Tools::Package* package)
{
    return runPackageTool(arguments, builtinPackageRegistry(), package);
}

#if !defined(SC_TOOLS_COMPILED_SEPARATELY) && !defined(SC_TOOLS_IMPORT)
StringView Tool::getToolName() { return "SC-package"; }
StringView Tool::getDefaultAction() { return "install"; }
Result     Tool::runTool(Tool::Arguments& arguments) { return runPackageTool(arguments); }
#endif
} // namespace Tools
} // namespace SC
