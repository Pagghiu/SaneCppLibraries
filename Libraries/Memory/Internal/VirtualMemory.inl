// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "../VirtualMemory.h"
#if SC_PLATFORM_WINDOWS
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h> // _SC_PAGESIZE
#endif

//------------------------------------------------------------------------------------------------------------------
// VirtualMemory
//------------------------------------------------------------------------------------------------------------------
SC::size_t SC::VirtualMemory::roundUpToPageSize(size_t size)
{
    const size_t pageSize = SC::VirtualMemory::getPageSize();
    return (size + pageSize - 1) / pageSize * pageSize;
}

SC::size_t SC::VirtualMemory::getPageSize()
{
#if SC_PLATFORM_WINDOWS
    static const size_t pageSize = []()
    {
        SYSTEM_INFO sysInfo;
        GetSystemInfo(&sysInfo);
        return sysInfo.dwPageSize;
    }();
#else
    static const size_t pageSize = static_cast<size_t>(sysconf(_SC_PAGESIZE));
#endif
    return pageSize;
}

bool SC::VirtualMemory::reserve(size_t maxCapacityInBytes)
{
    if (memory != nullptr)
        return false;
    reservedBytes  = roundUpToPageSize(maxCapacityInBytes);
    committedBytes = 0;
#if SC_PLATFORM_WINDOWS
    memory = ::VirtualAlloc(nullptr, maxCapacityInBytes, MEM_RESERVE, PAGE_NOACCESS);
#else
    memory = ::mmap(nullptr, reservedBytes, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (memory == MAP_FAILED)
        memory = nullptr;
#endif
    return memory != nullptr;
}

void SC::VirtualMemory::release()
{
    if (memory == nullptr)
        return;
#if SC_PLATFORM_WINDOWS
    const bool result = ::VirtualFree(memory, 0, MEM_RELEASE) == TRUE;
#else
    const bool result = ::munmap(memory, reservedBytes) == 0;
#endif
    SC_ASSERT_RELEASE(result);
    memory = nullptr;

    committedBytes = 0;
    reservedBytes  = 0;
}

bool SC::VirtualMemory::commit(size_t sizeInBytes)
{
    if (memory == nullptr or sizeInBytes > reservedBytes)
        return false;

    if (sizeInBytes <= committedBytes)
        return true;

    void*        commitAddress = reinterpret_cast<char*>(memory) + committedBytes;
    const size_t alignedSize   = roundUpToPageSize(sizeInBytes);
    const size_t sizeToCommit  = alignedSize - committedBytes;
#if SC_PLATFORM_WINDOWS
    if (::VirtualAlloc(commitAddress, sizeToCommit, MEM_COMMIT, PAGE_READWRITE) == nullptr)
        return false;
#else
    if (::mprotect(commitAddress, sizeToCommit, PROT_READ | PROT_WRITE) != 0)
        return false;
#endif
    committedBytes = alignedSize;
    return true;
}

bool SC::VirtualMemory::shrink(size_t sizeInBytes)
{
    if (memory == nullptr)
        return false;
    const size_t alignedSize = roundUpToPageSize(sizeInBytes);
    if (alignedSize >= committedBytes)
        return true;

    void*        decommitAddress = reinterpret_cast<char*>(memory) + alignedSize;
    const size_t sizeToDecommit  = committedBytes - alignedSize;

#if SC_PLATFORM_WINDOWS
    if (::VirtualFree(decommitAddress, sizeToDecommit, MEM_DECOMMIT) == FALSE)
        return false;
#else
    // Remove access to the pages
    if (::mprotect(decommitAddress, sizeToDecommit, PROT_NONE) != 0)
        return false;

    // Advise the kernel that these pages are no longer needed
    if (::madvise(decommitAddress, sizeToDecommit, MADV_DONTNEED) != 0)
        return false;
#endif
    committedBytes = alignedSize;
    return true;
}

//------------------------------------------------------------------------------------------------------------------
// VirtualAllocator
//------------------------------------------------------------------------------------------------------------------
SC::VirtualAllocator::VirtualAllocator(VirtualMemory& virtualMemory)
    : FixedAllocator(nullptr, 0), virtualMemory(virtualMemory)
{}

void* SC::VirtualAllocator::allocateImpl(const void* owner, size_t numBytes, size_t alignment)
{
    syncFixedAllocator();
    void* fixedMemory = FixedAllocator::allocateImpl(owner, numBytes, alignment);
    if (fixedMemory == nullptr and virtualMemory.commit(virtualMemory.size() + numBytes))
    {
        syncFixedAllocator();
        return FixedAllocator::allocateImpl(owner, numBytes, alignment);
    }
    return fixedMemory;
}

void* SC::VirtualAllocator::reallocateImpl(void* allocatedMemory, size_t numBytes)
{
    syncFixedAllocator();
    void* fixedMemory = FixedAllocator::reallocateImpl(allocatedMemory, numBytes);
    if (fixedMemory == nullptr and virtualMemory.commit(virtualMemory.size() + numBytes))
    {
        syncFixedAllocator();
        return FixedAllocator::reallocateImpl(allocatedMemory, numBytes);
    }
    return fixedMemory;
}

void SC::VirtualAllocator::syncFixedAllocator()
{
    FixedAllocator::memory        = virtualMemory.data();
    FixedAllocator::capacityBytes = virtualMemory.size();
}
