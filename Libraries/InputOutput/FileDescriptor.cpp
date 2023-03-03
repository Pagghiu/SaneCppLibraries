// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#include "FileDescriptor.h"

#if SC_PLATFORM_WINDOWS
#include "FileDescriptorInternalWindows.inl"
#else
#include "FileDescriptorInternalPosix.inl"
#endif

template <>
template <>
void SC::OpaqueFunctions<SC::FileNativeOpaqueUniqueTaggedHandle>::construct<sizeof(void*)>(uint8_t* buffer)
{
    static_assert(sizeof(void*) >= sizeof(FileNativeOpaqueUniqueTaggedHandle), "Increase size of unique static pimpl");
    new (buffer, PlacementNew()) FileNativeOpaqueUniqueTaggedHandle();
}
template <>
void SC::OpaqueFunctions<SC::FileNativeOpaqueUniqueTaggedHandle>::destruct(FileNativeOpaqueUniqueTaggedHandle& obj)
{
    obj.~FileNativeOpaqueUniqueTaggedHandle();
}
template <>
void SC::OpaqueFunctions<SC::FileNativeOpaqueUniqueTaggedHandle>::moveConstruct(
    uint8_t* buffer, FileNativeOpaqueUniqueTaggedHandle&& obj)
{
    new (buffer, PlacementNew()) FileNativeOpaqueUniqueTaggedHandle(forward<FileNativeOpaqueUniqueTaggedHandle>(obj));
}
template <>
void SC::OpaqueFunctions<SC::FileNativeOpaqueUniqueTaggedHandle>::moveAssign(FileNativeOpaqueUniqueTaggedHandle&  pthis,
                                                                             FileNativeOpaqueUniqueTaggedHandle&& obj)
{
    pthis = forward<FileNativeOpaqueUniqueTaggedHandle>(obj);
}

void           SC::FileDescriptor::detach() { fileNativeHandle.get().detach(); }
bool           SC::FileDescriptor::isValid() const { return fileNativeHandle.get().isValid(); }
SC::ReturnCode SC::FileDescriptor::close() { return fileNativeHandle.get().close(); }
SC::ReturnCode SC::FileDescriptor::assignMovingFrom(FileDescriptor& other)
{
    return fileNativeHandle.get().assignMovingFrom(other.fileNativeHandle.get());
}
