// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../Foundation/Containers/SmallVector.h"
#include "../Foundation/Containers/VectorMap.h"
#include "../Foundation/Language/Optional.h"
#include "../Foundation/Strings/String.h"
#include "../System/System.h"

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

struct SC::PluginFile
{
    SmallString<255> absolutePath;
};

struct SC::PluginIdentity
{
    PluginIdentifier identifier;
    SmallString<30>  name;
    SmallString<10>  version;

    bool operator==(const PluginIdentity& other) const { return identifier == other.identifier; }
};

struct SC::PluginDefinition
{
    PluginIdentity   identity;
    SmallString<255> description;
    SmallString<10>  category;
    SmallString<255> directory;

    SmallVector<PluginIdentifier, 10> dependencies;
    SmallVector<PluginFile, 10>       files;

    size_t pluginFileIndex = 0;

    [[nodiscard]] static bool find(const StringView text, StringView& extracted);
    [[nodiscard]] static bool parse(StringView text, PluginDefinition& pluginDefinition);
    [[nodiscard]] static bool parseLine(StringIteratorASCII& iterator, StringView& key, StringView& value);

    [[nodiscard]] ReturnCode getDynamicLibraryAbsolutePath(String& fullDynamicPath) const;
    [[nodiscard]] ReturnCode getDynamicLibraryPDBAbsolutePath(String& fullDynamicPath) const;
};

struct SC::PluginScanner
{
    [[nodiscard]] static ReturnCode scanDirectory(const StringView directory, Vector<PluginDefinition>& definitions);
};

struct SC::PluginCompiler
{
    ReturnCode compile(const PluginDefinition& definition) const;
    ReturnCode link(const PluginDefinition& definition, StringView executablePath) const;
    enum class Type
    {
        ClangCompiler,
        GnuCompiler,
        MicrosoftCompiler
    };
    Type              type         = Type::ClangCompiler;
    StringNative<256> compilerPath = StringEncoding::Native;
    StringNative<256> linkerPath   = StringEncoding::Native;
    StringNative<256> includePath  = StringEncoding::Native;

    [[nodiscard]] ReturnCode        compileFile(StringView sourceFile, StringView objectFile) const;
    [[nodiscard]] static ReturnCode findBestCompiler(PluginCompiler& compiler);

  private:
    struct Internal;
};

struct SC::PluginDynamicLibrary
{
    PluginDefinition     definition;
    SystemDynamicLibrary dynamicLibrary;
    void*                instance       = nullptr;
    bool (*pluginInit)(void*& instance) = nullptr;
    bool (*pluginClose)(void* instance) = nullptr;

  private:
    friend struct PluginRegistry;
    [[nodiscard]] ReturnCode load(const PluginCompiler& compiler, StringView executablePath);
    [[nodiscard]] ReturnCode unload();
};

struct SC::PluginRegistry
{
    [[nodiscard]] ReturnCode init(Vector<PluginDefinition>&& definitions);

    enum class LoadMode
    {
        Default = 0,
        Reload  = 1,
    };
    [[nodiscard]] ReturnCode loadPlugin(const StringView identifier, const PluginCompiler& compiler,
                                        StringView executablePath, LoadMode loadMode = LoadMode::Default);
    [[nodiscard]] ReturnCode unloadPlugin(const StringView identifier);
    [[nodiscard]] ReturnCode removeAllBuildProducts(const StringView identifier);

    [[nodiscard]] const PluginDynamicLibrary* findPlugin(const StringView identifier);

  private:
    VectorMap<PluginIdentifier, PluginDynamicLibrary> libraries;
};
