// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "AlignedStorage.h"
namespace SC
{
//! @addtogroup group_foundation_utility
//! @{

/// @brief  Static PIMPL helper. Typically user declares size in bytes of a structure in the header
///         but the structure is defined in an implementation .cpp file.
///         That's just a tiny helper to save some minimal amount of typing.
///         When creating this class you will be forced to redefining 4 functions to avoid linker errors
///         (construct, destruct, moveConstruct, moveAssign).
///         These functions are meant to be defined in a .cpp file that will know how to construct Object.
/// @tparam Definition Pass in a custom Definition declaring Sizes and alignment on different platforms
template <typename Definition>
struct OpaqueObject
{
    using Object = typename Definition::Object;

    OpaqueObject() { construct(buffer); }
    ~OpaqueObject() { destruct(get()); }
    OpaqueObject(OpaqueObject&& other) { moveConstruct(buffer, forward<Object>(other.get())); }
    OpaqueObject& operator=(OpaqueObject&& other)
    {
        moveAssign(get(), forward<Object>(other.get()));
        return *this;
    }

    // Disallow copy construction and copy assignment
    OpaqueObject(const OpaqueObject&)            = delete;
    OpaqueObject& operator=(const OpaqueObject&) = delete;

    Object&       get() { return reinterpret_cast<Object&>(buffer); }
    const Object& get() const { return reinterpret_cast<const Object&>(buffer); }

  private:
#if SC_PLATFORM_WINDOWS
    static constexpr int Size = Definition::Windows;
#elif SC_PLATFORM_APPLE
    static constexpr int Size = Definition::Apple;
#else
    static constexpr int Size = Definition::Default;
#endif
    static constexpr int Alignment = Definition::Alignment;

    AlignedStorage<Size, Alignment> buffer;

    using Handle = AlignedStorage<Size, Alignment>;
    static void construct(Handle& buffer);
    static void destruct(Object& obj);
    static void moveConstruct(Handle& buffer, Object&& obj);
    static void moveAssign(Object& pthis, Object&& obj);
};

//! }@
} // namespace SC
