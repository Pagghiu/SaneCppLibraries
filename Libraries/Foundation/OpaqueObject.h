// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "AlignedStorage.h"
namespace SC
{
//! @addtogroup group_foundation_utility
//! @{

/// @brief Hides implementation details from public headers (static PIMPL). @n
/// Opaque object avoids the heap allocation that often comes with PIMPL, allowing to hide OS specific details from
/// public headers.
/// User declares size in bytes of a struct in the header but the structure can be defined in an implementation .cpp
/// file. Choosing a size that is too small will generate a `static_assert` that contains in the error message the
/// minimum size to use. Up to 4 functions will need to be defined to avoid linker errors (`construct`, `destruct`,
/// `moveConstruct`, `moveAssign`). These functions are meant to be defined in a `.cpp` file that will know how to
/// construct Object, as it can see its definition.
/// @tparam Definition Pass in a custom Definition declaring Sizes and alignment on different platforms
///
/// Example:
///
/// ... in the header file
/// @snippet Libraries/FileSystemWatcher/FileSystemWatcher.h OpaqueDeclarationSnippet
/// ... in .cpp file
/// Declare Internal struct with all platform specific details (requiring OS specific headers).
/// @snippet Libraries/FileSystemWatcher/Internal/FileSystemWatcherApple.inl OpaqueDefinition1Snippet
/// Declare only two of the four functions to avoid linker errors.
/// @snippet Libraries/FileSystemWatcher/FileSystemWatcher.cpp OpaqueDefinition2Snippet
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
#elif SC_PLATFORM_LINUX
    static constexpr int Size = Definition::Linux;
#else
    static constexpr int Size = Definition::Default;
#endif
    static constexpr int Alignment = Definition::Alignment;

    AlignedStorage<Size, Alignment> buffer;

    using Handle = AlignedStorage<Size, Alignment>;
    static void construct(Handle& buffer);
    static void destruct(Object& obj);
    static void moveConstruct(Handle& buffer, Object&& obj);
    static void moveAssign(Object& selfPointer, Object&& obj);
};

//! @}
} // namespace SC
