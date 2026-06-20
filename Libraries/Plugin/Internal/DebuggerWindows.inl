// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../../Common/Deferred.h"
#include "../../Common/Result.h"
#include "PluginString.h"

namespace SC
{
struct Debugger;
}
/// @brief Checks debugger status and unlocking / deleting locked pdb files (used by [Plugin](@ref library_plugin))
struct SC::Debugger
{
    // Support deleting locked PDB files

    /// @brief Check if debugger is connected
    /// @return `true` if a debugger is connected to current process
    /// @note This is only supported on windows for now
    [[nodiscard]] static bool isDebuggerConnected();

    /// @brief Unlocks a file from other OS process.
    /// @param fileName The file to unlock
    /// @return Valid Result if file has been successfully unlocked
    /// @note This is only supported on windows for now
    [[nodiscard]] static Result unlockFileFromAllProcesses(StringSpan fileName);

    /// @brief Forcefully deletes a file previously unlocked by Debugger::unlockFileFromAllProcesses
    /// @param fileName The file to delete
    /// @return Valid Result if file has been successfully deleted
    /// @note This is only supported on windows for now
    [[nodiscard]] static Result deleteForcefullyUnlockedFile(StringSpan fileName);

  private:
    struct Internal;
};

#include <Windows.h>

#include <RestartManager.h>
#include <winternl.h>

#if SC_COMPILER_MSVC || SC_COMPILER_CLANG_CL
#pragma comment(lib, "ntdll.lib")
#pragma comment(lib, "Rstrtmgr.lib")
#endif

#if defined(__MINGW32__)
typedef SYSTEM_HANDLE_ENTRY SYSTEM_HANDLE, *PSYSTEM_HANDLE;

[[nodiscard]] static constexpr ULONG systemHandleCount(const SYSTEM_HANDLE_INFORMATION& handleInfo)
{
    return handleInfo.Count;
}

[[nodiscard]] static constexpr DWORD systemHandleProcessId(const SYSTEM_HANDLE& handle) { return handle.OwnerPid; }

[[nodiscard]] static constexpr ACCESS_MASK systemHandleGrantedAccess(const SYSTEM_HANDLE& handle)
{
    return handle.AccessMask;
}

[[nodiscard]] static constexpr USHORT systemHandleValue(const SYSTEM_HANDLE& handle) { return handle.HandleValue; }

[[nodiscard]] static constexpr NTSTATUS statusInfoLengthMismatchValue()
{
#if defined(STATUS_INFO_LENGTH_MISMATCH)
    return STATUS_INFO_LENGTH_MISMATCH;
#else
    return static_cast<NTSTATUS>(0xC0000004);
#endif
}
#else
typedef struct _SYSTEM_HANDLE
{
    ULONG       ProcessId;
    BYTE        ObjectTypeNumber;
    BYTE        Flags;
    USHORT      Handle;
    PVOID       Object;
    ACCESS_MASK GrantedAccess;
} SYSTEM_HANDLE, *PSYSTEM_HANDLE;

typedef struct _SYSTEM_HANDLE_INFORMATION
{
    ULONG         HandleCount;
    SYSTEM_HANDLE Handles[1];
} SYSTEM_HANDLE_INFORMATION, *PSYSTEM_HANDLE_INFORMATION;

constexpr NTSTATUS                 STATUS_INFO_LENGTH_MISMATCH = 0xc0000004;
constexpr SYSTEM_INFORMATION_CLASS SystemHandleInformation     = (SYSTEM_INFORMATION_CLASS)0x10;
constexpr OBJECT_INFORMATION_CLASS ObjectNameInformation       = (OBJECT_INFORMATION_CLASS)1;

typedef struct _OBJECT_NAME_INFORMATION
{
    UNICODE_STRING Name;
} OBJECT_NAME_INFORMATION, *POBJECT_NAME_INFORMATION;

[[nodiscard]] static constexpr ULONG systemHandleCount(const SYSTEM_HANDLE_INFORMATION& handleInfo)
{
    return handleInfo.HandleCount;
}

[[nodiscard]] static constexpr DWORD systemHandleProcessId(const SYSTEM_HANDLE& handle) { return handle.ProcessId; }

[[nodiscard]] static constexpr ACCESS_MASK systemHandleGrantedAccess(const SYSTEM_HANDLE& handle)
{
    return handle.GrantedAccess;
}

[[nodiscard]] static constexpr USHORT systemHandleValue(const SYSTEM_HANDLE& handle) { return handle.Handle; }

[[nodiscard]] static constexpr NTSTATUS statusInfoLengthMismatchValue() { return STATUS_INFO_LENGTH_MISMATCH; }
#endif
#if SC_PLATFORM_WINDOWS && SC_CONFIGURATION_DEBUG && (SC_COMPILER_MSVC || SC_COMPILER_CLANG_CL)
#pragma warning(push)
#pragma warning(disable : 4566) // malloc_dbg macro uses __FILE__ instead of a wide version
#endif

struct SC::Debugger::Internal
{
    // Loop all system handles and remotely close file handles inside given process that end with file name
    [[nodiscard]] static bool unlockFileFromProcess(SC::StringSpan theFile, DWORD processId)
    {
        StringSpan theFileDirectory = PluginString::withoutWindowsRoot(theFile);
        void*      nameMemory       = ::malloc(USHRT_MAX * sizeof(WCHAR));
        auto       deleteMemory     = MakeDeferred([&] { ::free(nameMemory); });

        Span<WCHAR> nameBuffer = {static_cast<WCHAR*>(nameMemory), USHRT_MAX};

        //! [DeferredSnippet]
        HANDLE processHandle = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_DUP_HANDLE, FALSE, processId);
        if (processHandle == nullptr)
        {
            return false;
        }
        auto deferDeleteProcessHandle = SC::MakeDeferred(
            // Function (or lambda) that will be invoked at the end of scope
            [&]
            {
                CloseHandle(processHandle);
                processHandle = nullptr;
            });
        // Use processHandle that will be disposed at end of scope by the Deferred

        // ...
        // Deferred can be disarmed, meaning that the dispose function will not be executed
        // deferDeleteProcessHandle.disarm();
        //! [DeferredSnippet]

        DWORD handleCount;
        BOOL  error = GetProcessHandleCount(processHandle, &handleCount);
        if (error == 0)
        {
            return false;
        }

        ULONG                      handleInfoSize = 0x100000;
        PSYSTEM_HANDLE_INFORMATION handleInfo = reinterpret_cast<PSYSTEM_HANDLE_INFORMATION>(::malloc(handleInfoSize));

        auto deferDeleteHandleInfo = SC::MakeDeferred(
            [&]
            {
                free(handleInfo);
                handleInfo = nullptr;
            });
        while (NtQuerySystemInformation(SystemHandleInformation, handleInfo, handleInfoSize, NULL) ==
               statusInfoLengthMismatchValue())
        {
            handleInfoSize *= 2;
            void* newMemory = ::realloc(handleInfo, handleInfoSize);
            if (newMemory == nullptr)
                return false;
            handleInfo = (PSYSTEM_HANDLE_INFORMATION)newMemory;
        }

        const HANDLE currentProcess = GetCurrentProcess();
        for (DWORD handleIdx = 0; handleIdx < systemHandleCount(*handleInfo); ++handleIdx)
        {
            HANDLE dupHandle;
#if defined(__MINGW32__)
            SYSTEM_HANDLE handle = handleInfo->Handle[handleIdx];
#else
            SYSTEM_HANDLE handle = handleInfo->Handles[handleIdx];
#endif
            if (systemHandleProcessId(handle) != processId)
                continue;

            // Skip handles that will block NtDuplicateObject
            if ((systemHandleGrantedAccess(handle) == 0x00120189) or
                (systemHandleGrantedAccess(handle) == 0x00100000) or
                (systemHandleGrantedAccess(handle) == 0x0012019f) or (systemHandleGrantedAccess(handle) == 0x001a019f))
            {
                continue;
            }
            BOOL res = DuplicateHandle(processHandle, (HANDLE)(ULONG_PTR)systemHandleValue(handle), currentProcess,
                                       &dupHandle, 0, FALSE, DUPLICATE_SAME_ACCESS);
            if (res == FALSE)
            {
                continue;
            }
            auto deferDeleteDupHandle = SC::MakeDeferred(
                [&]
                {
                    CloseHandle(dupHandle);
                    dupHandle = nullptr;
                });

            // Get the required buffer size
            ULONG    bufferSize = 0;
            NTSTATUS status     = NtQueryObject(dupHandle, ObjectNameInformation, nullptr, 0, &bufferSize);
            if (status != statusInfoLengthMismatchValue())
            {
                continue;
            }

            // Allocate memory for the object name information
            POBJECT_NAME_INFORMATION pObjectNameInfo =
                reinterpret_cast<POBJECT_NAME_INFORMATION>(LocalAlloc(LPTR, bufferSize));
            if (!pObjectNameInfo)
            {
                continue;
            }
            auto deferFreeObjectNameInfo = SC::MakeDeferred(
                [&]
                {
                    LocalFree(pObjectNameInfo);
                    pObjectNameInfo = nullptr;
                });

            // Retrieve the object name information
            status = NtQueryObject(dupHandle, ObjectNameInformation, pObjectNameInfo, bufferSize, nullptr);
            if (status != 0)
            {
                continue;
            }

            const size_t nameLengthInBytes = pObjectNameInfo->Name.Length;
            if (nameLengthInBytes == 0)
                continue;
            wcsncpy_s(nameBuffer.data(), nameBuffer.sizeInElements(), pObjectNameInfo->Name.Buffer,
                      nameLengthInBytes / sizeof(WCHAR));
            nameBuffer[nameLengthInBytes / sizeof(WCHAR)] = L'\0';

            StringSpan handleName = StringSpan({nameBuffer.data(), nameLengthInBytes / sizeof(WCHAR)}, true);
            // theFile          is something like              Y:\MyDir\Sub.pdb
            // theFileDirectory is something like                 MyDir\Sub.pdb
            // handleName       is something like \Device\Mup\Mac\MyDir\Sub.pdb
            if (PluginString::pathEndsWith(handleName, theFileDirectory))
            {
                CloseHandle(dupHandle);
                deferDeleteDupHandle.disarm();
                dupHandle = INVALID_HANDLE_VALUE;
                HANDLE newHandle;
                if (DuplicateHandle(processHandle, reinterpret_cast<HANDLE>(systemHandleValue(handle)), currentProcess,
                                    &newHandle, 0, FALSE, DUPLICATE_CLOSE_SOURCE | DUPLICATE_SAME_ACCESS))
                {
                    CloseHandle(newHandle);
                    return Result(true);
                }
            }
        }
        return false;
    }
};
#if SC_PLATFORM_WINDOWS && SC_CONFIGURATION_DEBUG && (SC_COMPILER_MSVC || SC_COMPILER_CLANG_CL)
#pragma warning(pop)
#endif

// Find all processes that have an handle open on the given fileName and unlock it
// https://devblogs.microsoft.com/oldnewthing/20120217-00/?p=8283
SC::Result SC::Debugger::unlockFileFromAllProcesses(SC::StringSpan fileName)
{
    using namespace SC;
    SC_TRY_MSG(fileName.isNullTerminated(), "Filename must be null terminated");
    SC_TRY_MSG(fileName.getEncoding() == SC::StringEncoding::Utf16, "Filename must be UTF16");
    DWORD dwSession;
    WCHAR szSessionKey[CCH_RM_SESSION_KEY + 1] = {0};

    DWORD dwError = RmStartSession(&dwSession, 0, szSessionKey);

    if (dwError == ERROR_SUCCESS)
    {
        const wchar_t* pszFile = fileName.getNullTerminatedNative();
        dwError                = RmRegisterResources(dwSession, 1, &pszFile, 0, NULL, 0, NULL);
        if (dwError == ERROR_SUCCESS)
        {
            DWORD dwReason;
            UINT  nProcInfoNeeded;
            UINT  nProcInfo = 10;

            RM_PROCESS_INFO rmProcessInfo[10];
            memset(rmProcessInfo, 0, sizeof(rmProcessInfo));
            dwError = RmGetList(dwSession, &nProcInfoNeeded, &nProcInfo, rmProcessInfo, &dwReason);
            if (dwError == ERROR_SUCCESS)
            {
                for (UINT i = 0; i < nProcInfo; i++)
                {
                    HANDLE hProcess =
                        OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, rmProcessInfo[i].Process.dwProcessId);
                    if (hProcess)
                    {
                        FILETIME ftCreate, ftExit, ftKernel, ftUser;
                        if (GetProcessTimes(hProcess, &ftCreate, &ftExit, &ftKernel, &ftUser) &&
                            CompareFileTime(&rmProcessInfo[i].Process.ProcessStartTime, &ftCreate) == 0)
                        {
                            SC_TRY(Internal::unlockFileFromProcess(fileName, rmProcessInfo[i].Process.dwProcessId));
                        }
                        CloseHandle(hProcess);
                    }
                }
            }
        }
        RmEndSession(dwSession);
    }
    return Result(true);
}

bool SC::Debugger::isDebuggerConnected() { return ::IsDebuggerPresent() == TRUE; }

SC::Result SC::Debugger::deleteForcefullyUnlockedFile(SC::StringSpan fileName)
{
    using namespace SC;
    SC_TRY_MSG(fileName.isNullTerminated(), "Filename must be null terminated");
    SC_TRY_MSG(fileName.getEncoding() == SC::StringEncoding::Utf16, "Filename must be UTF16");
    HANDLE fd = CreateFileW(fileName.getNullTerminatedNative(), // File path
                            GENERIC_READ | GENERIC_WRITE,       // Desired access
                            FILE_SHARE_DELETE,                  // Share mode (0 for exclusive access)
                            NULL,                               // Security attributes
                            OPEN_EXISTING,                      // Creation disposition
                            FILE_FLAG_DELETE_ON_CLOSE,          // File attributes and flags
                            NULL                                // Template file handle
    );
    if (fd == INVALID_HANDLE_VALUE)
        return Result::Error("deleteForcefullyUnlockedFile CreateFileW failed");
    return Result(::CloseHandle(fd) == TRUE);
}
