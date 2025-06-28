// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
//! @addtogroup group_foundation_utility
//! @{
namespace SC
{
/// @brief An Intrusive Double Linked List
/// @tparam T The Type being linked. It must declare two pointers to itself named `next` and `prev`.
///
/// This is an useful data structure when we want to delegate the allocation strategy to caller. @n
/// Both @ref library_async and @ref library_process use this data structure to store requests.
/// @note Include `Internal/IntrusiveDoubleLinkedList.inl` in the .cpp where any of the methods will be used.
///
/// \snippet Tests/Libraries/Foundation/IntrusiveDoubleLinkedListTest.cpp IntrusiveDoubleLinkedListSnippet
template <typename T>
struct IntrusiveDoubleLinkedList
{
    T* back  = nullptr; // has no next
    T* front = nullptr; // has no prev

    /// @brief Return true if the linked list is empty
    [[nodiscard]] bool isEmpty() const { return front == nullptr; }

    /// @brief Removes and returns the first element of the linked list
    [[nodiscard]] T* dequeueFront();

    /// @brief Clears this linked list removing links between all linked list elements
    void clear();

    /// @brief Appends another list at the back of current list
    void appendBack(IntrusiveDoubleLinkedList& other);

    /// @brief Appends item to the back of this linked list
    void queueBack(T& item);

    /// @brief Removes item from this linked list
    void remove(T& item);

  private:
    void queueBackUnchecked(T& item, T& newBack);
};
} // namespace SC
//! @}
