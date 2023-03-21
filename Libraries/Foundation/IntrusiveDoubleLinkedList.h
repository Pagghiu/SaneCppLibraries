// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "Assert.h"

namespace SC
{
template <typename T>
struct IntrusiveDoubleLinkedList;
} // namespace SC

template <typename T>
struct SC::IntrusiveDoubleLinkedList
{
    T* back  = nullptr; // has no next
    T* front = nullptr; // has no prev

    [[nodiscard]] T* peekFront() const { return front; }

    [[nodiscard]] bool isEmpty() const { return peekFront() == nullptr; }

    template <typename Lambda>
    void forEachFrontToBack(Lambda&& lambda)
    {
        for (T* it = front; it != nullptr; it = it->next)
        {
            lambda(it);
        }
    }

    template <typename Lambda>
    void forEachBackToFront(Lambda&& lambda)
    {
        for (T* it = back; it != nullptr; it = it->prev)
        {
            lambda(it);
        }
    }

    template <typename Lambda>
    void forEachFrontToBack(Lambda&& lambda) const
    {
        for (const T* it = front; it != nullptr; it = it->next)
        {
            lambda(it);
        }
    }

    template <typename Lambda>
    void forEachBackToFront(Lambda&& lambda) const
    {
        for (const T* it = back; it != nullptr; it = it->prev)
        {
            lambda(it);
        }
    }

    void queueBack(T& item)
    {
        SC_DEBUG_ASSERT(item.next == nullptr and item.prev == nullptr);
        if (back)
        {
            back->next = &item;
            item.prev  = back;
            back       = &item;
        }
        else
        {
            SC_DEBUG_ASSERT(front == nullptr);
            back  = &item;
            front = &item;
        }
    }

    [[nodiscard]] T* dequeueFront()
    {
        if (not front)
        {
            return nullptr;
        }
        T* item = front;
        front   = item->next;
        if (front)
        {
            front->prev = nullptr;
        }
        item->next = nullptr;
        item->prev = nullptr;
        if (back == item)
        {
            back = nullptr;
        }
        return item;
    }

    void remove(T& item)
    {
        if (&item == front)
        {
            front = front->next;
        }
        if (&item == back)
        {
            back = back->prev;
        }

        T* next = item.next;
        T* prev = item.prev;

        if (prev)
        {
            prev->next = next;
        }

        if (next)
        {
            next->prev = prev;
        }

        item.next = nullptr;
        item.prev = nullptr;
    }
};
