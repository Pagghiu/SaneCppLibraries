#pragma once
#include "test.h"
#include "vector.h"

namespace sanecpp
{
struct VectorTest;
struct VectorTestReport;
struct VectorTestClass;
} // namespace sanecpp

struct sanecpp::VectorTestReport
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
        SANECPP_DEBUG_ASSERT(numSequences < MAX_SEQUENCES);
        sequence[numSequences++] = operation;
    }

    void reset()
    {
        numSequences     = 0;
        numNextSequences = 0;
    }

    Operation nextOperation()
    {
        SANECPP_DEBUG_ASSERT(numNextSequences < numSequences);
        return sequence[numNextSequences++];
    }
    static sanecpp::VectorTestReport& get()
    {
        static sanecpp::VectorTestReport rep;
        return rep;
    }
};

struct sanecpp::VectorTestClass
{
    char_t* data;
    VectorTestClass(const char_t* initData)
    {
        copyString(initData);
        sanecpp::VectorTestReport::get();
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

    stringView toString() const
    {
        if (data == nullptr)
        {
            return stringView();
        }
        else
        {
            return stringView(data, strlen(data), true);
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

struct sanecpp::VectorTest : public sanecpp::TestCase
{
    VectorTest(sanecpp::TestReport& report) : TestCase(report, "VectorTest")
    {
        if (not report.isTestEnabled(TestCase::testName))
            return;
        testBasicType();
        testClassType();
    }

    void testClassType()
    {
        using namespace sanecpp;
        VectorTestReport& report = VectorTestReport::get();
        report.reset();
        if (START_SECTION("class_resize"))
        {
            stringView      myString("MyData");
            VectorTestClass testClass(myString.getText());
            SANECPP_TEST_EXPECT(report.nextOperation() == VectorTestReport::CONSTRUCTOR);
            SANECPP_TEST_EXPECT(myString == testClass.toString());
            sanecpp::vector<VectorTestClass> myVector;
            SANECPP_TEST_EXPECT(report.numSequences == 1);
            report.reset();
            auto result = myVector.resize(2);
            SANECPP_TEST_EXPECT(report.numSequences == 4);
            SANECPP_TEST_EXPECT(report.nextOperation() == VectorTestReport::CONSTRUCTOR);      // DEFAULT PARAM
            SANECPP_TEST_EXPECT(report.nextOperation() == VectorTestReport::COPY_CONSTRUCTOR); // FIRST ELEMENT
            SANECPP_TEST_EXPECT(report.nextOperation() == VectorTestReport::COPY_CONSTRUCTOR); // SECOND ELEMENT
            SANECPP_TEST_EXPECT(report.nextOperation() == VectorTestReport::DESTRUCTOR); // DEFAULT PARAM DESTRUCTOR
            SANECPP_TEST_EXPECT(myVector[0].toString().isEmpty());
            SANECPP_TEST_EXPECT(myVector[1].toString().isEmpty());

            report.reset();
            result = myVector.resize(3, VectorTestClass("Custom"));
            SANECPP_TEST_EXPECT(report.numSequences == 5);
            SANECPP_TEST_EXPECT(report.nextOperation() == VectorTestReport::CONSTRUCTOR);      // DEFAULT PARAM
            SANECPP_TEST_EXPECT(report.nextOperation() == VectorTestReport::MOVE_CONSTRUCTOR); // ITEM[1] CONSTRUCTOR
            SANECPP_TEST_EXPECT(report.nextOperation() == VectorTestReport::MOVE_CONSTRUCTOR); // ITEM[2] CONSTRUCTOR
            SANECPP_TEST_EXPECT(report.nextOperation() ==
                                VectorTestReport::COPY_CONSTRUCTOR); // ITEM[3] COPY_CONSTRUCTOR
            // (DEFAULT PARAM)
            SANECPP_TEST_EXPECT(report.nextOperation() == VectorTestReport::DESTRUCTOR); // DEFAULT PARAM DESTRUCTOR
            SANECPP_TEST_EXPECT(myVector[0].toString().isEmpty());
            SANECPP_TEST_EXPECT(myVector[1].toString().isEmpty());
            SANECPP_TEST_EXPECT(myVector[2].toString() == stringView("Custom"));
            report.reset();
            result = myVector.resize(2);
            SANECPP_TEST_EXPECT(report.numSequences == 3);
            SANECPP_TEST_EXPECT(report.nextOperation() == VectorTestReport::CONSTRUCTOR); // DEFAULT PARAM
            SANECPP_TEST_EXPECT(report.nextOperation() == VectorTestReport::DESTRUCTOR);  // ITEM[3] DESTRUCTOR
            SANECPP_TEST_EXPECT(report.nextOperation() == VectorTestReport::DESTRUCTOR);  // DEFAULT PARAM DESTRUCTOR
        }

        if (START_SECTION("class_shrink_to_fit"))
        {
            sanecpp::vector<VectorTestClass> myVector;
            SANECPP_TEST_EXPECT(myVector.resize(3));
            SANECPP_TEST_EXPECT(myVector.resize(2));
            report.reset();
            SANECPP_TEST_EXPECT(myVector.shrink_to_fit());
            SANECPP_TEST_EXPECT(report.numSequences == 2);
            SANECPP_TEST_EXPECT(report.nextOperation() == VectorTestReport::MOVE_CONSTRUCTOR); // ITEM[1] CONSTRUCTOR
            SANECPP_TEST_EXPECT(report.nextOperation() == VectorTestReport::MOVE_CONSTRUCTOR); // ITEM[2] CONSTRUCTOR
        }

        if (START_SECTION("class_clear"))
        {
            sanecpp::vector<VectorTestClass> myVector;
            SANECPP_TEST_EXPECT(myVector.resize(2));
            report.reset();
            myVector.clear();
            SANECPP_TEST_EXPECT(report.numSequences == 2);
            SANECPP_TEST_EXPECT(report.nextOperation() == VectorTestReport::DESTRUCTOR); // ITEM[1] DESTRUCTOR
            SANECPP_TEST_EXPECT(report.nextOperation() == VectorTestReport::DESTRUCTOR); // ITEM[2] DESTRUCTOR
        }

        if (START_SECTION("class_reserve"))
        {
            sanecpp::vector<VectorTestClass> newVector;
            report.reset();
            SANECPP_TEST_EXPECT(newVector.reserve(2));
            SANECPP_TEST_EXPECT(newVector.size() == 0);
            SANECPP_TEST_EXPECT(newVector.capacity() == 2);
            SANECPP_TEST_EXPECT(report.numSequences == 0);
        }

        if (START_SECTION("class_destructor"))
        {
            {
                sanecpp::vector<VectorTestClass> newVector;
                report.reset();
                SANECPP_TEST_EXPECT(newVector.resize(2, VectorTestClass("CIAO")));
            }
            SANECPP_TEST_EXPECT(report.numSequences == 6);

            SANECPP_TEST_EXPECT(report.nextOperation() == VectorTestReport::CONSTRUCTOR); // DEFAULT PARAM
            SANECPP_TEST_EXPECT(report.nextOperation() ==
                                VectorTestReport::COPY_CONSTRUCTOR); // ITEM[1] COPY CONSTRUCTOR
            SANECPP_TEST_EXPECT(report.nextOperation() ==
                                VectorTestReport::COPY_CONSTRUCTOR);                     // ITEM[2] COPY CONSTRUCTOR
            SANECPP_TEST_EXPECT(report.nextOperation() == VectorTestReport::DESTRUCTOR); // DEFAULT PARAM DESTRUCTOR
            SANECPP_TEST_EXPECT(report.nextOperation() == VectorTestReport::DESTRUCTOR); // ITEM[1] DESTRUCTOR
            SANECPP_TEST_EXPECT(report.nextOperation() == VectorTestReport::DESTRUCTOR); // ITEM[2] DESTRUCTOR
        }

        if (START_SECTION("class_copy_construct"))
        {
            sanecpp::vector<VectorTestClass> newVector;
            report.reset();
            VectorTestClass value = VectorTestClass("CIAO");
            SANECPP_TEST_EXPECT(newVector.resize(2, value));
            sanecpp::vector<VectorTestClass> otherVector = newVector;
            SANECPP_TEST_EXPECT(otherVector.size() == 2);
            SANECPP_TEST_EXPECT(otherVector.capacity() == 2);
            SANECPP_TEST_EXPECT(otherVector[0].toString() == stringView("CIAO"));
            SANECPP_TEST_EXPECT(otherVector[1].toString() == stringView("CIAO"));
        }

        if (START_SECTION("class_copy_assign"))
        {
            sanecpp::vector<VectorTestClass> newVector, otherVector;
            report.reset();
            VectorTestClass value = VectorTestClass("CIAO");
            SANECPP_TEST_EXPECT(newVector.resize(2, value));
            otherVector = newVector;
            SANECPP_TEST_EXPECT(otherVector.size() == 2);
            SANECPP_TEST_EXPECT(otherVector.capacity() == 2);
            SANECPP_TEST_EXPECT(otherVector[0].toString() == stringView("CIAO"));
            SANECPP_TEST_EXPECT(otherVector[1].toString() == stringView("CIAO"));
        }

        if (START_SECTION("class_move_assign"))
        {
            sanecpp::vector<VectorTestClass> newVector, otherVector;
            report.reset();
            VectorTestClass value = VectorTestClass("CIAO");
            SANECPP_TEST_EXPECT(newVector.resize(2, value));
            SANECPP_TEST_EXPECT(otherVector.resize(2, value));
            report.reset();
            otherVector = move(newVector);
            SANECPP_TEST_EXPECT(report.numSequences == 2);
            SANECPP_TEST_EXPECT(report.nextOperation() == VectorTestReport::DESTRUCTOR); // ITEM[1] DESTRUCTOR
            SANECPP_TEST_EXPECT(report.nextOperation() == VectorTestReport::DESTRUCTOR); // ITEM[2] DESTRUCTOR
            SANECPP_TEST_EXPECT(newVector.size() == 0);
            SANECPP_TEST_EXPECT(newVector.items == nullptr);
            SANECPP_TEST_EXPECT(otherVector.size() == 2);
            SANECPP_TEST_EXPECT(otherVector.capacity() == 2);
            SANECPP_TEST_EXPECT(otherVector[0].toString() == stringView("CIAO"));
            SANECPP_TEST_EXPECT(otherVector[1].toString() == stringView("CIAO"));
        }

        if (START_SECTION("class_insertMove_full_full_middle"))
        {
            sanecpp::vector<VectorTestClass> vector1, vector2;
            report.reset();
            SANECPP_TEST_EXPECT(vector1.push_back(VectorTestClass("0")));
            SANECPP_TEST_EXPECT(vector1.push_back(VectorTestClass("3")));
            SANECPP_TEST_EXPECT(vector1.push_back(VectorTestClass("4")));
            SANECPP_TEST_EXPECT(vector2.push_back(VectorTestClass("1")));
            SANECPP_TEST_EXPECT(vector2.push_back(VectorTestClass("2")));
            SANECPP_TEST_EXPECT(vector1.insertMove(1, vector2.begin(), vector2.size()));
            SANECPP_TEST_EXPECT(vector1.size() == 5);
            for (int32_t idx = 0; idx < 5; ++idx)
            {
                int32_t value = 0;
                SANECPP_TEST_EXPECT(vector1[idx].toString().parseInt32(&value));
                SANECPP_TEST_EXPECT(value == idx);
            }
        }

        if (START_SECTION("class_appendMove"))
        {
            sanecpp::vector<VectorTestClass> vector1, vector2;
            report.reset();
            SANECPP_TEST_EXPECT(vector1.push_back(VectorTestClass("0")));
            SANECPP_TEST_EXPECT(vector1.push_back(VectorTestClass("1")));
            SANECPP_TEST_EXPECT(vector1.push_back(VectorTestClass("2")));
            SANECPP_TEST_EXPECT(vector2.push_back(VectorTestClass("3")));
            SANECPP_TEST_EXPECT(vector2.push_back(VectorTestClass("4")));
            SANECPP_TEST_EXPECT(vector1.appendMove(vector2.begin(), vector2.size()));
            SANECPP_TEST_EXPECT(vector1.size() == 5);
            for (int32_t idx = 0; idx < 5; ++idx)
            {
                int32_t value = 0;
                SANECPP_TEST_EXPECT(vector1[idx].toString().parseInt32(&value));
                SANECPP_TEST_EXPECT(value == idx);
            }
        }

        if (START_SECTION("class_appendMove_empty"))
        {
            sanecpp::vector<VectorTestClass> vector1, vector2;
            report.reset();
            SANECPP_TEST_EXPECT(vector2.push_back(VectorTestClass("0")));
            SANECPP_TEST_EXPECT(vector2.push_back(VectorTestClass("1")));
            SANECPP_TEST_EXPECT(vector1.appendMove(vector2.begin(), vector2.size()));
            SANECPP_TEST_EXPECT(vector1.size() == 2);
            int idx = 0;
            for (const auto& it : vector1)
            {
                int32_t value = 0;
                SANECPP_TEST_EXPECT(it.toString().parseInt32(&value));
                SANECPP_TEST_EXPECT(value == idx++);
            }
        }

        if (START_SECTION("class_push_back_pop_back"))
        {
            sanecpp::vector<VectorTestClass> test;
            report.reset();
            SANECPP_TEST_EXPECT(test.push_back(VectorTestClass("1")));
            int32_t value = -1;
            SANECPP_TEST_EXPECT(test[0].toString().parseInt32(&value));
            SANECPP_TEST_EXPECT(value == 1);
            SANECPP_TEST_EXPECT(test.push_back(VectorTestClass("2")));
            SANECPP_TEST_EXPECT(test[0].toString().parseInt32(&value));
            SANECPP_TEST_EXPECT(value == 1);
            SANECPP_TEST_EXPECT(test[1].toString().parseInt32(&value));
            SANECPP_TEST_EXPECT(value == 2);
            SANECPP_TEST_EXPECT(test.size() == 2);
            SANECPP_TEST_EXPECT(test.push_back(VectorTestClass("3")));
            test.pop_front();
            SANECPP_TEST_EXPECT(test.size() == 2);
            SANECPP_TEST_EXPECT(test[0].toString().parseInt32(&value));
            SANECPP_TEST_EXPECT(value == 2);
            test.pop_back();
            SANECPP_TEST_EXPECT(test.size() == 1);
            SANECPP_TEST_EXPECT(test[0].toString().parseInt32(&value));
            SANECPP_TEST_EXPECT(value == 2);
        }

        if (START_SECTION("class_copy_assignment"))
        {
            sanecpp::vector<VectorTestClass> vector1, vector2;
            report.reset();
            SANECPP_TEST_EXPECT(vector1.push_back(VectorTestClass("0")));
            SANECPP_TEST_EXPECT(vector1.push_back(VectorTestClass("1")));

            vector2 = vector1;
            SANECPP_TEST_EXPECT(vector1.size() == 2);
            SANECPP_TEST_EXPECT(vector2.size() == 2);
            SANECPP_TEST_EXPECT(vector1[0].data != vector2[0].data);
            SANECPP_TEST_EXPECT(vector1[1].data != vector2[1].data);
            int32_t value;
            SANECPP_TEST_EXPECT(vector2[0].toString().parseInt32(&value) && value == 0);
            SANECPP_TEST_EXPECT(vector2[1].toString().parseInt32(&value) && value == 1);
        }

        if (START_SECTION("class_move_assignment"))
        {
            sanecpp::vector<VectorTestClass> vector1, vector2;
            report.reset();
            SANECPP_TEST_EXPECT(vector1.push_back(VectorTestClass("0")));
            SANECPP_TEST_EXPECT(vector1.push_back(VectorTestClass("1")));

            vector2 = move(vector1);
            SANECPP_TEST_EXPECT(vector1.size() == 0);
            SANECPP_TEST_EXPECT(vector2.size() == 2);
            int32_t value;
            SANECPP_TEST_EXPECT(vector2[0].toString().parseInt32(&value) && value == 0);
            SANECPP_TEST_EXPECT(vector2[1].toString().parseInt32(&value) && value == 1);
        }
    }

    void testBasicType()
    {
        using namespace sanecpp;
        if (START_SECTION("base_resize"))
        {
            vector<int> elements;
            SANECPP_TEST_EXPECT(elements.size() == 0);
            SANECPP_TEST_EXPECT(elements.capacity() == 0);
            SANECPP_TEST_EXPECT(elements.resize(10, 11));
            for (size_t idx = 0; idx < elements.size(); ++idx)
            {
                if (elements[idx] != 11)
                {
                    SANECPP_DEBUG_ASSERT(elements[idx] == 11);
                }
                elements[idx] = static_cast<int>(idx);
            }

            SANECPP_TEST_EXPECT(elements.size() == 10);
            SANECPP_TEST_EXPECT(elements.size() == elements.capacity());

            SANECPP_TEST_EXPECT(elements.resize(20));
            for (size_t idx = 0; idx < 10; ++idx)
            {
                if (elements[idx] != idx)
                {
                    SANECPP_DEBUG_ASSERT(elements[idx] == idx);
                }
            }
            for (size_t idx = 10; idx < 20; ++idx)
            {
                if (elements[idx] != 0)
                {
                    SANECPP_DEBUG_ASSERT(elements[idx] == 0);
                }
            }

            SANECPP_TEST_EXPECT(elements.resize(5));
            SANECPP_TEST_EXPECT(elements.size() == 5);
            SANECPP_TEST_EXPECT(elements.capacity() == 20);
            SANECPP_TEST_EXPECT(elements.shrink_to_fit());
            for (size_t idx = 0; idx < elements.size(); ++idx)
            {
                if (elements[idx] != idx)
                {
                    SANECPP_DEBUG_ASSERT(elements[idx] == idx);
                }
            }
            SANECPP_TEST_EXPECT(elements.size() == 5);
            SANECPP_TEST_EXPECT(elements.capacity() == 5);
            SANECPP_TEST_EXPECT(elements.resizeWithoutInitializing(10));
        }

        if (START_SECTION("base_clear"))
        {
            vector<int> elements;
            SANECPP_TEST_EXPECT(elements.resizeWithoutInitializing(10));
            elements.clear();
            SANECPP_TEST_EXPECT(elements.size() == 0);
            SANECPP_TEST_EXPECT(elements.capacity() == 10);
        }

        if (START_SECTION("base_shrink_t_fit"))
        {
            vector<int> elements;
            SANECPP_TEST_EXPECT(elements.resizeWithoutInitializing(10));
            elements.clear();
            SANECPP_TEST_EXPECT(elements.shrink_to_fit());
            SANECPP_TEST_EXPECT(elements.size() == 0);
            SANECPP_TEST_EXPECT(elements.capacity() == 0);
        }
    }
};
