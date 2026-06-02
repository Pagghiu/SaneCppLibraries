// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
// Keep Windows long-path specific command rewriting, manifest embedding, and
// depfile restoration here so BuildNative.inl can stay focused on native build
// orchestration.

namespace SC
{
namespace Build
{
class NativeBuildWindowsLongPath
{
  public:
    static bool isWindowsLongPathAware(const Project& project, const Configuration& configuration)
    {
        if (configuration.windows.longPathAware.hasBeenSet())
        {
            return configuration.windows.longPathAware;
        }
        return project.windows.longPathAware;
    }

  private:
    static Result appendResourceScriptString(String& output, StringView value)
    {
        SC_TRY(StringBuilder::createForAppendingTo(output).append("\""));
        const char* bytes = value.bytesWithoutTerminator();
        for (size_t idx = 0; idx < value.sizeInBytes(); ++idx)
        {
            const char c = bytes[idx];
            if (c == '"' or c == '\\')
            {
                SC_TRY(StringBuilder::createForAppendingTo(output).append("\\"));
            }
            const char character[] = {c, 0};
            SC_TRY(StringBuilder::createForAppendingTo(output).append(
                StringView::fromNullTerminated(character, StringEncoding::Utf8)));
        }
        SC_TRY(StringBuilder::createForAppendingTo(output).append("\""));
        return Result(true);
    }

  public:
    template <typename ResolvedProject>
    static Result writeWindowsLongPathManifest(FileSystem& fs, const ResolvedProject& resolvedProject)
    {
        if (resolvedProject.windowsLongPathManifestPath.isEmpty())
        {
            return Result(true);
        }
        String manifest = StringEncoding::Utf8;
        auto   builder  = StringBuilder::create(manifest);
        SC_TRY(
            builder.append("<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n"
                           "<assembly xmlns=\"urn:schemas-microsoft-com:asm.v1\" manifestVersion=\"1.0\">\n"
                           "  <application xmlns=\"urn:schemas-microsoft-com:asm.v3\">\n"
                           "    <windowsSettings xmlns:ws2=\"http://schemas.microsoft.com/SMI/2016/WindowsSettings\">\n"
                           "      <ws2:longPathAware>true</ws2:longPathAware>\n"
                           "    </windowsSettings>\n"
                           "  </application>\n"
                           "</assembly>\n"));
        builder.finalize();
        SC_TRY(fs.writeString(resolvedProject.windowsLongPathManifestPath.view(), manifest.view()));
        if (resolvedProject.adapter.family == Toolchain::LLVMMingw)
        {
            String resourceScript = StringEncoding::Utf8;
            SC_TRY(StringBuilder::createForAppendingTo(resourceScript).append("1 24 "));
            SC_TRY(appendResourceScriptString(resourceScript, resolvedProject.windowsLongPathManifestPath.view()));
            SC_TRY(StringBuilder::createForAppendingTo(resourceScript).append("\n"));
            SC_TRY(fs.writeString(resolvedProject.windowsLongPathResourceScriptPath.view(), resourceScript.view()));
        }
        return Result(true);
    }

  private:
    static bool relativePathEscapesRoot(StringView relativePath)
    {
        return relativePath == ".."_a8 or relativePath.startsWith("..\\"_a8) or relativePath.startsWith("../"_a8);
    }

    static Result tryNormalizeWindowsCommandPathForAlias(StringView actualRoot, StringView aliasName, StringView path,
                                                         String& output, bool& handled)
    {
        handled = false;
        if (actualRoot.isEmpty())
        {
            return Result(true);
        }

        String relative = StringEncoding::Utf8;
        if (not Path::relativeFromTo(relative, actualRoot, path, Path::AsNative, Path::AsNative))
        {
            return Result(true);
        }
        StringView relativeView = relative.view();
        if (relativePathEscapesRoot(relativeView))
        {
            return Result(true);
        }
        while (relativeView.startsWith(".\\"_a8) or relativeView.startsWith("./"_a8))
        {
            relativeView = relativeView.sliceStart(2);
        }

        if (relativeView.isEmpty() or relativeView == "."_a8)
        {
            SC_TRY(output.assign(aliasName));
        }
        else
        {
            SC_TRY(Path::join(output, {aliasName, relativeView}, Path::SeparatorStringView()));
        }
        handled = true;
        return Result(true);
    }

    static Result assignNormalizedDependencyPath(String& output, StringView path)
    {
        String normalized = StringEncoding::Utf8;
        SC_TRY(Path::normalize(normalized, path, Path::AsNative));
        output = move(normalized);
        return Result(true);
    }

    static Result tryNormalizeWindowsDependencyPathFromRelativeAlias(StringView aliasName, StringView actualRoot,
                                                                     StringView path, String& output, bool& handled)
    {
        handled = false;
        if (actualRoot.isEmpty())
        {
            return Result(true);
        }

        if (path == aliasName)
        {
            SC_TRY(assignNormalizedDependencyPath(output, actualRoot));
            handled = true;
            return Result(true);
        }
        if (not path.startsWith(aliasName))
        {
            return Result(true);
        }

        StringView relativeView = path.sliceStart(aliasName.sizeInBytes());
        if (not(relativeView.startsWith("\\"_a8) or relativeView.startsWith("/"_a8)))
        {
            return Result(true);
        }
        relativeView  = relativeView.sliceStart(1);
        String mapped = StringEncoding::Utf8;
        SC_TRY(Path::join(mapped, {actualRoot, relativeView}, Path::SeparatorStringView()));
        SC_TRY(assignNormalizedDependencyPath(output, mapped.view()));
        handled = true;
        return Result(true);
    }

    static Result tryNormalizeWindowsDependencyPathFromAlias(StringView aliasRoot, StringView actualRoot,
                                                             StringView path, String& output, bool& handled)
    {
        handled = false;
        if (aliasRoot.isEmpty() or actualRoot.isEmpty())
        {
            return Result(true);
        }

        String relative = StringEncoding::Utf8;
        if (not Path::relativeFromTo(relative, aliasRoot, path, Path::AsNative, Path::AsNative))
        {
            return Result(true);
        }
        StringView relativeView = relative.view();
        if (relativePathEscapesRoot(relativeView))
        {
            return Result(true);
        }
        while (relativeView.startsWith(".\\"_a8) or relativeView.startsWith("./"_a8))
        {
            relativeView = relativeView.sliceStart(2);
        }

        if (relativeView.isEmpty() or relativeView == "."_a8)
        {
            SC_TRY(assignNormalizedDependencyPath(output, actualRoot));
        }
        else
        {
            String mapped = StringEncoding::Utf8;
            SC_TRY(Path::join(mapped, {actualRoot, relativeView}, Path::SeparatorStringView()));
            SC_TRY(assignNormalizedDependencyPath(output, mapped.view()));
        }
        handled = true;
        return Result(true);
    }

    static Result tryNormalizeWindowsDependencyPathFromCommandRoots(StringView aliasName, StringView actualRoot,
                                                                    StringView path, String& output, bool& handled)
    {
        handled = false;
        if (actualRoot.isEmpty())
        {
            return Result(true);
        }

        StringView afterCommandRoot;
        if (not path.splitAfter("\\SC-build\\CommandRoots\\root-"_a8, afterCommandRoot) and
            not path.splitAfter("/SC-build/CommandRoots/root-"_a8, afterCommandRoot))
        {
            return Result(true);
        }

        String marker = StringEncoding::Utf8;
        SC_TRY(StringBuilder::format(marker, "\\{}\\", aliasName));
        StringView relativeView;
        if (not afterCommandRoot.splitAfter(marker.view(), relativeView))
        {
            SC_TRY(StringBuilder::format(marker, "/{}/", aliasName));
            if (not afterCommandRoot.splitAfter(marker.view(), relativeView))
            {
                return Result(true);
            }
        }

        String mapped = StringEncoding::Utf8;
        SC_TRY(Path::join(mapped, {actualRoot, relativeView}, Path::SeparatorStringView()));
        SC_TRY(assignNormalizedDependencyPath(output, mapped.view()));
        handled = true;
        return Result(true);
    }

    template <typename ResolvedProject>
    static Result normalizeWindowsCommandPath(const ResolvedProject& resolvedProject, StringView path, String& output)
    {
#if SC_PLATFORM_WINDOWS
        if (resolvedProject.windowsLongPathAware and
            resolvedProject.targetContext.targetMachine.platform == Platform::Windows)
        {
            bool handled = false;
            SC_TRY(tryNormalizeWindowsCommandPathForAlias(resolvedProject.commandProjectRoot.view(), "project"_a8, path,
                                                          output, handled));
            if (handled)
            {
                return Result(true);
            }
            SC_TRY(tryNormalizeWindowsCommandPathForAlias(resolvedProject.commandLibraryRoot.view(), "scroot"_a8, path,
                                                          output, handled));
            if (handled)
            {
                return Result(true);
            }
        }
#else
        (void)(resolvedProject);
#endif
        SC_TRY(output.assign(path));
        return Result(true);
    }

  public:
    template <typename CommandLine, typename ResolvedProject>
    static Result appendCommandPath(CommandLine& commandLine, const ResolvedProject& resolvedProject, StringView path)
    {
        String normalizedPath = StringEncoding::Utf8;
        SC_TRY(normalizeWindowsCommandPath(resolvedProject, path, normalizedPath));
        return commandLine.append(normalizedPath.view());
    }

    template <typename CommandLine, typename ResolvedProject>
    static Result appendCommandPathFlag(CommandLine& commandLine, const ResolvedProject& resolvedProject,
                                        StringView prefix, StringView path)
    {
        String normalizedPath = StringEncoding::Utf8;
        SC_TRY(normalizeWindowsCommandPath(resolvedProject, path, normalizedPath));
        String option = StringEncoding::Utf8;
        SC_TRY(StringBuilder::format(option, "{}{}", prefix, normalizedPath.view()));
        return commandLine.append(option.view());
    }

  private:
    template <typename ResolvedProject>
    static bool shouldEmbedWindowsLongPathManifest(const ResolvedProject& resolvedProject)
    {
        return resolvedProject.windowsLongPathAware and not resolvedProject.windowsLongPathManifestPath.isEmpty();
    }

  public:
    template <typename CommandLine, typename ResolvedProject>
    static Result appendMSVCWindowsLongPathManifest(CommandLine& commandLine, const ResolvedProject& resolvedProject)
    {
        if (not shouldEmbedWindowsLongPathManifest(resolvedProject))
        {
            return Result(true);
        }
        SC_TRY(commandLine.append("/MANIFEST:EMBED"));
        SC_TRY(appendCommandPathFlag(commandLine, resolvedProject, "/MANIFESTINPUT:"_a8,
                                     resolvedProject.windowsLongPathManifestPath.view()));
        return Result(true);
    }

    template <typename CommandLine, typename ResolvedProject>
    static Result appendGenericWindowsLongPathManifest(CommandLine& commandLine, const ResolvedProject& resolvedProject)
    {
        if (not shouldEmbedWindowsLongPathManifest(resolvedProject))
        {
            return Result(true);
        }
        if (resolvedProject.adapter.family != Toolchain::LLVMMingw)
        {
            return Result::Error("Windows long-path-aware manifests require an MSVC-style or llvm-mingw linker");
        }

        SC_TRY_MSG(not resolvedProject.windowsLongPathResourcePath.isEmpty(),
                   "Missing llvm-mingw Windows manifest resource path");
        SC_TRY(commandLine.append(resolvedProject.windowsLongPathResourcePath.view()));
        return Result(true);
    }

    template <typename CommandLine, typename ResolvedProject>
    static Result buildWindowsLongPathResourceCommand(const ResolvedProject& resolvedProject, CommandLine& commandLine)
    {
        if (not shouldEmbedWindowsLongPathManifest(resolvedProject) or
            resolvedProject.adapter.family != Toolchain::LLVMMingw)
        {
            return Result(true);
        }
        SC_TRY_MSG(not resolvedProject.adapter.executableResourceCompiler.isEmpty(),
                   "Missing llvm-mingw resource compiler");
        SC_TRY(commandLine.append(resolvedProject.adapter.executableResourceCompiler.view()));
        SC_TRY(commandLine.append(resolvedProject.windowsLongPathResourceScriptPath.view()));
        SC_TRY(commandLine.append("-O"));
        SC_TRY(commandLine.append("coff"));
        SC_TRY(commandLine.append("-o"));
        SC_TRY(commandLine.append(resolvedProject.windowsLongPathResourcePath.view()));
        return Result(true);
    }

  private:
    static bool mayNeedWindowsCommandRootAlias(StringView path)
    {
        constexpr size_t CommandRootAliasThreshold = 240;
        return path.sizeInBytes() >= CommandRootAliasThreshold;
    }

    template <typename ResolvedProject>
    static bool shouldUseWindowsCommandRootAlias(const ResolvedProject& resolvedProject)
    {
        if (not resolvedProject.windowsLongPathAware or
            resolvedProject.targetContext.targetMachine.platform != Platform::Windows or
            resolvedProject.commandProjectRoot.view().isEmpty())
        {
            return false;
        }

        if (mayNeedWindowsCommandRootAlias(resolvedProject.commandProjectRoot.view()) or
            mayNeedWindowsCommandRootAlias(resolvedProject.commandLibraryRoot.view()) or
            mayNeedWindowsCommandRootAlias(resolvedProject.targetDirectory.view()) or
            mayNeedWindowsCommandRootAlias(resolvedProject.intermediateDirectory.view()) or
            mayNeedWindowsCommandRootAlias(resolvedProject.executablePath.view()) or
            mayNeedWindowsCommandRootAlias(resolvedProject.linkCommandPath.view()) or
            mayNeedWindowsCommandRootAlias(resolvedProject.linkResponsePath.view()) or
            mayNeedWindowsCommandRootAlias(resolvedProject.compileCommandsPath.view()) or
            mayNeedWindowsCommandRootAlias(resolvedProject.workspaceCompileCommandsPath.view()) or
            mayNeedWindowsCommandRootAlias(resolvedProject.windowsLongPathManifestPath.view()) or
            mayNeedWindowsCommandRootAlias(resolvedProject.windowsLongPathResourceScriptPath.view()) or
            mayNeedWindowsCommandRootAlias(resolvedProject.windowsLongPathResourcePath.view()))
        {
            return true;
        }

        for (const auto& source : resolvedProject.sources)
        {
            if (mayNeedWindowsCommandRootAlias(source.sourcePath.view()) or
                mayNeedWindowsCommandRootAlias(source.objectPath.view()) or
                mayNeedWindowsCommandRootAlias(source.dependencyPath.view()) or
                mayNeedWindowsCommandRootAlias(source.commandPath.view()) or
                mayNeedWindowsCommandRootAlias(source.responsePath.view()))
            {
                return true;
            }
        }
        return false;
    }

    template <typename ResolvedProject>
    static Result disableWindowsCommandRootAlias(ResolvedProject& resolvedProject)
    {
        SC_TRY(resolvedProject.commandProjectRoot.assign({}));
        SC_TRY(resolvedProject.commandLibraryRoot.assign({}));
        SC_TRY(resolvedProject.commandWorkingDirectory.assign({}));
        return Result(true);
    }

  public:
    template <typename ResolvedProject>
    static Result prepareWindowsCommandWorkingDirectories(FileSystem& fs, Vector<ResolvedProject>& resolvedProjects)
    {
#if SC_PLATFORM_WINDOWS
        for (ResolvedProject& resolvedProject : resolvedProjects)
        {
            if (resolvedProject.commandProjectRoot.view().isEmpty())
            {
                continue;
            }
            if (not shouldUseWindowsCommandRootAlias(resolvedProject))
            {
                SC_TRY(disableWindowsCommandRootAlias(resolvedProject));
                continue;
            }

            wchar_t* localAppDataStorage = nullptr;
            wchar_t* tempDirStorage      = nullptr;
            auto     freeEnvStorage      = MakeDeferred(
                [&]
                {
                    ::free(localAppDataStorage);
                    ::free(tempDirStorage);
                });
            (void)::_wdupenv_s(&localAppDataStorage, nullptr, L"LOCALAPPDATA");
            (void)::_wdupenv_s(&tempDirStorage, nullptr, L"TEMP");

            const wchar_t* aliasBaseEnv = nullptr;
            if (localAppDataStorage != nullptr and localAppDataStorage[0] != L'\0')
            {
                aliasBaseEnv = localAppDataStorage;
            }
            else if (tempDirStorage != nullptr and tempDirStorage[0] != L'\0')
            {
                aliasBaseEnv = tempDirStorage;
            }
            if (aliasBaseEnv == nullptr or aliasBaseEnv[0] == L'\0')
            {
                continue;
            }

            String aliasBase = StringEncoding::Utf8;
            SC_TRY(aliasBase.assign(StringView::fromNullTerminated(aliasBaseEnv, StringEncoding::Utf16)));
            SC_TRY(Path::append(aliasBase, {"SC-build", "CommandRoots"}, Path::AsNative));
            SC_TRY(fs.makeDirectoryRecursive(aliasBase.view()));

            String aliasName = StringEncoding::Utf8;
            SC_TRY(StringBuilder::format(aliasName, "root-{}-{}", Time::Realtime::now().milliseconds,
                                         resolvedProject.project->targetName.view()));
            String aliasRoot = StringEncoding::Utf8;
            SC_TRY(Path::join(aliasRoot, {aliasBase.view(), aliasName.view()}));
            if (fs.existsAndIsLink(aliasRoot.view()))
            {
                SC_TRY(fs.removeEmptyDirectory(aliasRoot.view()));
            }
            else if (fs.existsAndIsDirectory(aliasRoot.view()))
            {
                SC_TRY(fs.removeDirectoryRecursive(aliasRoot.view()));
            }

            SC_TRY(fs.makeDirectoryRecursive(aliasRoot.view()));
            String projectAlias = StringEncoding::Utf8;
            SC_TRY(Path::join(projectAlias, {aliasRoot.view(), "project"_a8}, Path::SeparatorStringView()));
            SC_TRY(fs.createSymbolicLink(resolvedProject.commandProjectRoot.view(), projectAlias.view()));

            if (not resolvedProject.commandLibraryRoot.view().isEmpty() and
                resolvedProject.commandLibraryRoot.view() != resolvedProject.commandProjectRoot.view())
            {
                String libraryAlias = StringEncoding::Utf8;
                SC_TRY(Path::join(libraryAlias, {aliasRoot.view(), "scroot"_a8}, Path::SeparatorStringView()));
                SC_TRY(fs.createSymbolicLink(resolvedProject.commandLibraryRoot.view(), libraryAlias.view()));
            }
            SC_TRY(resolvedProject.commandWorkingDirectory.assign(aliasRoot.view()));
        }
#else
        (void)(fs);
        (void)(resolvedProjects);
#endif
        return Result(true);
    }

    template <typename ResolvedProject>
    static Result normalizeDependencyPath(const ResolvedProject& resolvedProject, String& dependencyPath)
    {
#if SC_PLATFORM_WINDOWS
        if (resolvedProject.windowsLongPathAware and
            resolvedProject.targetContext.targetMachine.platform == Platform::Windows)
        {
            bool handled = false;
            SC_TRY(tryNormalizeWindowsDependencyPathFromRelativeAlias("project"_a8,
                                                                      resolvedProject.commandProjectRoot.view(),
                                                                      dependencyPath.view(), dependencyPath, handled));
            if (handled)
            {
                return Result(true);
            }
            if (not resolvedProject.commandLibraryRoot.view().isEmpty() and
                resolvedProject.commandLibraryRoot.view() != resolvedProject.commandProjectRoot.view())
            {
                SC_TRY(tryNormalizeWindowsDependencyPathFromRelativeAlias(
                    "scroot"_a8, resolvedProject.commandLibraryRoot.view(), dependencyPath.view(), dependencyPath,
                    handled));
                if (handled)
                {
                    return Result(true);
                }
            }

            SC_TRY(tryNormalizeWindowsDependencyPathFromCommandRoots("project"_a8,
                                                                     resolvedProject.commandProjectRoot.view(),
                                                                     dependencyPath.view(), dependencyPath, handled));
            if (handled)
            {
                return Result(true);
            }
            if (not resolvedProject.commandLibraryRoot.view().isEmpty() and
                resolvedProject.commandLibraryRoot.view() != resolvedProject.commandProjectRoot.view())
            {
                SC_TRY(tryNormalizeWindowsDependencyPathFromCommandRoots(
                    "scroot"_a8, resolvedProject.commandLibraryRoot.view(), dependencyPath.view(), dependencyPath,
                    handled));
                if (handled)
                {
                    return Result(true);
                }
            }

            if (not resolvedProject.commandWorkingDirectory.view().isEmpty() and
                resolvedProject.commandWorkingDirectory.view() != resolvedProject.commandProjectRoot.view())
            {
                String projectAlias = StringEncoding::Utf8;
                SC_TRY(Path::join(projectAlias, {resolvedProject.commandWorkingDirectory.view(), "project"_a8},
                                  Path::SeparatorStringView()));
                SC_TRY(tryNormalizeWindowsDependencyPathFromAlias(projectAlias.view(),
                                                                  resolvedProject.commandProjectRoot.view(),
                                                                  dependencyPath.view(), dependencyPath, handled));
                if (handled)
                {
                    return Result(true);
                }

                if (not resolvedProject.commandLibraryRoot.view().isEmpty() and
                    resolvedProject.commandLibraryRoot.view() != resolvedProject.commandProjectRoot.view())
                {
                    String libraryAlias = StringEncoding::Utf8;
                    SC_TRY(Path::join(libraryAlias, {resolvedProject.commandWorkingDirectory.view(), "scroot"_a8},
                                      Path::SeparatorStringView()));
                    SC_TRY(tryNormalizeWindowsDependencyPathFromAlias(libraryAlias.view(),
                                                                      resolvedProject.commandLibraryRoot.view(),
                                                                      dependencyPath.view(), dependencyPath, handled));
                    if (handled)
                    {
                        return Result(true);
                    }
                }
            }
        }
#endif

        if (Path::isAbsolute(dependencyPath.view(), Path::AsNative))
        {
            return assignNormalizedDependencyPath(dependencyPath, dependencyPath.view());
        }
        String normalized = StringEncoding::Utf8;
        SC_TRY(Path::join(normalized, {resolvedProject.project->rootDirectory.view(), dependencyPath.view()},
                          Path::SeparatorStringView()));
        return assignNormalizedDependencyPath(dependencyPath, normalized.view());
    }
};
} // namespace Build
} // namespace SC
