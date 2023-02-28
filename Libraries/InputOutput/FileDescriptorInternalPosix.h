// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "FileDescriptor.h"

#include "../Foundation/MovableHandle.h"

namespace SC
{
using FileNativeDescriptor = int;
ReturnCode FileNativeDescriptorClose(const FileNativeDescriptor&);
struct FileNativeMovableHandle : public MovableHandle<FileNativeDescriptor, -1, ReturnCode, FileNativeDescriptorClose>
{
};
} // namespace SC
