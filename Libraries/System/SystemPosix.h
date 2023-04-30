// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../Foundation/Result.h"
#include <errno.h> // errno
#include <fcntl.h> // fcntl
namespace SC
{
struct FileDescriptorPosixHelpers;
}
struct SC::FileDescriptorPosixHelpers
{
  private:
    static ReturnCode getFileFlags(int flagRead, const int fileDescriptor, int& outFlags)
    {
        do
        {
            outFlags = ::fcntl(fileDescriptor, flagRead);
        } while (outFlags == -1 and errno == EINTR);
        if (outFlags == -1)
        {
            return "fcntl getFlag failed"_a8;
        }
        return true;
    }
    static ReturnCode setFileFlags(int flagRead, int flagWrite, const int fileDescriptor, const bool setFlag,
                                   const int flag)
    {
        int oldFlags;
        do
        {
            oldFlags = ::fcntl(fileDescriptor, flagRead);
        } while (oldFlags == -1 and errno == EINTR);
        if (oldFlags == -1)
        {
            return "fcntl getFlag failed"_a8;
        }
        const int newFlags = setFlag ? oldFlags | flag : oldFlags & (~flag);
        if (newFlags != oldFlags)
        {
            int res;
            do
            {
                res = ::fcntl(fileDescriptor, flagWrite, newFlags);
            } while (res == -1 and errno == EINTR);
            if (res != 0)
            {
                return "fcntl setFlag failed"_a8;
            }
        }
        return true;
    }

  public:
    template <int flag>
    static ReturnCode hasFileDescriptorFlags(int fileDescriptor, bool& hasFlag)
    {
        static_assert(flag == FD_CLOEXEC, "hasFileDescriptorFlags invalid value");
        int flags = 0;
        SC_TRY_IF(getFileFlags(F_GETFD, fileDescriptor, flags));
        hasFlag = (flags & flag) != 0;
        return true;
    }
    template <int flag>
    static ReturnCode hasFileStatusFlags(int fileDescriptor, bool& hasFlag)
    {
        static_assert(flag == O_NONBLOCK, "hasFileStatusFlags invalid value");
        int flags = 0;
        SC_TRY_IF(getFileFlags(F_GETFL, fileDescriptor, flags));
        hasFlag = (flags & flag) != 0;
        return true;
    }
    template <int flag>
    static ReturnCode setFileDescriptorFlags(int fileDescriptor, bool setFlag)
    {
        // We can OR the allowed flags here to provide some safety
        static_assert(flag == FD_CLOEXEC, "setFileStatusFlags invalid value");
        return setFileFlags(F_GETFD, F_SETFD, fileDescriptor, setFlag, flag);
    }

    template <int flag>
    static ReturnCode setFileStatusFlags(int fileDescriptor, bool setFlag)
    {
        // We can OR the allowed flags here to provide some safety
        static_assert(flag == O_NONBLOCK, "setFileStatusFlags invalid value");
        return setFileFlags(F_GETFL, F_SETFL, fileDescriptor, setFlag, flag);
    }
};
