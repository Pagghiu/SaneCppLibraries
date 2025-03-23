// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../Foundation/Memory.h"
namespace SC
{
struct SC_COMPILER_EXPORT Globals;
} // namespace SC
//! @addtogroup group_foundation_utility
//! @{

/// @brief Customizable thread-local and global variables for memory handling.
struct SC::Globals
{
    MemoryAllocator& allocator;
    Globals(MemoryAllocator& allocator) : allocator(allocator) {}

    /// @brief Explicit registration of globals
    static void registerGlobals();

    /// @brief Obtain current global instance
    static Globals& getGlobal();

    /// @brief Obtain current thread-local global instance
    static Globals& getThreadLocal();

    /// @brief Replaces current global with a new one
    static void pushGlobal(Globals& globals);

    /// @brief Restores previous global replaced by Globals::pushGlobal
    static void popGlobal();

    /// @brief Replaces current thread-local global with a new one
    static void pushThreadLocal(Globals& globals);

    /// @brief Restores previous thread-local global replaced by Globals::pushThreadLocal
    static void popThreadLocal();

  private:
    Globals* prev = nullptr;
    struct Internal;
};

//! @}
