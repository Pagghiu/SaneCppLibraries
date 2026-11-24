// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT

// Intentionally unguarded implementation fragment. Include inside a private namespace after StringPath,
// <unistd.h> and <string.h> have been included. NetBSD and FreeBSD also need <sys/sysctl.h> and <sys/types.h>.
static StringSpan getExecutablePath(StringPath& executablePath)
{
    size_t pathSize = StringPath::StorageCapacity;
    char*  buffer   = executablePath.writableSpan().data();
#if SC_PLATFORM_OPENBSD
    if (::getexecpath(buffer, pathSize) != 0)
        return {};
#else
#if SC_PLATFORM_NETBSD
    int mib[] = {CTL_KERN, KERN_PROC_ARGS, ::getpid(), KERN_PROC_PATHNAME};
#elif SC_PLATFORM_FREEBSD
    int mib[] = {CTL_KERN, KERN_PROC, KERN_PROC_PATHNAME, -1};
#endif
    if (::sysctl(mib, 4, buffer, &pathSize, nullptr, 0) != 0 or pathSize == 0 or pathSize > StringPath::StorageCapacity)
        return {};
#endif
    const char* terminator = static_cast<const char*>(::memchr(buffer, '\0', pathSize));
    if (terminator != nullptr and executablePath.resize(static_cast<size_t>(terminator - buffer)))
        return executablePath.view();
    return {};
}
