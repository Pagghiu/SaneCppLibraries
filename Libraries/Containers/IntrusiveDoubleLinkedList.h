// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../Foundation/Assert.h"

namespace SC
{
template <typename T>
struct IntrusiveDoubleLinkedList;
} // namespace SC

//! @addtogroup group_containers
//! @{

/// @brief An Intrusive Double Linked List
/// @tparam T The Type being linked. It must declare two pointers to itself named `next` and `prev`.
///
/// This is an useful data structure when we want to delegate the allocation strategy to caller. @n
/// Both @ref library_async and @ref library_process use this data structure to store requests.
///
/// \snippet Libraries/Containers/Tests/IntrusiveDoubleLinkedListTest.cpp IntrusiveDoubleLinkedListSnippet
template <typename T>
struct SC::IntrusiveDoubleLinkedList
{
    T* back  = nullptr; // has no next
    T* front = nullptr; // has no prev

    [[nodiscard]] T* peekFront() const { return front; }

    [[nodiscard]] bool isEmpty() const { return peekFront() == nullptr; }

    void clear()
    {
        for (T* current = front; current != nullptr;)
        {
            T* next       = static_cast<T*>(current->next);
            current->next = nullptr;
            current->prev = nullptr;
            current       = next;
        }
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
        front   = static_cast<T*>(item->next);
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
            it = static_cast<T*>(it->next);
        }
        SC_ASSERT_DEBUG(found);
#endif
        if (&item == front)
        {
            front = static_cast<T*>(front->next);
        }
        if (&item == back)
        {
            back = static_cast<T*>(back->prev);
        }

        T* next = static_cast<T*>(item.next);
        T* prev = static_cast<T*>(item.prev);

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
//! @}
