// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "../Foundation/TypeTraits.h" // RemoveReference, AddPointer, IsSame

namespace SC
{
//! @addtogroup group_foundation_utility
//! @{

/// @brief Wraps function pointers, member functions and lambdas without ever allocating. @n
/// @tparam FuncType Type of function to be wrapped (Lambda, free function or pointer to member function)
/// @note Size of lambdas or less than LAMBDA_SIZE (currently `2 * sizeof(void*)`). @n
///  If lambda is bigger than `LAMBDA_SIZE` the constructor will static assert.
///
/// Example:
/// \snippet Tests/Libraries/Foundation/FunctionTest.cpp FunctionMainSnippet
template <typename FuncType>
struct Function;

template <typename R, typename... Args>
struct Function<R(Args...)>
{
  private:
    enum class Operation
    {
        Destruct,
        CopyConstruct,
        MoveConstruct
    };
    using ExecuteFunction   = R (*)(const void* const*, typename TypeTraits::AddPointer<Args>::type...);
    using OperationFunction = void (*)(Operation operation, const void** other, const void* const*);

    struct VTable
    {
        ExecuteFunction   execute;
        OperationFunction operation;
    };

    static const int LAMBDA_SIZE = sizeof(void*) * 2;

    const VTable* vtable;

    union
    {
        const void* classInstance;
        char        lambdaMemory[LAMBDA_SIZE] = {0};
    };

    void checkedOperation(Operation operation, const void** other) const
    {
        if (vtable)
            vtable->operation(operation, other, &classInstance);
    }

  public:
    /// @brief Constructs an empty Function
    Function()
    {
        static_assert(sizeof(Function) == sizeof(void*) * 3, "Function Size");
        vtable = nullptr;
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
        vtable = nullptr;
        bind(forward<typename TypeTraits::RemoveReference<Lambda>::type>(lambda));
    }

    /// @brief Destroys the function wrapper
    ~Function() { checkedOperation(Operation::Destruct, nullptr); }

    /// @brief Move constructor for Function wrapper
    /// @param other The moved from function
    Function(Function&& other)
    {
        vtable        = other.vtable;
        classInstance = other.classInstance;
        other.checkedOperation(Operation::MoveConstruct, &classInstance);
        other.checkedOperation(Operation::Destruct, nullptr);
        other.vtable = nullptr;
    }

    /// @brief Copy constructor for Function wrapper
    /// @param other The function to be copied
    Function(const Function& other)
    {
        vtable = other.vtable;
        other.checkedOperation(Operation::CopyConstruct, &classInstance);
    }

    /// @brief Copy assign a function to current function wrapper. Destroys existing wrapper.
    /// @param other The function to be assigned to current function
    Function& operator=(const Function& other)
    {
        checkedOperation(Operation::Destruct, nullptr);
        vtable = other.vtable;
        other.checkedOperation(Operation::CopyConstruct, &classInstance);
        return *this;
    }

    /// @brief Move assign a function to current function wrapper. Destroys existing wrapper.
    /// @param other The function to be move-assigned to current function
    Function& operator=(Function&& other) noexcept
    {
        checkedOperation(Operation::Destruct, nullptr);
        vtable = other.vtable;
        other.checkedOperation(Operation::MoveConstruct, &classInstance);
        other.checkedOperation(Operation::Destruct, nullptr);
        other.vtable = nullptr;
        return *this;
    }

    /// @brief Check if current wrapper is bound to a function
    /// @return `true` if current wrapper is bound to a function
    [[nodiscard]] bool isValid() const { return vtable != nullptr; }

    /// @brief Returns true if this function was bound to a member function of a specific class instance
    [[nodiscard]] bool isBoundToClassInstance(void* instance) const { return classInstance == instance; }

    bool operator==(const Function& other) const
    {
        return vtable == other.vtable and classInstance == other.classInstance;
    }

    /// @brief Binds a Lambda to current function wrapper
    /// @tparam Lambda type of Lambda to be wrapped in current function wrapper
    /// @param lambda Instance of Lambda to be wrapped
    template <typename Lambda>
    void bind(Lambda&& lambda)
    {
        checkedOperation(Operation::Destruct, nullptr);
        vtable = nullptr;
        new (&classInstance, PlacementNew()) Lambda(forward<Lambda>(lambda));
        vtable = getVTableForLambda<Lambda>();
    }

  private:
    template <typename Lambda>
    static auto getVTableForLambda()
    {
        static_assert(sizeof(Lambda) <= sizeof(lambdaMemory), "Lambda is too big");
        static SC_LANGUAGE_IF_CONSTEXPR const VTable staticVTable = {
            [](const void* const* p, typename TypeTraits::AddPointer<Args>::type... args) SC_LANGUAGE_IF_CONSTEXPR
            {
                Lambda& lambda = *reinterpret_cast<Lambda*>(const_cast<void**>(p));
                return lambda(*args...);
            },
            [](Operation operation, const void** other, const void* const* p) SC_LANGUAGE_IF_CONSTEXPR
            {
                Lambda& lambda = *reinterpret_cast<Lambda*>(const_cast<void**>(p));
                if (operation == Operation::Destruct)
                    lambda.~Lambda();
                else if (operation == Operation::CopyConstruct)
                    new (other, PlacementNew()) Lambda(lambda);
                else if (operation == Operation::MoveConstruct)
                    new (other, PlacementNew()) Lambda(move(lambda));
            }};
        return &staticVTable;
    }

  public:
    /// @brief Unsafely retrieve the functor bound previously bound to this function
    /// @tparam Lambda type of Lambda passed to  Function::bind or Function::operator=
    /// @return Pointer to functor or null if Lambda is not the same type bound in bind()
    /// \snippet Tests/Libraries/Foundation/FunctionTest.cpp FunctionFunctorSnippet
    template <typename Lambda>
    Lambda* dynamicCastTo() const
    {
        if (getVTableForLambda<Lambda>() != vtable)
            return nullptr;
        else
            return &const_cast<Lambda&>(reinterpret_cast<const Lambda&>(classInstance));
    }

    /// @brief Binds a free function to function wrapper
    /// @tparam FreeFunction a regular static function to be wrapper, with a matching signature
    template <R (*FreeFunction)(Args...)>
    void bind()
    {
        checkedOperation(Operation::Destruct, nullptr);
        static SC_LANGUAGE_IF_CONSTEXPR const VTable staticVTable = {
            [](const void* const*, typename TypeTraits::RemoveReference<Args>::type*... args) SC_LANGUAGE_IF_CONSTEXPR
            { return FreeFunction(*args...); },
            [](Operation, const void**, const void* const*) SC_LANGUAGE_IF_CONSTEXPR {}};
        vtable        = &staticVTable;
        classInstance = nullptr;
    }

    /// @brief Binds a class member function to function wrapper
    /// @tparam Class Type of the Class holding MemberFunction
    /// @tparam MemberFunction Pointer to member function with a matching signature
    /// @param c Reference to the instance of class where the method must be bound to
    template <typename Class, R (Class::*MemberFunction)(Args...) const>
    void bind(const Class& c)
    {
        checkedOperation(Operation::Destruct, nullptr);
        static SC_LANGUAGE_IF_CONSTEXPR const VTable staticVTable = {
            [](const void* const* p, typename TypeTraits::RemoveReference<Args>::type*... args) SC_LANGUAGE_IF_CONSTEXPR
            {
                const Class* cls = static_cast<const Class*>(*p);
                return (cls->*MemberFunction)(*args...);
            },
            [](Operation operation, const void** other, const void* const* p) SC_LANGUAGE_IF_CONSTEXPR
            {
                if (operation != Operation::Destruct)
                    *other = *p;
            }};
        vtable        = &staticVTable;
        classInstance = &c;
    }

    /// @brief Binds a class member function to function wrapper
    /// @tparam Class Type of the Class holding MemberFunction
    /// @tparam MemberFunction Pointer to member function with a matching signature
    /// @param c Reference to the instance of class where the method must be bound to
    template <typename Class, R (Class::*MemberFunction)(Args...)>
    void bind(Class& c)
    {
        checkedOperation(Operation::Destruct, nullptr);
        static SC_LANGUAGE_IF_CONSTEXPR const VTable staticVTable = {
            [](const void* const* p, typename TypeTraits::RemoveReference<Args>::type*... args) SC_LANGUAGE_IF_CONSTEXPR
            {
                Class* cls = const_cast<Class*>(static_cast<const Class*>(*p));
                return (cls->*MemberFunction)(*args...);
            },
            [](Operation operation, const void** other, const void* const* p) SC_LANGUAGE_IF_CONSTEXPR
            {
                if (operation != Operation::Destruct)
                    *other = *p;
            }};
        vtable        = &staticVTable;
        classInstance = &c;
    }

    /// @brief Invokes the wrapped function. If no function is bound, this is UB.
    /// @param args Arguments to be passed to the wrapped function
    [[nodiscard]] R operator()(Args... args) const { return vtable->execute(&classInstance, &args...); }
};

template <typename T>
using Delegate = Function<void(T)>;
using Action   = Function<void()>;
//! @}
} // namespace SC
