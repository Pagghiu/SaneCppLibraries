// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../Containers/Vector.h"
#include "../Containers/VectorMap.h"
#include "../Foundation/Function.h"
#include "../Memory/String.h"
#include "../Time/Time.h"
#include "Internal/DynamicLibrary.h"

namespace SC
{
struct PluginDefinition;
struct PluginScanner;
struct PluginFile;
struct PluginIdentity;
struct PluginDynamicLibrary;
struct PluginCompiler;
struct PluginCompilerEnvironment;
struct PluginSysroot;
struct PluginRegistry;
using PluginIdentifier = SmallString<30>;
} // namespace SC

//! @defgroup group_plugin Plugin
//! @copybrief library_plugin (see @ref library_plugin for more details)
//!
//! \snippet Tests/Libraries/Plugin/PluginTest.cpp PluginSnippet

//! @addtogroup group_plugin
//! @{

/// @brief Holds path to a given plugin source file
struct SC::PluginFile
{
    SmallString<255> absolutePath; ///< Absolute path to a plugin source file
};

/// @brief Represents the unique signature / identity of a Plugin
struct SC::PluginIdentity
{
    PluginIdentifier identifier; ///< Unique string identifying the plugin
    SmallString<30>  name;       ///< Plugin name
    SmallString<10>  version;    ///< Plugin version (x.y.z)

    /// @brief Compares two plugins on Identity::identifier
    /// @param other Other plugin to compare
    /// @return `true` if the two plugins share the same identifier
    bool operator==(const PluginIdentity& other) const { return identifier == other.identifier; }
};

/// @brief Plugin description, category, dependencies, files and directory location
struct SC::PluginDefinition
{
    PluginIdentity   identity;    ///< Uniquely identifier a plugin
    SmallString<255> description; ///< Long description of plugin
    SmallString<10>  category;    ///< Category where plugin belongs to
    SmallString<255> directory;   ///< Path to the directory holding the plugin

    SmallVector<PluginIdentifier, 8> dependencies; ///< Dependencies necessary to load this plugin
    SmallVector<SmallString<10>, 8>  build;        ///< Build options
    SmallVector<PluginFile, 10>      files;        ///< Source files that compose this plugin

    /// @brief Get main plugin file, holding plugin definition
    /// @return File holding main plugin definition
    PluginFile& getMainPluginFile() { return files[pluginFileIndex]; }

    /// @brief Get main plugin file, holding plugin definition
    /// @return PluginFile holding main plugin definition
    const PluginFile& getMainPluginFile() const { return files[pluginFileIndex]; }

    /// @brief Extracts the plugin definition (SC_BEGIN_PLUGIN / SC_END_PLUGIN) comment from a .cpp file
    /// @param[in] text Content of .cpp file where to look for plugin definition
    /// @param[out] extracted Extracted comment block out of text
    /// @return `true` if a comment block has been found successfully
    [[nodiscard]] static bool find(const StringView text, StringView& extracted);

    /// @brief Parses an extracted plugin definition text
    /// @param[in] text An extracted plugin definition text (`extracted` of PluginDefinition::find)
    /// @param[out] pluginDefinition A valid PluginDefinition parsed from the given text
    /// @return `true` if the plugin definition can be parsed successfully
    [[nodiscard]] static bool parse(StringView text, PluginDefinition& pluginDefinition);

    /// @brief Gets absolute path of where compiled dynamic library will exist after plugin is compiled
    /// @param fullDynamicPath absolute path of where compiled dynamic library from plugin
    /// @return Valid result if string can be allocated successfully
    Result getDynamicLibraryAbsolutePath(String& fullDynamicPath) const;

    /// @brief Gets absolute path of where compiled Program Database File will exist after plugin is compiled
    /// @param fullDynamicPath absolute path of where compiled Program Database File from plugin
    /// @return Valid result if string can be allocated successfully
    Result getDynamicLibraryPDBAbsolutePath(String& fullDynamicPath) const;

  private:
    [[nodiscard]] static bool parseLine(StringIteratorASCII& iterator, StringView& key, StringView& value);

    size_t pluginFileIndex = 0;
    friend struct PluginScanner;
};

/// @brief Scans a directory for PluginDefinition
struct SC::PluginScanner
{
    /// @brief Scans a directory for PluginDefinition
    /// @param directory Root directory holding plugins (will recurse in subdirectories)
    /// @param definitions Parsed definitions
    /// @return Valid result if the given directory is accessible and valid PluginDefinition can be parsed
    [[nodiscard]] static Result scanDirectory(const StringView directory, Vector<PluginDefinition>& definitions);
};

/// @brief Compiles a plugin to a dynamic library
struct SC::PluginCompiler
{
    /// @brief Compiles a Definition to an object file
    /// @param definition A valid Definition parsed by Definition::parse
    /// @param sysroot A sysroot used (if requested) holding include / library paths to libc/libc++
    /// @param environment An environment used to populate CFLAGS and LDFLAGS from environment variables
    /// @param compilerLog If provided, will receive the log output produced by the compiler
    /// @return Valid result if all files of given definition can be compiled to valid object files
    Result compile(const PluginDefinition& definition, const PluginSysroot& sysroot,
                   const PluginCompilerEnvironment& environment, String& compilerLog) const;

    /// @brief Links a Definition into a dynamic library, with symbols from `executablePath`
    /// @param definition A valid Definition already compiled with PluginCompiler::compile
    /// @param sysroot A sysroot used (if requested) holding include / library paths to libc/libc++
    /// @param environment An environment used to populate CFLAGS and LDFLAGS from environment variables
    /// @param executablePath Path to the executable loading the given plugin, exposing symbols used by Plugin
    /// @param linkerLog If provided, will receive the log output produced by the linker
    /// @return Valid result if the Definition can be compiled to a dynamic library linking executablePath
    Result link(const PluginDefinition& definition, const PluginSysroot& sysroot,
                const PluginCompilerEnvironment& environment, StringView executablePath, String& linkerLog) const;

    /// @brief Compiler type (clang/gcc/msvc)
    enum class Type
    {
        ClangCompiler,
        GnuCompiler,
        MicrosoftCompiler
    };
    Type                   type         = Type::ClangCompiler;    ///< Compile Type
    SmallStringNative<256> compilerPath = StringEncoding::Native; ///< Path to the compiler
    SmallStringNative<256> linkerPath   = StringEncoding::Native; ///< Path to the linker

    SmallVector<SmallStringNative<256>, 8> includePaths; ///< Path to include directories used to compile plugin

    SmallVector<SmallStringNative<256>, 8> compilerIncludePaths; ///< Path to compiler include directories
    SmallVector<SmallStringNative<256>, 8> compilerLibraryPaths; ///< Path to compiler library directories

    /// @brief Look for best compiler on current system
    /// @param[out] compiler Best compiler found
    /// @return Valid Result if best compiler has been found
    [[nodiscard]] static Result findBestCompiler(PluginCompiler& compiler);

  private:
    mutable native_char_t buffer[4096];

    Result compileFile(const PluginDefinition& definition, const PluginSysroot& sysroot,
                       const PluginCompilerEnvironment& compilerEnvironment, StringView sourceFile,
                       StringView objectFile, String& compilerLog) const;
    struct Internal;
};

/// @brief Holds include and library paths for a system toolchain, used to let plugins link to libc and libc++
struct SC::PluginSysroot
{
    SmallVector<SmallStringNative<256>, 8> includePaths; ///< Path to system include directories
    SmallVector<SmallStringNative<256>, 8> libraryPaths; ///< Path to system library directories

    SmallStringNative<256> isysroot; ///< Path to sysroot include (optional)

    /// @brief Finds a reasonable sysroot for the given compiler
    /// @param compiler The PluginCompiler::Type to constrain the compatible PluginSysroot to look for
    /// @param[out] sysroot The PluginSysroot with filled in include and library path
    /// @return Valid Result if sysroot has been found
    [[nodiscard]] static Result findBestSysroot(PluginCompiler::Type compiler, PluginSysroot& sysroot);
};

/// @brief Reads and holds CFLAGS and LDFLAGS environment variables, mainly to pass down sysroot location
struct SC::PluginCompilerEnvironment
{
    StringView cFlags;
    StringView ldFlags;

  private:
    struct Internal;
    friend struct PluginCompiler;
};
/// @brief A plugin dynamic library loaded from a SC::PluginRegistry
struct SC::PluginDynamicLibrary
{
    PluginDefinition     definition;     ///< Definition of the loaded plugin
    SystemDynamicLibrary dynamicLibrary; ///< System handle of plugin's dynamic library
    Time::Absolute       lastLoadTime;   ///< Last time when this plugin was last loaded
    uint32_t             numReloads;     ///< Number of times that the plugin has been hot-reloaded
    String               lastErrorLog;   ///< Last error log of compiler / linker (if any)

    /// @brief Try to obtain a given interface as exported by a plugin through SC_PLUGIN_EXPORT_INTERFACES macro
    /// @param[out] outInterface Pointer to the interface that will be returned by the plugin, if it exists
    /// @return true if the plugin is loaded and the requested interface is implemented by the plugin itself
    template <typename T>
    [[nodiscard]] bool queryInterface(T*& outInterface) const
    {
        if (pluginQueryInterface and instance != nullptr)
        {
            return pluginQueryInterface(instance, T::InterfaceHash, reinterpret_cast<void**>(&outInterface));
        }
        return false;
    }

    PluginDynamicLibrary() : lastLoadTime(Time::Realtime::now()) { numReloads = 0; }

  private:
    void* instance                      = nullptr;
    bool (*pluginInit)(void*& instance) = nullptr;
    bool (*pluginClose)(void* instance) = nullptr;

    bool (*pluginQueryInterface)(void* instance, uint32_t hash, void** instanceInterface) = nullptr;

    friend struct PluginRegistry;
    Result load(const PluginCompiler& compiler, const PluginSysroot& sysroot, StringView executablePath);
    Result unload();
};

/// @brief Holds a registry of plugins, loading and compiling them on the fly
struct SC::PluginRegistry
{
    /// @brief Unregisters all plugins
    Result close();

    /// @brief Appends the definitions to registry
    /// @param definitions found plugin definitions
    /// @return Valid Result if definitions have been replaced successfully
    Result replaceDefinitions(Vector<PluginDefinition>&& definitions);

    /// @brief Instructs loadPlugin to Load or Reload the plugin
    enum class LoadMode
    {
        Load   = 0,
        Reload = 1,
    };

    /// @brief Loads a plugin with given identifier, compiling it with given PluginCompiler
    /// @param identifier The Plugin identifier that must be loaded
    /// @param compiler The compiler used
    /// @param sysroot The sysroot (library / include files) used
    /// @param executablePath The loader executable path holding symbols used by the plugin
    /// @param loadMode If to load or force reload of the plugin
    /// @return Valid Result if the plugin has been found, compiled, loaded and inited successfully
    Result loadPlugin(const StringView identifier, const PluginCompiler& compiler, const PluginSysroot& sysroot,
                      StringView executablePath, LoadMode loadMode = LoadMode::Load);

    /// @brief Unloads an already loaded plugin by its identifier
    /// @param identifier Identifier of a plugin that must be unloaded
    /// @return Valid Result if an already loaded plugin exists with the given identifier and it can be unloaded
    Result unloadPlugin(const StringView identifier);

    /// @brief Removes all temporary build products of the Plugin with given identifier
    /// @param identifier Identifier of the plugin
    /// @return Valid Result if all build products for the given plugin can be successfully removed
    Result removeAllBuildProducts(const StringView identifier);

    /// @brief Find a PluginDynamicLibrary in the registry with a given identifier
    /// @param identifier Identifier of the Plugin to find
    /// @return Pointer to the found PluginDynamicLibrary if found (or `nullptr`)
    [[nodiscard]] const PluginDynamicLibrary* findPlugin(const StringView identifier);

    /// @brief Returns the total number of registry entries (counting both loaded and unloaded plugins)
    [[nodiscard]] size_t getNumberOfEntries() const { return libraries.size(); }

    /// @brief Returns the PluginIdentifier corresponding to the index entry of the registry
    [[nodiscard]] const PluginIdentifier& getIdentifierAt(size_t index) const { return libraries.items[index].key; }

    /// @brief Returns the PluginIdentifier corresponding to the index entry of the registry
    [[nodiscard]] const PluginDynamicLibrary& getPluginDynamicLibraryAt(size_t index)
    {
        return libraries.items[index].value;
    }

    /// @brief Enumerates all plugins that must be reloaded when relativePath is modified
    /// @param relativePath A relative path of the file that has been modified
    /// @param tolerance How many milliseconds must be passed to consider a file as modified
    /// @param onPlugin Callback that will be called with Plugins affected by the modification
    void getPluginsToReloadBecauseOf(StringView relativePath, Time::Milliseconds tolerance,
                                     Function<void(const PluginIdentifier&)> onPlugin);

  private:
    VectorMap<PluginIdentifier, PluginDynamicLibrary> libraries;
};

//! @}
