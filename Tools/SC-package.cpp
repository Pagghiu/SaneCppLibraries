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
    if (listProcess.getExitStatus() != 0)
        return Result::Error(PackageResultCategory, PackageError::ArchiveListingFailed);

    StringViewTokenizer tokenizer(archiveListing.view());
    if (not tokenizer.tokenizeNext({'\n'}))
        return Result::Error(PackageResultCategory, PackageError::ArchiveEmpty);

    StringView rootDirectory = tokenizer.component.trimAnyOf({'\r', '\n', '/'});
    StringView nestedPath;
    if (rootDirectory.splitBefore("/", nestedPath))
    {
        rootDirectory = nestedPath;
    }
    if (rootDirectory.isEmpty())
        return Result::Error(PackageResultCategory, PackageError::ArchiveRootMissing);

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
    if (extractProcess.getExitStatus() != 0)
        return Result::Error(PackageResultCategory, PackageError::ArchiveExtractionFailed);
    if (not fs.existsAndIsDirectory(extractedRoot.view()))
        return Result::Error(PackageResultCategory, PackageError::ExtractedRootMissing);

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

#include "SC-package/BuiltinInstallers.inl"

#include "SC-package/BuiltinCatalog.inl"

#include "SC-package/PackageHealth.inl"

#include "SC-package/PackageCLI.inl"

#if !defined(SC_TOOLS_COMPILED_SEPARATELY) && !defined(SC_TOOLS_IMPORT)
StringView Tool::getToolName() { return "SC-package"; }
StringView Tool::getDefaultAction() { return "install"; }
Result     Tool::runTool(Tool::Arguments& arguments) { return runPackageTool(arguments); }
#endif
} // namespace Tools
} // namespace SC
