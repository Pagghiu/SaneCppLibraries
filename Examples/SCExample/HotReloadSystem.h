// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "Libraries/Async/Async.h"
#include "Libraries/FileSystem/FileSystem.h"
#include "Libraries/FileSystemWatcher/FileSystemWatcher.h"
#include "Libraries/Plugin/Plugin.h"
#include "Libraries/Strings/Path.h"
#include "Libraries/Strings/StringBuilder.h"
#include "Libraries/Strings/StringConverter.h"

#include "Examples/ISCExample.h"
#include "Examples/ImguiHelpers.h"

#ifndef SC_HOT_RELOAD_INCLUDE_PATHS
#define SC_HOT_RELOAD_INCLUDE_PATHS ""
#endif

namespace SC
{
static constexpr int ToolbarHeight = 35;

static bool rebuildHotReloadIncludePaths(StringView executableDirectory, StringView includePathsText,
                                         PluginCompiler& compiler)
{
    compiler.includePaths.clear();

    StringViewTokenizer lines(includePathsText);
    while (lines.tokenizeNext({'\n'}, StringViewTokenizer::IncludeEmpty))
    {
        const StringView line = lines.component.trimWhiteSpaces().trimEndAnyOf({'\r'});
        if (line.isEmpty())
            continue;

        StringPath includePath;
        if (Path::isAbsolute(line, Path::AsNative))
        {
            SC_TRY(includePath.assign(line));
        }
        else
        {
            SC_TRY(Path::join(includePath, {executableDirectory, line}));
        }
        SC_TRY_MSG(compiler.includePaths.push_back(includePath), "HotReloadSystem exceeded include path capacity");
    }
    return true;
}

struct HotReloadState
{
    String examplesPath;
    String includePathsText = StringEncoding::Utf8;

    StringPath executablePath;
    StringPath executableDirectory;
    Buffer     includePathsBuffer;

    char isysroot[255];
};

// A simple hot reload system using the Plugin and FileSystemWatcher library
struct HotReloadSystem
{
    struct Options
    {
        StringView examplesPath;
        StringView defaultIncludePathsText;
    };

    HotReloadState state;

    PluginDynamicLibrary storage[16]; // Max 16 examples
    PluginRegistry       registry;

    Result create(AsyncEventLoop& loop, const Options& options)
    {
        registry.init(storage);
        eventLoop = &loop;
        FileSystem fs;
        SC_TRY(fs.init("."));
        FileSystem::Operations::getExecutablePath(state.executablePath);
        SC_TRY(state.executableDirectory.assign(Path::dirname(state.executablePath.view(), Path::AsNative)));
        SC_TRY_MSG(not options.examplesPath.isEmpty(), "HotReloadSystem missing examples path");
        SC_TRY(state.examplesPath.assign(options.examplesPath));
        SC_TRY_MSG(fs.existsAndIsDirectory(state.examplesPath.view()), "HotReloadSystem examples path does not exist");
        StringView iosSysroot = "/var/mobile/theos/sdks/iPhoneOS14.4.sdk";
        if (FileSystem().existsAndIsDirectory(iosSysroot))
        {
            memcpy(state.isysroot, iosSysroot.bytesIncludingTerminator(), iosSysroot.sizeInBytesIncludingTerminator());
            setSysroot(iosSysroot);
        }

        // Setup Compiler
        SC_TRY(PluginCompiler::findBestCompiler(compiler));
        SC_TRY(PluginSysroot::findBestSysroot(compiler.type, sysroot));
        SC_TRY(state.includePathsText.assign(options.defaultIncludePathsText));
        SC_TRY(rebuildCompilerIncludePaths());

        // Setup File System Watcher
        fileSystemWatcherRunner.init(*eventLoop);
        SC_TRY(fileSystemWatcher.init(fileSystemWatcherRunner));
        folderWatcher.notifyCallback.bind<HotReloadSystem, &HotReloadSystem::onFileChange>(*this);
        SC_TRY(fileSystemWatcher.watch(folderWatcher, state.examplesPath.view()));
        return Result(true);
    }

    Result close()
    {
        Result result(true);
        auto   preserveFirstError = [&result](Result closeResult)
        {
            if (not closeResult and result)
            {
                result = closeResult;
            }
        };

        preserveFirstError(fileSystemWatcher.close());
        if (eventLoop)
        {
            for (size_t idx = 0; idx < registry.getNumberOfEntries(); ++idx)
            {
                const PluginDynamicLibrary& library = registry.getPluginDynamicLibraryAt(idx);
                ISCExample*                 example = nullptr;
                if (library.queryInterface(example) and example->closeAsync.isValid())
                {
                    preserveFirstError(example->closeAsync(*eventLoop));
                }
            }
        }
        eventLoop = nullptr;
        preserveFirstError(registry.close());
        return result;
    }

    Result syncRegistry()
    {
        PluginDefinition       definitions[16];
        Span<PluginDefinition> definitionSpan;
        Buffer                 fileBuffer;
        SC_TRY(PluginScanner::scanDirectory(state.examplesPath.view(), definitions, fileBuffer, definitionSpan))
        SC_TRY(registry.replaceDefinitions(move(definitionSpan)));
        return Result(true);
    }

    Result load(StringView identifier)
    {
        const PluginDynamicLibrary* exampleLibrary = registry.findPlugin(identifier);

        Buffer serializedModelState, serializedViewState;
        if (exampleLibrary)
        {
            ISCExample* example = nullptr;
            if (exampleLibrary->queryInterface(example) and example->serialize.isValid())
            {
                SC_TRY(example->serialize(serializedModelState, serializedViewState));
            }
            if (example and example->closeAsync.isValid())
            {
                SC_TRY(example->closeAsync(*eventLoop));
            }
        }

        SC_TRY(registry.loadPlugin(identifier, compiler, sysroot, state.executablePath.view(),
                                   PluginRegistry::LoadMode::Reload));

        ISCExample* example = nullptr;
        SC_TRY(exampleLibrary->queryInterface(example));
        if (example)
        {
            if (example->deserialize.isValid() and not serializedModelState.isEmpty())
            {
                SC_TRY(example->deserialize(serializedModelState.toSpanConst(), serializedViewState.toSpanConst()));
            }
            if (example->initAsync.isValid())
            {
                SC_TRY(example->initAsync(*eventLoop));
            }
        }
        return Result(true);
    }

    void unload(StringView identifier) { (void)registry.unloadPlugin(identifier); }

    void setSysroot(StringView isysroot) { (void)sysroot.isysroot.assign(isysroot); }

    Result rebuildCompilerIncludePaths()
    {
        SC_TRY(rebuildHotReloadIncludePaths(state.executableDirectory.view(), state.includePathsText.view(), compiler));
        return Result(true);
    }

    Result update()
    {
        if (not pendingReload.pending)
        {
            return Result(true);
        }
        const Time::Relative elapsed =
            Time::HighResolutionCounter().snap().subtractApproximate(pendingReload.lastEvent);
        if (elapsed < Time::Milliseconds(250))
        {
            return Result(true);
        }

        auto reload = [this](const PluginIdentifier& plugin) { (void)load(plugin.view()); };
        if (pendingReload.reloadAll)
        {
            registry.getPluginsToReloadBecauseOf(StringSpan(), Time::Milliseconds(-1), reload);
        }
        else
        {
            registry.getPluginsToReloadBecauseOf(pendingReload.relativePath.view(), Time::Milliseconds(-1), reload);
        }
        pendingReload = {};
        return Result(true);
    }

  private:
    AsyncEventLoop* eventLoop = nullptr;

    PluginCompiler compiler;
    PluginSysroot  sysroot;

    using FileSystemWatcherAsync = FileSystemWatcherAsyncT<AsyncEventLoop>;

    FileSystemWatcher      fileSystemWatcher;
    FileSystemWatcherAsync fileSystemWatcherRunner;

    FileSystemWatcher::FolderWatcher folderWatcher;

    struct PendingReload
    {
        bool pending   = false;
        bool reloadAll = false;

        StringPath relativePath;

        Time::HighResolutionCounter lastEvent;
    } pendingReload;

    void onFileChange(const FileSystemWatcher::Notification& notification)
    {
        pendingReload.pending = true;
        pendingReload.lastEvent.snap();
        if (notification.relativePath.isEmpty() or
            notification.operation == FileSystemWatcher::Operation::AddRemoveRename or pendingReload.reloadAll)
        {
            pendingReload.reloadAll = true;
            return;
        }
        if (pendingReload.relativePath.isEmpty())
        {
            if (not pendingReload.relativePath.assign(notification.relativePath))
            {
                pendingReload.reloadAll = true;
            }
        }
        else if (pendingReload.relativePath.view() != notification.relativePath)
        {
            pendingReload.reloadAll = true;
        }
    }
};

struct HotReloadViewState
{
    size_t page = 0;
};

struct HotReloadView
{
    HotReloadSystem&    system;
    HotReloadState&     state;
    HotReloadViewState& viewState;

    void drawSettings() { SC_TRUST_RESULT(drawInternal()); }

    Result drawInternal()
    {
        ImGui::Text("Sysroot Location:");
        ImGui::PushItemWidth(ImGui::GetWindowWidth() - ImGui::GetStyle().WindowPadding.x * 2);
        if (ImGui::InputText("##-isysroot", state.isysroot, sizeof(state.isysroot)))
        {
            StringView text = StringView::fromNullTerminated(state.isysroot, StringEncoding::Utf8);
            if (FileSystem().existsAndIsDirectory(text) or text.isEmpty())
            {
                system.setSysroot(text);
            }
        }
        ImGui::PopItemWidth();

        ImGui::Text("Include Paths:");
        bool         includePathsModified = false;
        const ImVec2 includePathsSize(ImGui::GetWindowWidth() - ImGui::GetStyle().WindowPadding.x * 2,
                                      ImGui::GetTextLineHeightWithSpacing() * 6.0f);
        SC_TRY(InputTextMultiline("##-include-paths", state.includePathsBuffer, state.includePathsText,
                                  includePathsSize, includePathsModified));
        if (includePathsModified)
        {
            SC_TRY(system.rebuildCompilerIncludePaths());
        }

        if (ImGui::Button("Sync Registry"))
        {
            SC_TRY(system.syncRegistry());
        }
        const size_t numberOfEntries = system.registry.getNumberOfEntries();
        if (ImGui::BeginTable("Table", 4))
        {
            ImGui::TableSetupColumn("Example");
            ImGui::TableSetupColumn("Reloads", ImGuiTableColumnFlags_WidthFixed);
            ImGui::TableSetupColumn("Time", ImGuiTableColumnFlags_WidthFixed);
            ImGui::TableSetupColumn("Actions");
            ImGui::TableHeadersRow();

            for (size_t idx = 0; idx < numberOfEntries; ++idx)
            {
                ImGui::PushID(static_cast<int>(idx));
                const PluginDynamicLibrary& library = system.registry.getPluginDynamicLibraryAt(idx);

                ImGui::TableNextColumn();
                if (not library.lastErrorLog.isEmpty())
                {
                    ImGui::PushStyleColor(ImGuiCol_Text, 0xff0000ff);
                }
                SmallString<256> pluginUTF8 = StringEncoding::Utf8;
                (void)StringConverter::appendEncodingTo(StringEncoding::Utf8, library.definition.identity.name.view(),
                                                        pluginUTF8, StringConverter::NullTerminate);
                ImGui::Text("%s", pluginUTF8.view().bytesIncludingTerminator());
                if (library.lastErrorLog.isEmpty())
                {
                    if (ImGui::IsItemHovered())
                    {
                        ImGui::SetTooltip("%s", library.definition.description.view().bytesIncludingTerminator());
                    }
                }
                else
                {
                    ImGui::PopStyleColor();
                    if (ImGui::IsItemHovered())
                    {
                        ImGui::SetTooltip("%s", library.lastErrorLog.bytesIncludingTerminator());
                    }
                }

                ImGui::TableNextColumn();
                ImGui::Text("%d", library.numReloads);

                ImGui::TableNextColumn();
                SmallString<128>            formattedTime;
                Time::Absolute::ParseResult local;
                SC_TRY(Time::Absolute(library.lastLoadTime).parseLocal(local));
                SC_TRY(StringBuilder::format(formattedTime, "{:02}:{:02}:{:02}", local.hour, local.minutes,
                                             local.seconds));
                ImGui::Text("%s", formattedTime.view().bytesIncludingTerminator());

                ImGui::TableNextColumn();
                if (ImGui::Button("Load"))
                {
                    (void)system.load(library.definition.identity.identifier.view());
                }
                if (library.dynamicLibrary.isValid())
                {
                    ImGui::SameLine();
                    if (ImGui::Button("Unload"))
                    {
                        system.unload(library.definition.identity.identifier.view());
                    }
                }
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
        return Result(true);
    }

    [[nodiscard]] bool drawToolbar()
    {
        bool res = false;
        for (size_t idx = 0; idx < system.registry.getNumberOfEntries(); ++idx)
        {
            const PluginDynamicLibrary& library = system.registry.getPluginDynamicLibraryAt(idx);
            ImGui::SameLine();

            SmallString<256> pluginUTF8 = StringEncoding::Utf8;
            (void)StringConverter::appendEncodingTo(StringEncoding::Utf8, library.definition.identity.name.view(),
                                                    pluginUTF8, StringConverter::NullTerminate);
            if (ImGui::Button(pluginUTF8.view().bytesIncludingTerminator()))
            {
                res            = true;
                viewState.page = idx;
                if (not library.dynamicLibrary.isValid())
                {
                    (void)system.load(library.definition.identity.identifier.view());
                }
            }
        }
        return res;
    }

    void drawBody()
    {
        const PluginDynamicLibrary& library = system.registry.getPluginDynamicLibraryAt(viewState.page);

        ISCExample* example = nullptr;
        if (library.queryInterface(example))
        {
            example->onDraw();
        }
        else if (not library.lastErrorLog.isEmpty())
        {
            SmallString<256> pluginUTF8 = StringEncoding::Utf8;
            (void)StringConverter::appendEncodingTo(StringEncoding::Utf8, library.definition.identity.name.view(),
                                                    pluginUTF8, StringConverter::NullTerminate);
            ImGui::Text("Example %s failed to compile:", pluginUTF8.view().bytesIncludingTerminator());
            ImGui::PushStyleColor(ImGuiCol_Text, 0xff0000ff);
            ImGui::Text("%s", library.lastErrorLog.bytesIncludingTerminator());
            ImGui::PopStyleColor();
        }
    }
};

} // namespace SC
