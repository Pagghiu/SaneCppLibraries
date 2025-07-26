// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../../Foundation/Assert.h"
#include "IntrusiveDoubleLinkedList.h"

template <typename T>
void SC::IntrusiveDoubleLinkedList<T>::clear()
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

template <typename T>
void SC::IntrusiveDoubleLinkedList<T>::appendBack(IntrusiveDoubleLinkedList& other)
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

template <typename T>
void SC::IntrusiveDoubleLinkedList<T>::queueBack(T& item)
{
    SC_ASSERT_DEBUG(item.next == nullptr and item.prev == nullptr);
    queueBackUnchecked(item, item);
}

template <typename T>
void SC::IntrusiveDoubleLinkedList<T>::queueBackUnchecked(T& item, T& newBack)
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

template <typename T>
[[nodiscard]] T* SC::IntrusiveDoubleLinkedList<T>::dequeueFront()
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

template <typename T>
void SC::IntrusiveDoubleLinkedList<T>::remove(T& item)
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
