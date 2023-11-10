// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "Handles.h" // OpaqueHandle
namespace SC
{

//! @addtogroup group_foundation_utility
//! @{

/// @brief Helper class to select the correct size of Opaque object on different platfoms
/// @tparam T Type of Opaque object, declared in the header but defined in the cpp
/// @tparam SizesDefinition Size of the OS Handle on Windows, Apple and elsewere
/// @tparam A Aligment in bytes of the handle
template <typename T, typename SizesDefinition, int A = alignof(void*)>
struct OpaqueSizes
{
    using Object = T;
    /// Will be set to Sizes::Windows, Sizes::Apple or Sizes::Default according to current platform
#if SC_PLATFORM_WINDOWS
    static constexpr int Size = SizesDefinition::Windows;
#elif SC_PLATFORM_APPLE
    static constexpr int Size = SizesDefinition::Apple;
#else
    static constexpr int Size = SizesDefinition::Default;
#endif
    /// @brief Alignment of the Handle
    static constexpr int Alignment = A;

    /// @brief Sized and aligned handle for current platform
    using Handle = OpaqueHandle<Size, Alignment>;
};

/// @brief  Declare struct capable of constructing, destructing and move construct/assign a class defined in a cpp file.
///         This is a support object to construct an OpaqueUnique.
///         The idea is to forward declare `Object` (declared as Traits::Object) in the header and define it in the
///         cpp, filling it OS specific types.
///         In the same cpp file can define all the 4 undefined static functions below.
/// @tparam ObjSizes Pass an OpaqueSizes object, built from a struct with sizes of the Opaque on each platform
template <typename ObjSizes>
struct OpaqueBuilder
{
    using Handle = typename ObjSizes::Handle;
    using Object = typename ObjSizes::Object;

    static constexpr int Size      = ObjSizes::Size;
    static constexpr int Alignment = ObjSizes::Alignment;

    // Funtions to be defined by user in its cpp file
    static void construct(Handle& buffer);
    static void destruct(Object& obj);
    static void moveConstruct(Handle& buffer, Object&& obj);
    static void moveAssign(Object& pthis, Object&& obj);
};

/// @brief  Wraps an Opaque object (a structure declared in header but defined in cpp file).
///         This is a kind of static PIMPL, where by declaring the size of the object in bytes in the header
///         we can avoid doing any dynamic allocation.
///         Such inline size of the object can be different on different Operating Systems.
/// @tparam Definition Pass in a custom Definition declaring Sizes and alignment on different platforms and object type
template <typename Definition>
struct OpaqueUnique
{
    static constexpr int Alignment = Definition::Alignment;

    using Object  = typename Definition::Object;
    using Sizes   = OpaqueSizes<Object, Definition, Alignment>;
    using Builder = OpaqueBuilder<Sizes>;

    static constexpr int Size = Sizes::Size;

    OpaqueUnique() { Builder::construct(buffer); }
    ~OpaqueUnique() { Builder::destruct(get()); }
    OpaqueUnique(OpaqueUnique&& other) { Builder::moveConstruct(buffer, forward<Object>(other.get())); }
    OpaqueUnique& operator=(OpaqueUnique&& other)
    {
        Builder::moveAssign(get(), forward<Object>(other.get()));
        return *this;
    }

    // Disallow copy construction and copy assignment
    OpaqueUnique(const OpaqueUnique&)            = delete;
    OpaqueUnique& operator=(const OpaqueUnique&) = delete;

    Object&       get() { return reinterpret_cast<Object&>(buffer); }
    const Object& get() const { return reinterpret_cast<const Object&>(buffer); }

  private:
    OpaqueHandle<Size, Alignment> buffer;
};

//! }@

} // namespace SC
