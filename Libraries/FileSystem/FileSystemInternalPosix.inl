// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#include "FileSystem.h"
// clang-format off
#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h> // mkdir
#include <unistd.h> // rmdir
#include <string.h> //strerror_r
#include <math.h> // round
#include <fcntl.h> // AT_FDCWD
#if __APPLE__
#include <copyfile.h>
#include <sys/attr.h>
#include <sys/clonefile.h>
#include <removefile.h>
#endif
// clang-format on

namespace SC
{
static constexpr SC::ReturnCode getErrorCode(int errorCode)
{
    switch (errorCode)
    {
    case EACCES: return ReturnCode::Error("EACCES");
    case EDQUOT: return ReturnCode::Error("EDQUOT");
    case EEXIST: return ReturnCode::Error("EEXIST");
    case EFAULT: return ReturnCode::Error("EFAULT");
    case EIO: return ReturnCode::Error("EIO");
    case ELOOP: return ReturnCode::Error("ELOOP");
    case EMLINK: return ReturnCode::Error("EMLINK");
    case ENAMETOOLONG: return ReturnCode::Error("ENAMETOOLONG");
    case ENOENT: return ReturnCode::Error("ENOENT");
    case ENOSPC: return ReturnCode::Error("ENOSPC");
    case ENOTDIR: return ReturnCode::Error("ENOTDIR");
    case EROFS: return ReturnCode::Error("EROFS");
    case EBADF: return ReturnCode::Error("EBADF");
    case EPERM: return ReturnCode::Error("EPERM");
    case ENOMEM: return ReturnCode::Error("ENOMEM");
    case ENOTSUP: return ReturnCode::Error("ENOTSUP");
    case EINVAL: return ReturnCode::Error("EINVAL");
    }
    return ReturnCode::Error("Unknown");
}
} // namespace SC
struct SC::FileSystem::Internal
{

#define SC_TRY_LIBC(func)                                                                                              \
    {                                                                                                                  \
                                                                                                                       \
        if (func != 0)                                                                                                 \
        {                                                                                                              \
            return false;                                                                                              \
        }                                                                                                              \
    }

    [[nodiscard]] static bool makeDirectory(const char* dir)
    {
        SC_TRY_LIBC(mkdir(dir, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH));
        return true;
    }

    [[nodiscard]] static bool exists(const char* path)
    {
        struct stat path_stat;
        return stat(path, &path_stat) == 0;
    }

    [[nodiscard]] static bool existsAndIsDirectory(const char* path)
    {
        struct stat path_stat;
        SC_TRY_LIBC(stat(path, &path_stat));
        return S_ISDIR(path_stat.st_mode);
    }

    [[nodiscard]] static bool existsAndIsFile(const char* path)
    {
        struct stat path_stat;
        SC_TRY_LIBC(stat(path, &path_stat));
        return S_ISREG(path_stat.st_mode) || S_ISLNK(path_stat.st_mode);
    }

    [[nodiscard]] static bool removeEmptyDirectory(const char* path) { return rmdir(path) == 0; }

    [[nodiscard]] static bool removeFile(const char* path) { return remove(path) == 0; }

    [[nodiscard]] static bool openFileRead(const char* path, FILE*& file)
    {
        file = fopen(path, "rb");
        return file != nullptr;
    }

    [[nodiscard]] static bool openFileWrite(const char* path, FILE*& file)
    {
        file = fopen(path, "wb");
        return file != nullptr;
    }

    [[nodiscard]] static bool formatError(int errorNumber, String& buffer)
    {
        buffer.encoding = StringEncoding::Utf8;
        SC_TRY(buffer.data.resizeWithoutInitializing(buffer.data.capacity()));
        const int res = strerror_r(errorNumber, buffer.nativeWritableBytesIncludingTerminator(),
                                   buffer.sizeInBytesIncludingTerminator());
        if (res == 0)
        {
            return buffer.data.resizeWithoutInitializing(strlen(buffer.nativeWritableBytesIncludingTerminator()) + 1);
        }
        SC_TRUST_RESULT(buffer.data.resizeWithoutInitializing(0));
        return false;
    }
#if __APPLE__
    // TODO: We should add a version of copyfile/clonefile that uses the file descriptors already opened by the file
    // walker
    [[nodiscard]] static bool copyFile(const StringView& source, const StringView& destination,
                                       FileSystem::CopyFlags options, bool isDirectory = false)
    {
        const char* sourceFile      = source.getNullTerminatedNative();
        const char* destinationFile = destination.getNullTerminatedNative();

        // Try clonefile and fallback to copyfile in case it fails with ENOTSUP
        // https://www.manpagez.com/man/2/clonefile/
        // https://www.manpagez.com/man/3/copyfile/
        if (options.useCloneIfSupported)
        {
            int cloneRes = clonefile(sourceFile, destinationFile, CLONE_NOFOLLOW | CLONE_NOOWNERCOPY);
            if (cloneRes != 0)
            {
                if ((errno == EEXIST) and options.overwrite)
                {
                    // TODO: We should probably renaming instead of deleting...and eventually rollback on failure
                    if (isDirectory)
                    {
                        removefile_state_t state = removefile_state_alloc();
                        int                res   = removefile(destinationFile, state, REMOVEFILE_RECURSIVE);
                        removefile_state_free(state);
                        if (res != 0)
                        {
                            return false;
                        }
                    }
                    else
                    {
                        remove(destinationFile);
                    }
                    cloneRes = clonefile(sourceFile, destinationFile, CLONE_NOFOLLOW | CLONE_NOOWNERCOPY);
                }
            }
            if (cloneRes == 0)
            {
                return true;
            }
            else if (errno != ENOTSUP)
            {
                // We only fallback in case of ENOTSUP
                return false;
            }
        }
        copyfile_state_t state;
        state          = copyfile_state_alloc();
        uint32_t flags = COPYFILE_CLONE_FORCE; // Note: clone in copyfile is best effort
        if (options.overwrite)
        {
            flags |= COPYFILE_UNLINK;
        }
        if (isDirectory)
        {
            flags |= COPYFILE_RECURSIVE;
        }
        // TODO: Should define flags to decide if to follow symlinks on source and destination
        const int copyRes = copyfile(sourceFile, destinationFile, state, flags);
        copyfile_state_free(state);
        return copyRes == 0;
    }

    [[nodiscard]] static bool copyDirectory(String& sourceFile, String& destinationFile, FileSystem::CopyFlags options)
    {
        return copyFile(sourceFile.view(), destinationFile.view(), options, true); // true == isDirectory
    }

    static bool removeDirectoryRecursive(String& directory)
    {
        removefile_state_t state = removefile_state_alloc();
        int                res   = removefile(directory.view().getNullTerminatedNative(), state, REMOVEFILE_RECURSIVE);
        removefile_state_free(state);
        return res == 0;
    }
#else
    [[nodiscard]] static bool copyFile(const char* file1, const char* file2)
    {
        // On Linux we should probably use sendfile, splice or copy_file_Range
        return false;
    }
#endif

    [[nodiscard]] static ReturnCode getFileTime(const char* file, FileTime& time)
    {
        struct stat st;
        if (::stat(file, &st) == 0)
        {
            time.modifiedTime = AbsoluteTime(
                static_cast<int64_t>(::round(st.st_mtimespec.tv_nsec / 1.0e6) + st.st_mtimespec.tv_sec * 1000));
            return ReturnCode(true);
        }
        return ReturnCode(false);
    }

    [[nodiscard]] static ReturnCode setLastModifiedTime(const char* file, AbsoluteTime time)
    {
        struct timespec times[2];
        times[0].tv_sec  = time.getMillisecondsSinceEpoch() / 1000;
        times[0].tv_nsec = time.getMillisecondsSinceEpoch() * 1000 * 1000;
        times[1]         = times[0];

        if (::utimensat(AT_FDCWD, file, times, 0) == 0)
        {
            return ReturnCode(true);
        }
        return ReturnCode(false);
    }

#undef SC_TRY_LIBC
};
