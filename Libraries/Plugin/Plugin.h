// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../Containers/SmallVector.h"
#include "../Containers/VectorMap.h"
#include "../Strings/SmallString.h"
#include "Internal/DynamicLibrary.h"

namespace SC
{
struct PluginDefinition;
struct PluginScanner;
struct PluginFile;
struct PluginIdentity;
struct PluginDynamicLibrary;
struct PluginCompiler;
struct PluginRegistry;
struct PluginNetwork;
using PluginIdentifier = SmallString<30>;
} // namespace SC

//! @defgroup group_plugin Plugin
//! @copybrief library_plugin (see @ref library_plugin for more details)

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

    SmallVector<PluginIdentifier, 10> dependencies; ///< Dependencies necessary to load this plugin
    SmallVector<PluginFile, 10>       files;        ///< Source files that compose this plugin

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
    [[nodiscard]] Result getDynamicLibraryAbsolutePath(String& fullDynamicPath) const;

    /// @brief Gets absolute path of where compiled Program Database File will exist after plugin is compiled
    /// @param fullDynamicPath absolute path of where compiled Program Database File from plugin
    /// @return Valid result if string can be allocated successfully
    [[nodiscard]] Result getDynamicLibraryPDBAbsolutePath(String& fullDynamicPath) const;

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
    /// @return Valid result if all files of given definition can be compiled to valid object files
    Result compile(const PluginDefinition& definition) const;

    /// @brief Links a Definition into a dynamic library, with symbols from `executablePath`
    /// @param definition A valid Definition already compiled with PluginCompiler::compile
    /// @param executablePath Path to the executable loading the given plugin, exposing symbols used by Plugin
    /// @return Valid result if the Definition can be compiled to a dynamic library linking executablePath
    Result link(const PluginDefinition& definition, StringView executablePath) const;

    /// @brief Compiler type (clang/gcc/msvc)
    enum class Type
    {
        ClangCompiler,
        GnuCompiler,
        MicrosoftCompiler
    };
    Type              type         = Type::ClangCompiler;    ///< Compile Type
    StringNative<256> compilerPath = StringEncoding::Native; ///< Path to the compiler
    StringNative<256> linkerPath   = StringEncoding::Native; ///< Path to the linker
    StringNative<256> includePath  = StringEncoding::Native; ///< Path to include directory used to compile plugin

    /// @brief Compiles a single source file to an object file, using PluginCompiler::compilerPath and
    /// PluginCompiler::linkerPath
    /// @param sourceFile Source .cpp file
    /// @param objectFile Location of object file
    /// @return valid Result if file can be compiled successfully
    [[nodiscard]] Result compileFile(StringView sourceFile, StringView objectFile) const;

    /// @brief Look for best compiler on current system
    /// @param[out] compiler Best compiler found
    /// @return Valid Result if best compiler has been found
    [[nodiscard]] static Result findBestCompiler(PluginCompiler& compiler);

  private:
    struct Internal;
};

/// @brief A plugin dynamic library loaded from a SC::PluginRegistry
struct SC::PluginDynamicLibrary
{
    PluginDefinition     definition;     ///< Definition of the loaded plugin
    SystemDynamicLibrary dynamicLibrary; ///< System handle of plugin's dynamic library
  private:
    void* instance                      = nullptr;
    bool (*pluginInit)(void*& instance) = nullptr;
    bool (*pluginClose)(void* instance) = nullptr;

    friend struct PluginRegistry;
    [[nodiscard]] Result load(const PluginCompiler& compiler, StringView executablePath);
    [[nodiscard]] Result unload();
};

/// @brief Holds a registry of plugins, loading and compiling them on the fly
struct SC::PluginRegistry
{
    /// @brief Inits the Registry with found plugins
    /// @param definitions found plugin definitions
    /// @return Valid Result if the given definitions can be added to the libraries registry
    [[nodiscard]] Result init(Vector<PluginDefinition>&& definitions);

    /// @brief Instructs loadPlugin to Load or Reload the plugin
    enum class LoadMode
    {
        Load   = 0,
        Reload = 1,
    };

    /// @brief Loads a plugin with given identifier, compiling it with given PluginCompiler
    /// @param identifier The Plugin identifier that must be loaded
    /// @param compiler The PluginCompiler used to compile the plugin
    /// @param executablePath The loader executable path holding symbols used by the plugin
    /// @param loadMode If to load or force reload of the plugin
    /// @return Valid Result if the plugin has been found, compiled, loaded and inited successfully
    [[nodiscard]] Result loadPlugin(const StringView identifier, const PluginCompiler& compiler,
                                    StringView executablePath, LoadMode loadMode = LoadMode::Load);

    /// @brief Unloads an already loaded plugin by its identifier
    /// @param identifier Identifier of a plugin that must be unloaded
    /// @return Valid Result if an already loaded plugin exists with the given identifier and it can be unloaded
    [[nodiscard]] Result unloadPlugin(const StringView identifier);

    /// @brief Removes all temporary build products of the Plugin with given identifier
    /// @param identifier Identifier of the plugin
    /// @return Valid Result if all build products for the given plugin can be successfully removed
    [[nodiscard]] Result removeAllBuildProducts(const StringView identifier);

    /// @brief Find a PluginDynamicLibrary in the registry with a given identifier
    /// @param identifier Identifier of the Plugin to find
    /// @return Pointer to the found PluginDynamicLibrary if found (or `nullptr`)
    [[nodiscard]] const PluginDynamicLibrary* findPlugin(const StringView identifier);

  private:
    VectorMap<PluginIdentifier, PluginDynamicLibrary> libraries;
};

//! @}
