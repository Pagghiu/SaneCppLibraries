// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "FileDescriptor.h"

#include "../Foundation/Opaque.h"

namespace SC
{
using FileNativeDescriptor = int;
ReturnCode FileNativeDescriptorClose(FileNativeDescriptor&);
struct FileNativeOpaqueUniqueTaggedHandle
    : public OpaqueUniqueTaggedHandle<FileNativeDescriptor, -1, ReturnCode, FileNativeDescriptorClose>
{
};
} // namespace SC
