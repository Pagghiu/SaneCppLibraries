// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../Foundation/Containers/VectorSet.h"
#include "../Foundation/Language/TaggedUnion.h"
#include "../Foundation/Strings/String.h"
#include "TaggedMap.h"

namespace SC
{
namespace Build
{

struct Platform
{
    enum Type
    {
        Unknown = 0,
        Windows,
        MacOS,
        iOS,
        Linux,
        Wasm
    };

    static constexpr StringView toString(Type type)
    {
        switch (type)
        {
        case Unknown: return "Unknown";
        case Windows: return "Windows";
        case MacOS: return "MacOS";
        case iOS: return "iOS";
        case Linux: return "Linux";
        case Wasm: return "Wasm";
        }
        Assert::unreachable();
    }
};

struct Architecture
{
    enum Type
    {
        Any = 0,
        Intel32,
        Intel64,
        Arm64,
        Wasm
    };

    static constexpr StringView toString(Type type)
    {
        switch (type)
        {
        case Any: return "Any";
        case Intel32: return "Intel32";
        case Intel64: return "Intel64";
        case Arm64: return "Arm64";
        case Wasm: return "Wasm";
        }
        Assert::unreachable();
    }
};

struct Generator
{
    enum Type
    {
        XCode14,
        VisualStudio2022,
    };

    static constexpr StringView toString(Type type)
    {
        switch (type)
        {
        case XCode14: return "XCode14";
        case VisualStudio2022: return "VisualStudio2022";
        }
        Assert::unreachable();
    }
};

struct Optimization
{
    enum Type
    {
        Debug,
        Release,
    };

    static constexpr StringView toString(Type type)
    {
        switch (type)
        {
        case Debug: return "Debug"_a8;
        case Release: return "Release"_a8;
        }
        Assert::unreachable();
    }
};

struct Compile
{
    enum Type
    {
        includePaths = 0,
        preprocessorDefines,
        optimizationLevel,
        enableASAN,
        enableRTTI,
        enableExceptions,
        enableStdCpp,
    };

    struct NameDescription
    {
        StringView name;
        StringView description;
    };

    static constexpr NameDescription typeToString(Type type)
    {
        switch (type)
        {
        case includePaths: return {"includePaths", "Description includePaths"};
        case preprocessorDefines: return {"preprocessorDefines", "Description preprocessorDefines"};
        case optimizationLevel: return {"optimizationLevel", "Description optimizationLevel"};
        case enableASAN: return {"enableASAN", "Description enableASAN"};
        case enableRTTI: return {"enableRTTI", "Description enableRTTI"};
        case enableExceptions: return {"enableExceptions", "Description enableExceptions"};
        case enableStdCpp: return {"enableStdCpp", "Description enableStdCpp"};
        }
        Assert::unreachable();
    }

    using FieldsTypes = TypeList<TaggedField<Type, includePaths, Vector<String>>,          //
                                 TaggedField<Type, preprocessorDefines, Vector<String>>,   //
                                 TaggedField<Type, optimizationLevel, Optimization::Type>, //
                                 TaggedField<Type, enableASAN, bool>,                      //
                                 TaggedField<Type, enableRTTI, bool>,                      //
                                 TaggedField<Type, enableExceptions, bool>,                //
                                 TaggedField<Type, enableStdCpp, bool>>;

    using Union = TaggedUnion<Compile>;
};

struct CompileFlags : public TaggedMap<Compile::Type, Compile::Union>
{
    [[nodiscard]] bool addIncludes(Span<const StringView> includes)
    {
        return getOrCreate<Compile::includePaths>()->push_back(includes);
    }

    [[nodiscard]] bool addDefines(Span<const StringView> defines)
    {
        return getOrCreate<Compile::preprocessorDefines>()->push_back(defines);
    }
};

struct Link
{
    enum Type
    {
        libraryPaths,
        libraryFrameworks,
        enableLTO,
        enableASAN,
        enableStdCpp,
    };

    static constexpr StringView typeToString(Type type)
    {
        switch (type)
        {
        case libraryPaths: return "libraryPaths";
        case libraryFrameworks: return "libraryFrameworks";
        case enableLTO: return "enableLTO";
        case enableASAN: return "enableASAN";
        case enableStdCpp: return "enableStdCpp";
        }
        Assert::unreachable();
    }

    using FieldsTypes = TypeList<TaggedField<Type, libraryPaths, Vector<String>>,      //
                                 TaggedField<Type, libraryFrameworks, Vector<String>>, //
                                 TaggedField<Type, enableLTO, bool>,                   //
                                 TaggedField<Type, enableASAN, bool>,                  //
                                 TaggedField<Type, enableStdCpp, bool>                 //
                                 >;

    using Union = TaggedUnion<Link>;
};

struct LinkFlags : public TaggedMap<Link::Type, Link::Union>
{
    [[nodiscard]] bool addLink(Span<const StringView> libraries)
    {
        return getOrCreate<Link::libraryPaths>()->push_back(libraries);
    }

    [[nodiscard]] bool addFrameworks(Span<const StringView> frameworks)
    {
        return getOrCreate<Link::libraryFrameworks>()->push_back(frameworks);
    }
};

struct Configuration
{
    enum class Preset
    {
        None,
        Debug,
        Release,
    };

    struct VisualStudio
    {
        StringView platformToolset;
    };

    VisualStudio visualStudio;

    static constexpr StringView PresetToString(Preset preset)
    {
        switch (preset)
        {
        case Configuration::Preset::Debug: return "Debug";
        case Configuration::Preset::Release: return "Release";
        case Configuration::Preset::None: return "None";
        }
        Assert::unreachable();
    }

    [[nodiscard]] bool applyPreset(Preset newPreset)
    {
        preset = newPreset;
        switch (preset)
        {
        case Configuration::Preset::Debug: SC_TRY(compile.set<Compile::optimizationLevel>(Optimization::Debug)); break;
        case Configuration::Preset::Release:
            SC_TRY(compile.set<Compile::optimizationLevel>(Optimization::Release));
            break;
        case Configuration::Preset::None: break;
        }
        return true;
    }

    String       name;
    CompileFlags compile;
    LinkFlags    link;
    String       outputPath;
    String       intermediatesPath;

    Preset             preset       = Preset::None;
    Architecture::Type architecture = Architecture::Any;
};

struct TargetType
{
    enum Type
    {
        Executable,
        DynamicLibrary,
        StaticLibrary
    };

    static constexpr StringView typeToString(Type type)
    {
        switch (type)
        {
        case Executable: return "Executable";
        case DynamicLibrary: return "DynamicLibrary";
        case StaticLibrary: return "StaticLibrary";
        }
        Assert::unreachable();
    }
};

struct Project
{
    struct File
    {
        enum Operation
        {
            Add,
            Remove
        };
        Operation operation = Add;
        String    base;
        String    mask;

        bool operator==(const File& other) const
        {
            // collectUniqueRootPaths doesn't care about de-duplicating also operation
            return base == other.base and mask == other.mask;
        }
    };

    TargetType::Type targetType = TargetType::Executable;

    String name;
    String rootDirectory;
    String targetName;

    Vector<File> files;
    CompileFlags compile;
    LinkFlags    link;

    Vector<Configuration> configurations;

    [[nodiscard]] bool setRootDirectory(StringView file);

    [[nodiscard]] bool addPresetConfiguration(Configuration::Preset preset,
                                              StringView            configurationName = StringView());

    [[nodiscard]] Configuration*       getConfiguration(StringView configurationName);
    [[nodiscard]] const Configuration* getConfiguration(StringView configurationName) const;

    [[nodiscard]] bool addFiles(StringView subdirectory, StringView filter);

    [[nodiscard]] bool removeFiles(StringView subdirectory, StringView filter);

    [[nodiscard]] Result validate() const;
};

struct Workspace
{
    String          name;
    Vector<Project> projects;

    CompileFlags compile;
    LinkFlags    link;

    [[nodiscard]] Result validate() const;
};

struct Definition
{
    Vector<Workspace> workspaces;
};

struct Parameters
{
    Array<Platform::Type, 5>     platforms     = {Platform::MacOS, Platform::iOS};
    Array<Architecture::Type, 5> architectures = {Architecture::Intel64, Architecture::Arm64};
    Generator::Type              generator     = Generator::XCode14;
};

struct DefinitionCompiler
{
    VectorMap<String, Vector<String>> resolvedPaths;

    Build::Definition& definition;
    DefinitionCompiler(Build::Definition& definition) : definition(definition) {}

    [[nodiscard]] Result validate();
    [[nodiscard]] Result build();

  private:
    [[nodiscard]] static Result fillPathsList(StringView path, const VectorSet<Project::File>& filters,
                                              VectorMap<String, Vector<String>>& filtersToFiles);
    [[nodiscard]] Result        collectUniqueRootPaths(VectorMap<String, VectorSet<Project::File>>& paths);
};

struct ProjectWriter
{
    Definition&         definition;
    DefinitionCompiler& definitionCompiler;
    Parameters&         parameters;

    ProjectWriter(Definition& definition, DefinitionCompiler& definitionCompiler, Parameters& parameters)
        : definition(definition), definitionCompiler(definitionCompiler), parameters(parameters)
    {}

    [[nodiscard]] bool write(StringView destinationDirectory, StringView filename);

  private:
    struct WriterXCode;
    struct WriterVisualStudio;
};

} // namespace Build
} // namespace SC
