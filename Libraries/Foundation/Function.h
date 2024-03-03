// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../Foundation/TypeTraits.h" // RemoveReference, AddPointer, IsSame

namespace SC
{
//! @addtogroup group_foundation_utility
//! @{

/// @brief Wraps function pointers, member functions and lambdas without ever allocating. @n
///
/// Example:
/**
 * @code{.cpp}
    struct MyClass
    {
        float memberValue = 2.0;
        int memberFunc(float a) { return static_cast<int>(a + memberValue); }
    };
    int someFunc(float a) { return static_cast<int>(a * 2); }

    struct BigClass
    {
        uint64_t values[4];
    };

    // ... somewhere later
    MyClass myClass;

    Function<int(float)> func;

    func = &someFunc;                                                   // Bind free func
    func.bind<MyClass, &MyClass::memberFunc>(myClass);                  // Bind member func
    func = [](float a) -> int { return static_cast<int>(a + 1.5); };    // Bind lambda func

    BigClass bigClass;

    // This will static_assert because sizeof(BigClass) (grabbed by copy) exceeds LAMBDA_SIZE
    // func = [bigClass](float a) -> int { return static_cast<int>(a);};

    @endcode

    Size of lambdas or less than LAMBDA_SIZE (currently `2 * sizeof(void*)`). @n
    If lambda is bigger than `LAMBDA_SIZE` the constructor will static assert.
    @tparam FuncType Type of function to be wrapped (Lambda, free function or pointer to member function)

 * */
template <typename FuncType>
struct Function;

template <typename R, typename... Args>
struct Function<R(Args...)>
{
  private:
    enum class FunctionErasedOperation
    {
        Destruct,
        CopyConstruct,
        MoveConstruct
    };
    using StubFunction      = R (*)(const void* const*, typename TypeTraits::AddPointer<Args>::type...);
    using OperationFunction = void (*)(FunctionErasedOperation operation, const void** other, const void* const*);

    static const int LAMBDA_SIZE = sizeof(void*) * 2;

    StubFunction      functionStub;
    OperationFunction functionOperation;

    union
    {
        const void* classInstance;
        char        lambdaMemory[LAMBDA_SIZE] = {0};
    };

    void executeOperation(FunctionErasedOperation operation, const void** other) const
    {
        if (functionOperation)
            (*functionOperation)(operation, other, &classInstance);
    }

    Function(const void* instance, StubFunction stub, OperationFunction operation)
        : functionStub(stub), functionOperation(operation)
    {
        classInstance = instance;
    }
    using FreeFunction = R (*)(Args...);

  public:
    /// @brief Constructs an empty Function
    Function()
    {
        static_assert(sizeof(Function) == sizeof(void*) * 4, "Function Size");
        functionStub      = nullptr;
        functionOperation = nullptr;
    }

    /// Constructs a function from a lambda with a compatible size (equal or less than LAMBDA_SIZE)
    /// If lambda is bigger than `LAMBDA_SIZE` a static assertion will be issued
    /// SFINAE is used to avoid universal reference from "eating" also copy constructor
    template <
        typename Lambda,
        typename = typename TypeTraits::EnableIf<
            not TypeTraits::IsSame<typename TypeTraits::RemoveReference<Lambda>::type, Function>::value, void>::type>
    Function(Lambda&& lambda)
    {
        functionStub      = nullptr;
        functionOperation = nullptr;
        bind(forward<typename TypeTraits::RemoveReference<Lambda>::type>(lambda));
    }

    /// @brief Destroys the function wrapper
    ~Function() { executeOperation(FunctionErasedOperation::Destruct, nullptr); }

    /// @brief Move constructor for Function wrapper
    /// @param other The moved from function
    Function(Function&& other)
    {
        functionStub      = other.functionStub;
        functionOperation = other.functionOperation;
        classInstance     = other.classInstance;
        other.executeOperation(FunctionErasedOperation::MoveConstruct, &classInstance);
        other.executeOperation(FunctionErasedOperation::Destruct, nullptr);
        other.functionStub      = nullptr;
        other.functionOperation = nullptr;
    }

    /// @brief Copy constructor for Function wrapper
    /// @param other The function to be copied
    Function(const Function& other)
    {
        functionStub      = other.functionStub;
        functionOperation = other.functionOperation;
        other.executeOperation(FunctionErasedOperation::CopyConstruct, &classInstance);
    }

    /// @brief Copy assign a function to current function wrapper. Destroys existing wrapper.
    /// @param other The function to be assigned to current function
    Function& operator=(const Function& other)
    {
        executeOperation(FunctionErasedOperation::Destruct, nullptr);
        functionStub      = other.functionStub;
        functionOperation = other.functionOperation;
        other.executeOperation(FunctionErasedOperation::CopyConstruct, &classInstance);
        return *this;
    }

    /// @brief Move assign a function to current function wrapper. Destroys existing wrapper.
    /// @param other The function to be move-assigned to current function
    Function& operator=(Function&& other) noexcept
    {
        executeOperation(FunctionErasedOperation::Destruct, nullptr);
        functionStub      = other.functionStub;
        functionOperation = other.functionOperation;
        other.executeOperation(FunctionErasedOperation::MoveConstruct, &classInstance);
        other.executeOperation(FunctionErasedOperation::Destruct, nullptr);
        other.functionStub = nullptr;
        return *this;
    }

    /// @brief Check if current wrapper is bound to a function
    /// @return `true` if current wrapper is bound to a function
    bool isValid() const { return functionStub != nullptr; }

    /// @brief Binds a Lambda to current function wrapper
    /// @tparam Lambda type of Lambda to be wrapped in current function wrapper
    /// @param lambda Instance of Lambda to be wrapped
    template <typename Lambda>
    void bind(Lambda&& lambda)
    {
        executeOperation(FunctionErasedOperation::Destruct, nullptr);
        functionStub      = nullptr;
        functionOperation = nullptr;

        new (&classInstance, PlacementNew()) Lambda(forward<Lambda>(lambda));
        static_assert(sizeof(Lambda) <= sizeof(lambdaMemory), "Lambda is too big");
        functionStub = [](const void* const* p, typename TypeTraits::AddPointer<Args>::type... args) -> R
        {
            Lambda& lambda = *reinterpret_cast<Lambda*>(const_cast<void**>(p));
            return lambda(*args...);
        };
        functionOperation = [](FunctionErasedOperation operation, const void** other, const void* const* p)
        {
            Lambda& lambda = *reinterpret_cast<Lambda*>(const_cast<void**>(p));
            if (operation == FunctionErasedOperation::Destruct)
                lambda.~Lambda();
            else if (operation == FunctionErasedOperation::CopyConstruct)
                new (other, PlacementNew()) Lambda(lambda);
            else if (operation == FunctionErasedOperation::MoveConstruct)
                new (other, PlacementNew()) Lambda(move(lambda));
            else
#if SC_COMPILER_MSVC
                __assume(false);
#else
                __builtin_unreachable();
#endif
        };
    }

    /// @brief Binds a free function to function wrapper
    /// @tparam FreeFunction a regular static function to be wrapper, with a matching signature
    template <R (*FreeFunction)(Args...)>
    void bind()
    {
        executeOperation(FunctionErasedOperation::Destruct, nullptr);
        classInstance     = nullptr;
        functionStub      = &FunctionWrapper<FreeFunction>;
        functionOperation = &FunctionOperation;
    }

    /// @brief Binds a class member function to function wrapper
    /// @tparam Class Type of the Class holding MemberFunction
    /// @tparam MemberFunction Pointer to member function with a matching signature
    /// @param c Reference to the instance of class where the method must be bound to
    template <typename Class, R (Class::*MemberFunction)(Args...) const>
    void bind(const Class& c)
    {
        executeOperation(FunctionErasedOperation::Destruct, nullptr);
        classInstance     = &c;
        functionStub      = &MemberWrapper<Class, MemberFunction>;
        functionOperation = &MemberOperation;
    }

    /// @brief Binds a class member function to function wrapper
    /// @tparam Class Type of the Class holding MemberFunction
    /// @tparam MemberFunction Pointer to member function with a matching signature
    /// @param c Reference to the instance of class where the method must be bound to
    template <typename Class, R (Class::*MemberFunction)(Args...)>
    void bind(Class& c)
    {
        executeOperation(FunctionErasedOperation::Destruct, nullptr);
        classInstance     = &c;
        functionStub      = &MemberWrapper<Class, MemberFunction>;
        functionOperation = &MemberOperation;
    }

    /// @brief Binds a class member function to function wrapper
    /// @tparam Class Type of the Class holding MemberFunction
    /// @tparam MemberFunction Pointer to member function with a matching signature
    /// @param c Reference to the instance of class where the method must be bound to
    /// @return The function wrapper
    template <typename Class, R (Class::*MemberFunction)(Args...)>
    static Function fromMember(Class& c)
    {
        return Function(&c, &MemberWrapper<Class, MemberFunction>, &MemberOperation);
    }

    /// @brief Binds a class member function to function wrapper
    /// @tparam Class Type of the Class holding MemberFunction
    /// @tparam MemberFunction Pointer to member function with a matching signature
    /// @param c Reference to the instance of class where the method must be bound to
    /// @return The function wrapper
    template <typename Class, R (Class::*MemberFunction)(Args...) const>
    static Function fromMember(const Class& c)
    {
        return Function(&c, &MemberWrapper<Class, MemberFunction>, &MemberOperation);
    }

    /// @brief Invokes the wrapped function. If no function is bound, this is UB.
    /// @param args Arguments to be passed to the wrapped function
    /// @return the return value of the invoked function.
    [[nodiscard]] R operator()(Args... args) const { return (*functionStub)(&classInstance, &args...); }

  private:
    static void MemberOperation(FunctionErasedOperation operation, const void** other, const void* const* p)
    {
        if (operation == FunctionErasedOperation::CopyConstruct or operation == FunctionErasedOperation::MoveConstruct)
            *other = *p;
    }

    template <typename Class, R (Class::*MemberFunction)(Args...)>
    static R MemberWrapper(const void* const* p, typename TypeTraits::RemoveReference<Args>::type*... args)
    {
        Class* cls = const_cast<Class*>(static_cast<const Class*>(*p));
        return (cls->*MemberFunction)(*args...);
    }

    template <typename Class, R (Class::*MemberFunction)(Args...) const>
    static R MemberWrapper(const void* const* p, typename TypeTraits::RemoveReference<Args>::type*... args)
    {
        const Class* cls = static_cast<const Class*>(*p);
        return (cls->*MemberFunction)(*args...);
    }

    template <R (*FreeFunction)(Args...)>
    static R FunctionWrapper(const void* const* p, typename TypeTraits::RemoveReference<Args>::type*... args)
    {
        SC_COMPILER_UNUSED(p);
        return FreeFunction(*args...);
    }
    static void FunctionOperation(FunctionErasedOperation, const void**, const void* const*) {}
};

template <typename T>
using Delegate = Function<void(T)>;
using Action   = Function<void()>;
//! @}
} // namespace SC
