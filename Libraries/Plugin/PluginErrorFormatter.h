// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once

#include "../Common/ResultErrorFormatter.h"
#include "PluginError.h"

namespace SC
{
namespace detail
{
inline bool appendPluginErrorDetail(ResultErrorFormatter& formatter, PluginErrorDetail detail)
{
    switch (detail)
    {
    case PluginErrorDetail::None: return true;
    case PluginErrorDetail::PathAssign: formatter.append("assign path"); break;
    case PluginErrorDetail::PathAppend: formatter.append("append path"); break;
    case PluginErrorDetail::MetadataName: formatter.append("parse metadata name"); break;
    case PluginErrorDetail::MetadataVersion: formatter.append("parse metadata version"); break;
    case PluginErrorDetail::MetadataDescription: formatter.append("parse metadata description"); break;
    case PluginErrorDetail::MetadataCategory: formatter.append("parse metadata category"); break;
    case PluginErrorDetail::MetadataDependency: formatter.append("parse metadata dependency"); break;
    case PluginErrorDetail::MetadataBuildOption: formatter.append("parse metadata build option"); break;
    case PluginErrorDetail::ScannerDefinitionStorage: formatter.append("store scanner definition"); break;
    case PluginErrorDetail::ScannerCandidatePath: formatter.append("build scanner candidate path"); break;
    case PluginErrorDetail::ScannerCandidateFile: formatter.append("store scanner candidate file"); break;
    case PluginErrorDetail::IteratorAssignDirectory: formatter.append("assign directory path"); break;
    case PluginErrorDetail::IteratorBuildSearchPath: formatter.append("build directory search path"); break;
    case PluginErrorDetail::IteratorBuildEntryPath: formatter.append("build directory entry path"); break;
    case PluginErrorDetail::CompilerFindConfiguration: formatter.append("find compiler configuration"); break;
    case PluginErrorDetail::CompilerBuildArguments: formatter.append("build compiler arguments"); break;
    case PluginErrorDetail::CompilerWriteArguments: formatter.append("write compiler arguments"); break;
    case PluginErrorDetail::CompilerBuildObjectPath: formatter.append("build object path"); break;
    case PluginErrorDetail::LinkerBuildArguments: formatter.append("build linker arguments"); break;
    case PluginErrorDetail::LinkerWriteArguments: formatter.append("write linker arguments"); break;
    case PluginErrorDetail::LinkerBuildOutputPath: formatter.append("build dynamic library path"); break;
    case PluginErrorDetail::SysrootFindVersion: formatter.append("find sysroot version"); break;
    case PluginErrorDetail::SysrootBuildIncludePath: formatter.append("build sysroot include path"); break;
    case PluginErrorDetail::SysrootBuildLibraryPath: formatter.append("build sysroot library path"); break;
    case PluginErrorDetail::RegistryUnloadList: formatter.append("build registry unload list"); break;
    case PluginErrorDetail::RegistryStoreDefinition: formatter.append("store registry definition"); break;
    case PluginErrorDetail::RegistryFindPlugin: formatter.append("find registry plugin"); break;
    case PluginErrorDetail::WindowsFileCreate: formatter.append("Windows create file"); break;
    case PluginErrorDetail::WindowsFileGetSize: formatter.append("Windows get file size"); break;
    case PluginErrorDetail::WindowsFileRead: formatter.append("Windows read file"); break;
    case PluginErrorDetail::WindowsFileDelete: formatter.append("Windows delete file"); break;
    case PluginErrorDetail::PosixFileOpen: formatter.append("POSIX open file"); break;
    case PluginErrorDetail::PosixFileStat: formatter.append("POSIX get file size"); break;
    case PluginErrorDetail::PosixFileRead: formatter.append("POSIX read file"); break;
    case PluginErrorDetail::PosixFileRemove: formatter.append("POSIX remove file"); break;
    case PluginErrorDetail::WindowsDirectoryFindFirst: formatter.append("Windows find first directory entry"); break;
    case PluginErrorDetail::WindowsDirectoryFindNext: formatter.append("Windows find next directory entry"); break;
    case PluginErrorDetail::PosixDirectoryOpen: formatter.append("POSIX open directory"); break;
    case PluginErrorDetail::PosixDirectoryRead: formatter.append("POSIX read directory"); break;
    case PluginErrorDetail::WindowsDynamicLibraryLoad: formatter.append("Windows load dynamic library"); break;
    case PluginErrorDetail::WindowsDynamicLibraryClose: formatter.append("Windows close dynamic library"); break;
    case PluginErrorDetail::WindowsDynamicLibraryGetSymbol:
        formatter.append("Windows get dynamic library symbol");
        break;
    case PluginErrorDetail::PosixDynamicLibraryLoad: formatter.append("POSIX load dynamic library"); break;
    case PluginErrorDetail::PosixDynamicLibraryClose: formatter.append("POSIX close dynamic library"); break;
    case PluginErrorDetail::PosixDynamicLibraryGetSymbol: formatter.append("POSIX get dynamic library symbol"); break;
    case PluginErrorDetail::WindowsSymbolNameConversion: formatter.append("Windows convert symbol name"); break;
    case PluginErrorDetail::PosixSymbolNameAssign: formatter.append("POSIX assign symbol name"); break;
    case PluginErrorDetail::WindowsDebuggerUnlock: formatter.append("Windows unlock debugger file"); break;
    case PluginErrorDetail::WindowsDebuggerDelete: formatter.append("Windows delete debugger file"); break;
    default: return false;
    }
    return true;
}

inline bool appendPluginError(ResultErrorFormatter& formatter, PluginError error)
{
    switch (error)
    {
    case PluginError::PathNotNullTerminated: formatter.append("Path is not null terminated"); break;
    case PluginError::UnsupportedPathEncoding: formatter.append("Path encoding is not supported"); break;
    case PluginError::InvalidDefinition: formatter.append("Plugin definition is invalid"); break;
    case PluginError::DynamicLibraryOperationUnsupported:
        formatter.append("Dynamic library operation is unsupported");
        break;
    case PluginError::PathCapacityExceeded: formatter.append("Path capacity exceeded"); break;
    case PluginError::DefinitionCapacityExceeded: formatter.append("Plugin definition capacity exceeded"); break;
    case PluginError::ScannerCapacityExceeded: formatter.append("Plugin scanner capacity exceeded"); break;
    case PluginError::RegistryCapacityExceeded: formatter.append("Plugin registry capacity exceeded"); break;
    case PluginError::CompilerConfigurationCapacityExceeded:
        formatter.append("Compiler configuration capacity exceeded");
        break;
    case PluginError::SysrootCapacityExceeded: formatter.append("Sysroot capacity exceeded"); break;
    case PluginError::ArgumentCapacityExceeded: formatter.append("Argument capacity exceeded"); break;
    case PluginError::FileOpenFailed: formatter.append("Failed to open file"); break;
    case PluginError::FileSizeQueryFailed: formatter.append("Failed to query file size"); break;
    case PluginError::FileBufferCapacityExceeded: formatter.append("File buffer capacity exceeded"); break;
    case PluginError::FileReadFailed: formatter.append("Failed to read file"); break;
    case PluginError::FileReadIncomplete: formatter.append("File read incomplete"); break;
    case PluginError::FileRemoveFailed: formatter.append("Failed to remove file"); break;
    case PluginError::DirectoryOpenFailed: formatter.append("Failed to open directory"); break;
    case PluginError::DirectoryReadFailed: formatter.append("Failed to read directory"); break;
    case PluginError::CompilerNotFound: formatter.append("Compiler not found"); break;
    case PluginError::SysrootNotFound: formatter.append("Sysroot not found"); break;
    case PluginError::CompilerEnvironmentReadFailed: formatter.append("Failed to read compiler environment"); break;
    case PluginError::CompilerExitedWithFailure: formatter.append("Compiler exited unsuccessfully"); break;
    case PluginError::LinkerExitedWithFailure: formatter.append("Linker exited unsuccessfully"); break;
    case PluginError::DynamicLibraryAlreadyLoaded: formatter.append("Dynamic library is already loaded"); break;
    case PluginError::DynamicLibraryLoadFailed: formatter.append("Failed to load dynamic library"); break;
    case PluginError::DynamicLibraryCloseFailed: formatter.append("Failed to close dynamic library"); break;
    case PluginError::DynamicLibraryNotLoaded: formatter.append("Dynamic library is not loaded"); break;
    case PluginError::SymbolNameConversionFailed: formatter.append("Failed to convert symbol name"); break;
    case PluginError::SymbolNameCapacityExceeded: formatter.append("Symbol name capacity exceeded"); break;
    case PluginError::SymbolNotFound: formatter.append("Symbol not found"); break;
    case PluginError::InitializationEntryPointNotFound:
        formatter.append("Plugin initialization entry point not found");
        break;
    case PluginError::CloseEntryPointNotFound: formatter.append("Plugin close entry point not found"); break;
    case PluginError::PluginNotFound: formatter.append("Plugin not found"); break;
    case PluginError::PluginInitializationFailed: formatter.append("Plugin initialization failed"); break;
    case PluginError::PluginShutdownFailed: formatter.append("Plugin shutdown failed"); break;
    case PluginError::DebuggerUnlockFailed: formatter.append("Failed to unlock debugger file"); break;
    case PluginError::ReloadUnsupported: formatter.append("Plugin reload is unsupported by this runtime"); break;
    default: return false;
    }
    return true;
}

inline ResultErrorFormat formatPluginErrorWithContext(PluginError error, PluginErrorDetail detail,
                                                      PluginErrorContextKind contextKind, PluginErrorContext context,
                                                      Span<char> output)
{
    ResultErrorFormatter formatter(output);
    if (not appendPluginError(formatter, error))
        return ResultErrorFormatter::failure(ResultErrorFormatStatus::UnknownError, output);
    if (detail != PluginErrorDetail::None)
    {
        formatter.append(" (detail: ");
        if (not appendPluginErrorDetail(formatter, detail))
            return ResultErrorFormatter::failure(ResultErrorFormatStatus::UnknownError, output);
        formatter.append(")");
    }
    switch (contextKind)
    {
    case PluginErrorContextKind::None: break;
    case PluginErrorContextKind::NativeError:
        formatter.append(" (native error: ");
        formatter.append(static_cast<uint64_t>(context.nativeError));
        formatter.append(")");
        break;
    case PluginErrorContextKind::ExitCode:
        formatter.append(" (exit code: ");
        if (context.exitCode < 0)
        {
            formatter.append("-");
            formatter.append(static_cast<uint64_t>(-static_cast<int64_t>(context.exitCode)));
        }
        else
            formatter.append(static_cast<uint64_t>(context.exitCode));
        formatter.append(")");
        break;
    case PluginErrorContextKind::RequiredBytes:
        formatter.append(" (required capacity: ");
        formatter.append(static_cast<uint64_t>(context.requiredBytes));
        formatter.append(" bytes)");
        break;
    case PluginErrorContextKind::RequiredElements:
        formatter.append(" (required capacity: ");
        formatter.append(static_cast<uint64_t>(context.requiredElements));
        formatter.append(" elements)");
        break;
    default: return ResultErrorFormatter::failure(ResultErrorFormatStatus::UnknownError, output);
    }
    return formatter.finish();
}
} // namespace detail

inline ResultErrorFormat formatPluginError(PluginError error, Span<char> output)
{
    return detail::formatPluginErrorWithContext(error, PluginErrorDetail::None, PluginErrorContextKind::None, {},
                                                output);
}

inline ResultErrorFormat formatPluginError(Result result, Span<char> output)
{
    if (result)
        return ResultErrorFormatter::failure(ResultErrorFormatStatus::NotAnError, output);
    if (result.category() != PluginResultCategory)
        return ResultErrorFormatter::failure(ResultErrorFormatStatus::ForeignCategory, output);
    return formatPluginError(static_cast<PluginError>(result.errorValue()), output);
}

inline ResultErrorFormat formatPluginError(ResultPlugin result, Span<char> output)
{
    if (result)
        return ResultErrorFormatter::failure(ResultErrorFormatStatus::NotAnError, output);
    if (result.result.category() != PluginResultCategory)
        return ResultErrorFormatter::failure(ResultErrorFormatStatus::ForeignCategory, output);
    return detail::formatPluginErrorWithContext(static_cast<PluginError>(result.result.errorValue()), result.detail,
                                                result.contextKind, result.context, output);
}
} // namespace SC
