// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#include "../FileSystem/Path.h"
#include "../Foundation/Containers/SmallVector.h"
#include "../Foundation/Strings/StringBuilder.h"
#include "../Foundation/Strings/StringConverter.h"
#include "../Threading/Atomic.h"
#include "System.h"

#include <Windows.h>
#pragma comment(lib, "Ws2_32.lib")

#include <RestartManager.h>
#include <winternl.h>

#pragma comment(lib, "ntdll.lib")
#pragma comment(lib, "Rstrtmgr.lib")

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

struct SC::SystemDebug::Internal
{
    // Loop all system handles and remotely close file handles inside given process that end with file name
    [[nodiscard]] static bool unlockFileFromProcess(SC::StringView theFile, DWORD processId)
    {
        SC::Vector<WCHAR> nameBuffer;
        SC_TRY(nameBuffer.resizeWithoutInitializing(USHRT_MAX));

        if (theFile.startsWithChar('\\'))
        {
            theFile = theFile.sliceStart(1); // Eat one slash
        }
        HANDLE processHandle = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_DUP_HANDLE, FALSE, processId);
        if (processHandle == nullptr)
        {
            return false;
        }
        auto deferDeleteProcessHandle = SC::MakeDeferred(
            [&]
            {
                CloseHandle(processHandle);
                processHandle = nullptr;
            });

        DWORD handleCount;
        BOOL  error = GetProcessHandleCount(processHandle, &handleCount);
        if (error == 0)
        {
            return false;
        }

        ULONG                      handleInfoSize = 0x100000;
        PSYSTEM_HANDLE_INFORMATION handleInfo = reinterpret_cast<PSYSTEM_HANDLE_INFORMATION>(malloc(handleInfoSize));

        auto deferDeleteHandleInfo = SC::MakeDeferred(
            [&]
            {
                free(handleInfo);
                handleInfo = nullptr;
            });
        while (NtQuerySystemInformation(SystemHandleInformation, handleInfo, handleInfoSize, NULL) ==
               STATUS_INFO_LENGTH_MISMATCH)
        {
            handleInfoSize *= 2;
            void* newMemory = realloc(handleInfo, handleInfoSize);
            if (newMemory == nullptr)
                return false;
            handleInfo = (PSYSTEM_HANDLE_INFORMATION)newMemory;
        }

        const HANDLE currentProcess = GetCurrentProcess();
        for (DWORD handleIdx = 0; handleIdx < handleInfo->HandleCount; ++handleIdx)
        {
            HANDLE        dupHandle;
            SYSTEM_HANDLE handle = handleInfo->Handles[handleIdx];
            if (handle.ProcessId != processId)
                continue;

            // Skip handles that will block NtDuplicateObject
            if ((handle.GrantedAccess == 0x00120189) or (handle.GrantedAccess == 0x00100000) or
                (handle.GrantedAccess == 0x0012019f) or (handle.GrantedAccess == 0x001a019f))
            {
                continue;
            }
            BOOL res = DuplicateHandle(processHandle, (HANDLE)(ULONG_PTR)handle.Handle, currentProcess, &dupHandle, 0,
                                       FALSE, DUPLICATE_SAME_ACCESS);
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
            if (status != STATUS_INFO_LENGTH_MISMATCH)
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
            wcsncpy_s(nameBuffer.data(), nameBuffer.size(), pObjectNameInfo->Name.Buffer,
                      nameLengthInBytes / sizeof(WCHAR));
            nameBuffer[nameLengthInBytes / sizeof(WCHAR)] = L'\0';

            StringView handleName = StringView(nameBuffer.data(), nameLengthInBytes, true);
            if (handleName.endsWith(theFile))
            {
                CloseHandle(dupHandle);
                deferDeleteDupHandle.disarm();
                dupHandle = INVALID_HANDLE_VALUE;
                HANDLE newHandle;
                if (DuplicateHandle(processHandle, reinterpret_cast<HANDLE>(handle.Handle), currentProcess, &newHandle,
                                    0, FALSE, DUPLICATE_CLOSE_SOURCE | DUPLICATE_SAME_ACCESS))
                {
                    CloseHandle(newHandle);
                    return true;
                }
            }
        }
        return false;
    }
};

// Find all processes that have an handle open on the given fileName and unlock it
// https://devblogs.microsoft.com/oldnewthing/20120217-00/?p=8283
SC::ReturnCode SC::SystemDebug::unlockFileFromAllProcesses(SC::StringView fileName)
{
    using namespace SC;
    SC_TRY_MSG(fileName.isNullTerminated(), "Filename must be null terminated"_a8);
    SC_TRY_MSG(fileName.getEncoding() == SC::StringEncoding::Utf16, "Filename must be UTF16"_a8);
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

            RM_PROCESS_INFO rgpi[10];
            dwError = RmGetList(dwSession, &nProcInfoNeeded, &nProcInfo, rgpi, &dwReason);
            if (dwError == ERROR_SUCCESS)
            {
                for (UINT i = 0; i < nProcInfo; i++)
                {
                    HANDLE hProcess =
                        OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, rgpi[i].Process.dwProcessId);
                    if (hProcess)
                    {
                        FILETIME ftCreate, ftExit, ftKernel, ftUser;
                        if (GetProcessTimes(hProcess, &ftCreate, &ftExit, &ftKernel, &ftUser) &&
                            CompareFileTime(&rgpi[i].Process.ProcessStartTime, &ftCreate) == 0)
                        {
                            SC_TRY(Internal::unlockFileFromProcess(fileName, rgpi[i].Process.dwProcessId));
                        }
                        CloseHandle(hProcess);
                    }
                }
            }
        }
        RmEndSession(dwSession);
    }
    return true;
}

bool SC::SystemDebug::isDebuggerConnected() { return ::IsDebuggerPresent() == TRUE; }

SC::ReturnCode SC::SystemDebug::deleteForcefullyUnlockedFile(SC::StringView fileName)
{
    using namespace SC;
    SC_TRY_MSG(fileName.isNullTerminated(), "Filename must be null terminated"_a8);
    SC_TRY_MSG(fileName.getEncoding() == SC::StringEncoding::Utf16, "Filename must be UTF16"_a8);
    HANDLE fd = CreateFileW(fileName.getNullTerminatedNative(), // File path
                            GENERIC_READ | GENERIC_WRITE,       // Desired access
                            FILE_SHARE_DELETE,                  // Share mode (0 for exclusive access)
                            NULL,                               // Security attributes
                            OPEN_EXISTING,                      // Creation disposition
                            FILE_FLAG_DELETE_ON_CLOSE,          // File attributes and flags
                            NULL                                // Template file handle
    );
    SC_TRY_MSG(fd != INVALID_HANDLE_VALUE, "deleteForcefullyUnlockedFile CreateFileW failed"_a8);
    return ::CloseHandle(fd) == TRUE;
}

SC::ReturnCode SC::SystemDynamicLibraryTraits::releaseHandle(Handle& handle)
{
    if (handle)
    {
        static_assert(sizeof(HMODULE) == sizeof(Handle), "sizeof(HMODULE)");
        static_assert(alignof(HMODULE) == alignof(Handle), "alignof(HMODULE)");
        HMODULE module;
        memcpy(&module, &handle, sizeof(HMODULE));
        handle         = nullptr;
        const BOOL res = ::FreeLibrary(module);
        return res == TRUE;
    }
    return true;
}

SC::ReturnCode SC::SystemDynamicLibrary::load(StringView fullPath)
{
    SC_TRY(close());
    SmallString<1024> string = StringEncoding::Native;
    StringConverter   converter(string);
    StringView        fullPathZeroTerminated;
    SC_TRY(converter.convertNullTerminateFastPath(fullPath, fullPathZeroTerminated));
    HMODULE module = ::LoadLibraryW(fullPathZeroTerminated.getNullTerminatedNative());
    if (module == nullptr)
    {
        return "LoadLibraryW failed"_a8;
    }
    memcpy(&handle, &module, sizeof(HMODULE));
    return true;
}

SC::ReturnCode SC::SystemDynamicLibrary::loadSymbol(StringView symbolName, void*& symbol) const
{
    SC_TRY_MSG(isValid(), "Invalid GetProcAddress handle"_a8);
    SmallString<1024> string = StringEncoding::Ascii;
    StringConverter   converter(string);
    StringView        symbolZeroTerminated;
    SC_TRY(converter.convertNullTerminateFastPath(symbolName, symbolZeroTerminated));
    HMODULE module;
    memcpy(&module, &handle, sizeof(HMODULE));
    symbol = reinterpret_cast<void*>(::GetProcAddress(module, symbolZeroTerminated.bytesIncludingTerminator()));
    return symbol != nullptr;
}

bool SC::SystemDirectories::init()
{
    // TODO: OsPaths::init() for Windows is messy. Tune the API to improve writing software like this.
    // Reason is because it's handy counting in wchars but we can't do it with StringNative.
    // Additionally we must convert to utf8 at the end otherwise path::dirname will not work
    SmallVector<wchar_t, MAX_PATH> buffer;

    size_t numChars;
    int    tries = 0;
    do
    {
        SC_TRY(buffer.resizeWithoutInitializing(buffer.size() + MAX_PATH));
        // Is returned null terminated
        numChars = GetModuleFileNameW(0L, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (tries++ >= 10)
        {
            return false;
        }
    } while (numChars == buffer.size() && GetLastError() == ERROR_INSUFFICIENT_BUFFER);

    SC_TRY(buffer.resizeWithoutInitializing(numChars + 1));
    SC_TRY(buffer[numChars] == 0);

    StringView utf16executable =
        StringView(Span<const wchar_t>(buffer.data(), (buffer.size() - 1) * sizeof(wchar_t)), true);

    // TODO: SystemDirectories::init - We must also convert to utf8 because dirname will not work on non utf8 or ascii
    // text assigning directly the SmallString inside StringNative will copy as is instad of converting utf16 to utf8
    executableFile = ""_u8;
    StringBuilder builder(executableFile);
    SC_TRY(builder.append(utf16executable));
    applicationRootDirectory = Path::Windows::dirname(executableFile.view());
    return true;
}

struct SC::SystemFunctions::Internal
{
    Atomic<bool> networkingInited = false;

    static Internal& get()
    {
        static Internal internal;
        return internal;
    }
};

bool SC::SystemFunctions::isNetworkingInited() { return Internal::get().networkingInited.load(); }

SC::ReturnCode SC::SystemFunctions::initNetworking()
{
    if (isNetworkingInited() == false)
    {
        WSADATA wsa;
        if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
        {
            return "WSAStartup failed"_a8;
        }
        Internal::get().networkingInited.exchange(true);
    }
    return true;
}

SC::ReturnCode SC::SystemFunctions::shutdownNetworking()
{
    WSACleanup();
    Internal::get().networkingInited.exchange(false);
    return true;
}
