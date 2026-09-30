// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once

#include "../Common/Result.h"

namespace SC
{
//! @addtogroup group_plugin
//! @{

/// @brief Stable portable failures returned by the Plugin library.
enum class PluginError : uint32_t
{
    PathNotNullTerminated = 1,
    UnsupportedPathEncoding,
    InvalidDefinition,
    DynamicLibraryOperationUnsupported,
    PathCapacityExceeded,
    DefinitionCapacityExceeded,
    ScannerCapacityExceeded,
    RegistryCapacityExceeded,
    CompilerConfigurationCapacityExceeded,
    SysrootCapacityExceeded,
    ArgumentCapacityExceeded,
    FileOpenFailed,
    FileSizeQueryFailed,
    FileBufferCapacityExceeded,
    FileReadFailed,
    FileReadIncomplete,
    FileRemoveFailed,
    DirectoryOpenFailed,
    DirectoryReadFailed,
    CompilerNotFound,
    SysrootNotFound,
    CompilerEnvironmentReadFailed,
    CompilerExitedWithFailure,
    LinkerExitedWithFailure,
    DynamicLibraryAlreadyLoaded,
    DynamicLibraryLoadFailed,
    DynamicLibraryCloseFailed,
    DynamicLibraryNotLoaded,
    SymbolNameConversionFailed,
    SymbolNameCapacityExceeded,
    SymbolNotFound,
    InitializationEntryPointNotFound,
    CloseEntryPointNotFound,
    PluginNotFound,
    PluginInitializationFailed,
    PluginShutdownFailed,
    DebuggerUnlockFailed,
};

/// @brief Stable operation or backend stage retained for a Plugin failure.
enum class PluginErrorDetail : uint16_t
{
    None = 0,
    PathAssign,
    PathAppend,
    MetadataName,
    MetadataVersion,
    MetadataDescription,
    MetadataCategory,
    MetadataDependency,
    MetadataBuildOption,
    ScannerDefinitionStorage,
    ScannerCandidatePath,
    ScannerCandidateFile,
    IteratorAssignDirectory,
    IteratorBuildSearchPath,
    IteratorBuildEntryPath,
    CompilerFindConfiguration,
    CompilerBuildArguments,
    CompilerWriteArguments,
    CompilerBuildObjectPath,
    LinkerBuildArguments,
    LinkerWriteArguments,
    LinkerBuildOutputPath,
    SysrootFindVersion,
    SysrootBuildIncludePath,
    SysrootBuildLibraryPath,
    RegistryUnloadList,
    RegistryStoreDefinition,
    RegistryFindPlugin,
    WindowsFileCreate,
    WindowsFileGetSize,
    WindowsFileRead,
    WindowsFileDelete,
    PosixFileOpen,
    PosixFileStat,
    PosixFileRead,
    PosixFileRemove,
    WindowsDirectoryFindFirst,
    WindowsDirectoryFindNext,
    PosixDirectoryOpen,
    PosixDirectoryRead,
    WindowsDynamicLibraryLoad,
    WindowsDynamicLibraryClose,
    WindowsDynamicLibraryGetSymbol,
    PosixDynamicLibraryLoad,
    PosixDynamicLibraryClose,
    PosixDynamicLibraryGetSymbol,
    WindowsSymbolNameConversion,
    PosixSymbolNameAssign,
    WindowsDebuggerUnlock,
    WindowsDebuggerDelete,
};

/// @brief Kind of the scalar diagnostic context stored in ResultPlugin.
enum class PluginErrorContextKind : uint16_t
{
    None = 0,
    NativeError,
    ExitCode,
    RequiredBytes,
    RequiredElements,
};

/// @brief A typed scalar payload for a Plugin error. Its interpretation is selected by PluginErrorContextKind.
union PluginErrorContext
{
    struct ExitCode
    {
    };
    struct RequiredBytes
    {
    };
    struct RequiredElements
    {
    };

    uint32_t nativeError;
    int32_t  exitCode;
    uint32_t requiredBytes;
    uint32_t requiredElements;

    constexpr PluginErrorContext(uint32_t value = 0) : nativeError(value) {}
    constexpr PluginErrorContext(ExitCode, int32_t value) : exitCode(value) {}
    constexpr PluginErrorContext(RequiredBytes, uint32_t value) : requiredBytes(value) {}
    constexpr PluginErrorContext(RequiredElements, uint32_t value) : requiredElements(value) {}
};

/// @brief Stable category assigned to errors owned by Plugin.
static constexpr ResultCategory PluginResultCategory = ResultCategory(7);

/// @brief Plugin result retaining a stable operation detail and one scalar diagnostic context.
/// @details The composed Result is authoritative. Plain and foreign conversions clear Plugin-specific context, while
/// same-domain copies retain it. Conversion to Result preserves only the category/error identity.
struct [[nodiscard]] ResultPlugin
{
    Result                 result;
    PluginErrorDetail      detail      = PluginErrorDetail::None;
    PluginErrorContextKind contextKind = PluginErrorContextKind::None;
    PluginErrorContext     context     = {};

    explicit constexpr ResultPlugin(bool valid = true) : result(valid) {}
    constexpr ResultPlugin(PluginError error, PluginErrorDetail detail = PluginErrorDetail::None)
        : result(Result::Error(PluginResultCategory, error)), detail(detail)
    {}
    constexpr ResultPlugin(PluginError error, PluginErrorDetail detail, PluginErrorContextKind contextKind,
                           PluginErrorContext context)
        : result(Result::Error(PluginResultCategory, error)), detail(detail), contextKind(contextKind), context(context)
    {}
    constexpr ResultPlugin(Result result) : result(result) {}

    template <typename ResultLike>
    constexpr ResultPlugin(const ResultLike& other) : result(other.toResult())
    {}

    static constexpr ResultPlugin withNativeError(PluginError error, PluginErrorDetail detail, uint32_t nativeError)
    {
        return {error, detail, PluginErrorContextKind::NativeError, PluginErrorContext(nativeError)};
    }

    static constexpr ResultPlugin withExitCode(PluginError error, PluginErrorDetail detail, int32_t exitCode)
    {
        return {error, detail, PluginErrorContextKind::ExitCode,
                PluginErrorContext(PluginErrorContext::ExitCode{}, exitCode)};
    }

    static constexpr ResultPlugin withRequiredBytes(PluginError error, PluginErrorDetail detail, uint32_t requiredBytes)
    {
        return {error, detail, PluginErrorContextKind::RequiredBytes,
                PluginErrorContext(PluginErrorContext::RequiredBytes{}, requiredBytes)};
    }

    static constexpr ResultPlugin withRequiredElements(PluginError error, PluginErrorDetail detail,
                                                       uint32_t requiredElements)
    {
        return {error, detail, PluginErrorContextKind::RequiredElements,
                PluginErrorContext(PluginErrorContext::RequiredElements{}, requiredElements)};
    }

    explicit constexpr operator bool() const { return static_cast<bool>(result); }
    constexpr          operator Result() const { return result; }
    constexpr Result   toResult() const { return result; }
    constexpr bool     isError(PluginError error) const { return result.isError(PluginResultCategory, error); }
};

//! @}
} // namespace SC
