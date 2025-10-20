// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "Build.h"

#include "../../Libraries/Containers/Algorithms/AlgorithmBubbleSort.h"
#include "../../Libraries/Containers/VectorMap.h"
#include "../../Libraries/Containers/VectorSet.h"
#include "../../Libraries/Strings/Path.h"
#include "../../Libraries/Strings/StringBuilder.h"

namespace SC
{
namespace Build
{

struct FilePathsResolver;
/// @brief Writes all project files for a given Definition with some Parameters using the provided FilePathsResolver
struct ProjectWriter
{
    const Definition&        definition;
    const FilePathsResolver& filePathsResolver;
    const Parameters&        parameters;

    ProjectWriter(const Definition& definition, const FilePathsResolver& filePathsResolver,
                  const Parameters& parameters)
        : definition(definition), filePathsResolver(filePathsResolver), parameters(parameters)
    {}

    /// @brief Write the project file at given directories
    Result write(StringView filename);

    /// @brief Holds a search / replace pair for StringBuilder::appendReplaceMultiple
    struct ReplacePair
    {
        StringView searchFor;   /// StringView to be searched for in source string
        StringView replaceWith; /// StringView that will replace all instances of 'searchFor'
    };

    /// @brief Appends source to destination buffer, replacing multiple substitutions pairs
    /// @param source The StringView to be appended
    /// @param substitutions For each substitution in the span, the first is searched and replaced with the second.
    /// @return `true` if append succeeded
    ///
    /// Example:
    /// @snippet Tests/Libraries/Build/BuildTest.cpp stringBuilderTestAppendReplaceMultipleSnippet
    [[nodiscard]] static bool appendReplaceMultiple(StringBuilder& builder, StringView source,
                                                    Span<const ReplacePair> substitutions)
    {
        String tempBuffer, other;
        SC_TRY(tempBuffer.assign(source));
        for (auto it : substitutions)
        {
            if (it.searchFor == it.replaceWith)
                continue;
            auto sb = StringBuilder::create(other);
            SC_TRY(sb.appendReplaceAll(tempBuffer.view(), it.searchFor, it.replaceWith));
            sb.finalize();
            swap(other, tempBuffer);
        }
        return builder.append(tempBuffer.view());
    }

  private:
    struct WriterXCode;
    struct WriterVisualStudio;
    struct WriterMakefile;
};
/// @brief Caches file paths by pre-resolving directory filter search masks
struct FilePathsResolver
{
    VectorMap<String, Vector<String>> resolvedPaths;

    Result resolve(const Build::Definition& definition);

    static Result enumerateFileSystemFor(StringView path, const VectorSet<FilesSelection>& filters,
                                         VectorMap<String, Vector<String>>& filtersToFiles);
    static Result mergePathsFor(const FilesSelection& selection, const StringView rootDirectory, String& buffer,
                                VectorMap<String, VectorSet<FilesSelection>>& paths);
};

struct RelativeDirectories
{
    SmallStringNative<256> relativeProjectsToOutputs;       // _Projects ->_Outputs
    SmallStringNative<256> relativeProjectsToIntermediates; // _Projects ->_Intermediate
    SmallStringNative<256> relativeProjectsToProjectRoot;   // _Projects -> Project::setRootDirectory
    SmallStringNative<256> projectRootRelativeToProjects;   // Project root (expressed relative to $(PROJECT_DIR)

    Result computeRelativeDirectories(Directories directories, Path::Type outputType, const Project& project,
                                      const StringView projectDirFormatString)
    {
        SC_TRY(Path::relativeFromTo(relativeProjectsToOutputs, directories.projectsDirectory.view(),
                                    directories.outputsDirectory.view(), Path::AsNative, outputType));
        SC_TRY(Path::relativeFromTo(relativeProjectsToIntermediates, directories.projectsDirectory.view(),
                                    directories.intermediatesDirectory.view(), Path::AsNative, outputType));

        SC_TRY(Path::relativeFromTo(relativeProjectsToProjectRoot, directories.projectsDirectory.view(),
                                    project.rootDirectory.view(), Path::AsNative, outputType));
        SC_TRY(StringBuilder::format(projectRootRelativeToProjects, projectDirFormatString,
                                     relativeProjectsToProjectRoot));

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
            ObjCFile,
            ObjCppFile,
            CFile,
            Framework,
            SystemLibrary,
            Configuration,
            DebugVisualizerFile,
            XCAsset
        };
        Type   type = Unknown;
        String name;
        // Paths
        String path;
        String referencePath;
        // Hashes
        String buildHash;
        String referenceHash;

        Vector<String>      platformFilters;
        const CompileFlags* compileFlags = nullptr;

        [[nodiscard]] static StringView getExtension(RenderItem::Type type)
        {
            switch (type)
            {
            case RenderItem::CppFile: {
                return ".cpp";
            }
            case RenderItem::CFile: {
                return ".c";
            }
            case RenderItem::ObjCFile: {
                return ".m";
            }
            case RenderItem::ObjCppFile: {
                return ".mm";
            }
            default: return StringView();
            }
        }
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

    [[nodiscard]] static Result getPathsRelativeTo(StringView referenceDirectory, StringView rootDirectory,
                                                   const SourceFiles& files, const FilePathsResolver& filePathsResolver,

                                                   Vector<RenderItem>& outputFiles)
    {
        String             renderedFile;
        Vector<StringView> components;
        for (const FilesSelection& file : files.selection)
        {
            if (Path::isAbsolute(file.base.view(), Path::AsNative))
            {
                SC_TRY(Path::normalize(renderedFile, file.base.view(), Path::AsPosix));
                SC_TRY(Path::append(renderedFile, {file.mask.view()}, Path::AsPosix));
            }
            else
            {
                StringView paths[3];
                paths[0] = rootDirectory;
                paths[1] = file.base.view();
                paths[2] = file.mask.view();
                // skipEmpty == true
                SC_TRY(Path::join(renderedFile, {paths}, Path::Posix::SeparatorStringView(), true));
            }
            const Vector<String>* res = filePathsResolver.resolvedPaths.get(renderedFile.view());
            if (res == nullptr)
            {
                return Result::Error("BuildWriter::getPathsRelativeTo - Cannot find path");
            }
            for (const String& it : *res)
            {
                RenderItem renderItem;
                renderItem.name = StringView(StringEncoding::Utf8); // To unify hashes
                auto sb         = StringBuilder::createForAppendingTo(renderItem.name);
                SC_TRY(sb.append(Path::basename(it.view(), Path::AsPosix)));
                auto nameView = sb.finalize();
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
                else if (nameView.endsWith(".m"))
                {
                    renderItem.type = RenderItem::ObjCFile;
                }
                else if (nameView.endsWith(".mm"))
                {
                    renderItem.type = RenderItem::ObjCppFile;
                }
                else if (nameView.endsWith(".inl"))
                {
                    renderItem.type = RenderItem::InlineFile;
                }
                else if (nameView.endsWith(".natvis") or nameView.endsWith(".lldbinit"))
                {
                    renderItem.type = RenderItem::DebugVisualizerFile;
                }
                renderItem.compileFlags = &files.compile;
                SC_TRY(Path::relativeFromTo(renderItem.path, referenceDirectory, it.view(),
                                            Path::Type::AsNative,  // input type
                                            Path::Type::AsPosix)); // output type
                SC_TRY(Path::relativeFromTo(renderItem.referencePath, rootDirectory, it.view(),
                                            Path::Type::AsNative,  // input type
                                            Path::Type::AsPosix)); // output type
                if (file.action == FilesSelection::Add)
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
        Algorithms::bubbleSort(outputFiles.begin(), outputFiles.end(), [](const RenderItem& a1, const RenderItem& a2)
                               { return a1.path.view().compare(a2.path.view()) == StringView::Comparison::Smaller; });
        return Result(true);
    }

    [[nodiscard]] static Result renderProject(StringView projectDirectory, const Project& project,
                                              const FilePathsResolver& filePathsResolver,
                                              Vector<RenderItem>&      outputFiles)
    {
        SC_TRY(WriterInternal::getPathsRelativeTo(projectDirectory, project.rootDirectory.view(), project.files,
                                                  filePathsResolver, outputFiles));
        // TODO: Improve per file flags handling, this is not 100% correct and not even efficient
        Vector<RenderItem> filesWithSpecificFlags;
        for (const SourceFiles& files : project.filesWithSpecificFlags)
        {
            SC_TRY(WriterInternal::getPathsRelativeTo(projectDirectory, project.rootDirectory.view(), files,
                                                      filePathsResolver, filesWithSpecificFlags));
        }
        for (RenderItem& it : outputFiles)
        {
            size_t index = 0;

            const bool hasPerFileFlags = filesWithSpecificFlags.find(
                [&](const RenderItem& item)
                {
                    if (it.path == item.path)
                    {
                        return true;
                    }
                    return false;
                },
                &index);
            if (hasPerFileFlags)
            {
                it.compileFlags = filesWithSpecificFlags[index].compileFlags;
            }
            else
            {
                it.compileFlags = nullptr; // This is the shared compile flags
            }
        }
        return Result(true);
    }
};
