// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../Foundation/Assert.h"

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

    void clear()
    {
        back  = nullptr;
        front = nullptr;
    }

    void appendBack(IntrusiveDoubleLinkedList& other)
    {
        if (&other == this)
            return;
        if (other.front)
        {
            SC_ASSERT_DEBUG(other.front->prev == nullptr);
            queueBackUnchecked(*other.front, *other.back);
        }
        other.clear();
    }

    void queueBack(T& item)
    {
        SC_ASSERT_DEBUG(item.next == nullptr and item.prev == nullptr);
        queueBackUnchecked(item, item);
    }

  private:
    void queueBackUnchecked(T& item, T& newBack)
    {
        if (back)
        {
            back->next = &item;
            item.prev  = back;
        }
        else
        {
            SC_ASSERT_DEBUG(front == nullptr);
            front = &item;
        }
        back = &newBack;
        SC_ASSERT_DEBUG(back->next == nullptr);
        SC_ASSERT_DEBUG(front->prev == nullptr);
    }

  public:
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
#if SC_CONFIGURATION_DEBUG
        bool found = false;
        auto it    = front;
        while (it)
        {
            if (it == &item)
            {
                found = true;
                break;
            }
            it = it->next;
        }
        SC_ASSERT_DEBUG(found);
#endif
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
