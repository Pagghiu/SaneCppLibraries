// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "FileDescriptor.h"

#include "../Foundation/Opaque.h"

#include <Windows.h>

namespace SC
{
using FileNativeDescriptor = HANDLE;
ReturnCode FileNativeDescriptorClose(FileNativeDescriptor&);
struct FileNativeOpaqueUniqueTaggedHandle
    : public OpaqueUniqueTaggedHandle<FileNativeDescriptor, INVALID_HANDLE_VALUE, ReturnCode, FileNativeDescriptorClose>
{
};
} // namespace SC
