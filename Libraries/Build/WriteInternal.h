// Copyright (c) 2022-2023, Stefano Cristiano
//
#pragma once
#include "../Foundation/AlgorithmSort.h"
#include "../Foundation/StringViewAlgorithms.h"
#include "Build.h"

namespace SC
{
namespace Build
{
struct WriterInternal;
}
} // namespace SC

struct SC::Build::WriterInternal
{
    struct RenderItem
    {
        enum Type
        {
            Unknown,
            HeaderFile,
            InlineFile,
            CppFile,
            Framework,
            Configuration
        };
        Type   type = Unknown;
        String name;
        // Paths
        String path;
        String referencePath;
        // Hashes
        String buildHash;
        String referenceHash;
    };

    struct RenderGroup
    {
        String name;
        String referenceHash;

        Map<String, RenderGroup> children;
    };

    struct Renderer
    {
        RenderGroup        rootGroup;
        Vector<RenderItem> renderItems;
    };

    [[nodiscard]] static bool fillFiles(const DefinitionCompiler& definitionCompiler, StringView destinationDirectory,
                                        const Project& project, Vector<RenderItem>& outputFiles)
    {
        String renderedFile;
        for (const auto& file : project.files)
        {
            SC_TRY_IF(Path::join(renderedFile, {project.rootDirectory.view(), file.base.view(), file.mask.view()},
                                 Path::Posix::SeparatorStringView()));
            const auto* res = definitionCompiler.resolvedPaths.get(renderedFile.view());
            if (res)
            {
                for (const auto& it : *res)
                {
                    RenderItem xcodeFile;
                    xcodeFile.name.encoding = StringEncoding::Utf8; // To unify hashes
                    SC_TRY_IF(StringBuilder(xcodeFile.name).append(Path::basename(it.view(), Path::AsPosix)));
                    if (xcodeFile.name.view().endsWith(".h"))
                    {
                        xcodeFile.type = RenderItem::HeaderFile;
                    }
                    else if (xcodeFile.name.view().endsWith(".cpp"))
                    {
                        xcodeFile.type = RenderItem::CppFile;
                    }
                    else if (xcodeFile.name.view().endsWith(".inl"))
                    {
                        xcodeFile.type = RenderItem::InlineFile;
                    }
                    SC_TRY_IF(
                        Path::relativeFromTo(destinationDirectory, it.view(), xcodeFile.path, Path::Type::AsPosix));
                    SC_TRY_IF(Path::relativeFromTo(project.rootDirectory.view(), it.view(), xcodeFile.referencePath,
                                                   Path::Type::AsPosix));
                    if (file.operation == Project::File::Add)
                    {
                        SC_TRY_IF(outputFiles.push_back(move(xcodeFile)));
                    }
                    else
                    {
                        (void)(outputFiles.removeAll([&](const auto& it)
                                                     { return it.referencePath == xcodeFile.referencePath; }));
                    }
                }
            }
            else
            {
                return false;
            }
        }
        bubbleSort(outputFiles.begin(), outputFiles.end(),
                   [](const RenderItem& a1, const RenderItem& a2)
                   { return a1.path.view().compare(a2.path.view()) == StringView::Comparison::Smaller; });
        return true;
    }
};
