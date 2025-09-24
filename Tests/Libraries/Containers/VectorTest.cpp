// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "Libraries/Containers/Vector.h"
#include "Libraries/Containers/Algorithms/AlgorithmBubbleSort.h"
#include "Libraries/Memory/Buffer.h"
#include "Libraries/Memory/String.h"
#include "Libraries/Strings/Console.h"
#include "Libraries/Strings/StringView.h"
#include "Libraries/Testing/Limits.h"
#include "Libraries/Testing/Testing.h"

namespace SC
{
struct VectorTest;
}

struct SC::VectorTest : public SC::TestCase
{
    static constexpr SC::size_t INSANE_NUMBER = SC::MaxValue();
    VectorTest(SC::TestReport& report);
    void basicTests();
    void testClassType();
    void testBasicType();
    bool vectorSnippet();
};

namespace SC
{
struct VectorTestReport;
struct VectorTestClass;
} // namespace SC

struct SC::VectorTestReport
{
    enum Operation
    {
        CONSTRUCTOR,
        DESTRUCTOR,
        MOVE_CONSTRUCTOR,
        COPY_CONSTRUCTOR,
        COPY_ASSIGNMENT,
        MOVE_ASSIGNMENT
    };
    static const uint32_t MAX_SEQUENCES = 100;

    Operation sequence[MAX_SEQUENCES];
    uint32_t  numSequences     = 0;
    uint32_t  numNextSequences = 0;

    void push(Operation operation)
    {
        SC_ASSERT_DEBUG(numSequences < MAX_SEQUENCES);
        sequence[numSequences++] = operation;
    }

    void reset()
    {
        numSequences     = 0;
        numNextSequences = 0;
    }

    Operation nextOperation()
    {
        SC_ASSERT_DEBUG(numNextSequences < numSequences);
        return sequence[numNextSequences++];
    }
    static SC::VectorTestReport& get()
    {
        static SC::VectorTestReport rep;
        return rep;
    }
};

struct SC::VectorTestClass
{
    char* data;
    VectorTestClass(const char* initData)
    {
        copyString(initData);
        SC::VectorTestReport::get();
        VectorTestReport::get().push(VectorTestReport::CONSTRUCTOR);
    }

    VectorTestClass() : data(nullptr) { VectorTestReport::get().push(VectorTestReport::CONSTRUCTOR); }

    VectorTestClass(const VectorTestClass& other)
    {
        data = nullptr;
        if (other.data != nullptr)
        {
            copyString(other.data);
        }
        VectorTestReport::get().push(VectorTestReport::COPY_CONSTRUCTOR);
    }

    VectorTestClass(VectorTestClass&& other) : data(other.data)
    {
        other.data = nullptr;
        VectorTestReport::get().push(VectorTestReport::MOVE_CONSTRUCTOR);
    }

    VectorTestClass& operator=(const VectorTestClass& other)
    {
        if (this != &other)
        {
            if (data != nullptr)
                Memory::release(data);
            data = nullptr;
            if (other.data != nullptr)
                copyString(other.data);
        }
        VectorTestReport::get().push(VectorTestReport::COPY_ASSIGNMENT);
        return *this;
    }

    VectorTestClass& operator=(VectorTestClass&& other)
    {
        if (this != &other)
        {
            if (data != nullptr)
                Memory::release(data);
            data       = other.data;
            other.data = nullptr;
        }
        VectorTestReport::get().push(VectorTestReport::MOVE_ASSIGNMENT);
        return *this;
    }

    StringView toString() const
    {
        if (data == nullptr)
        {
            return StringView();
        }
        else
        {
            return StringView({data, dataLength(data)}, true, StringEncoding::Ascii);
        }
    }

    ~VectorTestClass()
    {
        VectorTestReport::get().push(VectorTestReport::DESTRUCTOR);
        Memory::release(data);
    }

  private:
    static size_t dataLength(const char* str)
    {
        size_t idx = 0;
        while (str[idx] != 0)
        {
            ++idx;
        }
        return idx;
    }
    void copyString(const char* initData)
    {
        const size_t numBytes = dataLength(initData) + 1;
        data                  = static_cast<char*>(Memory::allocate(numBytes, 1));
        memcpy(data, initData, numBytes);
    }
};

SC::VectorTest::VectorTest(SC::TestReport& report) : TestCase(report, "VectorTest")
{
    basicTests();
    testBasicType();
    testClassType();
}

void SC::VectorTest::basicTests()
{
    if (test_section("remove"))
    {
        Vector<String> strings;
        SC_TEST_EXPECT(strings.append({"0", "1", "2", "3", "4", "5"}));
        SC_TEST_EXPECT(strings[1] == "1");
        SC_TEST_EXPECT(strings.removeAt(1));
        SC_TEST_EXPECT(strings[1] == "2");
        SC_TEST_EXPECT(strings.removeAt(3));
    }
    if (test_section("insert"))
    {
        Vector<String> strings;
        // insert empty
        SC_TEST_EXPECT(strings.insert(0, {"3"}));
        SC_TEST_EXPECT(strings[0] == "3");
        // insert front
        SC_TEST_EXPECT(strings.insert(0, {"0"}));
        SC_TEST_EXPECT(strings[0] == "0");
        SC_TEST_EXPECT(strings[1] == "3");
        SC_TEST_EXPECT(strings.reserve(3));
        SC_TEST_EXPECT(strings[0] == "0");
        SC_TEST_EXPECT(strings[1] == "3");
        // insert end
        SC_TEST_EXPECT(strings.insert(2, {"5"}));
        SC_TEST_EXPECT(strings[0] == "0");
        SC_TEST_EXPECT(strings[1] == "3");
        SC_TEST_EXPECT(strings[2] == "5");
        // insert one before end (no moved in elements)
        SC_TEST_EXPECT(strings.insert(2, {"4"}));
        SC_TEST_EXPECT(strings[0] == "0");
        SC_TEST_EXPECT(strings[1] == "3");
        SC_TEST_EXPECT(strings[2] == "4");
        SC_TEST_EXPECT(strings[3] == "5");
        // insert 3 before end (1 move assigned + 2 move constructed elements)
        SC_TEST_EXPECT(strings.insert(1, {"1", "2"}));
        SC_TEST_EXPECT(strings[0] == "0");
        SC_TEST_EXPECT(strings[1] == "1");
        SC_TEST_EXPECT(strings[2] == "2");
        SC_TEST_EXPECT(strings[3] == "3");
        SC_TEST_EXPECT(strings[4] == "4");
        SC_TEST_EXPECT(strings[5] == "5");
        SC_TEST_EXPECT(strings.size() == 6);
        // sanity check to allow inserting zero size with no effect
        SC_TEST_EXPECT(strings.insert(0, {}));
        SC_TEST_EXPECT(strings.size() == 6);
        SC_TEST_EXPECT(strings[0] == "0");
        SC_TEST_EXPECT(strings[1] == "1");
        SC_TEST_EXPECT(strings[2] == "2");
        SC_TEST_EXPECT(strings[3] == "3");
        SC_TEST_EXPECT(strings[4] == "4");
        SC_TEST_EXPECT(strings[5] == "5");
        // insert error outside range (after last element)
        SC_TEST_EXPECT(not strings.insert(7, {"6"}));
        {
            // Special insert case that will test some edge cases
            strings = {};
            SC_TEST_EXPECT(strings.insert(0, {"STRING1"_a8}));
            SC_TEST_EXPECT(strings.insert(0, {"STRING2"_a8, "STRING3"_a8}));
        }
    }

    if (test_section("Vector"))
    {
        Vector<String> strings;
        SC_TEST_EXPECT(strings.resize(2, "ASDF"));
        SC_TEST_EXPECT(strings[0] == "ASDF");
        SC_TEST_EXPECT(strings[1] == "ASDF");
        strings.front() = "ASDF1";
        strings.back()  = "ASDF2";

        Vector<String> strings2 = strings;
        strings.clear();
        SC_TEST_EXPECT(strings.isEmpty());
        SC_TEST_EXPECT(strings2[0] == "ASDF1");
        SC_TEST_EXPECT(strings2[1] == "ASDF2");
        Vector<String> strings3;
        SC_TEST_EXPECT(strings3.append({"1234", "5678"}));
        SC_TEST_EXPECT(strings3.size() == 2);
        SC_TEST_EXPECT(strings3.capacity() == 2);
        SC_TEST_EXPECT(strings3[0] == "1234");
        SC_TEST_EXPECT(strings3[1] == "5678");
        SC_TEST_EXPECT(strings3.append({"yeah", "ohyeah"}));
        SC_TEST_EXPECT(strings3.size() == 4);
        SC_TEST_EXPECT(strings3.capacity() >= 4);
        SC_TEST_EXPECT(strings3[0] == "1234");
        SC_TEST_EXPECT(strings3[1] == "5678");
        SC_TEST_EXPECT(strings3[2] == "yeah");
        SC_TEST_EXPECT(strings3[3] == "ohyeah");
        SC_TEST_EXPECT(strings3.resize(2));
        SC_TEST_EXPECT(strings3.size() == 2);
        SC_TEST_EXPECT(strings3.capacity() >= 4);
        SC_TEST_EXPECT(strings3[0] == "1234");
        SC_TEST_EXPECT(strings3[1] == "5678");
        SC_TEST_EXPECT(strings3.shrink_to_fit());
        SC_TEST_EXPECT(strings3.size() == 2);
        SC_TEST_EXPECT(strings3.capacity() == 2);
        SC_TEST_EXPECT(strings3[0] == "1234");
        SC_TEST_EXPECT(strings3[1] == "5678");

        strings3 = move(strings2);
        SC_TEST_EXPECT(strings2.isEmpty());
        SC_TEST_EXPECT(strings3[0] == "ASDF1");
        SC_TEST_EXPECT(strings3[1] == "ASDF2");
        SC_TEST_EXPECT(strings3.push_back("ASDF3"));
        SC_TEST_EXPECT(strings3[2] == "ASDF3");
        SC_TEST_EXPECT(strings3.push_back("ASDF4"));

        String popped;
        SC_TEST_EXPECT(strings3.pop_back(&popped));
        SC_TEST_EXPECT(popped == "ASDF4");
        SC_TEST_EXPECT(strings3.removeAt(1));
        SC_TEST_EXPECT(strings3[1] == "ASDF3");
        SC_TEST_EXPECT(strings3.pop_front(&popped));
        SC_TEST_EXPECT(popped == "ASDF1");
        strings3.clear();
        SC_TEST_EXPECT(not strings3.pop_back());
    }

    if (test_section("Vector trivial"))
    {
        Vector<int> strings;
        // insert empty
        SC_TEST_EXPECT(strings.insert(0, {3}));
        SC_TEST_EXPECT(strings[0] == 3);
        // insert front
        SC_TEST_EXPECT(strings.insert(0, {0}));
        SC_TEST_EXPECT(strings[0] == 0);
        SC_TEST_EXPECT(strings[1] == 3);
        SC_TEST_EXPECT(strings.reserve(3));
        SC_TEST_EXPECT(strings[0] == 0);
        SC_TEST_EXPECT(strings[1] == 3);
        // insert end
        SC_TEST_EXPECT(strings.insert(2, {5}));
        SC_TEST_EXPECT(strings[0] == 0);
        SC_TEST_EXPECT(strings[1] == 3);
        SC_TEST_EXPECT(strings[2] == 5);
        // insert one before end (no moved in elements)
        SC_TEST_EXPECT(strings.insert(2, {4}));
        SC_TEST_EXPECT(strings[0] == 0);
        SC_TEST_EXPECT(strings[1] == 3);
        SC_TEST_EXPECT(strings[2] == 4);
        SC_TEST_EXPECT(strings[3] == 5);
        // insert 3 before end (1 move assigned + 2 move constructed elements)
        SC_TEST_EXPECT(strings.insert(1, {1, 2}));
        SC_TEST_EXPECT(strings[0] == 0);
        SC_TEST_EXPECT(strings[1] == 1);
        SC_TEST_EXPECT(strings[2] == 2);
        SC_TEST_EXPECT(strings[3] == 3);
        SC_TEST_EXPECT(strings[4] == 4);
        SC_TEST_EXPECT(strings[5] == 5);
        SC_TEST_EXPECT(strings.size() == 6);
        // sanity check to allow inserting zero size with no effect
        SC_TEST_EXPECT(strings.insert(0, {}));
        SC_TEST_EXPECT(strings.size() == 6);
        SC_TEST_EXPECT(strings[0] == 0);
        SC_TEST_EXPECT(strings[1] == 1);
        SC_TEST_EXPECT(strings[2] == 2);
        SC_TEST_EXPECT(strings[3] == 3);
        SC_TEST_EXPECT(strings[4] == 4);
        SC_TEST_EXPECT(strings[5] == 5);
        // insert error outside range (after last element)
        SC_TEST_EXPECT(not strings.insert(7, {6}));
    }
}
void SC::VectorTest::testClassType()
{
    using namespace SC;
    VectorTestReport& vecReport = VectorTestReport::get();
    vecReport.reset();
    if (test_section("class_resize"))
    {
        StringView      myString("MyData");
        VectorTestClass testClass(myString.bytesWithoutTerminator());
        SC_TEST_EXPECT(vecReport.nextOperation() == VectorTestReport::CONSTRUCTOR);
        SC_TEST_EXPECT(myString == testClass.toString());
        SC::Vector<VectorTestClass> myVector;
        SC_TEST_EXPECT(vecReport.numSequences == 1);
        vecReport.reset();
        SC_TEST_EXPECT(myVector.resize(2));
        SC_TEST_EXPECT(vecReport.numSequences == 4);
        SC_TEST_EXPECT(vecReport.nextOperation() == VectorTestReport::CONSTRUCTOR);      // DEFAULT PARAM
        SC_TEST_EXPECT(vecReport.nextOperation() == VectorTestReport::COPY_CONSTRUCTOR); // FIRST ELEMENT
        SC_TEST_EXPECT(vecReport.nextOperation() == VectorTestReport::COPY_CONSTRUCTOR); // SECOND ELEMENT
        SC_TEST_EXPECT(vecReport.nextOperation() == VectorTestReport::DESTRUCTOR);       // DEFAULT PARAM DESTRUCTOR
        SC_TEST_EXPECT(myVector[0].toString().isEmpty());
        SC_TEST_EXPECT(myVector[1].toString().isEmpty());

        vecReport.reset();
        SC_TEST_EXPECT(myVector.resize(3, VectorTestClass("Custom")));
        SC_TEST_EXPECT(vecReport.numSequences == 7);
        SC_TEST_EXPECT(vecReport.nextOperation() == VectorTestReport::CONSTRUCTOR);      // DEFAULT PARAM
        SC_TEST_EXPECT(vecReport.nextOperation() == VectorTestReport::MOVE_CONSTRUCTOR); // ITEM[1] CONSTRUCTOR
        SC_TEST_EXPECT(vecReport.nextOperation() == VectorTestReport::MOVE_CONSTRUCTOR); // ITEM[2] CONSTRUCTOR
        SC_TEST_EXPECT(vecReport.nextOperation() == VectorTestReport::DESTRUCTOR);       // ITEM[1] MOVED_DESTRUCTOR
        SC_TEST_EXPECT(vecReport.nextOperation() == VectorTestReport::DESTRUCTOR);       // ITEM[2] MOVED_DESTRUCTOR
        SC_TEST_EXPECT(vecReport.nextOperation() == VectorTestReport::COPY_CONSTRUCTOR); // ITEM[3] COPY_CONSTRUCTOR
        // (DEFAULT PARAM)
        SC_TEST_EXPECT(vecReport.nextOperation() == VectorTestReport::DESTRUCTOR); // DEFAULT PARAM DESTRUCTOR
        SC_TEST_EXPECT(myVector[0].toString().isEmpty());
        SC_TEST_EXPECT(myVector[1].toString().isEmpty());
        SC_TEST_EXPECT(myVector[2].toString() == StringView("Custom"));
        vecReport.reset();
        SC_TEST_EXPECT(myVector.resize(2));
        SC_TEST_EXPECT(vecReport.numSequences == 3);
        SC_TEST_EXPECT(vecReport.nextOperation() == VectorTestReport::CONSTRUCTOR); // DEFAULT PARAM
        SC_TEST_EXPECT(vecReport.nextOperation() == VectorTestReport::DESTRUCTOR);  // ITEM[3] DESTRUCTOR
        SC_TEST_EXPECT(vecReport.nextOperation() == VectorTestReport::DESTRUCTOR);  // DEFAULT PARAM DESTRUCTOR
        SC_TEST_EXPECT(myVector.resize(0));
        vecReport.reset();
        SC_TEST_EXPECT(myVector.resize(1));
        SC_TEST_EXPECT(vecReport.numSequences == 3);
        SC_TEST_EXPECT(vecReport.nextOperation() == VectorTestReport::CONSTRUCTOR);      // DEFAULT PARAM
        SC_TEST_EXPECT(vecReport.nextOperation() == VectorTestReport::COPY_CONSTRUCTOR); // ITEM[3] COPY_CONSTRUCTOR
        SC_TEST_EXPECT(not myVector.resize(INSANE_NUMBER));
    }

    if (test_section("class_shrink_to_fit"))
    {
        SC::Vector<VectorTestClass> myVector;
        SC_TEST_EXPECT(myVector.shrink_to_fit());
        SC_TEST_EXPECT(myVector.size() == 0);
        SC_TEST_EXPECT(myVector.capacity() == 0);
        SC_TEST_EXPECT(myVector.resize(3));
        SC_TEST_EXPECT(myVector.resize(2));
        vecReport.reset();
        SC_TEST_EXPECT(myVector.shrink_to_fit());
        SC_TEST_EXPECT(vecReport.numSequences == 4);
        SC_TEST_EXPECT(vecReport.nextOperation() == VectorTestReport::MOVE_CONSTRUCTOR); // ITEM[1] CONSTRUCTOR
        SC_TEST_EXPECT(vecReport.nextOperation() == VectorTestReport::MOVE_CONSTRUCTOR); // ITEM[2] CONSTRUCTOR
        SC_TEST_EXPECT(vecReport.nextOperation() == VectorTestReport::DESTRUCTOR);       // ITEM[1] MOVE_DESTRUCTOR
        SC_TEST_EXPECT(vecReport.nextOperation() == VectorTestReport::DESTRUCTOR);       // ITEM[2] MOVE_DESTRUCTOR
    }

    if (test_section("class_clear"))
    {
        SC::Vector<VectorTestClass> myVector;
        SC_TEST_EXPECT(myVector.resize(2));
        vecReport.reset();
        myVector.clear();
        SC_TEST_EXPECT(vecReport.numSequences == 2);
        SC_TEST_EXPECT(vecReport.nextOperation() == VectorTestReport::DESTRUCTOR); // ITEM[1] DESTRUCTOR
        SC_TEST_EXPECT(vecReport.nextOperation() == VectorTestReport::DESTRUCTOR); // ITEM[2] DESTRUCTOR
    }

    if (test_section("class_reserve"))
    {
        SC::Vector<VectorTestClass> newVector;
        vecReport.reset();
        SC_TEST_EXPECT(newVector.reserve(2));
        SC_TEST_EXPECT(newVector.reserve(1));
        SC_TEST_EXPECT(newVector.size() == 0);
        SC_TEST_EXPECT(newVector.capacity() == 2);
        SC_TEST_EXPECT(vecReport.numSequences == 0);
    }

    if (test_section("class_destructor"))
    {
        {
            SC::Vector<VectorTestClass> newVector;
            vecReport.reset();
            SC_TEST_EXPECT(newVector.resize(2, VectorTestClass("CIAO")));
        }
        SC_TEST_EXPECT(vecReport.numSequences == 6);

        SC_TEST_EXPECT(vecReport.nextOperation() == VectorTestReport::CONSTRUCTOR);      // DEFAULT PARAM
        SC_TEST_EXPECT(vecReport.nextOperation() == VectorTestReport::COPY_CONSTRUCTOR); // ITEM[1] COPY CONSTRUCTOR
        SC_TEST_EXPECT(vecReport.nextOperation() == VectorTestReport::COPY_CONSTRUCTOR); // ITEM[2] COPY CONSTRUCTOR
        SC_TEST_EXPECT(vecReport.nextOperation() == VectorTestReport::DESTRUCTOR);       // DEFAULT PARAM DESTRUCTOR
        SC_TEST_EXPECT(vecReport.nextOperation() == VectorTestReport::DESTRUCTOR);       // ITEM[1] DESTRUCTOR
        SC_TEST_EXPECT(vecReport.nextOperation() == VectorTestReport::DESTRUCTOR);       // ITEM[2] DESTRUCTOR
    }

    if (test_section("class_copy_construct"))
    {
        SC::Vector<VectorTestClass> newVector;
        vecReport.reset();
        VectorTestClass value = VectorTestClass("CIAO");
        SC_TEST_EXPECT(newVector.resize(2, value));
        SC::Vector<VectorTestClass> otherVector = newVector;
        SC_TEST_EXPECT(otherVector.size() == 2);
        SC_TEST_EXPECT(otherVector.capacity() == 2);
        SC_TEST_EXPECT(otherVector[0].toString() == StringView("CIAO"));
        SC_TEST_EXPECT(otherVector[1].toString() == StringView("CIAO"));
    }

    if (test_section("class_copy_assign"))
    {
        SC::Vector<VectorTestClass> newVector, otherVector;
        vecReport.reset();
        VectorTestClass value = VectorTestClass("CIAO");
        SC_TEST_EXPECT(newVector.resize(2, value));
        otherVector = newVector;
        SC_TEST_EXPECT(otherVector.size() == 2);
        SC_TEST_EXPECT(otherVector.capacity() == 2);
        SC_TEST_EXPECT(otherVector[0].toString() == StringView("CIAO"));
        SC_TEST_EXPECT(otherVector[1].toString() == StringView("CIAO"));
    }

    if (test_section("class_move_assign"))
    {
        SC::Vector<VectorTestClass> newVector, otherVector;
        vecReport.reset();
        VectorTestClass value = VectorTestClass("CIAO");
        SC_TEST_EXPECT(newVector.resize(2, value));
        SC_TEST_EXPECT(otherVector.resize(2, value));
        vecReport.reset();
        otherVector = move(newVector);
        SC_TEST_EXPECT(vecReport.numSequences == 2);
        SC_TEST_EXPECT(vecReport.nextOperation() == VectorTestReport::DESTRUCTOR); // ITEM[1] DESTRUCTOR
        SC_TEST_EXPECT(vecReport.nextOperation() == VectorTestReport::DESTRUCTOR); // ITEM[2] DESTRUCTOR
        SC_TEST_EXPECT(newVector.size() == 0);
        // SC_TEST_EXPECT(newVector.items == nullptr);
        SC_TEST_EXPECT(otherVector.size() == 2);
        SC_TEST_EXPECT(otherVector.capacity() == 2);
        SC_TEST_EXPECT(otherVector[0].toString() == StringView("CIAO"));
        SC_TEST_EXPECT(otherVector[1].toString() == StringView("CIAO"));
    }

    if (test_section("class_copy_assign"))
    {
        SC::Vector<VectorTestClass> newVector, otherVector;
        vecReport.reset();
        VectorTestClass value = VectorTestClass("CIAO");
        SC_TEST_EXPECT(newVector.resize(2, value));
        SC_TEST_EXPECT(otherVector.resize(2, value));
        vecReport.reset();
        otherVector = newVector;
        SC_TEST_EXPECT(vecReport.numSequences == 2);
        SC_TEST_EXPECT(vecReport.nextOperation() == VectorTestReport::COPY_ASSIGNMENT); // ITEM[1] COPY
        SC_TEST_EXPECT(vecReport.nextOperation() == VectorTestReport::COPY_ASSIGNMENT); // ITEM[2] COPY
        SC_TEST_EXPECT(newVector.size() == 2);
        // SC_TEST_EXPECT(newVector.items == nullptr);
        SC_TEST_EXPECT(otherVector.size() == 2);
        SC_TEST_EXPECT(otherVector.capacity() == 2);
        SC_TEST_EXPECT(otherVector[0].toString() == StringView("CIAO"));
        SC_TEST_EXPECT(otherVector[1].toString() == StringView("CIAO"));
        SC_TEST_EXPECT(newVector.resize(4));
        vecReport.reset();
        otherVector = newVector;
        SC_TEST_EXPECT(vecReport.numSequences == 6);
        SC_TEST_EXPECT(vecReport.nextOperation() == VectorTestReport::DESTRUCTOR);       // ITEM[1] DESTRUCTOR
        SC_TEST_EXPECT(vecReport.nextOperation() == VectorTestReport::DESTRUCTOR);       // ITEM[2] DESTRUCTOR
        SC_TEST_EXPECT(vecReport.nextOperation() == VectorTestReport::COPY_CONSTRUCTOR); // ITEM[1] COPY_CONSTRUCTOR
        SC_TEST_EXPECT(vecReport.nextOperation() == VectorTestReport::COPY_CONSTRUCTOR); // ITEM[2] COPY_CONSTRUCTOR
        SC_TEST_EXPECT(vecReport.nextOperation() == VectorTestReport::COPY_CONSTRUCTOR); // ITEM[1] COPY_CONSTRUCTOR
        SC_TEST_EXPECT(vecReport.nextOperation() == VectorTestReport::COPY_CONSTRUCTOR); // ITEM[2] COPY_CONSTRUCTOR
        SC_TEST_EXPECT(newVector.resize(2));
        vecReport.reset();
        otherVector = newVector;
        SC_TEST_EXPECT(vecReport.numSequences == 4);
        SC_TEST_EXPECT(vecReport.nextOperation() == VectorTestReport::COPY_ASSIGNMENT); // ITEM[1] COPY_CONSTRUCTOR
        SC_TEST_EXPECT(vecReport.nextOperation() == VectorTestReport::COPY_ASSIGNMENT); // ITEM[2] COPY_CONSTRUCTOR
        SC_TEST_EXPECT(vecReport.nextOperation() == VectorTestReport::DESTRUCTOR);      // ITEM[1] DESTRUCTOR
        SC_TEST_EXPECT(vecReport.nextOperation() == VectorTestReport::DESTRUCTOR);      // ITEM[2] DESTRUCTOR
                                                                                        //
    }
    if (test_section("class_insertMove_full_full_middle"))
    {
        SC::Vector<VectorTestClass> vector1, vector2;
        vecReport.reset();
        SC_TEST_EXPECT(vector1.push_back(VectorTestClass("0")));
        SC_TEST_EXPECT(vector1.push_back(VectorTestClass("3")));
        SC_TEST_EXPECT(vector1.push_back(VectorTestClass("4")));
        SC_TEST_EXPECT(vector2.push_back(VectorTestClass("1")));
        SC_TEST_EXPECT(vector2.push_back(VectorTestClass("2")));
        SC_TEST_EXPECT(vector1.insert(1, vector2.toSpan()));
        SC_TEST_EXPECT(vector1.insert(1, {vector2.begin(), 0}));
        SC_TEST_EXPECT(not vector1.insert(1, {vector2.begin(), INSANE_NUMBER}));
        SC_TEST_EXPECT(vector1.size() == 5);
        for (size_t idx = 0; idx < 5; ++idx)
        {
            int32_t value = 0;
            SC_TEST_EXPECT(vector1[idx].toString().parseInt32(value));
            SC_TEST_EXPECT(value == static_cast<int32_t>(idx));
        }
    }

    if (test_section("class_appendMove"))
    {
        SC::Vector<VectorTestClass> vector1, vector2;
        vecReport.reset();
        SC_TEST_EXPECT(vector1.push_back(VectorTestClass("0")));
        SC_TEST_EXPECT(vector1.push_back(VectorTestClass("1")));
        SC_TEST_EXPECT(vector1.push_back(VectorTestClass("2")));
        SC_TEST_EXPECT(vector2.push_back(VectorTestClass("3")));
        SC_TEST_EXPECT(vector2.push_back(VectorTestClass("4")));
        SC_TEST_EXPECT(vector1.appendMove(move(vector2)));
        SC_TEST_EXPECT(vector1.size() == 5);
        for (size_t idx = 0; idx < 5; ++idx)
        {
            int32_t value = 0;
            SC_TEST_EXPECT(vector1[idx].toString().parseInt32(value));
            SC_TEST_EXPECT(value == static_cast<int32_t>(idx));
        }
    }

    if (test_section("class_appendMove_empty"))
    {
        SC::Vector<VectorTestClass> vector1, vector2;
        vecReport.reset();
        SC_TEST_EXPECT(vector2.push_back(VectorTestClass("1")));
        SC_TEST_EXPECT(vector2.push_front(VectorTestClass("0")));
        SC_TEST_EXPECT(vector1.appendMove(move(vector2)));
        SC_TEST_EXPECT(vector1.size() == 2);
        int idx = 0;
        for (const auto& it : vector1)
        {
            int32_t value = 0;
            SC_TEST_EXPECT(it.toString().parseInt32(value));
            SC_TEST_EXPECT(value == idx++);
        }
    }

    if (test_section("class_push_back_pop_back"))
    {
        SC::Vector<VectorTestClass> test;
        vecReport.reset();
        SC_TEST_EXPECT(test.push_back(VectorTestClass("1")));
        int32_t value = -1;
        SC_TEST_EXPECT(test[0].toString().parseInt32(value));
        SC_TEST_EXPECT(value == 1);
        SC_TEST_EXPECT(test.push_back(VectorTestClass("2")));
        SC_TEST_EXPECT(test[0].toString().parseInt32(value));
        SC_TEST_EXPECT(value == 1);
        SC_TEST_EXPECT(test[1].toString().parseInt32(value));
        SC_TEST_EXPECT(value == 2);
        SC_TEST_EXPECT(test.size() == 2);
        SC_TEST_EXPECT(test.push_back(VectorTestClass("3")));
        SC_TEST_EXPECT(test.pop_front());
        SC_TEST_EXPECT(test.size() == 2);
        SC_TEST_EXPECT(test[0].toString().parseInt32(value));
        SC_TEST_EXPECT(value == 2);
        SC_TEST_EXPECT(test.pop_back());
        SC_TEST_EXPECT(test.size() == 1);
        SC_TEST_EXPECT(test[0].toString().parseInt32(value));
        SC_TEST_EXPECT(value == 2);
        SC_TEST_EXPECT(test.pop_back());
        SC_TEST_EXPECT(not test.pop_back());
        SC_TEST_EXPECT(not test.pop_front());
    }

    if (test_section("class_copy_assignment"))
    {
        SC::Vector<VectorTestClass> vector1, vector2;
        vecReport.reset();
        SC_TEST_EXPECT(vector1.push_back(VectorTestClass("0")));
        SC_TEST_EXPECT(vector1.push_back(VectorTestClass("1")));

        vector2 = vector1;
        SC_TEST_EXPECT(vector1.size() == 2);
        SC_TEST_EXPECT(vector2.size() == 2);
        SC_TEST_EXPECT(vector1[0].data != vector2[0].data);
        SC_TEST_EXPECT(vector1[1].data != vector2[1].data);
        int32_t value;
        SC_TEST_EXPECT(vector2[0].toString().parseInt32(value) && value == 0);
        SC_TEST_EXPECT(vector2[1].toString().parseInt32(value) && value == 1);
    }

    if (test_section("class_move_assignment"))
    {
        SC::Vector<VectorTestClass> vector1, vector2;
        vecReport.reset();
        SC_TEST_EXPECT(vector1.push_back(VectorTestClass("0")));
        SC_TEST_EXPECT(vector1.push_back(VectorTestClass("1")));

        vector2 = move(vector1);
        SC_TEST_EXPECT(vector1.data() == nullptr);
#ifndef __clang_analyzer__
        SC_TEST_EXPECT(vector1.size() == 0);
#endif // not __clang_analyzer__
        SC_TEST_EXPECT(vector2.size() == 2);
        int32_t value;
        SC_TEST_EXPECT(vector2[0].toString().parseInt32(value) && value == 0);
        SC_TEST_EXPECT(vector2[1].toString().parseInt32(value) && value == 1);
    }

    if (test_section("class_remove_at"))
    {
        SC::Vector<VectorTestClass> vector1;
        vecReport.reset();
        SC_TEST_EXPECT(vector1.push_back(VectorTestClass("0")));
        SC_TEST_EXPECT(vector1.push_back(VectorTestClass("1")));
        SC_TEST_EXPECT(vector1.push_back(VectorTestClass("2")));
        SC_TEST_EXPECT(vector1.push_back(VectorTestClass("3")));

        SC_TEST_EXPECT(not vector1.removeAt(10));
        SC_TEST_EXPECT(vector1.removeAt(1));
        int32_t value;
        SC_TEST_EXPECT(vector1[0].toString().parseInt32(value) && value == 0);
        SC_TEST_EXPECT(vector1[1].toString().parseInt32(value) && value == 2);
        SC_TEST_EXPECT(vector1[2].toString().parseInt32(value) && value == 3);
        SC_TEST_EXPECT(vector1.removeAll([&](const VectorTestClass& val)
                                         { return val.toString().parseInt32(value) && value <= 2; }));
        SC_TEST_EXPECT(vector1.size() == 1);
        SC_TEST_EXPECT(vector1[0].toString().parseInt32(value) && value == 3);
    }
}

void SC::VectorTest::testBasicType()
{
    using namespace SC;
    if (test_section("base_resize"))
    {
        Vector<int> elements;
        SC_TEST_EXPECT(elements.size() == 0);
        SC_TEST_EXPECT(elements.capacity() == 0);
        SC_TEST_EXPECT(not elements.resize(INSANE_NUMBER));

        SC_TEST_EXPECT(elements.resize(10, 11));
        elements[0]        = -1;
        size_t numFailures = 0;
        for (size_t idx = 0; idx < elements.size(); ++idx)
        {
            if (elements[idx] != 11)
            {
                numFailures++;
            }
            elements[idx] = static_cast<int>(idx);
        }

        SC_TEST_EXPECT(numFailures == 1);

        SC_TEST_EXPECT(not elements.resize(INSANE_NUMBER));
        SC_TEST_EXPECT(elements.size() == 10);
        SC_TEST_EXPECT(elements.size() == elements.capacity());
        SC_TEST_EXPECT(elements.reserve(elements.capacity() + 1));

        SC_TEST_EXPECT(elements.resize(20));
        elements[0] = -1;
        numFailures = 0;
        for (size_t idx = 0; idx < 10; ++idx)
        {
            if (elements[idx] != static_cast<int>(idx))
            {
                numFailures++;
            }
        }
        SC_TEST_EXPECT(numFailures == 1);
        numFailures  = 0;
        elements[10] = -1;
        for (size_t idx = 10; idx < 20; ++idx)
        {
            if (elements[idx] != 0)
            {
                numFailures++;
            }
        }
        SC_TEST_EXPECT(numFailures == 1);
        SC_TEST_EXPECT(elements.resize(5));
        SC_TEST_EXPECT(elements.size() == 5);
        SC_TEST_EXPECT(elements.capacity() == 20);
        SC_TEST_EXPECT(elements.shrink_to_fit());
        for (size_t idx = 0; idx < elements.size(); ++idx)
        {
            if (elements[idx] != static_cast<int>(idx))
            {
                numFailures++;
            }
        }
        SC_TEST_EXPECT(numFailures == 2);
        SC_TEST_EXPECT(elements.size() == 5);
        SC_TEST_EXPECT(elements.capacity() == 5);
        SC_TEST_EXPECT(elements.resizeWithoutInitializing(10));
    }
    if (test_section("base_clear"))
    {
        Vector<int> elements;
        SC_TEST_EXPECT(elements.resizeWithoutInitializing(10));
        elements.clear();
        SC_TEST_EXPECT(elements.size() == 0);
        SC_TEST_EXPECT(elements.capacity() == 10);
    }

    if (test_section("base_shrink_to_fit"))
    {
        Vector<int> elements;
        SC_TEST_EXPECT(elements.resizeWithoutInitializing(10));
        elements.clear();
        SC_TEST_EXPECT(elements.shrink_to_fit());
        SC_TEST_EXPECT(elements.size() == 0);
        SC_TEST_EXPECT(elements.capacity() == 0);
    }
    if (test_section("sort"))
    {
        Vector<int> elements;
        SC_TRUST_RESULT(elements.push_back(1));
        SC_TRUST_RESULT(elements.push_back(0));
        SC_TRUST_RESULT(elements.push_back(2));
        Algorithms::bubbleSort(elements.begin(), elements.end());
        SC_TEST_EXPECT(elements[0] == 0);
        SC_TEST_EXPECT(elements[1] == 1);
        SC_TEST_EXPECT(elements[2] == 2);
    }
    if (test_section("contains/find"))
    {
        Vector<int> elements;
        SC_TRUST_RESULT(elements.push_back(1));
        SC_TRUST_RESULT(elements.push_back(0));
        SC_TRUST_RESULT(elements.push_back(2));
        size_t index = 0;
        SC_TEST_EXPECT(elements.contains(2, &index) && index == 2);
        SC_TEST_EXPECT(not elements.contains(44));
        SC_TRUST_RESULT(elements.push_back(2));
        index = 0;
        SC_TEST_EXPECT(elements.find([](auto& val) { return val >= 2; }, &index) && index == 2);
    }
    if (test_section("removeAll"))
    {
        Vector<int> elements;
        SC_TRUST_RESULT(elements.push_back(1));
        SC_TRUST_RESULT(elements.push_back(0));
        SC_TRUST_RESULT(elements.push_back(2));
        SC_TEST_EXPECT(elements.remove(0));
        SC_TEST_EXPECT(elements.size() == 2);
        SC_TEST_EXPECT(elements[0] == 1);
        SC_TEST_EXPECT(elements[1] == 2);
        elements.clear();
        SC_TEST_EXPECT(not elements.removeAt(1));
    }
}

bool SC::VectorTest::vectorSnippet()
{
    Console console;
    //! [VectorSnippet]
    Vector<int> myVector;
    SC_TRY(myVector.reserve(10));
    SC_TRY(myVector.push_back(1));
    console.print("[0]={}", myVector[0]);
    SC_TRY(myVector.push_back(2));
    SC_TRY(myVector.pop_back());
    SC_TRY(myVector.pop_front());
    console.print("Vector<int> is {}", myVector.isEmpty() ? "empty" : "not empty");
    //! [VectorSnippet]
    return true;
}

namespace SC
{
void runVectorTest(SC::TestReport& report) { VectorTest test(report); }
} // namespace SC
