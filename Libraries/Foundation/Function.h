// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../Foundation/TypeTraits.h" // RemoveReference, AddPointer, IsSame

namespace SC
{
//! @addtogroup group_foundation_utility
//! @{

/// @brief Wrapper for function pointers, member functions and lambdas up to `LAMBDA_SIZE`.
///
/// Example:
/// @code
/// int someFunc(float a) { return static_cast<int>(a * 2); }
///
/// Function<int(float)> func = &somefunc;
/// @endcode
/// Size of lambdas or less than LAMBDA_SIZE.
/// If lambda is bigger than `LAMBDA_SIZE` the constructor will static assert.
/// @tparam FuncType Type of function to be wrapped (Lambda, free function or pointer to member function)
template <typename FuncType>
struct Function;

template <typename R, typename... Args>
struct Function<R(Args...)>
{
  private:
    enum class FunctionErasedOperation
    {
        Execute = 0,
        LambdaDestruct,
        LambdaCopyConstruct,
        LambdaMoveConstruct
    };
    using StubFunction = R (*)(FunctionErasedOperation operation, const void** other, const void* const*,
                               typename TypeTraits::AddPointer<Args>::type...);

    static const int LAMBDA_SIZE = sizeof(void*) * 2;

    StubFunction functionStub;
    union
    {
        const void* classInstance;
        char        lambdaMemory[LAMBDA_SIZE] = {0};
    };
    void sendLambdaSignal(FunctionErasedOperation operation, const void** other) const
    {
        if (functionStub)
        {
            (*functionStub)(operation, other, &classInstance, typename TypeTraits::AddPointer<Args>::type(nullptr)...);
        }
    }

    Function(const void* instance, StubFunction stub) : functionStub(stub) { classInstance = instance; }
    using FreeFunction = R (*)(Args...);

  public:
    /// @brief Constructs an empty Function
    Function()
    {
        static_assert(sizeof(Function) == sizeof(void*) * 3, "Function Size");
        functionStub = nullptr;
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
        functionStub = nullptr;
        bind(forward<typename TypeTraits::RemoveReference<Lambda>::type>(lambda));
    }

    /// @brief Destroys the function wrapper
    ~Function() { sendLambdaSignal(FunctionErasedOperation::LambdaDestruct, nullptr); }

    /// @brief Move constructor for Function wrapper
    /// @param other The moved from function
    Function(Function&& other)
    {
        functionStub  = other.functionStub;
        classInstance = other.classInstance;
        other.sendLambdaSignal(FunctionErasedOperation::LambdaMoveConstruct, &classInstance);
        other.sendLambdaSignal(FunctionErasedOperation::LambdaDestruct, nullptr);
        other.functionStub = nullptr;
    }

    /// @brief Copy constructor for Function wrapper
    /// @param other The function to be copied
    Function(const Function& other)
    {
        functionStub = other.functionStub;
        other.sendLambdaSignal(FunctionErasedOperation::LambdaCopyConstruct, &classInstance);
    }

    /// @brief Copy assign a function to current function wrapper. Destroys existing wrapper.
    /// @param other The function to be assigned to current function
    Function& operator=(const Function& other)
    {
        sendLambdaSignal(FunctionErasedOperation::LambdaDestruct, nullptr);
        functionStub = other.functionStub;
        other.sendLambdaSignal(FunctionErasedOperation::LambdaCopyConstruct, &classInstance);
        return *this;
    }

    /// @brief Move assign a function to current function wrapper. Destroys existing wrapper.
    /// @param other The function to be move-assigned to current function
    Function& operator=(Function&& other)
    {
        sendLambdaSignal(FunctionErasedOperation::LambdaDestruct, nullptr);
        functionStub = other.functionStub;
        other.sendLambdaSignal(FunctionErasedOperation::LambdaMoveConstruct, &classInstance);
        other.sendLambdaSignal(FunctionErasedOperation::LambdaDestruct, nullptr);
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
        sendLambdaSignal(FunctionErasedOperation::LambdaDestruct, nullptr);
        functionStub = nullptr;

        new (&classInstance, PlacementNew()) Lambda(forward<Lambda>(lambda));
        static_assert(sizeof(Lambda) <= sizeof(lambdaMemory), "Lambda is too big");
        functionStub = [](FunctionErasedOperation operation, const void** other, const void* const* p,
                          typename TypeTraits::AddPointer<Args>::type... args) -> R
        {
            Lambda& lambda = *reinterpret_cast<Lambda*>(const_cast<void**>(p));
            if (operation == FunctionErasedOperation::Execute)
                SC_LANGUAGE_LIKELY { return lambda(*args...); }
            else if (operation == FunctionErasedOperation::LambdaDestruct)
                lambda.~Lambda();
            else if (operation == FunctionErasedOperation::LambdaCopyConstruct)
                new (other, PlacementNew()) Lambda(lambda);
            else if (operation == FunctionErasedOperation::LambdaMoveConstruct)
                new (other, PlacementNew()) Lambda(move(lambda));
            return R();
        };
    }

    /// @brief Binds a free function to function wrapper
    /// @tparam FreeFunction a regular static function to be wrapper, with a matching signature
    template <R (*FreeFunction)(Args...)>
    void bind()
    {
        sendLambdaSignal(FunctionErasedOperation::LambdaDestruct, nullptr);
        classInstance = nullptr;
        functionStub  = &FunctionWrapper<FreeFunction>;
    }

    /// @brief Binds a class member function to function wrapper
    /// @tparam Class Type of the Class holding MemberFunction
    /// @tparam MemberFunction Pointer to member function with a matching signature
    /// @param c Reference to the instance of class where the method must be bound to
    template <typename Class, R (Class::*MemberFunction)(Args...) const>
    void bind(const Class& c)
    {
        sendLambdaSignal(FunctionErasedOperation::LambdaDestruct, nullptr);
        classInstance = &c;
        functionStub  = &MemberWrapper<Class, MemberFunction>;
    }

    /// @brief Binds a class member function to function wrapper
    /// @tparam Class Type of the Class holding MemberFunction
    /// @tparam MemberFunction Pointer to member function with a matching signature
    /// @param c Reference to the instance of class where the method must be bound to
    template <typename Class, R (Class::*MemberFunction)(Args...)>
    void bind(Class& c)
    {
        sendLambdaSignal(FunctionErasedOperation::LambdaDestruct, nullptr);
        classInstance = &c;
        functionStub  = &MemberWrapper<Class, MemberFunction>;
    }

    /// @brief Binds a class member function to function wrapper
    /// @tparam Class Type of the Class holding MemberFunction
    /// @tparam MemberFunction Pointer to member function with a matching signature
    /// @param c Reference to the instance of class where the method must be bound to
    /// @return The function wrapper
    template <typename Class, R (Class::*MemberFunction)(Args...)>
    static Function fromMember(Class& c)
    {
        return Function(&c, &MemberWrapper<Class, MemberFunction>);
    }

    /// @brief Binds a class member function to function wrapper
    /// @tparam Class Type of the Class holding MemberFunction
    /// @tparam MemberFunction Pointer to member function with a matching signature
    /// @param c Reference to the instance of class where the method must be bound to
    /// @return The function wrapper
    template <typename Class, R (Class::*MemberFunction)(Args...) const>
    static Function fromMember(const Class& c)
    {
        return Function(&c, &MemberWrapper<Class, MemberFunction>);
    }

    /// @brief Invokes the wrapped function. If no function is bound, this is UB.
    /// @param args Arguments to be passed to the wrapped function
    /// @return the return value of the invoked function.
    [[nodiscard]] R operator()(Args... args) const
    {
        return (*functionStub)(FunctionErasedOperation::Execute, nullptr, &classInstance, &args...);
    }

  private:
    template <typename Class, R (Class::*MemberFunction)(Args...)>
    static R MemberWrapper(FunctionErasedOperation operation, const void** other, const void* const* p,
                           typename TypeTraits::RemoveReference<Args>::type*... args)
    {
        if (operation == FunctionErasedOperation::Execute)
            SC_LANGUAGE_LIKELY
            {
                Class* cls = const_cast<Class*>(static_cast<const Class*>(*p));
                return (cls->*MemberFunction)(*args...);
            }
        else if (operation == FunctionErasedOperation::LambdaCopyConstruct or
                 operation == FunctionErasedOperation::LambdaMoveConstruct)
        {
            *other = *p;
        }

        return R();
    }
    template <typename Class, R (Class::*MemberFunction)(Args...) const>
    static R MemberWrapper(FunctionErasedOperation operation, const void** other, const void* const* p,
                           typename TypeTraits::RemoveReference<Args>::type*... args)
    {
        if (operation == FunctionErasedOperation::Execute)
            SC_LANGUAGE_LIKELY
            {
                const Class* cls = static_cast<const Class*>(*p);
                return (cls->*MemberFunction)(*args...);
            }
        else if (operation == FunctionErasedOperation::LambdaCopyConstruct or
                 operation == FunctionErasedOperation::LambdaMoveConstruct)
        {
            *other = *p;
        }
        return R();
    }

    template <R (*FreeFunction)(Args...)>
    static R FunctionWrapper(FunctionErasedOperation operation, const void** other, const void* const* p,
                             typename TypeTraits::RemoveReference<Args>::type*... args)
    {
        SC_COMPILER_UNUSED(other);
        SC_COMPILER_UNUSED(p);
        if (operation == FunctionErasedOperation::Execute)
            SC_LANGUAGE_LIKELY { return FreeFunction(*args...); }
        return R();
    }
};

template <typename T>
using Delegate = Function<void(T)>;
using Action   = Function<void()>;
//! @}
} // namespace SC
