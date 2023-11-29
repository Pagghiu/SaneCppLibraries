// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../../Libraries/Reflection/Reflection.h"
#include "ReflectionAuto.h"

namespace SC
{
namespace Reflection
{
namespace AutoStructured
{
// UniversallyConstructible is Constructible from anything. It's declared with a size parameter
// to allow declaring a variable number of them using an index sequence
template <size_t>
struct UniversallyConstructible
{
    template <typename... Ts>
    constexpr UniversallyConstructible(Ts&&...)
    {}
};

// ArgumentGetAt returns the Nth argument passed to it's operator()
template <size_t N, typename = Auto::MakeIndexSequence<N>>
struct ArgumentGetAt;

template <size_t N, size_t... Index>
struct ArgumentGetAt<N, Auto::IndexSequence<Index...>>
{
    template <typename T, typename... Rest>
    constexpr decltype(auto) operator()(UniversallyConstructible<Index>..., T&& x, Rest&&...) const
    {
        return forward<T>(x);
    }
};

// Just a shorthand for ArgumentGetAt
template <size_t N, typename... T>
constexpr decltype(auto) ArgumentGet(T&&... args)
{
    return ArgumentGetAt<N>{}(SC::forward<T>(args)...);
}

// Pad can parametrically move the offset location of its Member m by manipulating the padding through O param
#pragma pack(push, 1)
template <typename Member, size_t O>
struct Pad
{
    char   pad[O];
    Member m;
};
#pragma pack(pop)

// Just the edge case when Padding is zero, to avoid zero-sized array
template <typename Member>
struct Pad<Member, 0>
{
    Member m;
};

// MemberApply decomposes type T into NumMembers fields and applies function F to all of the extracted members
template <int NumMembers, typename T, typename F>
constexpr decltype(auto) MemberApply(T& obj, F func)
{
    if constexpr (NumMembers == 1)
    {
        auto& [a] = obj;
        return func(a);
    }
    else if constexpr (NumMembers == 2)
    {
        auto& [a, b] = obj;
        return func(a, b);
    }
    else if constexpr (NumMembers == 3)
    {
        auto& [a, b, c] = obj;
        return func(a, b, c);
    }
    else if constexpr (NumMembers == 4)
    {
        auto& [a, b, c, d] = obj;
        return func(a, b, c, d);
    }
    else if constexpr (NumMembers == 5)
    {
        auto& [a, b, c, d, e] = obj;
        return func(a, b, c, d, e);
    }
    else if constexpr (NumMembers == 6)
    {
        auto& [a, b, c, d, e, f] = obj;
        return func(a, b, c, d, e, f);
    }
    else if constexpr (NumMembers == 7)
    {
        auto& [a, b, c, d, e, f, g] = obj;
        return func(a, b, c, d, e, f, g);
    }
    else if constexpr (NumMembers == 8)
    {
        auto& [a, b, c, d, e, f, g, h] = obj;
        return func(a, b, c, d, e, f, g, h);
    }
    else if constexpr (NumMembers == 9)
    {
        auto& [a, b, c, d, e, f, g, h, i] = obj;
        return func(a, b, c, d, e, f, g, h, i);
    }
    else if constexpr (NumMembers == 10)
    {
        auto& [a, b, c, d, e, f, g, h, i, j] = obj;
        return func(a, b, c, d, e, f, g, h, i, j);
    }
    else if constexpr (NumMembers == 11)
    {
        auto& [a, b, c, d, e, f, g, h, i, j, k] = obj;
        return func(a, b, c, d, e, f, g, h, i, j, k);
    }
    else if constexpr (NumMembers > 11)
    {
        static_assert(NumMembers == 11, "NOT IMPLEMENTED");
    }
    return func();
}

template <int NumMembers, int N, typename T>
constexpr auto MemberGetAddress(T& obj)
{
    if constexpr (NumMembers == 1)
    {
        auto& [a] = obj;
        return ArgumentGet<N>(&a);
    }
    else if constexpr (NumMembers == 2)
    {
        auto& [a, b] = obj;
        return ArgumentGet<N>(&a, &b);
    }
    else if constexpr (NumMembers == 3)
    {
        auto& [a, b, c] = obj;
        return ArgumentGet<N>(&a, &b, &c);
    }
    else if constexpr (NumMembers == 4)
    {
        auto& [a, b, c, d] = obj;
        return ArgumentGet<N>(&a, &b, &c, &d);
    }
    else if constexpr (NumMembers == 5)
    {
        auto& [a, b, c, d, e] = obj;
        return ArgumentGet<N>(&a, &b, &c, &d, &e);
    }
    else if constexpr (NumMembers == 6)
    {
        auto& [a, b, c, d, e, f] = obj;
        return ArgumentGet<N>(&a, &b, &c, &d, &e, &f);
    }
    else if constexpr (NumMembers == 7)
    {
        auto& [a, b, c, d, e, f, g] = obj;
        return ArgumentGet<N>(&a, &b, &c, &d, &e, &f, &g);
    }
    else if constexpr (NumMembers == 8)
    {
        auto& [a, b, c, d, e, f, g, h] = obj;
        return ArgumentGet<N>(&a, &b, &c, &d, &e, &f, &g, &h);
    }
    else if constexpr (NumMembers == 9)
    {
        auto& [a, b, c, d, e, f, g, h, i] = obj;
        return ArgumentGet<N>(&a, &b, &c, &d, &e, &f, &g, &h, &i);
    }
    else if constexpr (NumMembers == 10)
    {
        auto& [a, b, c, d, e, f, g, h, i, j] = obj;
        return ArgumentGet<N>(&a, &b, &c, &d, &e, &f, &g, &h, &i, &j);
    }
    else if constexpr (NumMembers == 11)
    {
        auto& [a, b, c, d, e, f, g, h, i, j, k] = obj;
        return ArgumentGet<N>(&a, &b, &c, &d, &e, &f, &g, &h, &i, &j, &k);
    }
    else if constexpr (NumMembers > 11)
    {
        static_assert(NumMembers == 11, "NOT IMPLEMENTED");
    }
}
// MakeUnion creates a static constexpr union of a non-constexpr Base class and a parametrically paddable member
// Its constructor is initializing just a char, so that Base and Pad can still be non-constexpr classes, as they
// will never need to be initialized. We will just be comparing their addresses in the union (See MemberOffsetOff)
template <typename Base, typename Member, size_t O>
struct MakeUnion
{
    union U
    {
        char           c;
        Base           base;
        Pad<Member, O> pad;
        constexpr U() noexcept : c{} {};
    };
    const static U u; // It * should * not generate storage as it's used only from constexpr context
};

// MemberOffsetOf will return the offset of a given Member in class T, that is its MemberIndex field and
// of type MemberType.
// The algorithm is just comparing the address of the artificially padded member (see Pad<M,O>) with
// the address of the Nth (MemberIndex) field of T. As these addresses belong to objects of the same
// static constexpr union, we can compare them and increment offset until they match.
template <typename T, typename MemberType, int MemberIndex, int NumMembers, int StartOffset = 0>
constexpr auto MemberOffsetOf()
{
#if __clang__
#pragma clang diagnostic push
    // Clang complains for MakeUnion::u not being a static constexpr, but it's only used in constexpr contexts
#pragma clang diagnostic ignored "-Wundefined-var-template"
#endif
    using UnionType = MakeUnion<T, MemberType, StartOffset>;
    // MSVC has issues with the decltype(auto) of the chain of MemberApply + ArgumentGetAt combo
    // with members that are C-arrays...not sure what automatic casting happens or fails
    // constexpr auto  memAddr = &MemberApply<NumMembers>(UnionType::u.base, ArgumentGetAt<MemberIndex>{});
    constexpr auto memAddr = MemberGetAddress<NumMembers, MemberIndex>(UnionType::u.base);
    constexpr auto padAddr = &UnionType::u.pad.m;
    if constexpr (memAddr == padAddr)
    {
        return StartOffset;
    }
    else if constexpr (memAddr > padAddr)
    {
        // TODO: Optimize with larger steps
        return MemberOffsetOf<T, MemberType, MemberIndex, NumMembers, StartOffset + 1>();
    }
    else if constexpr (memAddr < padAddr)
    {
        return MemberOffsetOf<T, MemberType, MemberIndex, NumMembers, StartOffset - 1>();
    }
#if __clang__
#pragma clang diagnostic pop
#endif
}

template <typename T, typename U>
struct loophole_type_insert;

template <typename T, int... NN>
struct loophole_type_insert<T, Auto::IntegerSequence<int, NN...>>
{
    template <typename... Types>
    using type = Auto::IndexSequenceFor<typename Auto::c_op<T, NN>::template type<Types>...>;
};

template <typename T, typename... Types>
using InsertAsFriendDeclarations =
    typename loophole_type_insert<T, Auto::MakeIntegerSequence<int, sizeof...(Types)>>::template type<Types...>;

template <typename T, int N>
struct DecomposeInElements
{
    static_assert(N <= 11, "Cannot handle more than 11 members");
};

// clang-format off
template <typename T>
struct DecomposeInElements<T, 1> { using type = decltype([](T& val) {
    [[maybe_unused]] auto& [a] = val;
    return InsertAsFriendDeclarations<T, decltype(a)>{};
});};

template <typename T>
struct DecomposeInElements<T, 2> { using type = decltype([](T& val){
    [[maybe_unused]] auto& [a, b] = val;
    return InsertAsFriendDeclarations<T, decltype(a), decltype(b)>{};
});};

template <typename T>
struct DecomposeInElements<T, 3> { using type = decltype([](T& val){
    [[maybe_unused]] auto& [a, b, c] = val;
    return InsertAsFriendDeclarations<T, decltype(a), decltype(b), decltype(c)>{};
});};

template <typename T>
struct DecomposeInElements<T, 4> { using type = decltype([](T& val){
    [[maybe_unused]] auto& [a, b, c, d] = val;
    return InsertAsFriendDeclarations<T, decltype(a), decltype(b), decltype(c), decltype(d)>{};
});};

template <typename T>
struct DecomposeInElements<T, 5> { using type = decltype([](T& val){
    [[maybe_unused]] auto& [a, b, c, d, e] = val;
    return InsertAsFriendDeclarations<T, decltype(a), decltype(b), decltype(c), decltype(d), decltype(e)>{};
});};

template <typename T>
struct DecomposeInElements<T, 6> { using type = decltype([](T& val){
    [[maybe_unused]] auto& [a, b, c, d, e, f] = val;
    return InsertAsFriendDeclarations<T, decltype(a), decltype(b), decltype(c), decltype(d), decltype(e), decltype(f)>{};
});};

template <typename T>
struct DecomposeInElements<T, 7> { using type = decltype([](T& val){
    [[maybe_unused]] auto& [a, b, c, d, e, f, g] = val;
    return InsertAsFriendDeclarations<T, decltype(a), decltype(b), decltype(c), decltype(d), decltype(e), decltype(f), decltype(g)>{};
});};

template <typename T>
struct DecomposeInElements<T, 8> { using type = decltype([](T& val){
    [[maybe_unused]] auto& [a, b, c, d, e, f, g, h] = val;
    return InsertAsFriendDeclarations<T, decltype(a), decltype(b), decltype(c), decltype(d), decltype(e), decltype(f), decltype(g), decltype(h)>{};
});};

template <typename T>
struct DecomposeInElements<T, 9> { using type = decltype([](T& val){
    [[maybe_unused]] auto& [a, b, c, d, e, f, g, h, i] = val;
    return InsertAsFriendDeclarations<T, decltype(a), decltype(b), decltype(c), decltype(d), decltype(e), decltype(f), decltype(g), decltype(h), decltype(i)>{};
});};
template <typename T>
struct DecomposeInElements<T, 10> { using type = decltype([](T& val){
    [[maybe_unused]] auto& [a, b, c, d, e, f, g, h, i, j] = val;
    return InsertAsFriendDeclarations<T, decltype(a), decltype(b), decltype(c), decltype(d), decltype(e), decltype(f), decltype(g), decltype(h), decltype(i), decltype(j)>{};
});};
template <typename T>
struct DecomposeInElements<T, 11> { using type = decltype([](T& val){
    [[maybe_unused]] auto& [a, b, c, d, e, f, g, h, i, j, k] = val;
    return InsertAsFriendDeclarations<T, decltype(a), decltype(b), decltype(c), decltype(d), decltype(e), decltype(f), decltype(g), decltype(h), decltype(i), decltype(j), decltype(k)>{};
});};
// clang-format on
template <int N>
struct UniversallyConvertible
{
    template <typename U>
    operator U();
};

template <typename T, int... NN>
constexpr int CountAggregates(...)
{
    return static_cast<int>(sizeof...(NN) - 1);
}
template <typename T, int... NN>
constexpr auto CountAggregates(int) -> decltype(T{{UniversallyConvertible<NN>{}}...}, 0)
{
    return CountAggregates<T, NN..., sizeof...(NN)>(0);
}
template <typename T, int N = CountAggregates<T>(0), typename F = typename DecomposeInElements<T, N>::type>
using TypeListFor = typename Auto::type_list<T, Auto::MakeIntegerSequence<int, N>>::type;

template <typename T, typename MemberVisitor, int NumMembers>
struct DescribeLoopholeVisitor
{
    MemberVisitor& builder;

    template <int Order, typename R>
    constexpr bool visit()
    {
        R T::*         ptr         = nullptr;
        constexpr auto fieldOffset = AutoStructured::MemberOffsetOf<T, R, Order, NumMembers>();
        return builder(Order, ptr, "", fieldOffset);
    }
};

template <typename T>
struct DescribeAutomaticStructured
{
    using TypeList = AutoStructured::TypeListFor<T>;
    [[nodiscard]] static constexpr TypeCategory getCategory() { return TypeCategory::TypeStruct; }

    template <typename MemberVisitor>
    [[nodiscard]] static constexpr bool build(MemberVisitor& builder)
    {
        if (not builder.types.writeAndAdvance(MemberVisitor::Type::template createStruct<T>()))
            return false;
        if (not visit(builder))
            return false;
        return true;
    }

    template <typename MemberVisitor>
    static constexpr bool visit(MemberVisitor&& builder)
    {
        return Auto::TypeListVisit<TypeList, TypeList::size>::visit(
            DescribeLoopholeVisitor<T, MemberVisitor, TypeList::size>{builder});
    }
};
} // namespace AutoStructured

template <typename Class>
struct Reflect : public AutoStructured::DescribeAutomaticStructured<Class>
{
};

} // namespace Reflection
} // namespace SC
