// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "Libraries/Containers/IntrusiveDoubleLinkedList.h"
#include "Libraries/Testing/Testing.h"

namespace SC
{
struct IntrusiveDoubleLinkedListTest;
}
struct SC::IntrusiveDoubleLinkedListTest : public SC::TestCase
{

    struct Item
    {
        Item* next = nullptr;
        Item* prev = nullptr;
        int   data = 0;
    };

    IntrusiveDoubleLinkedListTest(SC::TestReport& report) : TestCase(report, "IntrusiveDoubleLinkedListTest")
    {
        using namespace SC;
        if (test_section("basic"))
        {
            IntrusiveDoubleLinkedList<Item> queue;

            Item items[2];
            SC_TEST_EXPECT(queue.isEmpty());
            {
                Item* currentItem = items;
                currentItem->data = 0;
                queue.queueBack(*currentItem);
                currentItem++;
                currentItem->data = 1;
                queue.queueBack(*currentItem);
            }
            SC_TEST_EXPECT(not queue.isEmpty());
            Item* first = queue.dequeueFront();
            SC_TEST_EXPECT(first->data == 0);
            SC_TEST_EXPECT(not queue.isEmpty());

            Item* second = queue.dequeueFront();
            SC_TEST_EXPECT(second->data == 1);
            SC_TEST_EXPECT(queue.isEmpty());
        }
        if (test_section("remove"))
        {
            IntrusiveDoubleLinkedList<Item> queue;

            Item items[3];
            items[0].data = 0;
            items[1].data = -1;
            items[2].data = 1;
            queue.queueBack(items[0]);
            queue.queueBack(items[1]);
            queue.queueBack(items[2]);

            queue.remove(items[1]);
            queue.remove(items[0]);
            queue.remove(items[2]);
            SC_TEST_EXPECT(queue.isEmpty());
            SC_TEST_EXPECT(queue.back == nullptr and queue.front == nullptr);
            SC_TEST_EXPECT(items[0].next == nullptr);
            SC_TEST_EXPECT(items[0].prev == nullptr);
            SC_TEST_EXPECT(items[1].next == nullptr);
            SC_TEST_EXPECT(items[1].prev == nullptr);
            SC_TEST_EXPECT(items[2].next == nullptr);
            SC_TEST_EXPECT(items[2].prev == nullptr);
        }

        if (test_section("queue"))
        {
            IntrusiveDoubleLinkedList<Item> queue1, queue2;

            Item items1[3];
            items1[0].data = 0;
            items1[1].data = 1;
            items1[2].data = 2;
            queue1.queueBack(items1[0]);
            queue1.queueBack(items1[1]);
            queue1.queueBack(items1[2]);
            Item items2[3];
            items2[0].data = 3;
            items2[1].data = 4;
            items2[2].data = 5;
            queue2.queueBack(items2[0]);
            queue2.queueBack(items2[1]);
            queue2.queueBack(items2[2]);

            queue1.appendBack(queue2);

            SC_TEST_EXPECT(queue2.isEmpty());
            int expected = 0;
            while (Item* item = queue1.dequeueFront())
            {
                SC_TEST_EXPECT(item->data == expected);
                expected++;
            }
        }
    }

    bool intrusiveDoubleLinkedListSnippet();
};

bool SC::IntrusiveDoubleLinkedListTest::intrusiveDoubleLinkedListSnippet()
{
    //! [IntrusiveDoubleLinkedListSnippet]
    struct Node
    {
        Node* next = nullptr; // <-- Required by IntrusiveDoubleLinkedList
        Node* prev = nullptr; // <-- Required by IntrusiveDoubleLinkedList
        int   data = 0;
    };
    IntrusiveDoubleLinkedList<Node> queue;

    Node items[2];
    items[0].data = 0;
    items[1].data = 1;
    SC_TRY(queue.isEmpty());

    queue.queueBack(items[0]);
    queue.queueBack(items[1]);

    SC_TRY(not queue.isEmpty());
    Node* first = queue.dequeueFront();
    SC_TRY(first->data == 0);
    SC_TRY(not queue.isEmpty());

    Node* second = queue.dequeueFront();
    SC_TRY(second->data == 1);
    SC_TRY(queue.isEmpty());
    //! [IntrusiveDoubleLinkedListSnippet]
    return true;
}

namespace SC
{
void runIntrusiveDoubleLinkedListTest(SC::TestReport& report) { IntrusiveDoubleLinkedListTest test(report); }
} // namespace SC
