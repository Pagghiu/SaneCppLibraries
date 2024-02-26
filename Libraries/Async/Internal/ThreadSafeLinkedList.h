// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../../Threading/Threading.h"

namespace SC
{
template <typename T>
struct ThreadSafeLinkedList
{
    void push(T& item)
    {
        mutex.lock();
        if (head == nullptr)
        {
            head = &item;
            tail = &item;
        }
        else
        {
            tail->next = &item;
            tail       = &item;
        }
        mutex.unlock();
    }

    T* pop()
    {
        mutex.lock();
        T* item = head;
        if (head)
        {
            head = head->next;
        }

        if (head == nullptr)
        {
            tail = nullptr;
        }
        mutex.unlock();
        return item;
    }

    void remove(T& item)
    {
        mutex.lock();
        T* current = head;
        T* prev    = nullptr;

        do
        {
            if (current == &item)
            {
                if (prev)
                {
                    prev->next = current->next;
                }
                if (current == head)
                {
                    head = current->next;
                }
                if (current == tail)
                {
                    tail = prev;
                }
                else
                {
                    current->next = nullptr;
                }
                break;
            }
            prev    = current;
            current = current->next;
        } while (current != nullptr);
        mutex.unlock();
    }

  private:
    Mutex mutex;

    T* head = nullptr;
    T* tail = nullptr;
};
} // namespace SC
