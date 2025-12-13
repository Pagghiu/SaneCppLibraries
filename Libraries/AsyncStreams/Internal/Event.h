// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../../Foundation/Function.h"

namespace SC
{
//! @addtogroup group_foundation_utility
//! @{

/// @brief Tracks multiple listeners that must be notified for an event that happened.
/// Listeners can be removed with the integer written by Event::addListener in a parameter pointer.
/// @note The ordering of listeners will __NOT__ be preserved under multiple add / remove
template <int MaxListeners, typename... T>
struct SC_COMPILER_EXPORT Event
{
    /// @brief Emits the event, calling all registered listeners with the given parameters
    template <typename... U>
    void emit(U&&... t)
    {
        Event eventCopy = *this;
        for (int idx = 0; idx < eventCopy.numListeners; ++idx)
        {
            eventCopy.listeners[idx](t...);
        }
    }

    /// @brief Adds a listener to this event, optionally saving the index to use for its removal
    /// @see Event::removeListener
    template <typename Class, void (Class::*MemberFunction)(T...)>
    [[nodiscard]] bool addListener(Class& pself, int* idx = nullptr)
    {
        if (numListeners + 1 > MaxListeners)
        {
            return false;
        }
        else
        {
            Function<void(T...)> func;
            func.template bind<Class, MemberFunction>(pself);
            if (idx)
            {
                *idx = numListeners;
            }
            listeners[numListeners++] = move(func);
            return true;
        }
    }

    /// @brief Adds a listener to this event, optionally saving the index to use for its removal
    /// @see Event::removeListener
    template <typename Func>
    [[nodiscard]] bool addListener(Func&& func)
    {
        if (numListeners + 1 > MaxListeners)
        {
            return false;
        }
        else
        {
            listeners[numListeners++] = move(func);
            return true;
        }
    }

    template <typename Class, void (Class::*MemberFunction)(T...)>
    [[nodiscard]] bool removeListener(Class& pself)
    {
        Function<void(T...)> func;
        func.template bind<Class, MemberFunction>(pself);
        for (int idx = 0; idx < numListeners; ++idx)
        {
            if (listeners[idx] == func)
            {
                return removeListenerAt(idx);
            }
        }
        return false;
    }

    template <typename Class>
    [[nodiscard]] bool removeAllListenersBoundTo(Class& pself)
    {
        bool someRemoved = false;
        for (int idx = 0; idx < numListeners; ++idx)
        {
            if (listeners[idx].isBoundToClassInstance(&pself))
            {
                someRemoved |= removeListenerAt(idx);
            }
        }
        return someRemoved;
    }

    /// @brief Removes a listener where operator == evaluates to true for the passed in func
    template <typename Func>
    [[nodiscard]] bool removeListener(Func& func)
    {
        for (int idx = 0; idx < numListeners; ++idx)
        {
            if (listeners[idx] == func)
            {
                return removeListenerAt(idx);
            }
        }
        return false;
    }

    /// @brief Removes a listener at a given index
    /// @see Event::addListener
    [[nodiscard]] bool removeListenerAt(int idx)
    {
        if (idx < 0 or idx >= numListeners)
            return false;
        listeners[idx] = {};
        if (idx + 1 != numListeners)
        {
            listeners[idx] = move(listeners[numListeners - 1]);
        }
        numListeners--;
        return true;
    }

  private:
    // Avoid pulling Array<T, N> to reduce inter-dependencies
    Function<void(T...)> listeners[MaxListeners];
    int                  numListeners = 0;
};
//! @}

} // namespace SC
