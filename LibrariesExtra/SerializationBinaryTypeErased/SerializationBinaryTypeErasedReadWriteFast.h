// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
// This needs to go before the compiler
#include "../../Libraries/Reflection/ReflectionSC.h"
// Compiler must be after
#include "../../Libraries/SerializationBinary/SerializationBinaryBuffer.h"
#include "../../Libraries/SerializationBinary/SerializationBinarySkipper.h"
#include "SerializationBinaryTypeErasedCompiler.h"

namespace SC
{
/// @brief Serializes C++ objects to a binary format (see @ref library_serialization_binary_type_erased)
namespace SerializationBinaryTypeErased
{
/// @brief Writes object `T` to a buffer
struct WriteFast
{
    /// @brief Writes object `T` to a buffer
    /// @tparam T Type of object to be serialized
    /// @param object The object to be serialized
    /// @param buffer The buffer receiving serialized bytes
    /// @return `true` if the operation succeeded
    template <typename T>
    [[nodiscard]] constexpr bool serialize(const T& object, SerializationBinary::Buffer& buffer)
    {
        constexpr auto flatSchema = Reflection::SchemaTypeErased::compile<T>();

        arrayAccess.vectorVtable = {flatSchema.vtables.vector.values, flatSchema.vtables.vector.size};

        sourceTypes     = {flatSchema.typeInfos.values, flatSchema.typeInfos.size};
        sourceNames     = {flatSchema.typeNames.values, flatSchema.typeNames.size};
        sourceObject    = sourceObject.reinterpret_object(object);
        sourceTypeIndex = 0;

        destination = &buffer;

        destination->numberOfOperations = 0;

        if (sourceTypes.sizeInBytes() == 0 || sourceTypes.data()[0].type != Reflection::TypeCategory::TypeStruct)
        {
            return false;
        }
        return write();
    }

  private:
    [[nodiscard]] bool write();
    [[nodiscard]] bool writeStruct();
    [[nodiscard]] bool writeArrayVector();

    SerializationBinary::Buffer* destination = nullptr;

    Span<const Reflection::TypeInfo>       sourceTypes;
    Span<const Reflection::TypeStringView> sourceNames;

    Span<const uint8_t>  sourceObject;
    uint32_t             sourceTypeIndex;
    Reflection::TypeInfo sourceType;

    detail::ArrayAccess arrayAccess;
};

/// @brief Reads object `T` from a buffer, assuming no versioning changes with data in buffer
struct ReadFast
{
    /// @brief Reads object `T` from a buffer, assuming no versioning changes
    /// @tparam T Type of object to be deserialized
    /// @param object The object to be deserialized
    /// @param buffer The buffer providing bytes for deserialization
    /// @return `true` if the operation succeeded
    template <typename T>
    [[nodiscard]] bool serialize(T& object, SerializationBinary::Buffer& buffer)
    {
        constexpr auto flatSchema = Reflection::SchemaTypeErased::compile<T>();

        arrayAccess.vectorVtable = {flatSchema.vtables.vector.values, flatSchema.vtables.vector.size};

        sinkTypes     = {flatSchema.typeInfos.values, flatSchema.typeInfos.size};
        sinkNames     = {flatSchema.typeNames.values, flatSchema.typeNames.size};
        sinkObject    = sinkObject.reinterpret_object(object);
        sinkTypeIndex = 0;

        source = &buffer;

        if (sinkTypes.sizeInBytes() == 0 || sinkTypes.data()[0].type != Reflection::TypeCategory::TypeStruct)
        {
            return false;
        }
        return read();
    }

  private:
    [[nodiscard]] bool read();
    [[nodiscard]] bool readStruct();
    [[nodiscard]] bool readArrayVector();

    SerializationBinary::Buffer* source = nullptr;

    Span<const Reflection::TypeInfo>       sinkTypes;
    Span<const Reflection::TypeStringView> sinkNames;

    Reflection::TypeInfo sinkType;
    uint32_t             sinkTypeIndex = 0;
    Span<uint8_t>        sinkObject;

    detail::ArrayAccess arrayAccess;
};
} // namespace SerializationBinaryTypeErased
} // namespace SC
