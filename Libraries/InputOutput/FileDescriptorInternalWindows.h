// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "FileDescriptor.h"

#include "../Foundation/MovableHandle.h"

#include <Windows.h>

namespace SC
{
using FileNativeDescriptor = HANDLE;
ReturnCode FileNativeDescriptorClose(FileNativeDescriptor&);
struct FileNativeMovableHandle
    : public MovableHandle<FileNativeDescriptor, INVALID_HANDLE_VALUE, ReturnCode, FileNativeDescriptorClose>
{
};
} // namespace SC
