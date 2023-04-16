// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../Testing/Test.h"
#include "IntrusiveDoubleLinkedList.h"

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
    }
};
