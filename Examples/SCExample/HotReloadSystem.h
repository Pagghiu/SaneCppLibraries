// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "Libraries/FileSystem/FileSystemDirectories.h"
#include "Libraries/FileSystem/Path.h"
#include "Libraries/FileSystemWatcher/FileSystemWatcher.h"
#include "Libraries/Plugin/Plugin.h"
#include "Plugins/Interfaces/IExampleDrawing.h"

#include "imgui.h"

namespace SC
{

struct HotReloadState
{
    String libraryRootDirectory;
    String imguiPath;
    String pluginsPath;
    String executablePath;
};

/// @brief Implements a simple hot reload system using the Plugin and FileSystemWatcher library
struct HotReloadSystem
{
    HotReloadState state;
    PluginRegistry registry;

    Result create(AsyncEventLoop& eventLoop)
    {
        // Setup Paths
        FileSystemDirectories directories;
        SC_TRY(directories.init());
        state.executablePath = directories.getExecutablePath();
        SmallVector<StringView, 50> components;
        SC_TRY(Path::normalizeUNCAndTrimQuotes(SC_COMPILER_LIBRARY_PATH, components, state.libraryRootDirectory,
                                               Path::AsNative));
        constexpr const StringView imguiPath = SC_COMPILER_MACRO_TO_LITERAL(SC_COMPILER_MACRO_ESCAPE(SC_IMGUI_PATH));
        SC_TRY(Path::normalizeUNCAndTrimQuotes(imguiPath, components, state.imguiPath, Path::AsNative));
        SC_TRY(Path::join(state.pluginsPath, {state.libraryRootDirectory.view(), "Examples", "SCExample", "Plugins"}));

        // Setup Compiler
        SC_TRY(PluginCompiler::findBestCompiler(compiler));
        SC_TRY(PluginSysroot::findBestSysroot(compiler.type, sysroot));
        SC_TRY(compiler.includePaths.push_back(state.libraryRootDirectory.view()));
        SC_TRY(compiler.includePaths.push_back(state.imguiPath.view()));

        // Setup File System Watcher
        SC_TRY(fileSystemWatcher.init(fileSystemWatcherRunner, eventLoop));
        folderWatcher.notifyCallback.bind<HotReloadSystem, &HotReloadSystem::onFileChange>(*this);
        SC_TRY(fileSystemWatcher.watch(folderWatcher, state.pluginsPath.view()));
        return Result(true);
    }

    Result close() { return fileSystemWatcher.close(); }

    Result syncRegistry(StringView pluginsPath)
    {
        Vector<PluginDefinition> definitions;
        SC_TRY(PluginScanner::scanDirectory(pluginsPath, definitions))
        SC_TRY(registry.replaceDefinitions(move(definitions)));
        return Result(true);
    }

    void load(StringView identifier)
    {
        // TODO: Implement some form of error reporting
        (void)registry.loadPlugin(identifier, compiler, sysroot, state.executablePath.view(),
                                  PluginRegistry::LoadMode::Reload);
    }

    void unload(StringView identifier)
    {
        // TODO: Implement some form of error reporting
        (void)registry.unloadPlugin(identifier);
    }

  private:
    PluginCompiler compiler;
    PluginSysroot  sysroot;

    FileSystemWatcher fileSystemWatcher;

    FileSystemWatcher::FolderWatcher   folderWatcher;
    FileSystemWatcher::EventLoopRunner fileSystemWatcherRunner;

    void onFileChange(const FileSystemWatcher::Notification& notification)
    {
        if (notification.relativePath.endsWith(".cpp"))
        {
            const size_t numberOfPlugins = registry.getNumberOfEntries();
            for (size_t idx = 0; idx < numberOfPlugins; ++idx)
            {
                for (PluginFile& file : registry.getPluginDynamicLibraryAt(idx).definition.files)
                {
                    if (file.absolutePath.view().endsWith(notification.relativePath))
                    {
                        Result res = registry.loadPlugin(registry.getIdentifierAt(idx).view(), compiler, sysroot,
                                                         state.executablePath.view(), PluginRegistry::LoadMode::Reload);
                        SC_COMPILER_UNUSED(res); // TODO: Implement some form of error reporting
                        return;
                    }
                }
            }
        }
    }
};

struct HotReloadView
{
    HotReloadSystem& system;
    HotReloadState&  state;

    void draw() { SC_TRUST_RESULT(drawInternal()); }

    Result drawInternal()
    {
        if (ImGui::Button("Sync Registry"))
        {
            SC_TRY(system.syncRegistry(state.pluginsPath.view()));
        }
        const size_t numberOfEntries = system.registry.getNumberOfEntries();
        for (size_t idx = 0; idx < numberOfEntries; ++idx)
        {
            ImGui::PushID(static_cast<int>(idx));
            const PluginIdentifier& identifier = system.registry.getIdentifierAt(idx);
            SC_ASSERT_RELEASE(identifier.view().getEncoding() != StringEncoding::Utf16);
            ImGui::Text("[%d] %s", static_cast<int>(idx), identifier.view().bytesIncludingTerminator());
            ImGui::SameLine();
            if (ImGui::Button("Load"))
            {
                system.load(identifier.view());
            }
            ImGui::SameLine();
            if (ImGui::Button("Unload"))
            {
                system.unload(identifier.view());
            }
            ImGui::PopID();
        }

        for (size_t idx = 0; idx < numberOfEntries; ++idx)
        {
            const float entrySize = ImGui::GetIO().DisplaySize.y / numberOfEntries;
            ImGui::SetNextWindowPos(ImVec2(350, entrySize * idx));
            ImGui::SetNextWindowSize(ImVec2(ImGui::GetIO().DisplaySize.x - 350, entrySize));
            if (idx % 2 == 0)
            {
                ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(30, 30, 30, 255));
            }
            else
            {
                ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(60, 60, 60, 255));
            }
            SmallString<16> label;
            SC_TRY(StringBuilder(label).format("Content{}", idx));
            if (ImGui::Begin(label.view().bytesIncludingTerminator(), nullptr,
                             ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar))
            {
                const PluginDynamicLibrary& library = system.registry.getPluginDynamicLibraryAt(idx);

                IExampleDrawing* drawingInterface = nullptr;
                if (library.queryInterface(drawingInterface))
                {
                    drawingInterface->onDraw();
                }
            }
            ImGui::End();
            ImGui::PopStyleColor();
        }
        return Result(true);
    }
};
} // namespace SC
