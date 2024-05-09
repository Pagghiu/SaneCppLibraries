// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../Containers/Array.h"
#include "../Containers/VectorSet.h"
#include "../Foundation/Result.h"
#include "../Foundation/TaggedUnion.h"
#include "../Strings/String.h"
#include "Internal/TaggedMap.h"

namespace SC
{
/// @brief Minimal build system where builds are described in C++ (see @ref library_build)
namespace Build
{
//! @defgroup group_build Build
//! @copybrief library_build (see @ref library_build for more details)

//! @addtogroup group_build
//! @{

/// @brief Build Platform (Operating System)
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

    /// @brief Get StringView from Platform::Type
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

/// @brief Build Architecture (Processor / Instruction set)
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

    /// @brief Get StringView from Architecture::Type
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

/// @brief Build system generator (Xcode / Visual Studio)
struct Generator
{
    enum Type
    {
        XCode,            ///< Generate projects for XCode (Version 14+)
        VisualStudio2022, ///< Generate projects for Visual Studio 2022
        VisualStudio2019, ///< Generate projects for Visual Studio 2019
        Make,             ///< Generate posix makefiles
    };

    /// @brief Get StringView from Generator::Type
    static constexpr StringView toString(Type type)
    {
        switch (type)
        {
        case XCode: return "XCode";
        case VisualStudio2022: return "VisualStudio2022";
        case VisualStudio2019: return "VisualStudio2019";
        case Make: return "Make";
        }
        Assert::unreachable();
    }
};

/// @brief Optimization level (Debug / Release)
struct Optimization
{
    enum Type
    {
        Debug,   ///< Optimizations set to debug
        Release, ///< Optimizations set to release
    };

    /// @brief Get StringView from Optimization::Type
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

/// @brief Compilation switches (include paths, preprocessor defines, etc.)
struct Compile
{
    enum Type
    {
        includePaths = 0,    ///< Include paths
        preprocessorDefines, ///< Preprocessor defines
        optimizationLevel,   ///< Optimization Level (debug / release)
        enableASAN,          ///< Address Sanitizer
        enableRTTI,          ///< Runtime Type Identification
        enableExceptions,    ///< C++ Exceptions
        enableStdCpp,        ///< C++ Standard Library
        enableCoverage,      ///< Enables code coverage instrumentation
    };

    /// @brief Two StringViews representing name and description
    struct NameDescription
    {
        StringView name;
        StringView description;
    };

    /// @brief Get name and description from Compile::Type
    static constexpr NameDescription typeToString(Type type)
    {
        switch (type)
        {
        case includePaths: return {"includePaths", "Include paths"};
        case preprocessorDefines: return {"preprocessorDefines", "Preprocessor defines"};
        case optimizationLevel: return {"optimizationLevel", "Optimization level"};
        case enableASAN: return {"enableASAN", "Address Sanitizer"};
        case enableRTTI: return {"enableRTTI", "Runtime Type Identification"};
        case enableExceptions: return {"enableExceptions", "C++ Exceptions"};
        case enableStdCpp: return {"enableStdCpp", "C++ Standard Library"};
        case enableCoverage: return {"enableCoverage", "Code coverage instrumentation"};
        }
        Assert::unreachable();
    }
    template <Type E, typename T>
    using Tag = TaggedType<Type, E, T>; // Helper to save some typing

    using FieldsTypes = TypeTraits::TypeList<Tag<includePaths, Vector<String>>,          //
                                             Tag<preprocessorDefines, Vector<String>>,   //
                                             Tag<optimizationLevel, Optimization::Type>, //
                                             Tag<enableASAN, bool>,                      //
                                             Tag<enableRTTI, bool>,                      //
                                             Tag<enableExceptions, bool>,                //
                                             Tag<enableCoverage, bool>,                  //
                                             Tag<enableStdCpp, bool>>;

    using Union = TaggedUnion<Compile>;
};

/// @brief Map of SC::Build::Compile flags (include paths, preprocessor defines etc.)
struct CompileFlags : public TaggedMap<Compile::Type, Compile::Union>
{
    /// @brief Add paths to includes search paths list
    [[nodiscard]] bool addIncludes(Span<const StringView> includes)
    {
        return getOrCreate<Compile::includePaths>()->append(includes);
    }

    /// @brief Add define to preprocessor definitions
    [[nodiscard]] bool addDefines(Span<const StringView> defines)
    {
        return getOrCreate<Compile::preprocessorDefines>()->append(defines);
    }
};

/// @brief Linking switches (library paths, LTO etc.)
struct Link
{
    enum Type
    {
        libraryPaths,      ///< Library paths
        libraryFrameworks, ///< Frameworks paths
        enableLTO,         ///< Link Time Optimization
        enableASAN,        ///< Address Sanitizer
        enableStdCpp,      ///< C++ Standard Library
    };

    /// @brief Get StringView describing Link::Type
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
    template <Type E, typename T>
    using Tag = TaggedType<Type, E, T>; // Helper to save some typing

    using FieldsTypes = TypeTraits::TypeList<Tag<libraryPaths, Vector<String>>,      //
                                             Tag<libraryFrameworks, Vector<String>>, //
                                             Tag<enableLTO, bool>,                   //
                                             Tag<enableASAN, bool>,                  //
                                             Tag<enableStdCpp, bool>                 //
                                             >;

    using Union = TaggedUnion<Link>;
};

/// @brief Map of SC::Build::Link flags (library paths, LTO switch etc.)
struct LinkFlags : public TaggedMap<Link::Type, Link::Union>
{
    /// @brief Add the given paths to the library search paths list
    [[nodiscard]] bool addLibraryPath(Span<const StringView> libraries)
    {
        return getOrCreate<Link::libraryPaths>()->append(libraries);
    }

    /// @brief Add framework to list of frameworks to link
    [[nodiscard]] bool addFrameworks(Span<const StringView> frameworks)
    {
        return getOrCreate<Link::libraryFrameworks>()->append(frameworks);
    }
};

/// @brief Groups SC::Build::CompileFlags and SC::Build::LinkFlags for a given SC::Build::Architecture
struct Configuration
{
    /// @brief A pre-made preset with pre-configured set of options
    enum class Preset
    {
        None,          ///< Custom configuration
        Debug,         ///< Debug configuration
        DebugCoverage, ///< Debug coverage configuration
        Release,       ///< Release configuration
    };

    /// @brief Visual Studio platform toolset
    struct VisualStudio
    {
        StringView platformToolset;
    };

    VisualStudio visualStudio; ///< Customize VisualStudio platformToolset

    /// @brief Convert Preset to StringView
    [[nodiscard]] static constexpr StringView PresetToString(Preset preset)
    {
        switch (preset)
        {
        case Configuration::Preset::Debug: return "Debug";
        case Configuration::Preset::DebugCoverage: return "DebugCoverage";
        case Configuration::Preset::Release: return "Release";
        case Configuration::Preset::None: return "None";
        }
        Assert::unreachable();
    }

    /// @brief Set compile flags depending on the given Preset
    [[nodiscard]] bool applyPreset(Preset newPreset)
    {
        preset = newPreset;
        switch (preset)
        {
        case Configuration::Preset::DebugCoverage:
            SC_TRY(compile.set<Compile::enableCoverage>(true));
            SC_TRY(compile.set<Compile::optimizationLevel>(Optimization::Debug));
            SC_TRY(compile.addDefines({"DEBUG=1"}));
            break;
        case Configuration::Preset::Debug:
            SC_TRY(compile.set<Compile::optimizationLevel>(Optimization::Debug));
            SC_TRY(compile.addDefines({"DEBUG=1"}));
            break;
        case Configuration::Preset::Release:
            SC_TRY(compile.set<Compile::optimizationLevel>(Optimization::Release));
            SC_TRY(compile.addDefines({"NDEBUG=1"}));
            break;
        case Configuration::Preset::None: break;
        }
        return true;
    }

    [[nodiscard]] static constexpr StringView getStandardBuildDirectory()
    {
        return "$(TARGET_OS)-$(TARGET_ARCHITECTURES)-$(BUILD_SYSTEM)-$(COMPILER)-$(CONFIGURATION)";
    }

    String name;              ///< Configuration name
    String outputPath;        ///< Exe path. If relative, it's appended to _Outputs relative to $(PROJECT_DIR).
    String intermediatesPath; ///< Obj path. If relative, it's appended to _Intermediates relative to $(PROJECT_DIR).

    CompileFlags compile; ///< Configuration compile flags
    LinkFlags    link;    ///< Configuration link flags

    Preset             preset       = Preset::None;      ///< Build preset applied to this configuration
    Architecture::Type architecture = Architecture::Any; ///< Restrict this configuration to a specific architecture

    Configuration();
};

/// @brief Type of target artifact to build (executable, library)
struct TargetType
{
    /// @brief Type of artifact
    enum Type
    {
        Executable,     ///< Create executable program
        DynamicLibrary, ///< Create dynamic library
        StaticLibrary   ///< Create static library
    };

    /// @brief Convert TargetType to StringView
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

/// @brief Groups multiple Configuration and source files with their compile and link flags
struct Project
{
    /// @brief Project list of files
    struct File
    {
        /// @brief Indicates if this is an additive or subtractive files operation
        enum Operation
        {
            Add,   ///< Add files
            Remove ///< Remove files
        };
        Operation operation = Add; ///< Operation type (add or remove files)
        String    base;            ///< Base path (not containing `*`)
        String    mask;            ///< Mask suffix (can contain `*`)

        bool operator==(const File& other) const
        {
            // collectUniqueRootPaths doesn't care about de-duplicating also operation
            return base == other.base and mask == other.mask;
        }
    };

    TargetType::Type targetType = TargetType::Executable; ///< Type of build artifact

    String name;          ///< Project name
    String rootDirectory; ///< Project root directory
    String targetName;    ///< Project target name

    Vector<File> files;   ///< Files that belong to the project
    CompileFlags compile; ///< Shared CompileFlags for all files in the project
    LinkFlags    link;    ///< Shared LinkFlags for all files in the project

    Vector<Configuration> configurations; ///< Build configurations created inside the project

    /// @brief Set root directory for this project (all relative paths will be relative to this one)
    [[nodiscard]] bool setRootDirectory(StringView file);

    /// @brief Add a configuration with a given name, started by cloning options of a specific Preset
    [[nodiscard]] bool addPresetConfiguration(Configuration::Preset preset,
                                              StringView            configurationName = StringView());

    /// @brief Get Configuration with the matching `configurationName`
    [[nodiscard]] Configuration* getConfiguration(StringView configurationName);
    /// @brief Get Configuration with the matching `configurationName`
    [[nodiscard]] const Configuration* getConfiguration(StringView configurationName) const;

    /// @brief Add all files from specific subdirectory (relative to project root) matching given filter
    /// @param subdirectory The subdirectory to search files from, absolute or relative to project root. No `*` allowed.
    /// @param filter The suffix filter that is appended to `subdirectory` (can contain `*`)
    [[nodiscard]] bool addDirectory(StringView subdirectory, StringView filter);

    /// @brief Add a single file to the project
    [[nodiscard]] bool addFile(StringView singleFile);

    /// @brief Remove files matching the given filter. Useful to remove only a specific file type after
    /// Project::addDirectory
    /// @param subdirectory The subdirectory to search files into, absolute or relative to project root. No `*` allowed.
    /// @param filter The suffix filter that is appended to `subdirectory` (can contain `*`)
    [[nodiscard]] bool removeFiles(StringView subdirectory, StringView filter);

    /// @brief Validates this project for it to contain a valid combination of flags
    [[nodiscard]] Result validate() const;
};

/// @brief Groups multiple Project together with shared compile and link flags
struct Workspace
{
    String          name;     ///< Workspace name
    Vector<Project> projects; ///< List of projects in this workspace
    CompileFlags    compile;  ///< Global workspace compile flags for all projects
    LinkFlags       link;     ///< Global workspace link flags for all projects

    /// @brief Validates all projects in this workspace
    [[nodiscard]] Result validate() const;
};

struct Directories
{
    String projectsDirectory;
    String intermediatesDirectory;
    String outputsDirectory;
    String libraryDirectory;
};

/// @brief Describes a specific set of platforms, architectures and build generators to generate projects for
struct Parameters
{
    Array<Platform::Type, 5>     platforms;     ///< Platforms to generate
    Array<Architecture::Type, 5> architectures; ///< Architectures to generate
    Generator::Type              generator;     ///< Build system types to generate

    Directories directories;
};

/// @brief Top level build description holding all Workspace objects
struct Definition
{
    Vector<Workspace> workspaces; ///< Workspaces to be generated

    /// @brief Generates projects for all workspaces, with specified parameters at given root path.
    /// @param projectName Name of the workspace file / directory to generate
    /// @param parameters Set of parameters with the wanted platforms, architectures and generators to generate
    /// @param directories Contains projects, outputs and intermediates paths
    Result configure(StringView projectName, const Parameters& parameters, Directories directories) const;
};

/// @brief Caches file paths by pre-resolving directory filter search masks
struct DefinitionCompiler
{
    VectorMap<String, Vector<String>> resolvedPaths;

    const Build::Definition& definition;
    DefinitionCompiler(const Build::Definition& definition) : definition(definition) {}

    [[nodiscard]] Result validate();
    [[nodiscard]] Result build();

  private:
    [[nodiscard]] static Result fillPathsList(StringView path, const VectorSet<Project::File>& filters,
                                              VectorMap<String, Vector<String>>& filtersToFiles);
    [[nodiscard]] Result        collectUniqueRootPaths(VectorMap<String, VectorSet<Project::File>>& paths);
};
//! @}

//-----------------------------------------------------------------------------------------------------------------------
// Implementations Details
//-----------------------------------------------------------------------------------------------------------------------

struct Action
{
    enum Type
    {
        Configure,
        Compile,
        Run,
        Print,
        Coverage
    };
    using ConfigureFunction = Result (*)(Build::Definition& definition, Build::Parameters& parameters);

    static Result execute(const Action& action, ConfigureFunction configure, StringView projectName);

    Type action = Configure;

    Generator::Type    generator    = Generator::Type::Make;
    Architecture::Type architecture = Architecture::Any;

    Directories directories;
    StringView  configuration;

  private:
    struct Internal;
};

// Defined inside SC-Build.cpp
Result executeAction(const Action& action);
} // namespace Build
} // namespace SC
