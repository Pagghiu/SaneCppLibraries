// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../Foundation/PrimitiveTypes.h"
namespace SC
{
struct SC_COMPILER_EXPORT GlobalSettings;
struct SC_COMPILER_EXPORT Globals;
struct MemoryAllocator;
} // namespace SC
//! @addtogroup group_memory
//! @{

/// @brief Settings to initialize Globals
struct SC::GlobalSettings
{
    size_t ownershipTrackingBytes = 0; ///< Memory to allocate for ownership tracking
};

/// @brief Customizable thread-local and global variables for memory handling.
/// This class holds pointers to systems that must be globally available, like the memory allocator.
/// It allows "stacking" different Globals through a push / pop mechanism, connecting them through a linked list.
/// The Default allocator is automatically setup and uses standard `malloc`, `realloc`, `free` for allocations.
/// @note Globals use no locking mechanism so they are thread-unsafe.
/// Every method however requires a Globals::Type parameter that can be set to Globals::ThreadLocal to avoid such
/// issues.
///
/// Example (Fixed Allocator):
/// \snippet Tests/Libraries/Memory/GlobalsTest.cpp GlobalsSnippetFixed
///
/// Example (Virtual Allocator):
/// \snippet Tests/Libraries/Memory/GlobalsTest.cpp GlobalsSnippetVirtual
///
/// Example (Memory dump):
/// \snippet Tests/Libraries/Containers/GlobalsContainerTest.cpp GlobalContainerVirtualMemoryDumpSnippet
struct SC::Globals
{
    enum Type
    {
        Global      = 0, ///< Shared globals (NOT thread-safe)
        ThreadLocal = 1, ///< Thread-specific globals (separate copy for each thread)
    };
    MemoryAllocator& allocator;

    Globals(MemoryAllocator& allocator) : allocator(allocator) {}

    /// @brief Initialized Globals for current thread
    /// @note Each thread can use different GlobalSettings
    static void init(Type type, GlobalSettings settings = {});

    /// @brief Sets Globals as current, saving previous one
    /// @return Pointer to Globals that have been replaced
    static Globals* push(Type type, Globals& globals);

    /// @brief Restores Globals previously replaced by a push
    /// @return Pointer to Globals that are no longer current (or `nullptr` if current is the Default Allocator)
    static Globals* pop(Type type);

    /// @brief Obtains current set of Globals
    static Globals& get(Type type);

  private:
    Globals* prev = nullptr; // Pointer to previous Globals to restore on ::pop()
    struct Internal;
};

//! @}
