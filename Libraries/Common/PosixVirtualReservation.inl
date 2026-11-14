// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT

// Include privately after the platform macros and <sys/mman.h>.
static void* reserveReadWritePages(SC::size_t size)
{
    int protection = PROT_NONE;
#if SC_PLATFORM_NETBSD
    // Keep the reservation inaccessible while allowing later read/write commits under PaX MPROTECT.
    protection |= PROT_MPROTECT(PROT_READ | PROT_WRITE);
#endif
    void* address = ::mmap(nullptr, size, protection, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    return address == MAP_FAILED ? nullptr : address;
}
