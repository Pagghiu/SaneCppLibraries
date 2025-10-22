// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../../Libraries/Containers/Vector.h"
#include "../../Libraries/Foundation/Result.h"
#include "../../Libraries/Memory/String.h"
#include "../../Libraries/Strings/StringView.h"

namespace SC
{
/// @brief Minimal build system where builds are described in C++ (see @ref page_build)
namespace Build
{
//! @defgroup group_build Build

//! @addtogroup group_build
//! @{
struct Parameters;
struct Project;

template <typename T>
struct Parameter
{
    Parameter() = default;
    Parameter(const T& other) : value(other) {}
    operator const T&() const { return value; }

    bool hasBeenSet() const { return valueSet; }
    void unset() { valueSet = false; }

    Parameter& operator=(const T& other)
    {
        value    = other;
        valueSet = true;
        return *this;
    }

  private:
    T    value;
    bool valueSet = false;
};

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

/// @brief Describe a compile warning to disable
struct Warning
{
    /// @brief Warning disabled state
    enum State
    {
        Disabled // TODO: Add enabled state
    };

    /// @brief What compiler this warning applies to
    enum Type
    {
        MSVCWarning,
        NotMSVCWarning,
        ClangWarning,
        GCCWarning,
    };

    State      state = Disabled;
    Type       type  = MSVCWarning;
    StringView name;
    uint32_t   number = 0;

    Warning(State state, StringView name, Type type = NotMSVCWarning) : state(state), type(type), name(name) {}
    Warning(State state, uint32_t number) : state(state), type(MSVCWarning), number(number) {}
};

struct CppStandard
{
    enum Type
    {
        CPP11,
        CPP14,
        CPP17,
        CPP20,
        CPP23,
    };

    /// @brief Get StringView from CppStandard::Type
    static constexpr StringView toString(Type type)
    {
        switch (type)
        {
        case CPP11: return "c++11";
        case CPP14: return "c++14";
        case CPP17: return "c++17";
        case CPP20: return "c++20";
        case CPP23: return "c++23";
        }
        Assert::unreachable();
    }

    /// @brief Get MSVC LanguageStandard value from CppStandard::Type (e.g., stdcpp14)
    static constexpr StringView toMSVCString(Type type)
    {
        switch (type)
        {
        case CPP11: return "stdcpp11";
        case CPP14: return "stdcpp14";
        case CPP17: return "stdcpp17";
        case CPP20: return "stdcpp20";
        case CPP23: return "stdcpp23";
        }
        Assert::unreachable();
    }

    /// @brief Get Makefile -std= flag from CppStandard::Type
    static StringView toMakefileFlag(Type type) { return toString(type); }
};
/// @brief Compile flags (include paths, preprocessor defines etc.)
struct CompileFlags
{
    Vector<String>  includePaths; ///< Include search paths list
    Vector<String>  defines;      ///< Preprocessor defines list
    Vector<Warning> warnings;     ///< Warnings list

    Parameter<Optimization::Type> optimizationLevel = Optimization::Release; ///< Optimization level

    Parameter<bool> enableASAN       = false; ///< Enable Address Sanitizer
    Parameter<bool> enableRTTI       = false; ///< Enable C++ Runtime Type Identification
    Parameter<bool> enableExceptions = false; ///< Enable C++ Exceptions
    Parameter<bool> enableStdCpp     = false; ///< Enable and include C++ Standard Library
    Parameter<bool> enableCoverage   = false; ///< Enables code coverage instrumentation

    Parameter<CppStandard::Type> cppStandard = CppStandard::CPP14; ///< C++ language standard version

    /// @brief Merges opinions about flags into target flags
    /// @param opinions Opinions about flags from strongest to weakest
    /// @param flags Output flags
    static Result merge(Span<const CompileFlags*> opinions, CompileFlags& flags);

    /// @brief Disable a warning for MSVC
    [[nodiscard]] bool disableWarnings(Span<const uint32_t> number);

    /// @brief Disable a warning for non-MSVC compiler
    [[nodiscard]] bool disableWarnings(Span<const StringView> name);

    /// @brief Disable a warning for Clang
    [[nodiscard]] bool disableClangWarnings(Span<const StringView> name);

    /// @brief Disable a warning for GCC
    [[nodiscard]] bool disableGCCWarnings(Span<const StringView> name);

    /// @brief Adds paths to include paths list
    [[nodiscard]] bool addIncludePaths(Span<const StringView> includePaths);

    /// @brief Adds some pre-processor defines
    [[nodiscard]] bool addDefines(Span<const StringView> defines);

  private:
    friend struct LinkFlags;
    struct Internal;
};

/// @brief Link flags (library paths, libraries to link, etc.)
struct LinkFlags
{
    Vector<String> libraryPaths;    ///< Libraries search paths list
    Vector<String> libraries;       ///< Names of libraries to link
    Vector<String> frameworks;      ///< Frameworks to link on both iOS and macOS
    Vector<String> frameworksIOS;   ///< Frameworks to link on iOS only
    Vector<String> frameworksMacOS; ///< Frameworks to link on macOS only

    Parameter<bool> enableASAN = false; ///< Enable linking Address Sanitizer

    /// @brief Merges opinions about flags into target flags
    /// @param opinions Opinions about flags from strongest to weakest
    /// @param flags Output flags
    static Result merge(Span<const LinkFlags*> opinions, LinkFlags& flags);
};

/// @brief Describes an additive / subtractive selection of files
struct FilesSelection
{
    /// @brief Add or removes from selection
    enum Action
    {
        Add,   ///< Add files
        Remove ///< Remove files
    };
    Action action = Add; ///< Operation type (add or remove files)
    String base;         ///< Base path (not containing `*`)
    String mask;         ///< Mask suffix (can contain `*`)

    bool operator==(const FilesSelection& other) const
    {
        // collectUniqueRootPaths doesn't care about de-duplicating also operation
        return base == other.base and mask == other.mask;
    }
};

/// @brief A selection of files with their associated compile flags
struct SourceFiles
{
    Vector<FilesSelection> selection;
    CompileFlags           compile;

    /// @brief Add some files from a directory to the selection
    [[nodiscard]] bool addSelection(StringView directory, StringView filter);

    /// @brief Remove some files from a directory to the selection
    [[nodiscard]] bool removeSelection(StringView directory, StringView filter);
};

/// @brief Groups SC::Build::CompileFlags and SC::Build::LinkFlags for a given SC::Build::Architecture
struct Configuration
{
    /// @brief A pre-made preset with pre-configured set of options
    enum class Preset
    {
        Debug,         ///< Compile for debug, enabling ASAN (if not set on project and never on VStudio)
        DebugCoverage, ///< Compile for debug, enabling coverage (sets ClangCL for VStudio)
        Release,       ///< Compile for release
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
        }
        Assert::unreachable();
    }

    /// @brief Set compile flags depending on the given Preset
    [[nodiscard]] bool applyPreset(const Project& project, Preset newPreset, const Parameters& parameters);

    [[nodiscard]] static constexpr StringView getStandardBuildDirectory()
    {
        return "$(TARGET_OS)-$(TARGET_ARCHITECTURES)-$(BUILD_SYSTEM)-$(COMPILER)-$(CONFIGURATION)";
    }

    String name;              ///< Configuration name
    String outputPath;        ///< Exe path. If relative, it's appended to _Outputs relative to $(PROJECT_DIR).
    String intermediatesPath; ///< Obj path. If relative, it's appended to _Intermediates relative to $(PROJECT_DIR).

    CompileFlags compile; ///< Configuration compile flags
    LinkFlags    link;    ///< Configuration link flags

    Architecture::Type architecture = Architecture::Any; ///< Restrict this configuration to a specific architecture

    Configuration();
};

/// @brief Type of target artifact to build (executable, library)
struct TargetType
{
    /// @brief Type of artifact
    enum Type
    {
        ConsoleExecutable, ///< Create console executable program
        GUIApplication,    ///< Create graphical application program
    };
};

/// @brief Groups multiple Configuration and source files with their compile and link flags
struct Project
{
    Project() = default;
    Project(TargetType::Type targetType, StringView name) : targetType(targetType), name(name), targetName(name) {}

    TargetType::Type targetType = TargetType::ConsoleExecutable; ///< Type of build artifact

    String name;          ///< Project name
    String rootDirectory; ///< Project root directory
    String targetName;    ///< Project target name
    String iconPath;      ///< Icon location

    SourceFiles files; ///< Project source files with their associated compile flags
    LinkFlags   link;  ///< Linker flags applied to all files in the project

    Vector<SourceFiles> filesWithSpecificFlags; ///< List of files with specific flags different from project/config

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

    /// @brief Add all source or header/inline files from a subdirectory (relative to project root) matching the given
    /// filter
    /// @param subdirectory The subdirectory to search files from, absolute or relative to project root. No `*` allowed.
    /// @param filter The suffix filter that is appended to `subdirectory` (can contain `*`)
    /// @note Files with header or inline extension (`.h`, `.hpp`, `.inl`) will be considered non-source files.
    [[nodiscard]] bool addFiles(StringView subdirectory, StringView filter);

    /// @brief Add a single source or header/inline file to the project, relative to project root
    [[nodiscard]] bool addFile(StringView singleFile);

    /// @brief Add a set of flags that apply to some files only
    [[nodiscard]] bool addSpecificFileFlags(SourceFiles selection);

    /// @brief Adds paths to include paths list
    [[nodiscard]] bool addIncludePaths(Span<const StringView> includePaths);

    /// @brief Adds paths to libraries paths list
    [[nodiscard]] bool addLinkLibraryPaths(Span<const StringView> libraryPaths);

    /// @brief Adds libraries to be linked
    [[nodiscard]] bool addLinkLibraries(Span<const StringView> linkLibraries);

    /// @brief Add frameworks shared with all apple os
    [[nodiscard]] bool addLinkFrameworks(Span<const StringView> frameworks);

    /// @brief Add frameworks only for macOS
    [[nodiscard]] bool addLinkFrameworksMacOS(Span<const StringView> frameworks);

    /// @brief Add frameworks only for iOS
    [[nodiscard]] bool addLinkFrameworksIOS(Span<const StringView> frameworks);

    /// @brief Adds some pre-processor defines
    [[nodiscard]] bool addDefines(Span<const StringView> defines);

    /// @brief Remove files matching a filter, to remove only a specific file type after Project::addDirectory
    /// @param subdirectory The subdirectory to search files into, absolute or relative to project root. No `*` allowed.
    /// @param filter The suffix filter that is appended to `subdirectory` (can contain `*`)
    [[nodiscard]] bool removeFiles(StringView subdirectory, StringView filter);

    /// @brief Validates this project for it to contain a valid combination of flags
    Result validate() const;
};

/// @brief Groups multiple Project together with shared compile and link flags
struct Workspace
{
    Workspace() = default;
    Workspace(StringView name) : name(name) {}

    String          name;     ///< Workspace name
    Vector<Project> projects; ///< List of projects in this workspace

    /// @brief Validates all projects in this workspace
    Result validate() const;
};

/// @brief Collects all directories used during build generation
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
    /// @param workspaceName Name of the workspace to generate
    /// @param parameters Set of parameters with the wanted platforms, architectures and generators to generate
    Result configure(StringView workspaceName, const Parameters& parameters) const;
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

    static Result execute(const Action& action, ConfigureFunction configure, StringView defaultWorkspaceName);

    Type action = Configure;

    Parameters parameters;
    StringView configuration;
    StringView target;
    StringView workspaceName;

    Span<const StringView> additionalArguments;

  private:
    struct Internal;
};

// Defined inside SC-Build.cpp
Result executeAction(const Action& action);
} // namespace Build
} // namespace SC
