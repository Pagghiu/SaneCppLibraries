// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
namespace SC
{
struct SC_COMPILER_EXPORT GlobalSettings;
struct SC_COMPILER_EXPORT Globals;
struct SC_COMPILER_EXPORT MemoryAllocator;
} // namespace SC
//! @addtogroup group_foundation_utility
//! @{

/// @brief Settings to initialize Globals
struct SC::GlobalSettings
{
    size_t ownershipTrackingBytes = 0; ///< Memory to allocate for ownership tracking
};

/// @brief Customizable thread-local and global variables for memory handling.
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
    static Globals* push(Type type, Globals& globals);

    /// @brief Restores Globals previously replaced by a push
    static Globals* pop(Type type);

    /// @brief Obtains current set of Globals
    static Globals& get(Type type);

  private:
    Globals* prev = nullptr;
    struct Internal;
};

//! @}
