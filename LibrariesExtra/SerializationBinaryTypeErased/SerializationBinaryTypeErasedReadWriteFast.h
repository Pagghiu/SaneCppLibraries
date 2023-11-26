// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
// This needs to go before the compiler
#include "../../Libraries/Reflection/ReflectionSC.h"
// Compiler must be after
#include "../../Libraries/SerializationBinary/SerializationBinarySkipper.h"
#include "SerializationBinaryTypeErasedCompiler.h"

namespace SC
{
namespace SerializationBinaryTypeErased
{
struct SerializerReadWriteFast
{
    SerializerReadWriteFast(Serialization::BinaryBuffer& destination) : destination(destination) {}

    template <typename T>
    [[nodiscard]] constexpr bool serialize(const T& object)
    {
        constexpr auto flatSchema      = Reflection::FlatSchemaTypeErased::compile<T>();
        sourceProperties               = {flatSchema.typeInfos.values, flatSchema.typeInfos.size};
        sourceNames                    = {flatSchema.typeNames.values, flatSchema.typeNames.size};
        arrayAccess.vectorVtable       = {flatSchema.vtables.vector.values, flatSchema.vtables.vector.size};
        sourceObject                   = sourceObject.reinterpret_object(object);
        sourceTypeIndex                = 0;
        destination.numberOfOperations = 0;
        if (sourceProperties.sizeInBytes() == 0 ||
            sourceProperties.data()[0].type != Reflection::TypeCategory::TypeStruct)
        {
            return false;
        }
        return write();
    }

  private:
    [[nodiscard]] bool write();
    [[nodiscard]] bool writeStruct();
    [[nodiscard]] bool writeArrayVector();

    Span<const Reflection::TypeInfo>       sourceProperties;
    Span<const Reflection::TypeStringView> sourceNames;
    Serialization::BinaryBuffer&           destination;
    Span<const uint8_t>                    sourceObject;
    uint32_t                               sourceTypeIndex;
    Reflection::TypeInfo                   sourceProperty;
    ArrayAccess                            arrayAccess;
};

struct SimpleBinaryReader
{
    SimpleBinaryReader(Serialization::BinaryBuffer& source) : source(source) {}

    template <typename T>
    [[nodiscard]] bool serialize(T& object)
    {
        constexpr auto flatSchema = Reflection::FlatSchemaTypeErased::compile<T>();

        sinkProperties = {flatSchema.typeInfos.values, flatSchema.typeInfos.size};
        sinkNames      = {flatSchema.typeNames.values, flatSchema.typeNames.size};
        sinkObject     = sinkObject.reinterpret_object(object);
        sinkTypeIndex  = 0;

        arrayAccess.vectorVtable = {flatSchema.vtables.vector.values, flatSchema.vtables.vector.size};

        if (sinkProperties.sizeInBytes() == 0 || sinkProperties.data()[0].type != Reflection::TypeCategory::TypeStruct)
        {
            return false;
        }
        return read();
    }

  private:
    [[nodiscard]] bool read();
    [[nodiscard]] bool readStruct();
    [[nodiscard]] bool readArrayVector();

    Span<const Reflection::TypeInfo>       sinkProperties;
    Span<const Reflection::TypeStringView> sinkNames;
    Reflection::TypeInfo                   sinkProperty;
    uint32_t                               sinkTypeIndex = 0;
    Span<uint8_t>                          sinkObject;
    Serialization::BinaryBuffer&           source;
    ArrayAccess                            arrayAccess;
};

} // namespace SerializationBinaryTypeErased
} // namespace SC
