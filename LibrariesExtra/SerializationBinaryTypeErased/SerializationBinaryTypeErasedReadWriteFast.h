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
namespace SerializationBinaryTypeErased
{
struct SerializerWriteFast
{
    SerializerWriteFast(SerializationBinary::BinaryBuffer& destination) : destination(destination) {}

    template <typename T>
    [[nodiscard]] constexpr bool serialize(const T& object)
    {
        constexpr auto flatSchema      = Reflection::SchemaTypeErased::compile<T>();
        sourceTypes                    = {flatSchema.typeInfos.values, flatSchema.typeInfos.size};
        sourceNames                    = {flatSchema.typeNames.values, flatSchema.typeNames.size};
        arrayAccess.vectorVtable       = {flatSchema.vtables.vector.values, flatSchema.vtables.vector.size};
        sourceObject                   = sourceObject.reinterpret_object(object);
        sourceTypeIndex                = 0;
        destination.numberOfOperations = 0;
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

    Span<const Reflection::TypeInfo>       sourceTypes;
    Span<const Reflection::TypeStringView> sourceNames;
    SerializationBinary::BinaryBuffer&     destination;
    Span<const uint8_t>                    sourceObject;
    uint32_t                               sourceTypeIndex;
    Reflection::TypeInfo                   sourceProperty;
    ArrayAccess                            arrayAccess;
};

struct SerializerReadFast
{
    SerializerReadFast(SerializationBinary::BinaryBuffer& source) : source(source) {}

    template <typename T>
    [[nodiscard]] bool serialize(T& object)
    {
        constexpr auto flatSchema = Reflection::SchemaTypeErased::compile<T>();

        sinkTypes     = {flatSchema.typeInfos.values, flatSchema.typeInfos.size};
        sinkNames     = {flatSchema.typeNames.values, flatSchema.typeNames.size};
        sinkObject    = sinkObject.reinterpret_object(object);
        sinkTypeIndex = 0;

        arrayAccess.vectorVtable = {flatSchema.vtables.vector.values, flatSchema.vtables.vector.size};

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

    Span<const Reflection::TypeInfo>       sinkTypes;
    Span<const Reflection::TypeStringView> sinkNames;
    Reflection::TypeInfo                   sinkProperty;
    uint32_t                               sinkTypeIndex = 0;
    Span<uint8_t>                          sinkObject;
    SerializationBinary::BinaryBuffer&     source;
    ArrayAccess                            arrayAccess;
};

} // namespace SerializationBinaryTypeErased
} // namespace SC
