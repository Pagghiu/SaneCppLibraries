// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT

// Intentionally unguarded implementation fragment. Include inside a private namespace after StringPath,
// <sys/types.h>, <sys/sysctl.h>, <unistd.h>, and <string.h> have been included.
static StringSpan getExecutablePath(StringPath& executablePath)
{
#if SC_PLATFORM_NETBSD
    int mib[] = {CTL_KERN, KERN_PROC_ARGS, ::getpid(), KERN_PROC_PATHNAME};
#elif SC_PLATFORM_FREEBSD
    int mib[] = {CTL_KERN, KERN_PROC, KERN_PROC_PATHNAME, -1};
#endif
    size_t pathSize = StringPath::StorageCapacity;
    char*  buffer   = executablePath.writableSpan().data();
    if (::sysctl(mib, 4, buffer, &pathSize, nullptr, 0) == 0 and pathSize > 0 and
        pathSize <= StringPath::StorageCapacity)
    {
        const char* terminator = static_cast<const char*>(::memchr(buffer, '\0', pathSize));
        if (terminator != nullptr and executablePath.resize(static_cast<size_t>(terminator - buffer)))
            return executablePath.view();
    }
    return {};
}
