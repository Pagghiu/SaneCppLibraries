// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "Language.h"

namespace SC
{
template <typename HandleType, HandleType InvalidSentinel, typename CloseReturnType,
          CloseReturnType (*FuncPointer)(const HandleType&)>
struct MovableHandle;
} // namespace SC

// MovableHandle is used to wrap HANDLE files on Windows or file descriptors on Posix
template <typename HandleType, HandleType InvalidSentinel, typename CloseReturnType,
          CloseReturnType (*DeleteFunc)(const HandleType&)>
struct SC::MovableHandle
{
    static constexpr HandleType   InvalidHandle() { return InvalidSentinel; }
    [[nodiscard]] CloseReturnType assign(const HandleType& externalHandle)
    {
        if (close())
        {
            handle = externalHandle;
            return true;
        }
        return false;
    }

    MovableHandle()                                      = default;
    MovableHandle(const MovableHandle& v)                = delete;
    MovableHandle& operator=(const MovableHandle& other) = delete;
    MovableHandle(MovableHandle&& v) : handle(v.handle) { v.detach(); }
    MovableHandle(const HandleType& externalHandle) : handle(externalHandle) {}
    ~MovableHandle() { (void)close(); }

    [[nodiscard]] CloseReturnType assignMovingFrom(MovableHandle& other)
    {
        if (close())
        {
            handle = other.handle;
            other.detach();
            return true;
        }
        return false;
    }
    MovableHandle& operator=(MovableHandle&& other)
    {
        (void)(assignMovingFrom(other));
        return *this;
    }

    [[nodiscard]]      operator bool() const { return handle != InvalidSentinel; }
    [[nodiscard]] bool isValid() const { return handle != InvalidSentinel; }
    void               detach() { handle = InvalidSentinel; }

    [[nodiscard]] CloseReturnType get(HandleType& outHandle, CloseReturnType invalidReturnType) const
    {
        if (isValid())
        {
            outHandle = handle;
            return true;
        }
        return invalidReturnType;
    }

    [[nodiscard]] CloseReturnType close()
    {
        if (isValid())
        {
            HandleType handleCopy = handle;
            detach();
            return DeleteFunc(handleCopy);
        }
        return true;
    }

  private:
    HandleType handle = InvalidSentinel;
};
