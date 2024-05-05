// Copyright (c) 2022-2023, Stefano Cristiano
//
#pragma once
#include "../../Algorithms/AlgorithmBubbleSort.h"
#include "../Build.h"

namespace SC
{
namespace Build
{
struct RelativeDirectories
{
    StringNative<256> relativeProjectsToOutputs;       // _Projects ->_Outputs
    StringNative<256> relativeProjectsToIntermediates; // _Projects ->_Intermediate
    StringNative<256> relativeProjectsToProjectRoot;   // _Projects -> Project::setRootDirectory
    StringNative<256> projectRootRelativeToProjects;   // Project root (expressed relative to $(PROJECT_DIR)

    Result computeRelativeDirectories(Directories directories, Path::Type outputType, const Project& project,
                                      const StringView projectDirFormatString)
    {
        SC_TRY(Path::relativeFromTo(directories.projectsDirectory.view(), directories.outputsDirectory.view(),
                                    relativeProjectsToOutputs, Path::AsNative, outputType));
        SC_TRY(Path::relativeFromTo(directories.projectsDirectory.view(), directories.intermediatesDirectory.view(),
                                    relativeProjectsToIntermediates, Path::AsNative, outputType));

        SC_TRY(Path::relativeFromTo(directories.projectsDirectory.view(), project.rootDirectory.view(),
                                    relativeProjectsToProjectRoot, Path::AsNative, outputType));
        SC_TRY(StringBuilder(projectRootRelativeToProjects, StringBuilder::Clear)
                   .format(projectDirFormatString, relativeProjectsToProjectRoot));

        return Result(true);
    }
};
struct WriterInternal;
} // namespace Build
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
            CFile,
            Framework,
            Configuration,
            DebugVisualizerFile
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

    [[nodiscard]] static bool appendPrefixIfRelativePosix(StringView relativeVariable, StringBuilder& builder,
                                                          StringView text, StringView prefix)
    {
        if (not text.startsWith(relativeVariable) and not Path::isAbsolute(text, Path::AsNative))
        {
            SC_TRY(builder.append(relativeVariable));
            SC_TRY(builder.append(Path::Posix::SeparatorStringView()));
            SC_TRY(builder.append(prefix));
            SC_TRY(builder.append(Path::Posix::SeparatorStringView()));
        }
        return true;
    }

    [[nodiscard]] static bool appendPrefixIfRelativeMSVC(StringView relativeVariable, StringBuilder& builder,
                                                         StringView text, StringView prefix)
    {
        if (not text.startsWith(relativeVariable) and not Path::isAbsolute(text, Path::AsNative))
        {
            SC_TRY(builder.append(relativeVariable));
            SC_TRY(builder.append(prefix));
            SC_TRY(builder.append(Path::Windows::SeparatorStringView()));
        }
        return true;
    }

    [[nodiscard]] static bool getPathsRelativeTo(StringView                referenceDirectory,
                                                 const DefinitionCompiler& definitionCompiler, const Project& project,
                                                 Vector<RenderItem>& outputFiles)
    {
        String renderedFile;
        for (const auto& file : project.files)
        {
            SC_TRY(Path::join(renderedFile, {project.rootDirectory.view(), file.base.view(), file.mask.view()},
                              Path::Posix::SeparatorStringView(), true)); // skipEmpty == true
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
                    else if (nameView.endsWith(".c"))
                    {
                        renderItem.type = RenderItem::CFile;
                    }
                    else if (nameView.endsWith(".inl"))
                    {
                        renderItem.type = RenderItem::InlineFile;
                    }
                    else if (nameView.endsWith(".natvis") or nameView.endsWith(".lldbinit"))
                    {
                        renderItem.type = RenderItem::DebugVisualizerFile;
                    }
                    SC_TRY(Path::relativeFromTo(referenceDirectory, it.view(), renderItem.path,
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
        Algorithms::bubbleSort(outputFiles.begin(), outputFiles.end(),
                               [](const RenderItem& a1, const RenderItem& a2)
                               { return a1.path.view().compare(a2.path.view()) == StringView::Comparison::Smaller; });
        return true;
    }
};
