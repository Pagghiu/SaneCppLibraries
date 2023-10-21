// Copyright (c) 2022-2023, Stefano Cristiano
//
#pragma once
#include "../../Foundation/Algorithms/AlgorithmSort.h"
#include "../Build.h"

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
            Configuration,
            DebugVisualizerfile
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

        VectorMap<String, RenderGroup> children;
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
            SC_TRY(Path::join(renderedFile, {project.rootDirectory.view(), file.base.view(), file.mask.view()},
                              Path::Posix::SeparatorStringView()));
            const auto* res = definitionCompiler.resolvedPaths.get(renderedFile.view());
            if (res)
            {
                for (const auto& it : *res)
                {
                    RenderItem renderItem;
                    renderItem.name = StringEncoding::Utf8; // To unify hashes
                    SC_TRY(StringBuilder(renderItem.name).append(Path::basename(it.view(), Path::AsPosix)));
                    auto nameView = renderItem.name.view();
                    if (nameView.endsWith(".h"))
                    {
                        renderItem.type = RenderItem::HeaderFile;
                    }
                    else if (nameView.endsWith(".cpp"))
                    {
                        renderItem.type = RenderItem::CppFile;
                    }
                    else if (nameView.endsWith(".inl"))
                    {
                        renderItem.type = RenderItem::InlineFile;
                    }
                    else if (nameView.endsWith(".natvis") or nameView.endsWith(".lldbinit"))
                    {
                        renderItem.type = RenderItem::DebugVisualizerfile;
                    }
                    SC_TRY(Path::relativeFromTo(destinationDirectory, it.view(), renderItem.path,
                                                Path::Type::AsNative,  // input type
                                                Path::Type::AsPosix)); // output type
                    SC_TRY(Path::relativeFromTo(project.rootDirectory.view(), it.view(), renderItem.referencePath,
                                                Path::Type::AsNative,  // input type
                                                Path::Type::AsPosix)); // output type
                    if (file.operation == Project::File::Add)
                    {
                        SC_TRY(outputFiles.push_back(move(renderItem)));
                    }
                    else
                    {
                        (void)(outputFiles.removeAll([&](const auto& it)
                                                     { return it.referencePath == renderItem.referencePath; }));
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
