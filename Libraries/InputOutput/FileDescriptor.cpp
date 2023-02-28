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
void SC::CompilerFirewallFuncs<SC::FileNativeMovableHandle>::construct<sizeof(void*)>(uint8_t* buffer)
{
    static_assert(sizeof(void*) >= sizeof(FileNativeMovableHandle), "Increase size of unique static pimpl");
    new (buffer, PlacementNew()) FileNativeMovableHandle();
}
template <>
void SC::CompilerFirewallFuncs<SC::FileNativeMovableHandle>::destruct(FileNativeMovableHandle& obj)
{
    obj.~FileNativeMovableHandle();
}
template <>
void SC::CompilerFirewallFuncs<SC::FileNativeMovableHandle>::moveConstruct(uint8_t*                  buffer,
                                                                           FileNativeMovableHandle&& obj)
{
    new (buffer, PlacementNew()) FileNativeMovableHandle(forward<FileNativeMovableHandle>(obj));
}
template <>
void SC::CompilerFirewallFuncs<SC::FileNativeMovableHandle>::moveAssign(FileNativeMovableHandle&  pthis,
                                                                        FileNativeMovableHandle&& obj)
{
    pthis = forward<FileNativeMovableHandle>(obj);
}

void           SC::FileDescriptor::detach() { fileNativeHandle.get().detach(); }
bool           SC::FileDescriptor::isValid() const { return fileNativeHandle.get().isValid(); }
SC::ReturnCode SC::FileDescriptor::close() { return fileNativeHandle.get().close(); }
SC::ReturnCode SC::FileDescriptor::assignMovingFrom(FileDescriptor& other)
{
    return fileNativeHandle.get().assignMovingFrom(other.fileNativeHandle.get());
}
