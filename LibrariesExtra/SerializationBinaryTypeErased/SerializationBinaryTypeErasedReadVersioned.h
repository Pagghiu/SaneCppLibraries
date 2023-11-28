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
struct VersionSchema
{
    Span<const Reflection::TypeInfo> sourceTypes;
};

struct SerializerReadVersioned
{
    struct Options
    {
        bool allowFloatToIntTruncation    = true;
        bool allowDropEccessArrayItems    = true;
        bool allowDropEccessStructMembers = true;
    };
    Options options;

    template <typename T>
    [[nodiscard]] bool readVersioned(T& object, SerializationBinary::BinaryBuffer& source, VersionSchema& schema)
    {
        constexpr auto flatSchema = Reflection::FlatSchemaTypeErased::compile<T>();
        sourceTypes               = schema.sourceTypes;
        sinkTypes                 = {flatSchema.typeInfos.values, flatSchema.typeInfos.size};
        sinkNames                 = {flatSchema.typeNames.values, flatSchema.typeNames.size};
        sinkObject                = sinkObject.reinterpret_object(object);
        sourceObject              = &source;
        sinkTypeIndex             = 0;
        sourceTypeIndex           = 0;
        arrayAccess.vectorVtable  = {flatSchema.vtables.vector.values, flatSchema.vtables.vector.size};

        if (sourceTypes.sizeInBytes() == 0 || sourceTypes.data()[0].type != Reflection::TypeCategory::TypeStruct ||
            sinkTypes.sizeInBytes() == 0 || sinkTypes.data()[0].type != Reflection::TypeCategory::TypeStruct)
        {
            return false;
        }
        return read();
    }

  private:
    Span<const Reflection::TypeStringView> sinkNames;

    ArrayAccess arrayAccess;

    Span<const Reflection::TypeInfo> sinkTypes;
    Span<uint8_t>                    sinkObject;
    Reflection::TypeInfo             sinkProperty;
    uint32_t                         sinkTypeIndex = 0;

    Span<const Reflection::TypeInfo>   sourceTypes;
    SerializationBinary::BinaryBuffer* sourceObject = nullptr;
    Reflection::TypeInfo               sourceProperty;
    uint32_t                           sourceTypeIndex = 0;

    [[nodiscard]] bool read();
    [[nodiscard]] bool readStruct();
    [[nodiscard]] bool readArrayVector();
    [[nodiscard]] bool skipCurrent();
};

} // namespace SerializationBinaryTypeErased
} // namespace SC
