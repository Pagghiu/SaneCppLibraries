#pragma once
#include "Types.h"

namespace SC
{
template <typename FuncType>
struct Function;
enum class FunctionErasedOperation
{
    Execute = 0,
    LambdaDestruct,
    LambdaCopyConstruct,
    LambdaMoveConstruct
};
} // namespace SC

template <typename R, typename... Args>
struct SC::Function<R(Args...)>
{
  private:
    using StubFunction = R (*)(FunctionErasedOperation operation, void* other, const void* const*, Args...);

    static const int LAMBDA_SIZE = sizeof(uint64_t) * 2;

    StubFunction functionStub;
    union
    {
        const void* classInstance;
        char_t      lambdaMemory[LAMBDA_SIZE];
    };
    void sendLambdaSignal(FunctionErasedOperation operation, void* other) const
    {
        if (functionStub)
        {
            (*functionStub)(operation, other, &classInstance, Args()...);
        }
    }

  public:
    Function()
    {
        functionStub = nullptr;
        memset(lambdaMemory, 0, LAMBDA_SIZE);
    }

    Function(const void* instance, StubFunction stub) : functionStub(stub)
    {
        memset(lambdaMemory, 0, LAMBDA_SIZE);
        classInstance = instance;
    }

    template <typename Lambda>
    Function(Lambda&& lambda)
    {
        functionStub = nullptr;
        bind(forward<Lambda>(lambda));
    }

    typedef R (*FreeFunction)(Args...);

    ~Function() { sendLambdaSignal(FunctionErasedOperation::LambdaDestruct, nullptr); }

    Function(Function&& other)
    {
        functionStub  = other.functionStub;
        classInstance = other.classInstance;
        other.sendLambdaSignal(FunctionErasedOperation::LambdaMoveConstruct, &classInstance);
        other.sendLambdaSignal(FunctionErasedOperation::LambdaDestruct, nullptr);
        other.functionStub = nullptr;
    }

    Function(const Function& other)
    {
        functionStub = other.functionStub;
        other.sendLambdaSignal(FunctionErasedOperation::LambdaCopyConstruct, &classInstance);
    }

    Function& operator=(const Function& other)
    {
        sendLambdaSignal(FunctionErasedOperation::LambdaDestruct, nullptr);
        functionStub = other.functionStub;
        other.sendLambdaSignal(FunctionErasedOperation::LambdaCopyConstruct, &classInstance);
        return *this;
    }

    Function& operator=(Function&& other)
    {
        sendLambdaSignal(FunctionErasedOperation::LambdaDestruct, nullptr);
        functionStub = other.functionStub;
        other.sendLambdaSignal(FunctionErasedOperation::LambdaMoveConstruct, &classInstance);
        other.sendLambdaSignal(FunctionErasedOperation::LambdaDestruct, nullptr);
        other.functionStub = nullptr;
        return *this;
    }

    bool operator==(const Function& other) const
    {
        return functionStub == other.functionStub && memcmp(lambdaMemory, other.lambdaMemory, LAMBDA_SIZE) == 0;
    }

    template <typename Lambda>
    void bind(Lambda&& lambda)
    {
        new (&classInstance, PlacementNew()) Lambda(lambda);
        static_assert(sizeof(Lambda) <= sizeof(lambdaMemory), "Lambda is too big");
        functionStub = [](FunctionErasedOperation operation, void* other, const void* const* p, Args... args) -> R
        {
            Lambda& lambda = *reinterpret_cast<Lambda*>(const_cast<void**>(p));
            if (operation == FunctionErasedOperation::Execute)
                SC_LIKELY { return lambda(forward<Args>(args)...); }
            else if (operation == FunctionErasedOperation::LambdaDestruct)
                lambda.~Lambda();
            else if (operation == FunctionErasedOperation::LambdaCopyConstruct)
                new (other, PlacementNew()) Lambda(lambda);
            else if (operation == FunctionErasedOperation::LambdaMoveConstruct)
                new (other, PlacementNew()) Lambda(move(lambda));
            return R();
        };
    }

    template <R (*FreeFunction)(Args...)>
    void bind()
    {
        sendLambdaSignal(FunctionErasedOperation::LambdaDestruct, nullptr);
        classInstance = nullptr;
        functionStub  = &FunctionWrapper<FreeFunction>;
    }

    template <typename Class, R (Class::*MemberFunction)(Args...) const>
    void bind(const Class* c)
    {
        sendLambdaSignal(FunctionErasedOperation::LambdaDestruct, nullptr);
        SC_RELEASE_ASSERT(c != nullptr);
        classInstance = c;
        functionStub  = &MemberWrapper<Class, MemberFunction>;
    }

    template <typename Class, R (Class::*MemberFunction)(Args...)>
    void bind(Class* c)
    {
        sendLambdaSignal(FunctionErasedOperation::LambdaDestruct, nullptr);
        SC_RELEASE_ASSERT(c != nullptr);
        classInstance = c;
        functionStub  = &MemberWrapper<Class, MemberFunction>;
    }

    [[nodiscard]] R operator()(Args... args) const
    {
        return (*functionStub)(FunctionErasedOperation::Execute, nullptr, &classInstance, forward<Args>(args)...);
    }

  private:
    template <typename R2, class Class2, typename... Args2>
    friend struct FunctionDeducerCreator;
    template <typename Class, R (Class::*MemberFunction)(Args...)>
    static R MemberWrapper(FunctionErasedOperation operation, void* other, const void* const* p, Args... args)
    {
        if (operation == FunctionErasedOperation::Execute)
            SC_LIKELY
            {
                Class* cls = const_cast<Class*>(static_cast<const Class*>(*p));
                return (cls->*MemberFunction)(forward<Args>(args)...);
            }
        return R();
    }
    template <typename Class, R (Class::*MemberFunction)(Args...) const>
    static R MemberWrapper(FunctionErasedOperation operation, void* other, const void* const* p, Args... args)
    {
        if (operation == FunctionErasedOperation::Execute)
            SC_LIKELY
            {
                const Class* cls = static_cast<const Class*>(*p);
                return (cls->*MemberFunction)(forward<Args>(args)...);
            }
        return R();
    }

    template <R (*FreeFunction)(Args...)>
    static R FunctionWrapper(FunctionErasedOperation operation, void* other, const void* const* p, Args... args)
    {
        if (operation == FunctionErasedOperation::Execute)
            SC_LIKELY { return FreeFunction(forward<Args>(args)...); }
        return R();
    }
};

namespace SC
{
template <typename R, class Class, typename... Args>
struct FunctionDeducerCreator
{
    template <R (*FreeFunction)(Args...)>
    static Function<R(Args...)> Bind()
    {
        return Function<R(Args...)>(nullptr, &Function<R(Args...)>::template FunctionWrapper<FreeFunction>);
    }

    template <R (Class::*MemberFunction)(Args...)>
    static Function<R(Args...)> Bind(Class* object)
    {
        SC_RELEASE_ASSERT(object != nullptr);
        return Function<R(Args...)>(object, &Function<R(Args...)>::template MemberWrapper<Class, MemberFunction>);
    }

    template <R (Class::*MemberFunction)(Args...) const>
    static Function<R(Args...)> Bind(const Class* object)
    {
        SC_RELEASE_ASSERT(object != nullptr);
        return Function<R(Args...)>(object, &Function<R(Args...)>::template MemberWrapper<Class, MemberFunction>);
    }
};

template <typename R, class T, typename... Args>
FunctionDeducerCreator<R, T, Args...> FunctionDeducer(R (T::*)(Args...))
{
    return FunctionDeducerCreator<R, T, Args...>();
}

template <typename R, class T, typename... Args>
FunctionDeducerCreator<R, T, Args...> FunctionDeducer(R (T::*)(Args...) const)
{
    return FunctionDeducerCreator<R, T, Args...>();
}

class FunctionEmptyClass
{
};
template <typename R, typename... Args>
FunctionDeducerCreator<R, FunctionEmptyClass, Args...> FunctionDeducer(R(Args...))
{
    return FunctionDeducerCreator<R, FunctionEmptyClass, Args...>();
}
#define SC_FUNCTION_FREE(FUNCTION_PARAM) SC::FunctionDeducer(FUNCTION_PARAM).Bind<FUNCTION_PARAM>()
#define SC_FUNCTION_MEMBER(FUNCTION_PARAM, FUNCTION_OBJECT)                                                            \
    SC::FunctionDeducer(FUNCTION_PARAM).Bind<FUNCTION_PARAM>(FUNCTION_OBJECT)
} // namespace SC
