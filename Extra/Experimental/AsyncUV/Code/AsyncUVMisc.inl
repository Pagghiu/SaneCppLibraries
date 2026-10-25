// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "AsyncUV.h"

#include "../../../../Libraries/Threading/Threading.h"

#if SC_PLATFORM_WINDOWS
#include <direct.h>
#else
#include <dlfcn.h>
#include <grp.h>
#include <netdb.h>
#if SC_PLATFORM_APPLE
#include <mach-o/dyld.h>
#endif
#include <pwd.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <sys/utsname.h>
#include <time.h>
#include <unistd.h>
#endif

#include <stdlib.h>

#if !SC_PLATFORM_WINDOWS
extern char** environ;
#endif

namespace SC
{
namespace
{
int copyUvString(const char* source, char* buffer, size_t* size)
{
    if (source == nullptr or buffer == nullptr or size == nullptr or *size == 0)
    {
        return UV_EINVAL;
    }

    const size_t length = strlen(source);
    if (*size <= length)
    {
        *size = length + 1;
        return UV_ENOBUFS;
    }

    memcpy(buffer, source, length + 1);
    *size = length;
    return 0;
}

void fillRandomBytes(void* buffer, size_t length)
{
    if (buffer == nullptr or length == 0)
    {
        return;
    }

    static uint64_t seed = 0x9e3779b97f4a7c15ULL;

    uint64_t state = seed ^ uv_hrtime() ^ reinterpret_cast<uintptr_t>(buffer) ^ length;
    seed           = state + 0x9e3779b97f4a7c15ULL;

    uint8_t* bytes = reinterpret_cast<uint8_t*>(buffer);
    for (size_t idx = 0; idx < length; ++idx)
    {
        state ^= state << 13;
        state ^= state >> 7;
        state ^= state << 17;

        uint8_t value = static_cast<uint8_t>(state >> 56);
        if (value == 0)
        {
            value = static_cast<uint8_t>(idx + 1);
        }
        bytes[idx] = value;
    }
}

void trimTrailingSeparator(char* path)
{
    size_t length = strlen(path);
    while (length > 1)
    {
#if SC_PLATFORM_WINDOWS
        if (length == 3 and path[1] == ':')
        {
            return;
        }
        if (path[length - 1] != '\\' and path[length - 1] != '/')
#else
        if (path[length - 1] != '/')
#endif
        {
            return;
        }
        path[length - 1] = '\0';
        length--;
    }
}

int copyEnvPath(const char* name, char* buffer, size_t* size)
{
#if SC_PLATFORM_WINDOWS
    char   storage[4096];
    DWORD  length = GetEnvironmentVariableA(name, storage, sizeof(storage));
    size_t copiedLength;
    if (length == 0)
    {
        return UV_ENOENT;
    }
    if (length >= sizeof(storage))
    {
        return UV_ENOBUFS;
    }
    trimTrailingSeparator(storage);
    copiedLength = strlen(storage);
    if (copiedLength == 0)
    {
        return UV_ENOENT;
    }
    return copyUvString(storage, buffer, size);
#else
    const char* value = getenv(name);
    if (value == nullptr or value[0] == '\0')
    {
        return UV_ENOENT;
    }
    char storage[4096];
    if (strlen(value) >= sizeof(storage))
    {
        return UV_ENOBUFS;
    }
    memcpy(storage, value, strlen(value) + 1);
    trimTrailingSeparator(storage);
    return copyUvString(storage, buffer, size);
#endif
}

int systemErrorToUv(int sys_errno)
{
    if (sys_errno <= 0)
    {
        return sys_errno;
    }
#if SC_PLATFORM_WINDOWS
    switch (sys_errno)
    {
    case ERROR_ACCESS_DENIED:
    case ERROR_ELEVATION_REQUIRED: return UV_EACCES;
    case ERROR_NOACCESS: return UV_EFAULT;
    case ERROR_BAD_PIPE: return UV_EPIPE;
    case WSAEADDRINUSE: return UV_EADDRINUSE;
    case WSAECONNABORTED: return UV_ECONNABORTED;
    case WSAECONNREFUSED: return UV_ECONNREFUSED;
    case WSAECONNRESET: return UV_ECONNRESET;
    case WSAEHOSTUNREACH: return UV_EHOSTUNREACH;
    case WSAEMSGSIZE: return UV_EMSGSIZE;
    case WSAENETUNREACH: return UV_ENETUNREACH;
    default: return -sys_errno;
    }
#else
    return -sys_errno;
#endif
}

const char* errorName(int err)
{
    switch (err)
    {
    case 0: return "Success";
    case UV_E2BIG: return "argument list too long";
    case UV_EACCES: return "permission denied";
    case UV_EADDRINUSE: return "address already in use";
    case UV_EADDRNOTAVAIL: return "address not available";
    case UV_EAFNOSUPPORT: return "address family not supported";
    case UV_EALREADY: return "connection already in progress";
    case UV_EBADF: return "bad file descriptor";
    case UV_EBUSY: return "resource busy or locked";
    case UV_ECANCELED: return "operation canceled";
    case UV_ECONNABORTED: return "software caused connection abort";
    case UV_ECONNREFUSED: return "connection refused";
    case UV_ECONNRESET: return "connection reset by peer";
    case UV_EFAULT: return "bad address";
    case UV_EHOSTUNREACH: return "host is unreachable";
    case UV_EINVAL: return "invalid argument";
    case UV_EMSGSIZE: return "message too long";
    case UV_ENETUNREACH: return "network is unreachable";
    case UV_ENOBUFS: return "no buffer space available";
    case UV_ENODEV: return "no such device";
    case UV_ENOMEM: return "not enough memory";
    case UV_ENOENT: return "no such file or directory";
    case UV_ENOEXEC: return "exec format error";
    case UV_ENOSPC: return "no space left on device";
    case UV_ENOSYS: return "function not implemented";
    case UV_ENOTCONN: return "socket is not connected";
    case UV_ENOTSUP: return "operation not supported on socket";
    case UV_EPERM: return "operation not permitted";
    case UV_EPIPE: return "broken pipe";
    case UV_ERANGE: return "result too large";
    case UV_ETIMEDOUT: return "connection timed out";
    case UV_EOF: return "end of file";
    default: return "Unknown error";
    }
}

const char* errorCodeName(int err)
{
    switch (err)
    {
    case 0: return "Unknown system error 0";
    case UV_E2BIG: return "E2BIG";
    case UV_EACCES: return "EACCES";
    case UV_EADDRINUSE: return "EADDRINUSE";
    case UV_EADDRNOTAVAIL: return "EADDRNOTAVAIL";
    case UV_EAFNOSUPPORT: return "EAFNOSUPPORT";
    case UV_EALREADY: return "EALREADY";
    case UV_EBADF: return "EBADF";
    case UV_EBUSY: return "EBUSY";
    case UV_ECANCELED: return "ECANCELED";
    case UV_ECONNABORTED: return "ECONNABORTED";
    case UV_ECONNREFUSED: return "ECONNREFUSED";
    case UV_ECONNRESET: return "ECONNRESET";
    case UV_EFAULT: return "EFAULT";
    case UV_EHOSTUNREACH: return "EHOSTUNREACH";
    case UV_EINVAL: return "EINVAL";
    case UV_EMSGSIZE: return "EMSGSIZE";
    case UV_ENETUNREACH: return "ENETUNREACH";
    case UV_ENOBUFS: return "ENOBUFS";
    case UV_ENODEV: return "ENODEV";
    case UV_ENOMEM: return "ENOMEM";
    case UV_ENOENT: return "ENOENT";
    case UV_ENOEXEC: return "ENOEXEC";
    case UV_ENOSPC: return "ENOSPC";
    case UV_ENOSYS: return "ENOSYS";
    case UV_ENOTCONN: return "ENOTCONN";
    case UV_ENOTSUP: return "ENOTSUP";
    case UV_EPERM: return "EPERM";
    case UV_EPIPE: return "EPIPE";
    case UV_ERANGE: return "ERANGE";
    case UV_ETIMEDOUT: return "ETIMEDOUT";
    case UV_EOF: return "EOF";
    default: return "Unknown system error";
    }
}

char* duplicateString(const char* source)
{
    if (source == nullptr)
    {
        source = "";
    }
    const size_t length = strlen(source);
    char*        copy   = static_cast<char*>(malloc(length + 1));
    if (copy == nullptr)
    {
        return nullptr;
    }
    memcpy(copy, source, length + 1);
    return copy;
}

void clearPasswd(uv_passwd_t* pwd)
{
    pwd->username = nullptr;
    pwd->uid      = static_cast<unsigned long>(-1);
    pwd->gid      = static_cast<unsigned long>(-1);
    pwd->shell    = nullptr;
    pwd->homedir  = nullptr;
}

void clearGroup(uv_group_t* grp)
{
    grp->groupname = nullptr;
    grp->gid       = static_cast<unsigned long>(-1);
    grp->members   = nullptr;
}

void copyUnameField(char* destination, const char* source)
{
    if (source == nullptr)
    {
        destination[0] = '\0';
        return;
    }
    snprintf(destination, 256, "%s", source);
}

int copyTruncatedUvString(const char* source, char* buffer, size_t* size)
{
    if (source == nullptr or buffer == nullptr or size == nullptr or *size == 0)
    {
        return UV_EINVAL;
    }
    const size_t capacity = *size;
    const size_t length   = strlen(source);
    size_t       copied   = capacity - 1;
    if (copied > length)
    {
        copied = length;
    }
    if (copied > 0)
    {
        memcpy(buffer, source, copied);
    }
    buffer[copied] = '\0';
    *size          = copied;
    return 0;
}

#if SC_PLATFORM_WINDOWS
DWORD processIdFromUv(uv_pid_t pid) { return pid == 0 ? GetCurrentProcessId() : static_cast<DWORD>(pid); }

HANDLE openPriorityProcess(uv_pid_t pid, DWORD access)
{
    if (pid == 0 or static_cast<DWORD>(pid) == GetCurrentProcessId())
    {
        return GetCurrentProcess();
    }
    return OpenProcess(access, FALSE, processIdFromUv(pid));
}

int priorityClassToUv(DWORD priorityClass)
{
    switch (priorityClass)
    {
    case IDLE_PRIORITY_CLASS: return UV_PRIORITY_LOW;
    case BELOW_NORMAL_PRIORITY_CLASS: return UV_PRIORITY_BELOW_NORMAL;
    case NORMAL_PRIORITY_CLASS: return UV_PRIORITY_NORMAL;
    case ABOVE_NORMAL_PRIORITY_CLASS: return UV_PRIORITY_ABOVE_NORMAL;
    case HIGH_PRIORITY_CLASS: return UV_PRIORITY_HIGH;
    case REALTIME_PRIORITY_CLASS: return UV_PRIORITY_HIGHEST;
    default: return UV_PRIORITY_NORMAL;
    }
}

DWORD uvPriorityToClass(int priority)
{
    if (priority < UV_PRIORITY_HIGH)
    {
        return HIGH_PRIORITY_CLASS;
    }
    if (priority < UV_PRIORITY_ABOVE_NORMAL)
    {
        return HIGH_PRIORITY_CLASS;
    }
    if (priority < UV_PRIORITY_NORMAL)
    {
        return ABOVE_NORMAL_PRIORITY_CLASS;
    }
    if (priority < UV_PRIORITY_BELOW_NORMAL)
    {
        return NORMAL_PRIORITY_CLASS;
    }
    if (priority < UV_PRIORITY_LOW)
    {
        return BELOW_NORMAL_PRIORITY_CLASS;
    }
    return IDLE_PRIORITY_CLASS;
}
#endif
} // namespace

char       processTitle[512] = "SCAsyncUVTest";
SC::Mutex& processTitleMutex()
{
    static SC::Mutex mutex;
    return mutex;
}
} // namespace SC

UV_API const char* uv_err_name(int err) { return SC::errorCodeName(err); }

UV_API const char* uv_strerror(int err) { return SC::errorName(err); }

UV_API char* uv_strerror_r(int err, char* buf, size_t buflen)
{
    if (buf == nullptr or buflen == 0)
    {
        return buf;
    }
    if (SC::errorName(err)[0] != 'U')
    {
        snprintf(buf, buflen, "%s", SC::errorName(err));
    }
    else
    {
        snprintf(buf, buflen, "Unknown error %d", err);
    }
    return buf;
}

UV_API int uv_translate_sys_error(int sys_errno) { return SC::systemErrorToUv(sys_errno); }

UV_API int uv_exepath(char* buffer, size_t* size)
{
    if (buffer == nullptr or size == nullptr or *size == 0)
    {
        return UV_EINVAL;
    }
    char storage[4096];
#if SC_PLATFORM_WINDOWS
    DWORD length = GetModuleFileNameA(nullptr, storage, sizeof(storage));
    if (length == 0 or length >= sizeof(storage))
    {
        return UV_EINVAL;
    }
    storage[length] = '\0';
#elif SC_PLATFORM_APPLE
    uint32_t capacity = sizeof(storage);
    if (_NSGetExecutablePath(storage, &capacity) != 0)
    {
        return UV_ENOBUFS;
    }
#else
    ssize_t length = readlink("/proc/self/exe", storage, sizeof(storage) - 1);
    if (length < 0)
    {
        return SC::systemErrorToUv(errno);
    }
    storage[length] = '\0';
#endif
    return SC::copyTruncatedUvString(storage, buffer, size);
}

UV_API int uv_dlopen(const char* filename, uv_lib_t* lib)
{
    if (filename == nullptr or lib == nullptr)
    {
        return -1;
    }
    lib->handle = nullptr;
    lib->errmsg = nullptr;
#if SC_PLATFORM_WINDOWS
    HMODULE handle = LoadLibraryA(filename);
    if (handle != nullptr)
    {
        lib->handle = handle;
        return 0;
    }
    char message[512];
    snprintf(message, sizeof(message), "%s: error %lu", filename, static_cast<unsigned long>(GetLastError()));
    lib->errmsg = SC::duplicateString(message);
    return -1;
#else
    dlerror();
    void* handle = dlopen(filename, RTLD_LAZY);
    if (handle != nullptr)
    {
        lib->handle = handle;
        return 0;
    }
    const char* error = dlerror();
    if (error != nullptr)
    {
        lib->errmsg = SC::duplicateString(error);
    }
    else
    {
        lib->errmsg = SC::duplicateString(filename);
    }
    return -1;
#endif
}

UV_API void uv_dlclose(uv_lib_t* lib)
{
    if (lib == nullptr)
    {
        return;
    }
    if (lib->handle != nullptr)
    {
#if SC_PLATFORM_WINDOWS
        FreeLibrary(static_cast<HMODULE>(lib->handle));
#else
        dlclose(lib->handle);
#endif
    }
    free(lib->errmsg);
    lib->handle = nullptr;
    lib->errmsg = nullptr;
}

UV_API const char* uv_dlerror(const uv_lib_t* lib)
{
    if (lib == nullptr or lib->errmsg == nullptr)
    {
        return "no error";
    }
    return lib->errmsg;
}

UV_API int uv_set_process_title(const char* title)
{
    if (title == nullptr)
    {
        return UV_EINVAL;
    }

    SC::Mutex& mutex = SC::processTitleMutex();
    mutex.lock();
    const size_t length = strlen(title);
    size_t       copied = sizeof(SC::processTitle) - 1;
    if (copied > length)
    {
        copied = length;
    }
    if (copied > 0)
    {
        memcpy(SC::processTitle, title, copied);
    }
    SC::processTitle[copied] = '\0';
    mutex.unlock();
    return 0;
}

UV_API int uv_get_process_title(char* buffer, size_t size)
{
    if (buffer == nullptr or size == 0)
    {
        return UV_EINVAL;
    }

    SC::Mutex& mutex = SC::processTitleMutex();
    mutex.lock();
    const size_t length = strlen(SC::processTitle);
    if (size <= length)
    {
        mutex.unlock();
        return UV_ENOBUFS;
    }
    memcpy(buffer, SC::processTitle, length + 1);
    mutex.unlock();
    return 0;
}

UV_API int uv_ip4_name(const struct sockaddr_in* src, char* dst, size_t size)
{
    return uv_inet_ntop(AF_INET, &src->sin_addr, dst, size) == 0 ? 0 : UV_EINVAL;
}

UV_API int uv_ip6_name(const struct sockaddr_in6* src, char* dst, size_t size)
{
    return uv_inet_ntop(AF_INET6, &src->sin6_addr, dst, size) == 0 ? 0 : UV_EINVAL;
}

UV_API int uv_ip_name(const struct sockaddr* src, char* dst, size_t size)
{
    if (src->sa_family == AF_INET)
    {
        return uv_ip4_name(reinterpret_cast<const struct sockaddr_in*>(src), dst, size);
    }
    if (src->sa_family == AF_INET6)
    {
        return uv_ip6_name(reinterpret_cast<const struct sockaddr_in6*>(src), dst, size);
    }
    return UV_EAFNOSUPPORT;
}

UV_API int uv_gettimeofday(uv_timeval64_t* tv)
{
    if (tv == nullptr)
    {
        return UV_EINVAL;
    }
#if SC_PLATFORM_WINDOWS
    FILETIME fileTime;
    GetSystemTimeAsFileTime(&fileTime);
    uint64_t ticks = (static_cast<uint64_t>(fileTime.dwHighDateTime) << 32) | fileTime.dwLowDateTime;
    ticks -= 116444736000000000ULL;
    tv->tv_sec  = static_cast<int64_t>(ticks / 10000000ULL);
    tv->tv_usec = static_cast<int32_t>((ticks % 10000000ULL) / 10ULL);
#else
    timeval value;
    if (gettimeofday(&value, nullptr) != 0)
    {
        return SC::systemErrorToUv(errno);
    }
    tv->tv_sec  = static_cast<int64_t>(value.tv_sec);
    tv->tv_usec = static_cast<int32_t>(value.tv_usec);
#endif
    return 0;
}

UV_API int uv_clock_gettime(uv_clock_id clock_id, uv_timespec64_t* ts)
{
    if (ts == nullptr)
    {
        return UV_EFAULT;
    }
    if (clock_id != UV_CLOCK_MONOTONIC and clock_id != UV_CLOCK_REALTIME)
    {
        return UV_EINVAL;
    }
#if SC_PLATFORM_WINDOWS
    if (clock_id == UV_CLOCK_MONOTONIC)
    {
        const uint64_t nanoseconds = uv_hrtime();
        ts->tv_sec                 = static_cast<int64_t>(nanoseconds / 1000000000ULL);
        ts->tv_nsec                = static_cast<int32_t>(nanoseconds % 1000000000ULL);
    }
    else
    {
        uv_timeval64_t tv;
        int            result = uv_gettimeofday(&tv);
        if (result != 0)
        {
            return result;
        }
        ts->tv_sec  = tv.tv_sec;
        ts->tv_nsec = tv.tv_usec * 1000;
    }
#else
    timespec        value;
    const clockid_t id = clock_id == UV_CLOCK_MONOTONIC ? CLOCK_MONOTONIC : CLOCK_REALTIME;
    if (clock_gettime(id, &value) != 0)
    {
        return SC::systemErrorToUv(errno);
    }
    ts->tv_sec  = static_cast<int64_t>(value.tv_sec);
    ts->tv_nsec = static_cast<int32_t>(value.tv_nsec);
#endif
    return 0;
}

UV_API int uv_cwd(char* buffer, size_t* size)
{
    if (buffer == nullptr or size == nullptr or *size == 0)
    {
        return UV_EINVAL;
    }
    char storage[4096];
#if SC_PLATFORM_WINDOWS
    if (_getcwd(storage, sizeof(storage)) == nullptr)
#else
    if (getcwd(storage, sizeof(storage)) == nullptr)
#endif
    {
        return SC::systemErrorToUv(errno);
    }
    return SC::copyUvString(storage, buffer, size);
}

UV_API int uv_chdir(const char* dir)
{
    if (dir == nullptr)
    {
        return UV_EINVAL;
    }
#if SC_PLATFORM_WINDOWS
    return _chdir(dir) == 0 ? 0 : SC::systemErrorToUv(errno);
#else
    return chdir(dir) == 0 ? 0 : SC::systemErrorToUv(errno);
#endif
}

UV_API int uv_os_homedir(char* buffer, size_t* size)
{
#if SC_PLATFORM_WINDOWS
    return SC::copyEnvPath("USERPROFILE", buffer, size);
#else
    return SC::copyEnvPath("HOME", buffer, size);
#endif
}

UV_API int uv_os_tmpdir(char* buffer, size_t* size)
{
#if SC_PLATFORM_WINDOWS
    DWORD tmpLength = GetEnvironmentVariableA("TMP", nullptr, 0);
    if (tmpLength > 0)
    {
        return SC::copyEnvPath("TMP", buffer, size);
    }
    if (GetLastError() == ERROR_SUCCESS)
    {
        return UV_ENOENT;
    }
    return SC::copyEnvPath("TEMP", buffer, size);
#else
    const int envResult = SC::copyEnvPath("TMPDIR", buffer, size);
    if (envResult != UV_ENOENT)
    {
        return envResult;
    }
    return SC::copyUvString("/tmp", buffer, size);
#endif
}

UV_API int uv_os_gethostname(char* buffer, size_t* size)
{
    if (buffer == nullptr or size == nullptr or *size == 0)
    {
        return UV_EINVAL;
    }
    char storage[UV_MAXHOSTNAMESIZE];
#if SC_PLATFORM_WINDOWS
    DWORD length = sizeof(storage);
    if (not GetComputerNameA(storage, &length))
    {
        return UV_EINVAL;
    }
    storage[length] = '\0';
#else
    if (gethostname(storage, sizeof(storage)) != 0)
    {
        return SC::systemErrorToUv(errno);
    }
    storage[sizeof(storage) - 1] = '\0';
#endif
    return SC::copyUvString(storage, buffer, size);
}

UV_API int uv_resident_set_memory(size_t* rss)
{
    if (rss == nullptr)
    {
        return UV_EINVAL;
    }
#if SC_PLATFORM_WINDOWS
    *rss = 0;
    return 0;
#else
    struct rusage usage = {};
    if (getrusage(RUSAGE_SELF, &usage) != 0)
    {
        return SC::systemErrorToUv(errno);
    }
    *rss = static_cast<size_t>(usage.ru_maxrss);
    return 0;
#endif
}

UV_API int uv_uptime(double* uptime)
{
    if (uptime == nullptr)
    {
        return UV_EINVAL;
    }
#if SC_PLATFORM_WINDOWS
    *uptime = static_cast<double>(GetTickCount64()) / 1000.0;
    return 0;
#else
    timespec ts = {};
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0)
    {
        return SC::systemErrorToUv(errno);
    }
    *uptime = static_cast<double>(ts.tv_sec) + static_cast<double>(ts.tv_nsec) / 1000000000.0;
    return 0;
#endif
}

UV_API int uv_getrusage(uv_rusage_t* rusage)
{
    if (rusage == nullptr)
    {
        return UV_EINVAL;
    }
    memset(rusage, 0, sizeof(*rusage));
#if SC_PLATFORM_WINDOWS
    return 0;
#else
    struct rusage usage = {};
    if (getrusage(RUSAGE_SELF, &usage) != 0)
    {
        return SC::systemErrorToUv(errno);
    }
    rusage->ru_utime.tv_sec  = usage.ru_utime.tv_sec;
    rusage->ru_utime.tv_usec = usage.ru_utime.tv_usec;
    rusage->ru_stime.tv_sec  = usage.ru_stime.tv_sec;
    rusage->ru_stime.tv_usec = usage.ru_stime.tv_usec;
    rusage->ru_maxrss        = static_cast<uint64_t>(usage.ru_maxrss);
    rusage->ru_ixrss         = static_cast<uint64_t>(usage.ru_ixrss);
    rusage->ru_idrss         = static_cast<uint64_t>(usage.ru_idrss);
    rusage->ru_isrss         = static_cast<uint64_t>(usage.ru_isrss);
    rusage->ru_minflt        = static_cast<uint64_t>(usage.ru_minflt);
    rusage->ru_majflt        = static_cast<uint64_t>(usage.ru_majflt);
    rusage->ru_nswap         = static_cast<uint64_t>(usage.ru_nswap);
    rusage->ru_inblock       = static_cast<uint64_t>(usage.ru_inblock);
    rusage->ru_oublock       = static_cast<uint64_t>(usage.ru_oublock);
    rusage->ru_msgsnd        = static_cast<uint64_t>(usage.ru_msgsnd);
    rusage->ru_msgrcv        = static_cast<uint64_t>(usage.ru_msgrcv);
    rusage->ru_nsignals      = static_cast<uint64_t>(usage.ru_nsignals);
    rusage->ru_nvcsw         = static_cast<uint64_t>(usage.ru_nvcsw);
    rusage->ru_nivcsw        = static_cast<uint64_t>(usage.ru_nivcsw);
    return 0;
#endif
}

UV_API int uv_os_setenv(const char* name, const char* value)
{
    if (name == nullptr or value == nullptr)
    {
        return UV_EINVAL;
    }
#if SC_PLATFORM_WINDOWS
    return SetEnvironmentVariableA(name, value) ? 0 : UV_EINVAL;
#else
    return setenv(name, value, 1) == 0 ? 0 : SC::systemErrorToUv(errno);
#endif
}

UV_API int uv_os_getenv(const char* name, char* buffer, size_t* size)
{
    if (name == nullptr or buffer == nullptr or size == nullptr or *size == 0)
    {
        return UV_EINVAL;
    }
#if SC_PLATFORM_WINDOWS
    char  storage[32768];
    DWORD length = GetEnvironmentVariableA(name, storage, sizeof(storage));
    if (length == 0 and GetLastError() == ERROR_ENVVAR_NOT_FOUND)
    {
        return UV_ENOENT;
    }
    if (length >= sizeof(storage))
    {
        *size = length + 1;
        return UV_ENOBUFS;
    }
    return SC::copyUvString(storage, buffer, size);
#else
    const char* value = getenv(name);
    if (value == nullptr)
    {
        return UV_ENOENT;
    }
    return SC::copyUvString(value, buffer, size);
#endif
}

UV_API int uv_os_unsetenv(const char* name)
{
    if (name == nullptr)
    {
        return UV_EINVAL;
    }
#if SC_PLATFORM_WINDOWS
    return SetEnvironmentVariableA(name, nullptr) ? 0 : UV_EINVAL;
#else
    return unsetenv(name) == 0 ? 0 : SC::systemErrorToUv(errno);
#endif
}

UV_API int uv_os_environ(uv_env_item_t** envitems, int* count)
{
    if (envitems == nullptr or count == nullptr)
    {
        return UV_EINVAL;
    }

#if SC_PLATFORM_WINDOWS
    *envitems = nullptr;
    *count    = 0;
    return UV_ENOSYS;
#else
    int numberOfItems = 0;
    while (environ[numberOfItems] != nullptr)
    {
        numberOfItems++;
    }

    uv_env_item_t* items = static_cast<uv_env_item_t*>(calloc(static_cast<size_t>(numberOfItems), sizeof(*items)));
    if (items == nullptr)
    {
        return UV_ENOMEM;
    }

    for (int idx = 0; idx < numberOfItems; ++idx)
    {
        const char*  item        = environ[idx];
        const char*  equal       = strchr(item, '=');
        const size_t nameLength  = equal != nullptr ? static_cast<size_t>(equal - item) : strlen(item);
        const char*  value       = equal != nullptr ? equal + 1 : "";
        const size_t valueLength = strlen(value);

        items[idx].name  = static_cast<char*>(malloc(nameLength + 1));
        items[idx].value = static_cast<char*>(malloc(valueLength + 1));
        if (items[idx].name == nullptr or items[idx].value == nullptr)
        {
            uv_os_free_environ(items, numberOfItems);
            return UV_ENOMEM;
        }
        memcpy(items[idx].name, item, nameLength);
        items[idx].name[nameLength] = '\0';
        memcpy(items[idx].value, value, valueLength + 1);
    }

    *envitems = items;
    *count    = numberOfItems;
    return 0;
#endif
}

UV_API void uv_os_free_environ(uv_env_item_t* envitems, int count)
{
    if (envitems == nullptr)
    {
        return;
    }
    for (int idx = 0; idx < count; ++idx)
    {
        free(envitems[idx].name);
        free(envitems[idx].value);
    }
    free(envitems);
}

UV_API int uv_os_get_passwd2(uv_passwd_t* pwd, uv_uid_t uid)
{
#if SC_PLATFORM_WINDOWS
    (void)pwd;
    (void)uid;
    return UV_ENOTSUP;
#else
    if (pwd == nullptr)
    {
        return UV_EINVAL;
    }
    SC::clearPasswd(pwd);

    long bufferSize = sysconf(_SC_GETPW_R_SIZE_MAX);
    if (bufferSize <= 0)
    {
        bufferSize = 16384;
    }
    char* buffer = static_cast<char*>(malloc(static_cast<size_t>(bufferSize)));
    if (buffer == nullptr)
    {
        return UV_ENOMEM;
    }

    passwd  storage = {};
    passwd* result  = nullptr;
    int     err     = getpwuid_r(static_cast<uid_t>(uid), &storage, buffer, static_cast<size_t>(bufferSize), &result);
    if (err != 0 or result == nullptr)
    {
        free(buffer);
        return err == 0 ? UV_ENOENT : SC::systemErrorToUv(err);
    }

    pwd->username = SC::duplicateString(result->pw_name);
    pwd->shell    = SC::duplicateString(result->pw_shell);
    pwd->homedir  = SC::duplicateString(result->pw_dir);
    if (pwd->username == nullptr or pwd->shell == nullptr or pwd->homedir == nullptr)
    {
        free(buffer);
        uv_os_free_passwd(pwd);
        return UV_ENOMEM;
    }
    pwd->uid = static_cast<unsigned long>(result->pw_uid);
    pwd->gid = static_cast<unsigned long>(result->pw_gid);

    free(buffer);
    return 0;
#endif
}

UV_API int uv_os_get_passwd(uv_passwd_t* pwd)
{
#if SC_PLATFORM_WINDOWS
    if (pwd == nullptr)
    {
        return UV_EINVAL;
    }
    SC::clearPasswd(pwd);

    char  username[256]  = "unknown";
    DWORD usernameSize   = sizeof(username);
    DWORD usernameLength = GetEnvironmentVariableA("USERNAME", username, usernameSize);
    if (usernameLength == 0 or usernameLength >= usernameSize)
    {
        snprintf(username, sizeof(username), "%s", "unknown");
    }

    char  homedir[4096] = "C:\\";
    DWORD homedirSize   = sizeof(homedir);
    DWORD homedirLength = GetEnvironmentVariableA("USERPROFILE", homedir, homedirSize);
    if (homedirLength == 0 or homedirLength >= homedirSize)
    {
        snprintf(homedir, sizeof(homedir), "%s", "C:\\");
    }
    const size_t homeLength = strlen(homedir);
    if (homeLength > 3 and (homedir[homeLength - 1] == '\\' or homedir[homeLength - 1] == '/'))
    {
        homedir[homeLength - 1] = '\0';
    }

    pwd->username = SC::duplicateString(username);
    pwd->homedir  = SC::duplicateString(homedir);
    if (pwd->username == nullptr or pwd->homedir == nullptr)
    {
        uv_os_free_passwd(pwd);
        return UV_ENOMEM;
    }
    return 0;
#else
    if (pwd == nullptr)
    {
        return UV_EINVAL;
    }
    return uv_os_get_passwd2(pwd, static_cast<uv_uid_t>(geteuid()));
#endif
}

UV_API void uv_os_free_passwd(uv_passwd_t* pwd)
{
    if (pwd == nullptr)
    {
        return;
    }
    free(pwd->username);
    free(pwd->shell);
    free(pwd->homedir);
    SC::clearPasswd(pwd);
}

UV_API int uv_os_get_group(uv_group_t* grp, uv_gid_t gid)
{
#if SC_PLATFORM_WINDOWS
    (void)grp;
    (void)gid;
    return UV_ENOTSUP;
#else
    if (grp == nullptr)
    {
        return UV_EINVAL;
    }
    SC::clearGroup(grp);

    long bufferSize = sysconf(_SC_GETGR_R_SIZE_MAX);
    if (bufferSize <= 0)
    {
        bufferSize = 16384;
    }
    char* buffer = static_cast<char*>(malloc(static_cast<size_t>(bufferSize)));
    if (buffer == nullptr)
    {
        return UV_ENOMEM;
    }

    group  storage = {};
    group* result  = nullptr;
    int    err     = getgrgid_r(static_cast<gid_t>(gid), &storage, buffer, static_cast<size_t>(bufferSize), &result);
    if (err != 0 or result == nullptr)
    {
        free(buffer);
        return err == 0 ? UV_ENOENT : SC::systemErrorToUv(err);
    }

    int numberOfMembers = 0;
    while (result->gr_mem != nullptr and result->gr_mem[numberOfMembers] != nullptr)
    {
        numberOfMembers++;
    }

    grp->groupname = SC::duplicateString(result->gr_name);
    grp->members   = static_cast<char**>(calloc(static_cast<size_t>(numberOfMembers + 1), sizeof(char*)));
    if (grp->groupname == nullptr or grp->members == nullptr)
    {
        free(buffer);
        uv_os_free_group(grp);
        return UV_ENOMEM;
    }
    for (int idx = 0; idx < numberOfMembers; ++idx)
    {
        grp->members[idx] = SC::duplicateString(result->gr_mem[idx]);
        if (grp->members[idx] == nullptr)
        {
            free(buffer);
            uv_os_free_group(grp);
            return UV_ENOMEM;
        }
    }
    grp->gid = static_cast<unsigned long>(result->gr_gid);

    free(buffer);
    return 0;
#endif
}

UV_API void uv_os_free_group(uv_group_t* grp)
{
    if (grp == nullptr)
    {
        return;
    }
    free(grp->groupname);
    if (grp->members != nullptr)
    {
        for (char** member = grp->members; *member != nullptr; ++member)
        {
            free(*member);
        }
    }
    free(grp->members);
    SC::clearGroup(grp);
}

UV_API uv_pid_t uv_os_getpid(void)
{
#if SC_PLATFORM_WINDOWS
    return static_cast<uv_pid_t>(GetCurrentProcessId());
#else
    return static_cast<uv_pid_t>(getpid());
#endif
}

UV_API uv_pid_t uv_os_getppid(void)
{
#if SC_PLATFORM_WINDOWS
    return 1;
#else
    return static_cast<uv_pid_t>(getppid());
#endif
}

UV_API int uv_os_getpriority(uv_pid_t pid, int* priority)
{
    if (priority == nullptr)
    {
        return UV_EINVAL;
    }
#if SC_PLATFORM_WINDOWS
    HANDLE process = SC::openPriorityProcess(pid, PROCESS_QUERY_LIMITED_INFORMATION);
    if (process == nullptr)
    {
        return SC::systemErrorToUv(GetLastError());
    }
    const DWORD priorityClass = GetPriorityClass(process);
    if (pid != 0 and static_cast<DWORD>(pid) != GetCurrentProcessId())
    {
        CloseHandle(process);
    }
    if (priorityClass == 0)
    {
        return SC::systemErrorToUv(GetLastError());
    }
    *priority = SC::priorityClassToUv(priorityClass);
    return 0;
#else
    errno           = 0;
    const int value = getpriority(PRIO_PROCESS, static_cast<id_t>(pid));
    if (value == -1 and errno != 0)
    {
        return SC::systemErrorToUv(errno);
    }
    *priority = value;
    return 0;
#endif
}

UV_API int uv_os_setpriority(uv_pid_t pid, int priority)
{
    if (priority < UV_PRIORITY_HIGHEST or priority > UV_PRIORITY_LOW)
    {
        return UV_EINVAL;
    }
#if SC_PLATFORM_WINDOWS
    HANDLE process = SC::openPriorityProcess(pid, PROCESS_SET_INFORMATION);
    if (process == nullptr)
    {
        return SC::systemErrorToUv(GetLastError());
    }
    const BOOL success = SetPriorityClass(process, SC::uvPriorityToClass(priority));
    if (pid != 0 and static_cast<DWORD>(pid) != GetCurrentProcessId())
    {
        CloseHandle(process);
    }
    return success ? 0 : SC::systemErrorToUv(GetLastError());
#else
    if (setpriority(PRIO_PROCESS, static_cast<id_t>(pid), priority) == 0)
    {
        return 0;
    }
    if (errno == EACCES or errno == EPERM)
    {
        return UV_EACCES;
    }
    return SC::systemErrorToUv(errno);
#endif
}

UV_API int uv_recv_buffer_size(uv_handle_t* handle, int* value)
{
    if (handle == nullptr or value == nullptr)
    {
        return UV_EINVAL;
    }

    SC::SocketDescriptor* descriptor = nullptr;
    if (handle->type == UV_TCP)
    {
        descriptor = &SC::AsyncUVTcp::from(reinterpret_cast<uv_tcp_t*>(handle))->descriptor;
    }
    else if (handle->type == UV_UDP)
    {
        descriptor = &SC::AsyncUVUdp::from(reinterpret_cast<uv_udp_t*>(handle))->descriptor;
    }
    else
    {
        return UV_EINVAL;
    }

    SC::SocketDescriptor::Handle socketHandle = SC::SocketDescriptor::Invalid;
    if (not descriptor->get(socketHandle, SC::Result(false)))
    {
        return UV_EBADF;
    }

    if (*value > 0)
    {
        int requested = *value;
#if SC_PLATFORM_WINDOWS
        if (setsockopt(socketHandle, SOL_SOCKET, SO_RCVBUF, reinterpret_cast<const char*>(&requested),
                       sizeof(requested)) != 0)
        {
            return SC::systemErrorToUv(WSAGetLastError());
        }
#else
        if (setsockopt(socketHandle, SOL_SOCKET, SO_RCVBUF, &requested, sizeof(requested)) != 0)
        {
            return SC::systemErrorToUv(errno);
        }
#endif
    }

    int       current = 0;
    socklen_t length  = sizeof(current);
#if SC_PLATFORM_WINDOWS
    if (getsockopt(socketHandle, SOL_SOCKET, SO_RCVBUF, reinterpret_cast<char*>(&current), &length) != 0)
    {
        return SC::systemErrorToUv(WSAGetLastError());
    }
#else
    if (getsockopt(socketHandle, SOL_SOCKET, SO_RCVBUF, &current, &length) != 0)
    {
        return SC::systemErrorToUv(errno);
    }
#endif
    *value = current;
    return 0;
}

UV_API int uv_send_buffer_size(uv_handle_t* handle, int* value)
{
    if (handle == nullptr or value == nullptr)
    {
        return UV_EINVAL;
    }

    SC::SocketDescriptor* descriptor = nullptr;
    if (handle->type == UV_TCP)
    {
        descriptor = &SC::AsyncUVTcp::from(reinterpret_cast<uv_tcp_t*>(handle))->descriptor;
    }
    else if (handle->type == UV_UDP)
    {
        descriptor = &SC::AsyncUVUdp::from(reinterpret_cast<uv_udp_t*>(handle))->descriptor;
    }
    else
    {
        return UV_EINVAL;
    }

    SC::SocketDescriptor::Handle socketHandle = SC::SocketDescriptor::Invalid;
    if (not descriptor->get(socketHandle, SC::Result(false)))
    {
        return UV_EBADF;
    }

    if (*value > 0)
    {
        int requested = *value;
#if SC_PLATFORM_WINDOWS
        if (setsockopt(socketHandle, SOL_SOCKET, SO_SNDBUF, reinterpret_cast<const char*>(&requested),
                       sizeof(requested)) != 0)
        {
            return SC::systemErrorToUv(WSAGetLastError());
        }
#else
        if (setsockopt(socketHandle, SOL_SOCKET, SO_SNDBUF, &requested, sizeof(requested)) != 0)
        {
            return SC::systemErrorToUv(errno);
        }
#endif
    }

    int       current = 0;
    socklen_t length  = sizeof(current);
#if SC_PLATFORM_WINDOWS
    if (getsockopt(socketHandle, SOL_SOCKET, SO_SNDBUF, reinterpret_cast<char*>(&current), &length) != 0)
    {
        return SC::systemErrorToUv(WSAGetLastError());
    }
#else
    if (getsockopt(socketHandle, SOL_SOCKET, SO_SNDBUF, &current, &length) != 0)
    {
        return SC::systemErrorToUv(errno);
    }
#endif
    *value = current;
    return 0;
}

UV_API void uv_loadavg(double avg[3])
{
#if SC_PLATFORM_WINDOWS
    avg[0] = 0;
    avg[1] = 0;
    avg[2] = 0;
#else
    if (getloadavg(avg, 3) != 3)
    {
        avg[0] = 0;
        avg[1] = 0;
        avg[2] = 0;
    }
#endif
}

UV_API uint64_t uv_get_total_memory(void)
{
#if SC_PLATFORM_WINDOWS
    MEMORYSTATUSEX status;
    status.dwLength = sizeof(status);
    return GlobalMemoryStatusEx(&status) ? status.ullTotalPhys : 0;
#else
    const long pages    = sysconf(_SC_PHYS_PAGES);
    const long pageSize = sysconf(_SC_PAGESIZE);
    return pages > 0 and pageSize > 0 ? static_cast<uint64_t>(pages) * static_cast<uint64_t>(pageSize) : 0;
#endif
}

UV_API uint64_t uv_get_free_memory(void)
{
#if SC_PLATFORM_WINDOWS
    MEMORYSTATUSEX status;
    status.dwLength = sizeof(status);
    return GlobalMemoryStatusEx(&status) ? status.ullAvailPhys : 0;
#elif defined(_SC_AVPHYS_PAGES)
    const long pages    = sysconf(_SC_AVPHYS_PAGES);
    const long pageSize = sysconf(_SC_PAGESIZE);
    return pages > 0 and pageSize > 0 ? static_cast<uint64_t>(pages) * static_cast<uint64_t>(pageSize) : 0;
#else
    return uv_get_total_memory() / 2;
#endif
}

UV_API uint64_t uv_get_constrained_memory(void) { return 0; }

UV_API uint64_t uv_get_available_memory(void) { return uv_get_free_memory(); }

UV_API unsigned int uv_available_parallelism(void)
{
#if SC_PLATFORM_WINDOWS
    SYSTEM_INFO info;
    GetSystemInfo(&info);
    return info.dwNumberOfProcessors > 0 ? info.dwNumberOfProcessors : 1;
#elif defined(_SC_NPROCESSORS_ONLN)
    const long processors = sysconf(_SC_NPROCESSORS_ONLN);
    return processors > 0 ? static_cast<unsigned int>(processors) : 1;
#else
    return 1;
#endif
}

UV_API int uv_cpu_info(uv_cpu_info_t** cpu_infos, int* count)
{
    if (cpu_infos == nullptr or count == nullptr)
    {
        return UV_EINVAL;
    }
    const unsigned int processors = uv_available_parallelism();
    uv_cpu_info_t*     infos      = static_cast<uv_cpu_info_t*>(calloc(processors, sizeof(*infos)));
    if (infos == nullptr)
    {
        return UV_ENOMEM;
    }
    for (unsigned int idx = 0; idx < processors; ++idx)
    {
        infos[idx].model = SC::duplicateString("unknown");
        if (infos[idx].model == nullptr)
        {
            uv_free_cpu_info(infos, static_cast<int>(idx));
            return UV_ENOMEM;
        }
    }
    *cpu_infos = infos;
    *count     = static_cast<int>(processors);
    return 0;
}

UV_API void uv_free_cpu_info(uv_cpu_info_t* cpu_infos, int count)
{
    if (cpu_infos == nullptr)
    {
        return;
    }
    for (int idx = 0; idx < count; ++idx)
    {
        free(cpu_infos[idx].model);
    }
    free(cpu_infos);
}

void SC::AsyncUVRandom::init(uv_random_t* req)
{
    ::memset(req, 0, sizeof(uv_random_t));
    req->type = UV_RANDOM;
    placementNew(from(req));
}

SC::AsyncUVRandom& SC::AsyncUVRandom::from(uv_random_t* req)
{
    return *reinterpret_cast<SC::AsyncUVRandom*>(req->sc_opaque);
}

UV_API int uv_random(uv_loop_t* loop, uv_random_t* req, void* buf, size_t buflen, unsigned int flags, uv_random_cb cb)
{
    if (buflen > 2147483647u)
    {
        return UV_E2BIG;
    }
    if (flags != 0 or (buf == nullptr and buflen > 0))
    {
        return UV_EINVAL;
    }

    if (cb == nullptr)
    {
        SC::fillRandomBytes(buf, buflen);
        return 0;
    }
    if (loop == nullptr or req == nullptr)
    {
        return UV_EINVAL;
    }

    SC::AsyncUVRandom::init(req);
    SC::AsyncUVRandom& random = SC::AsyncUVRandom::from(req);
    random.buffer             = buf;
    random.length             = buflen;
    random.cb                 = cb;

    SC::fillRandomBytes(buf, buflen);

    random.asyncRandom.callback = [req](SC::AsyncLoopTimeout::Result&)
    {
        SC::AsyncUVRandom& random = SC::AsyncUVRandom::from(req);
        random.cb(req, 0, random.buffer, random.length);
    };
    return random.asyncRandom.start(SC::AsyncUVLoop::getLoop(loop), SC::Time::Milliseconds(0)) ? 0 : UV_EINVAL;
}

void SC::AsyncUVGetNameInfo::init(uv_getnameinfo_t* req)
{
    void* data = req->data;
    ::memset(req, 0, sizeof(uv_getnameinfo_t));
    req->data = data;
    req->type = UV_GETNAMEINFO;
    placementNew(from(req));
}

SC::AsyncUVGetNameInfo& SC::AsyncUVGetNameInfo::from(uv_getnameinfo_t* req)
{
    return *reinterpret_cast<SC::AsyncUVGetNameInfo*>(req->sc_opaque);
}

UV_API int uv_getnameinfo(uv_loop_t* loop, uv_getnameinfo_t* req, uv_getnameinfo_cb getnameinfo_cb,
                          const struct sockaddr* addr, int flags)
{
    if (req == nullptr or addr == nullptr)
    {
        return UV_EINVAL;
    }
    if (getnameinfo_cb != nullptr and loop == nullptr)
    {
        return UV_EINVAL;
    }

    SC::AsyncUVGetNameInfo::init(req);
    req->loop           = loop;
    req->getnameinfo_cb = getnameinfo_cb;

    const socklen_t addrLength = addr->sa_family == AF_INET6 ? static_cast<socklen_t>(sizeof(sockaddr_in6))
                                                             : static_cast<socklen_t>(sizeof(sockaddr_in));
    const int       result =
        ::getnameinfo(addr, addrLength, req->host, sizeof(req->host), req->service, sizeof(req->service), flags);
    SC::AsyncUVGetNameInfo& nameInfo = SC::AsyncUVGetNameInfo::from(req);
    nameInfo.status                  = result == 0 ? 0 : UV_EINVAL;
    if (nameInfo.status != 0)
    {
        req->host[0]    = '\0';
        req->service[0] = '\0';
    }

    if (getnameinfo_cb == nullptr)
    {
        return nameInfo.status;
    }

    nameInfo.asyncGetNameInfo.callback = [req](SC::AsyncLoopTimeout::Result&)
    {
        SC::AsyncUVGetNameInfo& nameInfo = SC::AsyncUVGetNameInfo::from(req);
        req->getnameinfo_cb(req, nameInfo.status, req->host, req->service);
    };
    return nameInfo.asyncGetNameInfo.start(SC::AsyncUVLoop::getLoop(loop), SC::Time::Milliseconds(0)) ? 0 : UV_EINVAL;
}

void SC::AsyncUVGetAddrInfo::init(uv_getaddrinfo_t* req)
{
    void* data = req->data;
    ::memset(req, 0, sizeof(uv_getaddrinfo_t));
    req->data = data;
    req->type = UV_GETADDRINFO;
    placementNew(from(req));
}

SC::AsyncUVGetAddrInfo& SC::AsyncUVGetAddrInfo::from(uv_getaddrinfo_t* req)
{
    return *reinterpret_cast<SC::AsyncUVGetAddrInfo*>(req->sc_opaque);
}

UV_API int uv_getaddrinfo(uv_loop_t* loop, uv_getaddrinfo_t* req, uv_getaddrinfo_cb getaddrinfo_cb, const char* node,
                          const char* service, const struct addrinfo* hints)
{
    if (req == nullptr or (node == nullptr and service == nullptr))
    {
        return UV_EINVAL;
    }
    if (getaddrinfo_cb != nullptr and loop == nullptr)
    {
        return UV_EINVAL;
    }

    SC::AsyncUVGetAddrInfo::init(req);
    req->loop           = loop;
    req->getaddrinfo_cb = getaddrinfo_cb;

    const int result = ::getaddrinfo(node, service, hints, &req->addrinfo);

    SC::AsyncUVGetAddrInfo& addrInfo = SC::AsyncUVGetAddrInfo::from(req);
    addrInfo.status                  = result == 0 ? 0 : UV_EINVAL;
    if (addrInfo.status != 0)
    {
        req->addrinfo = nullptr;
    }

    if (getaddrinfo_cb == nullptr)
    {
        return addrInfo.status;
    }

    addrInfo.asyncGetAddrInfo.callback = [req](SC::AsyncLoopTimeout::Result&)
    {
        SC::AsyncUVGetAddrInfo& addrInfo = SC::AsyncUVGetAddrInfo::from(req);
        req->getaddrinfo_cb(req, addrInfo.status, req->addrinfo);
    };
    return addrInfo.asyncGetAddrInfo.start(SC::AsyncUVLoop::getLoop(loop), SC::Time::Milliseconds(0)) ? 0 : UV_EINVAL;
}

UV_API void uv_freeaddrinfo(struct addrinfo* ai)
{
    if (ai != nullptr)
    {
        ::freeaddrinfo(ai);
    }
}

void SC::AsyncUVWork::init(uv_work_t* req)
{
    ::memset(req, 0, sizeof(uv_work_t));
    req->type = UV_WORK;
    placementNew(from(req));
}

SC::AsyncUVWork& SC::AsyncUVWork::from(uv_work_t* req) { return *reinterpret_cast<SC::AsyncUVWork*>(req->sc_opaque); }

UV_API int uv_queue_work(uv_loop_t* loop, uv_work_t* req, uv_work_cb work_cb, uv_after_work_cb after_work_cb)
{
    if (loop == nullptr or req == nullptr or work_cb == nullptr)
    {
        return UV_EINVAL;
    }

    void* data = req->data;
    SC::AsyncUVWork::init(req);
    req->data = data;

    SC::AsyncUVWork& work   = SC::AsyncUVWork::from(req);
    work.workCb             = work_cb;
    work.afterWorkCb        = after_work_cb;
    work.asyncWork.callback = [req](SC::AsyncLoopTimeout::Result&)
    {
        SC::AsyncUVWork& work = SC::AsyncUVWork::from(req);
        work.workCb(req);
        if (work.afterWorkCb != nullptr)
        {
            work.afterWorkCb(req, 0);
        }
    };
    return work.asyncWork.start(SC::AsyncUVLoop::getLoop(loop), SC::Time::Milliseconds(0)) ? 0 : UV_EINVAL;
}

UV_API int uv_fs_stat(uv_loop_t* loop, uv_fs_t* req, const char* path, uv_fs_cb cb)
{
    if (req == nullptr or path == nullptr)
    {
        return UV_EINVAL;
    }

    memset(req, 0, sizeof(uv_fs_t));
    req->type    = UV_FS;
    req->loop    = loop;
    req->fs_type = UV_FS_STAT;
    req->path    = path;
    req->ptr     = &req->statbuf;

#if SC_PLATFORM_WINDOWS
    const int result = ::_stat(path, &req->statbuf);
#else
    const int result = ::stat(path, &req->statbuf);
#endif
    req->result = result == 0 ? 0 : UV_EINVAL;
    if (cb != nullptr)
    {
        cb(req);
    }
    return static_cast<int>(req->result);
}

UV_API void uv_fs_req_cleanup(uv_fs_t* req)
{
    if (req != nullptr)
    {
        req->path = nullptr;
        req->ptr  = nullptr;
    }
}

UV_API uv_fs_type uv_fs_get_type(const uv_fs_t* req) { return req != nullptr ? req->fs_type : UV_FS_UNKNOWN; }

UV_API ssize_t uv_fs_get_result(const uv_fs_t* req) { return req != nullptr ? req->result : UV_EINVAL; }

UV_API void* uv_fs_get_ptr(const uv_fs_t* req) { return req != nullptr ? req->ptr : nullptr; }

UV_API const char* uv_fs_get_path(const uv_fs_t* req) { return req != nullptr ? req->path : nullptr; }

UV_API uv_stat_t* uv_fs_get_statbuf(uv_fs_t* req) { return req != nullptr ? &req->statbuf : nullptr; }

UV_API int uv_os_uname(uv_utsname_t* buffer)
{
    if (buffer == nullptr)
    {
        return UV_EINVAL;
    }
#if SC_PLATFORM_WINDOWS
    SC::copyUnameField(buffer->sysname, "Windows");
    SC::copyUnameField(buffer->release, "");
    SC::copyUnameField(buffer->version, "");
#if defined(_M_X64) || defined(_M_AMD64)
    SC::copyUnameField(buffer->machine, "x86_64");
#elif defined(_M_ARM64)
    SC::copyUnameField(buffer->machine, "arm64");
#else
    SC::copyUnameField(buffer->machine, "");
#endif
#else
    utsname name;
    if (uname(&name) != 0)
    {
        return SC::systemErrorToUv(errno);
    }
    SC::copyUnameField(buffer->sysname, name.sysname);
    SC::copyUnameField(buffer->release, name.release);
    SC::copyUnameField(buffer->version, name.version);
    SC::copyUnameField(buffer->machine, name.machine);
#endif
    return 0;
}
