// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
struct SC_FILE_BASIC_INFORMATION
{
    LARGE_INTEGER CreationTime;
    LARGE_INTEGER LastAccessTime;
    LARGE_INTEGER LastWriteTime;
    LARGE_INTEGER ChangeTime;
    DWORD         FileAttributes;
};

struct SC_FILE_COMPLETION_INFORMATION
{
    HANDLE Port;
    PVOID  Key;
};
#ifndef _NTDEF_
typedef _Return_type_success_(return >= 0) LONG NTSTATUS;
typedef NTSTATUS* PNTSTATUS;
#endif
struct SC_IO_STATUS_BLOCK
{
    union
    {
        NTSTATUS Status;
        PVOID    Pointer;
    };
    ULONG_PTR Information;
};

enum SC_FILE_INFORMATION_CLASS
{
    FileReplaceCompletionInformation = 0x3D
};

typedef NTSTATUS(NTAPI* SC_NtSetInformationFile)(HANDLE fileHandle, struct SC_IO_STATUS_BLOCK* ioStatusBlock,
                                                 void* fileInformation, ULONG length,
                                                 enum SC_FILE_INFORMATION_CLASS fileInformationClass);

static constexpr NTSTATUS STATUS_SUCCESS = 0;
