// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
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
/**
 @code{.cpp}
    HANDLE processHandle = OpenProcess(PROCESS_QUERY_INFORMATION |
                                        PROCESS_DUP_HANDLE, FALSE, processId);
    if (processHandle == nullptr)
    {
        return false;
    }
    auto deferDeleteProcessHandle = SC::MakeDeferred(
        [&] // Function (or lambda) that will be invoked at the end of scope
        {
            CloseHandle(processHandle);
            processHandle = nullptr;
        });

    // Use processHandle that will be disposed at end of scope by the Deferred

    // ...
    // Deferred can be disarmed, meaning that the dispose function will not be executed
    deferDeleteProcessHandle.disarm()
 @endcode
*/
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
