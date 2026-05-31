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
        Native,           ///< Build directly on the host without generating project files first
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
        case Native: return "Native";
        case XCode: return "XCode";
        case VisualStudio2022: return "VisualStudio2022";
        case VisualStudio2019: return "VisualStudio2019";
        case Make: return "Make";
        }
        Assert::unreachable();
    }
};

/// @brief Describes the target runtime / ABI environment for a machine
struct TargetEnvironment
{
    enum Type
    {
        Native = 0,
        WindowsGNU,
        WindowsMSVC,
        LinuxGlibc,
        LinuxMusl,
    };

    /// @brief Get StringView from TargetEnvironment::Type
    static constexpr StringView toString(Type type)
    {
        switch (type)
        {
        case Native: return "native";
        case WindowsGNU: return "windows-gnu";
        case WindowsMSVC: return "windows-msvc";
        case LinuxGlibc: return "linux-glibc";
        case LinuxMusl: return "linux-musl";
        }
        Assert::unreachable();
    }
};

/// @brief Describes the machine a build runs on or targets
struct Machine
{
    Platform::Type          platform     = Platform::Unknown;
    Architecture::Type      architecture = Architecture::Any;
    TargetEnvironment::Type environment  = TargetEnvironment::Native;
};

/// @brief Describes how a standalone backend should invoke the toolchain
struct Toolchain
{
    enum Family
    {
        HostDefault,
        Clang,
        FilC,
        GCC,
        MSVC,
        ClangCL,
        LLVMMingw,
        CustomDriver,
    };

    Family family = HostDefault;

    String compilerC;
    String compilerCpp;
    String linker;
    String archiver;

    String targetTriple;
    String sysroot;

    Vector<String> extraCompilerFlags;
    Vector<String> extraLinkerFlags;

    Platform::Type     platform     = Platform::Unknown;
    Architecture::Type architecture = Architecture::Any;
};

/// @brief Controls how the native backend presents build progress and child process output
struct OutputMode
{
    enum Type
    {
        Quiet,
        Normal,
        Verbose,
    };

    /// @brief Get StringView from OutputMode::Type
    static constexpr StringView toString(Type type)
    {
        switch (type)
        {
        case Quiet: return "quiet"_a8;
        case Normal: return "normal"_a8;
        case Verbose: return "verbose"_a8;
        }
        Assert::unreachable();
    }
};

/// @brief Controls how the standalone native backend executes jobs
struct ExecutionOptions
{
    size_t maxParallelJobs         = 0;     ///< Native backend max parallel compile jobs, 0 means auto-detect
    bool   useCompilerDependencies = true;  ///< Enables native backend to consume compiler-generated dependencies
    bool   verbose                 = false; ///< Legacy compatibility alias for outputMode == Verbose

    Parameter<OutputMode::Type> outputMode = OutputMode::Normal;
};

/// @brief Describes how a built executable should be launched
struct RunnerSpec
{
    enum Type
    {
        Auto,
        None,
        Wine,
        QEMU,
        Custom,
    };

    /// @brief Get StringView from RunnerSpec::Type
    static constexpr StringView toString(Type type)
    {
        switch (type)
        {
        case Auto: return "auto";
        case None: return "none";
        case Wine: return "wine";
        case QEMU: return "qemu";
        case Custom: return "custom";
        }
        Assert::unreachable();
    }

    Type type = Auto;

    String         executable;
    Vector<String> arguments;
};

/// @brief Support status for a build or run capability in the native backend support matrix
struct SupportStatus
{
    enum Type
    {
        Unsupported,
        NotYet,
        SmokeSupported,
        Supported,
        Experimental,
        Blocked,
    };

    /// @brief Get StringView from SupportStatus::Type
    static constexpr StringView toString(Type type)
    {
        switch (type)
        {
        case Unsupported: return "unsupported";
        case NotYet: return "not-yet";
        case SmokeSupported: return "smoke-supported";
        case Supported: return "supported";
        case Experimental: return "experimental";
        case Blocked: return "blocked";
        }
        Assert::unreachable();
    }
};

/// @brief Confidence tier for a row in the native backend support matrix
struct SupportTier
{
    enum Type
    {
        Unsupported,
        Experimental,
        Tier1,
        Tier2,
        Blocked,
    };

    /// @brief Get StringView from SupportTier::Type
    static constexpr StringView toString(Type type)
    {
        switch (type)
        {
        case Unsupported: return "unsupported";
        case Experimental: return "experimental";
        case Tier1: return "tier1";
        case Tier2: return "tier2";
        case Blocked: return "blocked";
        }
        Assert::unreachable();
    }
};

/// @brief One host->target support claim for the native backend
struct SupportMatrixEntry
{
    Machine hostMachine;
    Machine targetMachine;

    SupportStatus::Type buildSupport = SupportStatus::Unsupported;
    SupportStatus::Type runSupport   = SupportStatus::Unsupported;
    SupportTier::Type   tier         = SupportTier::Unsupported;
    RunnerSpec::Type    runner       = RunnerSpec::None;

    StringView validation;
};

/// @brief Returns the current native-backend cross-compilation support matrix
[[nodiscard]] inline Span<const SupportMatrixEntry> getNativeBackendSupportMatrix()
{
    static constexpr SupportMatrixEntry entries[] = {
        {{Platform::Apple, Architecture::Any, TargetEnvironment::Native},
         {Platform::Windows, Architecture::Intel64, TargetEnvironment::WindowsGNU},
         SupportStatus::Supported,
         SupportStatus::Supported,
         SupportTier::Tier1,
         RunnerSpec::Wine,
         "fixture-compile-and-wine-run"},
        {{Platform::Apple, Architecture::Any, TargetEnvironment::Native},
         {Platform::Windows, Architecture::Arm64, TargetEnvironment::WindowsGNU},
         SupportStatus::Supported,
         SupportStatus::NotYet,
         SupportTier::Tier1,
         RunnerSpec::Wine,
         "fixture-compile-packaged-wine-missing-arm64-loader"},
        {{Platform::Apple, Architecture::Any, TargetEnvironment::Native},
         {Platform::Windows, Architecture::Intel64, TargetEnvironment::WindowsMSVC},
         SupportStatus::Supported,
         SupportStatus::SmokeSupported,
         SupportTier::Tier2,
         RunnerSpec::Wine,
         "portable-msvc-compile-link-and-smoke-run"},
        {{Platform::Apple, Architecture::Any, TargetEnvironment::Native},
         {Platform::Windows, Architecture::Arm64, TargetEnvironment::WindowsMSVC},
         SupportStatus::Supported,
         SupportStatus::NotYet,
         SupportTier::Tier2,
         RunnerSpec::Wine,
         "portable-msvc-arm64-compile-packaged-wine-missing-arm64-loader"},
        {{Platform::Apple, Architecture::Any, TargetEnvironment::Native},
         {Platform::Linux, Architecture::Intel64, TargetEnvironment::LinuxGlibc},
         SupportStatus::Supported,
         SupportStatus::NotYet,
         SupportTier::Tier1,
         RunnerSpec::QEMU,
         "packaged-llvm-glibc-sysroot-compile-opportunistic-qemu-smoke"},
        {{Platform::Apple, Architecture::Any, TargetEnvironment::Native},
         {Platform::Linux, Architecture::Arm64, TargetEnvironment::LinuxGlibc},
         SupportStatus::Supported,
         SupportStatus::NotYet,
         SupportTier::Tier1,
         RunnerSpec::QEMU,
         "packaged-llvm-glibc-sysroot-compile-opportunistic-qemu-smoke"},
        {{Platform::Apple, Architecture::Any, TargetEnvironment::Native},
         {Platform::Linux, Architecture::Intel64, TargetEnvironment::LinuxMusl},
         SupportStatus::Supported,
         SupportStatus::NotYet,
         SupportTier::Tier1,
         RunnerSpec::QEMU,
         "packaged-llvm-musl-sysroot-compile-opportunistic-qemu-smoke"},
        {{Platform::Apple, Architecture::Any, TargetEnvironment::Native},
         {Platform::Linux, Architecture::Arm64, TargetEnvironment::LinuxMusl},
         SupportStatus::Supported,
         SupportStatus::NotYet,
         SupportTier::Tier1,
         RunnerSpec::QEMU,
         "packaged-llvm-musl-sysroot-compile-opportunistic-qemu-smoke"},
        {{Platform::Windows, Architecture::Any, TargetEnvironment::Native},
         {Platform::Linux, Architecture::Arm64, TargetEnvironment::LinuxGlibc},
         SupportStatus::Supported,
         SupportStatus::NotYet,
         SupportTier::Tier2,
         RunnerSpec::QEMU,
         "windows-packaged-llvm-glibc-sysroot-compile"},
        {{Platform::Windows, Architecture::Any, TargetEnvironment::Native},
         {Platform::Linux, Architecture::Intel64, TargetEnvironment::LinuxMusl},
         SupportStatus::Supported,
         SupportStatus::NotYet,
         SupportTier::Tier2,
         RunnerSpec::QEMU,
         "windows-packaged-llvm-musl-sysroot-compile"},
        {{Platform::Linux, Architecture::Any, TargetEnvironment::Native},
         {Platform::Windows, Architecture::Intel64, TargetEnvironment::WindowsGNU},
         SupportStatus::Supported,
         SupportStatus::Supported,
         SupportTier::Tier1,
         RunnerSpec::Wine,
         "fixture-compile-and-wine-run"},
        {{Platform::Linux, Architecture::Any, TargetEnvironment::Native},
         {Platform::Windows, Architecture::Arm64, TargetEnvironment::WindowsGNU},
         SupportStatus::Supported,
         SupportStatus::SmokeSupported,
         SupportTier::Tier1,
         RunnerSpec::Wine,
         "linux-arm64-sctest-targeted-wine-smoke"},
        {{Platform::Linux, Architecture::Any, TargetEnvironment::Native},
         {Platform::Windows, Architecture::Intel64, TargetEnvironment::WindowsMSVC},
         SupportStatus::Supported,
         SupportStatus::SmokeSupported,
         SupportTier::Tier2,
         RunnerSpec::Wine,
         "linux-arm64-portable-msvc-targeted-wine-smoke"},
        {{Platform::Linux, Architecture::Any, TargetEnvironment::Native},
         {Platform::Windows, Architecture::Arm64, TargetEnvironment::WindowsMSVC},
         SupportStatus::Supported,
         SupportStatus::SmokeSupported,
         SupportTier::Tier2,
         RunnerSpec::Wine,
         "linux-arm64-portable-msvc-targeted-wine-smoke"},
    };
    return entries;
}

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
    Parameter<bool> enableCoverage   = false; ///< Enables code coverage instrumentation
    Parameter<bool> includeStdCpp    = true;  ///< Include C++ standard library headers

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
    friend struct SaneCppFlags;
    struct Internal;
};

struct LinkFlags;

/// @brief Sane C++ Libraries specific build policy flags
struct SaneCppFlags
{
    Parameter<bool> enabled                = false; ///< Emit Sane C++ Libraries policy macros for this target
    Parameter<bool> provideCppRuntimeShims = false; ///< Provide C++ runtime ABI shims from Sane C++ Libraries

    /// @brief Merges opinions about Sane C++ policy into target flags
    /// @param opinions Opinions about flags from strongest to weakest
    /// @param flags Output flags
    static void merge(Span<const SaneCppFlags*> opinions, SaneCppFlags& flags);

    /// @brief Adds preprocessor defines implied by this Sane C++ policy
    Result applyTo(CompileFlags& flags, const LinkFlags& linkFlags) const;
};

/// @brief Link flags (library paths, libraries to link, etc.)
struct LinkFlags
{
    Vector<String> libraryPaths;    ///< Libraries search paths list
    Vector<String> libraries;       ///< Names of libraries to link
    Vector<String> frameworks;      ///< Frameworks to link on both iOS and macOS
    Vector<String> frameworksIOS;   ///< Frameworks to link on iOS only
    Vector<String> frameworksMacOS; ///< Frameworks to link on macOS only

    Parameter<bool> enableASAN              = false; ///< Enable linking Address Sanitizer
    Parameter<bool> enableDeadCodeStripping = false; ///< Enable linker dead code stripping
    Parameter<bool> preserveExportedSymbols = false; ///< Keep explicitly exported symbols alive while stripping
                                                     ///< (currently ignored by the XCode backend, which disables dead
                                                     ///< code stripping instead)
    Parameter<bool> linkStdCpp = true;               ///< Link the C++ standard-library runtime

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

/// @brief Coverage specific flags
struct CoverageFlags
{
    String excludeRegex; ///< Regex of files to exclude from coverage
};

struct WindowsTargetOptions
{
    Parameter<bool> longPathAware = false; ///< Embed a long-path-aware Windows manifest on supported runtime targets
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

    VisualStudio         visualStudio; ///< Customize VisualStudio platformToolset
    WindowsTargetOptions windows;

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
    String outputPath;        ///< If relative, it's appended to _Outputs relative to $(PROJECT_DIR).
    String intermediatesPath; ///< If relative, it's appended to _Intermediates relative to $(PROJECT_DIR).

    CompileFlags compile; ///< Configuration compile flags
    LinkFlags    link;    ///< Configuration link flags
    SaneCppFlags saneCpp; ///< Sane C++ Libraries specific policy flags

    CoverageFlags coverage; ///< Configuration coverage flags

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
        SharedLibrary,     ///< Create shared library / dynamic library
        StaticLibrary,     ///< Create static library archive
    };
};

/// @brief How a project consumes Sane C++ Libraries
enum class Libraries
{
    SingleFile, ///< Add the repository root `SC.cpp` unity build file
    Multiple,   ///< Add all files under `Libraries/`
};

/// @brief Groups multiple Configuration and source files with their compile and link flags
struct Project
{
    Project(StringView name = {}, TargetType::Type targetType = TargetType::ConsoleExecutable)
        : name(name), targetType(targetType)
    {}

    String               name;                                       ///< Project name
    TargetType::Type     targetType = TargetType::ConsoleExecutable; ///< Type of build artifact
    WindowsTargetOptions windows;

    String rootDirectory; ///< Project root directory (== Parameters::projectDirectory if empty)
    String targetName;    ///< Project target name (== Project::name if empty)
    String iconPath;      ///< Icon location

    SourceFiles  files;   ///< Project source files with their associated compile flags
    LinkFlags    link;    ///< Linker flags applied to all files in the project
    SaneCppFlags saneCpp; ///< Sane C++ Libraries specific policy flags

    Vector<String> exportLibraries;   ///< SC libraries that this executable explicitly exports for plugins
    Vector<String> exportDirectories; ///< Additional source roots whose symbols are exported for plugins

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
    /// @note Native and Make backends merge the full compile flags. Generated XCode / Visual Studio backends currently
    /// emit per-file include paths, defines and backend-specific warning disables only.
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

    /// @brief Export selected Sane C++ libraries from this target for plugin consumers
    [[nodiscard]] bool addExportLibraries(Span<const StringView> libraries);

    /// @brief Export all Sane C++ libraries from this target for plugin consumers
    [[nodiscard]] bool addExportAllLibraries();

    /// @brief Export symbols produced from sources under the given directories for plugin consumers
    [[nodiscard]] bool addExportDirectories(Span<const StringView> directories);

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
    String buildCacheDirectory;
    String packagesCacheDirectory;
    String packagesInstallDirectory;
    String libraryDirectory;
    String projectDirectory;
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
        generator    = Generator::Native;
    }
    Directories directories;
    Toolchain   toolchain;
    RunnerSpec  runner;

    Machine hostMachine;   ///< Machine that owns host-side tools and paths
    Machine targetMachine; ///< Machine the produced artifacts are built for

    ExecutionOptions execution;
};

/// @brief Add Sane C++ Libraries to a project using either `SC.cpp` or the individual library sources
[[nodiscard]] Result addSaneCppLibraries(Project& project, const Parameters& parameters,
                                         Libraries mode = Libraries::SingleFile);

/// @brief Top level build description holding all Workspace objects
struct Definition
{
    Vector<Workspace> workspaces; ///< Workspaces to be generated

    /// @brief Adds a project to the default workspace
    Result addProject(Project&& project);

    /// @brief Generates projects for all workspaces, with specified parameters at given root path.
    /// @param workspaceName Name of the workspace to generate
    /// @param parameters Set of parameters with the wanted platforms, architectures and generators to generate
    /// @note When `parameters.generator == Generator::Native`, this only prepares the directory layout. Direct native
    /// builds are driven through `Action::Compile`, `Action::Run`, and `Action::Coverage`.
    Result configure(StringView workspaceName, const Parameters& parameters) const;

    /// @brief Fills relevant defaults for parameters that are not specified
    Result enforceDefaults(const Parameters& parameters);

    /// @brief Finds the configuration with given name in given workspace and project
    [[nodiscard]] bool findConfiguration(StringView workspaceName, StringView projectName, StringView configurationName,
                                         Workspace*& workspace, Project*& project, Configuration*& configuration);

    /// @brief Const overload of findConfiguration
    [[nodiscard]] bool findConfiguration(StringView workspaceName, StringView projectName, StringView configurationName,
                                         const Workspace*& workspace, const Project*& project,
                                         const Configuration*& configuration) const;
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

    static Result execute(const Action& action, ConfigureFunction configure);

    Type action = Configure;

    Parameters parameters;
    StringView configurationName;
    StringView projectName;
    StringView workspaceName;

    bool allTargets = false;

    Span<const StringView> additionalArguments;

  private:
    struct Internal;
};

// Defined by the SC-build implementation
Result executeAction(const Action& action);
struct NativeBuild;
} // namespace Build
} // namespace SC
