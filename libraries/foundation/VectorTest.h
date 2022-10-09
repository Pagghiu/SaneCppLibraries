#pragma once
#include "Test.h"
namespace SC
{
struct VectorTest;
}

struct SC::VectorTest : public SC::TestCase
{
    VectorTest(SC::TestReport& report);
    void testClassType();
    void testBasicType();
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
        SC_DEBUG_ASSERT(numSequences < MAX_SEQUENCES);
        sequence[numSequences++] = operation;
    }

    void reset()
    {
        numSequences     = 0;
        numNextSequences = 0;
    }

    Operation nextOperation()
    {
        SC_DEBUG_ASSERT(numNextSequences < numSequences);
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
    char_t* data;
    VectorTestClass(const char_t* initData)
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
                memoryRelease(data);
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
                memoryRelease(data);
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
            return StringView(data, strlen(data), true);
        }
    }

    ~VectorTestClass()
    {
        VectorTestReport::get().push(VectorTestReport::DESTRUCTOR);
        memoryRelease(data);
    }

  private:
    void copyString(const char_t* initData)
    {
        const size_t numBytes = strlen(initData) + 1;
        data                  = static_cast<char_t*>(memoryAllocate(numBytes));
        memcpy(data, initData, numBytes);
    }
};

SC::VectorTest::VectorTest(SC::TestReport& report) : TestCase(report, "VectorTest")
{
    testBasicType();
    testClassType();
}
const SC::size_t INSANE_NUMBER = static_cast<SC::size_t>(static_cast<SC::uint32_t>(SC::MaxValue())) + 1;

void SC::VectorTest::testClassType()
{
    using namespace SC;
    VectorTestReport& report = VectorTestReport::get();
    report.reset();
    if (test_section("class_resize"))
    {
        StringView      myString("MyData");
        VectorTestClass testClass(myString.getText());
        SC_TEST_EXPECT(report.nextOperation() == VectorTestReport::CONSTRUCTOR);
        SC_TEST_EXPECT(myString == testClass.toString());
        SC::Vector<VectorTestClass> myVector;
        SC_TEST_EXPECT(report.numSequences == 1);
        report.reset();
        auto result = myVector.resize(2);
        SC_TEST_EXPECT(report.numSequences == 4);
        SC_TEST_EXPECT(report.nextOperation() == VectorTestReport::CONSTRUCTOR);      // DEFAULT PARAM
        SC_TEST_EXPECT(report.nextOperation() == VectorTestReport::COPY_CONSTRUCTOR); // FIRST ELEMENT
        SC_TEST_EXPECT(report.nextOperation() == VectorTestReport::COPY_CONSTRUCTOR); // SECOND ELEMENT
        SC_TEST_EXPECT(report.nextOperation() == VectorTestReport::DESTRUCTOR);       // DEFAULT PARAM DESTRUCTOR
        SC_TEST_EXPECT(myVector[0].toString().isEmpty());
        SC_TEST_EXPECT(myVector[1].toString().isEmpty());

        report.reset();
        result = myVector.resize(3, VectorTestClass("Custom"));
        SC_TEST_EXPECT(report.numSequences == 5);
        SC_TEST_EXPECT(report.nextOperation() == VectorTestReport::CONSTRUCTOR);      // DEFAULT PARAM
        SC_TEST_EXPECT(report.nextOperation() == VectorTestReport::MOVE_CONSTRUCTOR); // ITEM[1] CONSTRUCTOR
        SC_TEST_EXPECT(report.nextOperation() == VectorTestReport::MOVE_CONSTRUCTOR); // ITEM[2] CONSTRUCTOR
        SC_TEST_EXPECT(report.nextOperation() == VectorTestReport::COPY_CONSTRUCTOR); // ITEM[3] COPY_CONSTRUCTOR
        // (DEFAULT PARAM)
        SC_TEST_EXPECT(report.nextOperation() == VectorTestReport::DESTRUCTOR); // DEFAULT PARAM DESTRUCTOR
        SC_TEST_EXPECT(myVector[0].toString().isEmpty());
        SC_TEST_EXPECT(myVector[1].toString().isEmpty());
        SC_TEST_EXPECT(myVector[2].toString() == StringView("Custom"));
        report.reset();
        result = myVector.resize(2);
        SC_TEST_EXPECT(report.numSequences == 3);
        SC_TEST_EXPECT(report.nextOperation() == VectorTestReport::CONSTRUCTOR); // DEFAULT PARAM
        SC_TEST_EXPECT(report.nextOperation() == VectorTestReport::DESTRUCTOR);  // ITEM[3] DESTRUCTOR
        SC_TEST_EXPECT(report.nextOperation() == VectorTestReport::DESTRUCTOR);  // DEFAULT PARAM DESTRUCTOR
        SC_TEST_EXPECT(myVector.resize(0));
        report.reset();
        SC_TEST_EXPECT(myVector.resize(1));
        SC_TEST_EXPECT(report.numSequences == 3);
        SC_TEST_EXPECT(report.nextOperation() == VectorTestReport::CONSTRUCTOR);      // DEFAULT PARAM
        SC_TEST_EXPECT(report.nextOperation() == VectorTestReport::COPY_CONSTRUCTOR); // ITEM[3] COPY_CONSTRUCTOR
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
        report.reset();
        SC_TEST_EXPECT(myVector.shrink_to_fit());
        SC_TEST_EXPECT(report.numSequences == 2);
        SC_TEST_EXPECT(report.nextOperation() == VectorTestReport::MOVE_CONSTRUCTOR); // ITEM[1] CONSTRUCTOR
        SC_TEST_EXPECT(report.nextOperation() == VectorTestReport::MOVE_CONSTRUCTOR); // ITEM[2] CONSTRUCTOR
    }

    if (test_section("class_clear"))
    {
        SC::Vector<VectorTestClass> myVector;
        SC_TEST_EXPECT(myVector.resize(2));
        report.reset();
        myVector.clear();
        SC_TEST_EXPECT(report.numSequences == 2);
        SC_TEST_EXPECT(report.nextOperation() == VectorTestReport::DESTRUCTOR); // ITEM[1] DESTRUCTOR
        SC_TEST_EXPECT(report.nextOperation() == VectorTestReport::DESTRUCTOR); // ITEM[2] DESTRUCTOR
    }

    if (test_section("class_reserve"))
    {
        SC::Vector<VectorTestClass> newVector;
        report.reset();
        SC_TEST_EXPECT(newVector.reserve(2));
        SC_TEST_EXPECT(newVector.reserve(1));
        SC_TEST_EXPECT(newVector.size() == 0);
        SC_TEST_EXPECT(newVector.capacity() == 2);
        SC_TEST_EXPECT(report.numSequences == 0);
    }

    if (test_section("class_destructor"))
    {
        {
            SC::Vector<VectorTestClass> newVector;
            report.reset();
            SC_TEST_EXPECT(newVector.resize(2, VectorTestClass("CIAO")));
        }
        SC_TEST_EXPECT(report.numSequences == 6);

        SC_TEST_EXPECT(report.nextOperation() == VectorTestReport::CONSTRUCTOR);      // DEFAULT PARAM
        SC_TEST_EXPECT(report.nextOperation() == VectorTestReport::COPY_CONSTRUCTOR); // ITEM[1] COPY CONSTRUCTOR
        SC_TEST_EXPECT(report.nextOperation() == VectorTestReport::COPY_CONSTRUCTOR); // ITEM[2] COPY CONSTRUCTOR
        SC_TEST_EXPECT(report.nextOperation() == VectorTestReport::DESTRUCTOR);       // DEFAULT PARAM DESTRUCTOR
        SC_TEST_EXPECT(report.nextOperation() == VectorTestReport::DESTRUCTOR);       // ITEM[1] DESTRUCTOR
        SC_TEST_EXPECT(report.nextOperation() == VectorTestReport::DESTRUCTOR);       // ITEM[2] DESTRUCTOR
    }

    if (test_section("class_copy_construct"))
    {
        SC::Vector<VectorTestClass> newVector;
        report.reset();
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
        report.reset();
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
        report.reset();
        VectorTestClass value = VectorTestClass("CIAO");
        SC_TEST_EXPECT(newVector.resize(2, value));
        SC_TEST_EXPECT(otherVector.resize(2, value));
        report.reset();
        otherVector = move(newVector);
        SC_TEST_EXPECT(report.numSequences == 2);
        SC_TEST_EXPECT(report.nextOperation() == VectorTestReport::DESTRUCTOR); // ITEM[1] DESTRUCTOR
        SC_TEST_EXPECT(report.nextOperation() == VectorTestReport::DESTRUCTOR); // ITEM[2] DESTRUCTOR
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
        report.reset();
        VectorTestClass value = VectorTestClass("CIAO");
        SC_TEST_EXPECT(newVector.resize(2, value));
        SC_TEST_EXPECT(otherVector.resize(2, value));
        report.reset();
        otherVector = newVector;
        SC_TEST_EXPECT(report.numSequences == 2);
        SC_TEST_EXPECT(report.nextOperation() == VectorTestReport::COPY_ASSIGNMENT); // ITEM[1] COPY
        SC_TEST_EXPECT(report.nextOperation() == VectorTestReport::COPY_ASSIGNMENT); // ITEM[2] COPY
        SC_TEST_EXPECT(newVector.size() == 2);
        // SC_TEST_EXPECT(newVector.items == nullptr);
        SC_TEST_EXPECT(otherVector.size() == 2);
        SC_TEST_EXPECT(otherVector.capacity() == 2);
        SC_TEST_EXPECT(otherVector[0].toString() == StringView("CIAO"));
        SC_TEST_EXPECT(otherVector[1].toString() == StringView("CIAO"));
        SC_TEST_EXPECT(newVector.resize(4));
        report.reset();
        otherVector = newVector;
        SC_TEST_EXPECT(report.numSequences == 6);
        SC_TEST_EXPECT(report.nextOperation() == VectorTestReport::DESTRUCTOR);       // ITEM[1] DESTRUCTOR
        SC_TEST_EXPECT(report.nextOperation() == VectorTestReport::DESTRUCTOR);       // ITEM[2] DESTRUCTOR
        SC_TEST_EXPECT(report.nextOperation() == VectorTestReport::COPY_CONSTRUCTOR); // ITEM[1] COPY_CONSTRUCTOR
        SC_TEST_EXPECT(report.nextOperation() == VectorTestReport::COPY_CONSTRUCTOR); // ITEM[2] COPY_CONSTRUCTOR
        SC_TEST_EXPECT(report.nextOperation() == VectorTestReport::COPY_CONSTRUCTOR); // ITEM[1] COPY_CONSTRUCTOR
        SC_TEST_EXPECT(report.nextOperation() == VectorTestReport::COPY_CONSTRUCTOR); // ITEM[2] COPY_CONSTRUCTOR
        SC_TEST_EXPECT(newVector.resize(2));
        report.reset();
        otherVector = newVector;
        SC_TEST_EXPECT(report.numSequences == 4);
        SC_TEST_EXPECT(report.nextOperation() == VectorTestReport::COPY_ASSIGNMENT); // ITEM[1] COPY_CONSTRUCTOR
        SC_TEST_EXPECT(report.nextOperation() == VectorTestReport::COPY_ASSIGNMENT); // ITEM[2] COPY_CONSTRUCTOR
        SC_TEST_EXPECT(report.nextOperation() == VectorTestReport::DESTRUCTOR);      // ITEM[1] DESTRUCTOR
        SC_TEST_EXPECT(report.nextOperation() == VectorTestReport::DESTRUCTOR);      // ITEM[2] DESTRUCTOR
                                                                                     //
    }
    if (test_section("class_insertMove_full_full_middle"))
    {
        SC::Vector<VectorTestClass> vector1, vector2;
        report.reset();
        SC_TEST_EXPECT(vector1.push_back(VectorTestClass("0")));
        SC_TEST_EXPECT(vector1.push_back(VectorTestClass("3")));
        SC_TEST_EXPECT(vector1.push_back(VectorTestClass("4")));
        SC_TEST_EXPECT(vector2.push_back(VectorTestClass("1")));
        SC_TEST_EXPECT(vector2.push_back(VectorTestClass("2")));
        SC_TEST_EXPECT(vector1.insertMove(1, vector2.begin(), vector2.size()));
        SC_TEST_EXPECT(vector1.insertMove(1, vector2.begin(), 0));
        SC_TEST_EXPECT(not vector1.insertMove(1, vector2.begin(), INSANE_NUMBER));
        SC_TEST_EXPECT(vector1.size() == 5);
        for (int32_t idx = 0; idx < 5; ++idx)
        {
            int32_t value = 0;
            SC_TEST_EXPECT(vector1[idx].toString().parseInt32(&value));
            SC_TEST_EXPECT(value == idx);
        }
    }

    if (test_section("class_appendMove"))
    {
        SC::Vector<VectorTestClass> vector1, vector2;
        report.reset();
        SC_TEST_EXPECT(vector1.push_back(VectorTestClass("0")));
        SC_TEST_EXPECT(vector1.push_back(VectorTestClass("1")));
        SC_TEST_EXPECT(vector1.push_back(VectorTestClass("2")));
        SC_TEST_EXPECT(vector2.push_back(VectorTestClass("3")));
        SC_TEST_EXPECT(vector2.push_back(VectorTestClass("4")));
        SC_TEST_EXPECT(vector1.appendMove(vector2.begin(), vector2.size()));
        SC_TEST_EXPECT(vector1.size() == 5);
        for (int32_t idx = 0; idx < 5; ++idx)
        {
            int32_t value = 0;
            SC_TEST_EXPECT(vector1[idx].toString().parseInt32(&value));
            SC_TEST_EXPECT(value == idx);
        }
    }

    if (test_section("class_appendMove_empty"))
    {
        SC::Vector<VectorTestClass> vector1, vector2;
        report.reset();
        SC_TEST_EXPECT(vector2.push_back(VectorTestClass("0")));
        SC_TEST_EXPECT(vector2.push_back(VectorTestClass("1")));
        SC_TEST_EXPECT(vector1.appendMove(vector2.begin(), vector2.size()));
        SC_TEST_EXPECT(vector1.size() == 2);
        int idx = 0;
        for (const auto& it : vector1)
        {
            int32_t value = 0;
            SC_TEST_EXPECT(it.toString().parseInt32(&value));
            SC_TEST_EXPECT(value == idx++);
        }
    }

    if (test_section("class_push_back_pop_back"))
    {
        SC::Vector<VectorTestClass> test;
        report.reset();
        SC_TEST_EXPECT(test.push_back(VectorTestClass("1")));
        int32_t value = -1;
        SC_TEST_EXPECT(test[0].toString().parseInt32(&value));
        SC_TEST_EXPECT(value == 1);
        SC_TEST_EXPECT(test.push_back(VectorTestClass("2")));
        SC_TEST_EXPECT(test[0].toString().parseInt32(&value));
        SC_TEST_EXPECT(value == 1);
        SC_TEST_EXPECT(test[1].toString().parseInt32(&value));
        SC_TEST_EXPECT(value == 2);
        SC_TEST_EXPECT(test.size() == 2);
        SC_TEST_EXPECT(test.push_back(VectorTestClass("3")));
        SC_TEST_EXPECT(test.pop_front());
        SC_TEST_EXPECT(test.size() == 2);
        SC_TEST_EXPECT(test[0].toString().parseInt32(&value));
        SC_TEST_EXPECT(value == 2);
        SC_TEST_EXPECT(test.pop_back());
        SC_TEST_EXPECT(test.size() == 1);
        SC_TEST_EXPECT(test[0].toString().parseInt32(&value));
        SC_TEST_EXPECT(value == 2);
        SC_TEST_EXPECT(test.pop_back());
        SC_TEST_EXPECT(not test.pop_back());
        SC_TEST_EXPECT(not test.pop_front());
    }

    if (test_section("class_copy_assignment"))
    {
        SC::Vector<VectorTestClass> vector1, vector2;
        report.reset();
        SC_TEST_EXPECT(vector1.push_back(VectorTestClass("0")));
        SC_TEST_EXPECT(vector1.push_back(VectorTestClass("1")));

        vector2 = vector1;
        SC_TEST_EXPECT(vector1.size() == 2);
        SC_TEST_EXPECT(vector2.size() == 2);
        SC_TEST_EXPECT(vector1[0].data != vector2[0].data);
        SC_TEST_EXPECT(vector1[1].data != vector2[1].data);
        int32_t value;
        SC_TEST_EXPECT(vector2[0].toString().parseInt32(&value) && value == 0);
        SC_TEST_EXPECT(vector2[1].toString().parseInt32(&value) && value == 1);
    }

    if (test_section("class_move_assignment"))
    {
        SC::Vector<VectorTestClass> vector1, vector2;
        report.reset();
        SC_TEST_EXPECT(vector1.push_back(VectorTestClass("0")));
        SC_TEST_EXPECT(vector1.push_back(VectorTestClass("1")));

        vector2 = move(vector1);
        SC_TEST_EXPECT(vector1.size() == 0);
        SC_TEST_EXPECT(vector2.size() == 2);
        int32_t value;
        SC_TEST_EXPECT(vector2[0].toString().parseInt32(&value) && value == 0);
        SC_TEST_EXPECT(vector2[1].toString().parseInt32(&value) && value == 1);
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

        SC_TEST_EXPECT(numFailures = 1);

        SC_TEST_EXPECT(not elements.resize(INSANE_NUMBER));
        SC_TEST_EXPECT(elements.size() == 10);
        SC_TEST_EXPECT(elements.size() == elements.capacity());
        SC_TEST_EXPECT(elements.reserve(elements.capacity() + 1));

        SC_TEST_EXPECT(elements.resize(20));
        elements[0] = -1;
        numFailures = 0;
        for (size_t idx = 0; idx < 10; ++idx)
        {
            if (elements[idx] != idx)
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
            if (elements[idx] != idx)
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
        (void)elements.push_back(1);
        (void)elements.push_back(0);
        (void)elements.push_back(2);
        elements.sort();
        SC_TEST_EXPECT(elements[0] == 0);
        SC_TEST_EXPECT(elements[1] == 1);
        SC_TEST_EXPECT(elements[2] == 2);
    }
}
