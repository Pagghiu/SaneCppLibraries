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
