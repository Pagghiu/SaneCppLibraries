// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
// This needs to go before the compiler
#include "../../../Libraries/Reflection/ReflectionSC.h"
// Compiler must be after
#include "../../../Libraries/SerializationBinary/Internal/SerializationBinarySchema.h"
#include "SerializationBinaryTypeErasedCompiler.h"
namespace SC
{
struct SerializationBinaryBufferReader;
/// @brief De-serializes binary data with its associated schema into object `T`
struct SerializationBinaryTypeErasedReadVersioned
{
    template <typename T>
    [[nodiscard]] bool loadVersioned(T& object, SerializationBinaryBufferReader& source, SerializationSchema& schema)
    {
        constexpr auto flatSchema = Reflection::SchemaTypeErased::compile<T>();

        options         = schema.options;
        sourceTypes     = schema.sourceTypes;
        sinkTypes       = {flatSchema.typeInfos.values, flatSchema.typeInfos.size};
        sinkNames       = {flatSchema.typeNames.values, flatSchema.typeNames.size};
        sinkObject      = sinkObject.reinterpret_object(object);
        sourceObject    = &source;
        sinkTypeIndex   = 0;
        sourceTypeIndex = 0;

        arrayAccess.vectorVtable = {flatSchema.vtables.vector.values, flatSchema.vtables.vector.size};

        if (sourceTypes.sizeInBytes() == 0 || sourceTypes.data()[0].type != Reflection::TypeCategory::TypeStruct ||
            sinkTypes.sizeInBytes() == 0 || sinkTypes.data()[0].type != Reflection::TypeCategory::TypeStruct)
        {
            return false;
        }
        return read();
    }

  private:
    Span<const Reflection::TypeStringView> sinkNames;

    detail::SerializationBinaryTypeErasedArrayAccess arrayAccess;

    SerializationBinaryOptions options;

    Span<const Reflection::TypeInfo> sinkTypes;
    Span<uint8_t>                    sinkObject;
    Reflection::TypeInfo             sinkType;
    uint32_t                         sinkTypeIndex = 0;

    Span<const Reflection::TypeInfo> sourceTypes;
    SerializationBinaryBufferReader* sourceObject = nullptr;
    Reflection::TypeInfo             sourceType;
    uint32_t                         sourceTypeIndex = 0;

    [[nodiscard]] bool read();
    [[nodiscard]] bool readStruct();
    [[nodiscard]] bool readArrayVector();
    [[nodiscard]] bool skipCurrent();
};
} // namespace SC
