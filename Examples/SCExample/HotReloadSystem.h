// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "Libraries/FileSystem/FileSystem.h"
#include "Libraries/FileSystemWatcher/FileSystemWatcher.h"
#include "Libraries/Plugin/Plugin.h"
#include "Libraries/Strings/Path.h"

#include "Examples/ISCExample.h"

namespace SC
{
static constexpr int ToolbarHeight = 35;

struct HotReloadState
{
    String libraryRootDirectory;
    String imguiPath;
    String pluginsPath;

    StringPath executablePath;

    char isysroot[255];
};

// A simple hot reload system using the Plugin and FileSystemWatcher library
struct HotReloadSystem
{
    HotReloadState state;
    PluginRegistry registry;

    Result create(AsyncEventLoop& loop)
    {
        eventLoop = &loop;
        // Setup Paths
        FileSystem::Operations::getExecutablePath(state.executablePath);
        StringView components[64];
        SC_TRY(Path::normalizeUNCAndTrimQuotes(state.libraryRootDirectory, SC_COMPILER_LIBRARY_PATH, Path::AsNative,
                                               components));
        constexpr const StringView imguiPath = SC_COMPILER_MACRO_TO_LITERAL(SC_COMPILER_MACRO_ESCAPE(SC_IMGUI_PATH));
        SC_TRY(Path::normalizeUNCAndTrimQuotes(state.imguiPath, imguiPath, Path::AsNative, components));
        SC_TRY(Path::join(state.pluginsPath, {state.libraryRootDirectory.view(), "Examples", "SCExample", "Examples"}));
        StringView iosSysroot = "/var/mobile/theos/sdks/iPhoneOS14.4.sdk";
        if (FileSystem().existsAndIsDirectory(iosSysroot))
        {
            memcpy(state.isysroot, iosSysroot.bytesIncludingTerminator(), iosSysroot.sizeInBytesIncludingTerminator());
            setSysroot(iosSysroot);
        }

        // Setup Compiler
        SC_TRY(PluginCompiler::findBestCompiler(compiler));
        SC_TRY(PluginSysroot::findBestSysroot(compiler.type, sysroot));
        SC_TRY(compiler.includePaths.push_back(state.libraryRootDirectory.view()));
        SC_TRY(compiler.includePaths.push_back(state.imguiPath.view()));

        // Setup File System Watcher
        SC_TRY(fileSystemWatcher.init(fileSystemWatcherRunner, *eventLoop));
        folderWatcher.notifyCallback.bind<HotReloadSystem, &HotReloadSystem::onFileChange>(*this);
        SC_TRY(fileSystemWatcher.watch(folderWatcher, state.pluginsPath.view()));
        return Result(true);
    }

    Result close()
    {
        SC_TRY(fileSystemWatcher.close());
        eventLoop = nullptr;
        return registry.close();
    }

    Result syncRegistry()
    {
        Vector<PluginDefinition> definitions;
        SC_TRY(PluginScanner::scanDirectory(state.pluginsPath.view(), definitions))
        SC_TRY(registry.replaceDefinitions(move(definitions)));
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

    void setSysroot(StringView isysroot) { sysroot.isysroot = isysroot; }

  private:
    AsyncEventLoop* eventLoop = nullptr;

    PluginCompiler compiler;
    PluginSysroot  sysroot;

    FileSystemWatcher fileSystemWatcher;

    FileSystemWatcher::FolderWatcher   folderWatcher;
    FileSystemWatcher::EventLoopRunner fileSystemWatcherRunner;

    void onFileChange(const FileSystemWatcher::Notification& notification)
    {
        if (StringView(notification.relativePath).endsWith(".cpp"))
        {
            auto reload = [this](const PluginIdentifier& plugin) { (void)load(plugin.view()); };
            registry.getPluginsToReloadBecauseOf(notification.relativePath, Time::Milliseconds(500), reload);
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
                ImGui::Text("%s", library.definition.identity.name.view().bytesIncludingTerminator());
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
                        ImGui::SetTooltip("%s", library.lastErrorLog.view().bytesIncludingTerminator());
                    }
                }

                ImGui::TableNextColumn();
                ImGui::Text("%d", library.numReloads);

                ImGui::TableNextColumn();
                SmallString<128>            formattedTime;
                Time::Absolute::ParseResult local;
                SC_TRY(library.lastLoadTime.parseLocal(local));
                SC_TRY(
                    StringBuilder(formattedTime).format("{:02}:{:02}:{:02}", local.hour, local.minutes, local.seconds));
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

            if (ImGui::Button(library.definition.identity.name.view().bytesIncludingTerminator()))
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
            ImGui::Text("Example %s failed to compile:",
                        library.definition.identity.name.view().bytesIncludingTerminator());
            ImGui::PushStyleColor(ImGuiCol_Text, 0xff0000ff);
            ImGui::Text("%s", library.lastErrorLog.view().bytesIncludingTerminator());
            ImGui::PopStyleColor();
        }
    }
};

} // namespace SC
