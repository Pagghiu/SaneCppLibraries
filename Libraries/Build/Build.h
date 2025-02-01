// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../Foundation/Result.h"
#include "../Strings/String.h"

namespace SC
{
/// @brief Minimal build system where builds are described in C++ (see @ref library_build)
namespace Build
{
//! @defgroup group_build Build
//! @copybrief library_build (see @ref library_build for more details)

//! @addtogroup group_build
//! @{
struct Parameters;
/// @brief Build Platform (Operating System)
struct Platform
{
    enum Type
    {
        Unknown = 0,
        Windows,
        Apple,
        Linux,
        Wasm
    };

    /// @brief Get StringView from Platform::Type
    static constexpr StringView toString(Type type)
    {
        switch (type)
        {
        case Unknown: return "unknown";
        case Windows: return "windows";
        case Apple: return "apple";
        case Linux: return "linux";
        case Wasm: return "wasm";
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

/// @brief Compile flags (include paths, preprocessor defines etc.)
struct CompileFlags
{
    Vector<String> includePaths; ///< Include search paths list
    Vector<String> defines;      ///< Preprocessor defines

    Optimization::Type optimizationLevel = Optimization::Release; ///< Optimization level

    bool enableASAN       = false; ///< Enable Address Sanitizer
    bool enableRTTI       = false; ///< Enable C++ Runtime Type Identification
    bool enableExceptions = false; ///< Enable C++ Exceptions
    bool enableStdCpp     = false; ///< Enable and include C++ Standard Library
    bool enableCoverage   = false; ///< Enables code coverage instrumentation
};

/// @brief Link flags (library paths, libraries to link, etc.)
struct LinkFlags
{
    Vector<String> libraryPaths;    ///< Libraries search paths list
    Vector<String> libraries;       ///< Names of libraries to link
    Vector<String> frameworks;      ///< Frameworks to link on both iOS and macOS
    Vector<String> frameworksIOS;   ///< Frameworks to link on iOS only
    Vector<String> frameworksMacOS; ///< Frameworks to link on macOS only

    bool guiApplication = false; ///< Link target as GUI application
    bool enableLTO      = false; ///< Enable Link Time Optimization
    bool enableASAN     = false; ///< Enable linking Address Sanitizer
    bool enableStdCpp   = false; ///< Enable and link C++ Standard Library
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
    [[nodiscard]] bool applyPreset(Preset newPreset, const Parameters& parameters);

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

    Project() = default;
    Project(TargetType::Type targetType, StringView name) : targetType(targetType), name(name), targetName(name) {}

    TargetType::Type targetType = TargetType::Executable; ///< Type of build artifact

    String name;          ///< Project name
    String rootDirectory; ///< Project root directory
    String targetName;    ///< Project target name
    String iconPath;      ///< Icon location

    Vector<File> files;   ///< Files that belong to the project
    CompileFlags compile; ///< Shared CompileFlags for all files in the project
    LinkFlags    link;    ///< Shared LinkFlags for all files in the project

    Vector<Configuration> configurations; ///< Build configurations created inside the project

    /// @brief Set root directory for this project (all relative paths will be relative to this one)
    [[nodiscard]] bool setRootDirectory(StringView file);

    /// @brief Add a configuration with a given name, started by cloning options of a specific Preset
    [[nodiscard]] bool addPresetConfiguration(Configuration::Preset preset, const Parameters& parameters,
                                              StringView configurationName = StringView());

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
    Workspace() = default;
    Workspace(StringView name) : name(name) {}

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
    String packagesCacheDirectory;
    String packagesInstallDirectory;
    String libraryDirectory;
};

/// @brief Describes a specific set of platforms, architectures and build generators to generate projects for
struct Parameters
{
    Platform::Type     platform;     ///< Platform to generate
    Architecture::Type architecture; ///< Architecture to generate
    Generator::Type    generator;    ///< Build system types to generate

    Parameters()
    {
        platform     = Platform::Linux;
        architecture = Architecture::Any;
        generator    = Generator::Make;
    }
    Directories directories;
};

/// @brief Top level build description holding all Workspace objects
struct Definition
{
    Vector<Workspace> workspaces; ///< Workspaces to be generated

    /// @brief Generates projects for all workspaces, with specified parameters at given root path.
    /// @param projectName Name of the workspace file / directory to generate
    /// @param parameters Set of parameters with the wanted platforms, architectures and generators to generate
    Result configure(StringView projectName, const Parameters& parameters) const;
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
    using ConfigureFunction = Result (*)(Build::Definition& definition, const Build::Parameters& parameters);

    static Result execute(const Action& action, ConfigureFunction configure, StringView projectName);

    Type action = Configure;

    Parameters parameters;
    StringView configuration;
    StringView target;

  private:
    struct Internal;
};

// Defined inside SC-Build.cpp
Result executeAction(const Action& action);
} // namespace Build
} // namespace SC
