// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
struct FILE_BASIC_INFORMATION
{
    LARGE_INTEGER CreationTime;
    LARGE_INTEGER LastAccessTime;
    LARGE_INTEGER LastWriteTime;
    LARGE_INTEGER ChangeTime;
    DWORD         FileAttributes;
};

struct FILE_COMPLETION_INFORMATION
{
    HANDLE Port;
    PVOID  Key;
};

struct IO_STATUS_BLOCK
{
    union
    {
        NTSTATUS Status;
        PVOID    Pointer;
    };
    ULONG_PTR Information;
};

enum FILE_INFORMATION_CLASS
{
    FileReplaceCompletionInformation = 0x3D
};

typedef NTSTATUS(NTAPI* NTSetInformationFile)(HANDLE fileHandle, struct IO_STATUS_BLOCK* ioStatusBlock,
                                              void* fileInformation, ULONG length,
                                              enum FILE_INFORMATION_CLASS fileInformationClass);

static constexpr NTSTATUS STATUS_SUCCESS = 0;