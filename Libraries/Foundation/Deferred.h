// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../Foundation/Compiler.h"

namespace SC
{
//! @addtogroup group_foundation_utility
//! @{

/// @brief Executes a function at end of current scope (in the spirit of Zig `defer` keyword).
/// @tparam F The lambda / function to execute
///
/// Example:
/// @snippet Libraries/Plugin/Internal/DebuggerWindows.inl DeferredSnippet
template <typename F>
struct Deferred
{
    /// @brief Constructs Deferred object with a functor F
    Deferred(F&& f) : f(forward<F>(f)) {}

    /// @brief Invokes the function F upon destruction, if disarm() has not been previously called.
    ~Deferred()
    {
        if (armed)
            f();
    }

    /// @brief Disarms the Deferred object, preventing function invocation on destruction.
    void disarm() { armed = false; }

  private:
    F    f;            ///< The function to be invoked.
    bool armed = true; ///< Indicates whether the Deferred object is 'armed' for function invocation.
};

/// @brief Creates a Deferred object holding a function that will be invoked at end of current scope.
/// @param f The lambda to be invoked at end of current scope
template <typename F>
Deferred<F> MakeDeferred(F&& f)
{
    return Deferred<F>(forward<F>(f));
}

//! @}
} // namespace SC
