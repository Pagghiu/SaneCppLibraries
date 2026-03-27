// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once

#include "../../Foundation/Function.h"

namespace SC
{
template <int MaxListeners, typename... T>
struct SC_COMPILER_EXPORT HttpClientEvent
{
    template <typename... U>
    void emit(U&&... t)
    {
        HttpClientEvent eventCopy = *this;
        for (int idx = 0; idx < eventCopy.numListeners; ++idx)
        {
            eventCopy.listeners[idx](forward<U>(t)...);
        }
    }

    template <typename Class, void (Class::*MemberFunction)(T...)>
    [[nodiscard]] bool addListener(Class& self)
    {
        if (numListeners + 1 > MaxListeners)
        {
            return false;
        }

        Function<void(T...)> func;
        func.template bind<Class, MemberFunction>(self);
        listeners[numListeners++] = move(func);
        return true;
    }

    template <typename Func>
    [[nodiscard]] bool addListener(Func&& func)
    {
        if (numListeners + 1 > MaxListeners)
        {
            return false;
        }

        listeners[numListeners++] = move(func);
        return true;
    }

    template <typename Class, void (Class::*MemberFunction)(T...)>
    [[nodiscard]] bool removeListener(Class& self)
    {
        Function<void(T...)> func;
        func.template bind<Class, MemberFunction>(self);
        for (int idx = 0; idx < numListeners; ++idx)
        {
            if (listeners[idx] == func)
            {
                return removeListenerAt(idx);
            }
        }
        return false;
    }

    template <typename Func>
    [[nodiscard]] bool removeListener(Func&& func)
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

  private:
    [[nodiscard]] bool removeListenerAt(int idx)
    {
        if (idx < 0 or idx >= numListeners)
        {
            return false;
        }

        for (int curr = idx; curr < numListeners - 1; ++curr)
        {
            listeners[curr] = move(listeners[curr + 1]);
        }
        numListeners -= 1;
        return true;
    }

    Function<void(T...)> listeners[MaxListeners];
    int                  numListeners = 0;
};
} // namespace SC
