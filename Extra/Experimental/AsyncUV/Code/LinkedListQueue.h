// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once

namespace SC
{

template <typename T, typename Q, Q T::* Field>
struct LinkedListQueue
{
    LinkedListQueue()
    {
        queue.next = &queue;
        queue.prev = &queue;
    }

    [[nodiscard]] bool isEmpty() const { return &queue == queue.next; }

    void clear()
    {
        for (auto* it = queue.next; it != &queue; it = it->next)
        {
            it->next = it;
            it->prev = it;
        }
        queue.next = &queue;
        queue.prev = &queue;
    }

    T* dequeueFront()
    {
        if (isEmpty())
        {
            return nullptr;
        }
        auto* item = queue.next;
        T*    t    = reinterpret_cast<T*>(&item.*Field);
        remove(*t);
        return t;
    }

    void queueBack(T& t)
    {
        Q& item         = *reinterpret_cast<Q*>(&(t.*Field));
        item.next       = &queue;
        item.prev       = queue.prev;
        item.prev->next = &item;
        queue.prev      = &item;
    }

    void queueFront(T& t)
    {
        Q& item         = *reinterpret_cast<Q*>(&(t.*Field));
        item.prev       = &queue;
        item.next       = queue.next;
        item.next->prev = &item;
        queue.next      = &item;
    }

    void remove(T& t)
    {
        Q& item         = *reinterpret_cast<Q*>(&(t.*Field));
        item.prev->next = item.next;
        item.next->prev = item.prev;
    }

    template <typename Lambda>
    void forEach(Lambda&& lambda) const
    {
        static const auto QueueOffset = offset_of<Q, T>(Field);

        Q* it = queue.next;
        while (it != &queue)
        {
            Q* next = it->next;
            T* item = reinterpret_cast<T*>(reinterpret_cast<char*>(it) - QueueOffset);
            if (not lambda(*item))
            {
                break;
            }
            it = next;
        }
    }

  private:
    template <typename T1, typename T2>
    static size_t constexpr offset_of(T1 T2::* member)
    {
        constexpr T2 object{};
        return size_t(&(object.*member)) - size_t(&object);
    }
    Q queue;
};

} // namespace SC
