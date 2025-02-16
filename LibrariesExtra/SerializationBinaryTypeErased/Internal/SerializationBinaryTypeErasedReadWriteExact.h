// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
// This needs to go before the compiler
#include "../../../Libraries/Reflection/ReflectionSC.h"
// Compiler must be after
#include "../../../Libraries/SerializationBinary/Internal/SerializationBinaryBuffer.h"
#include "SerializationBinaryTypeErasedCompiler.h"

namespace SC
{
struct SerializationBinaryBufferWriter;

/// @brief Writes object `T` to a buffer
struct SerializationBinaryTypeErasedWriteExact
{
    template <typename T>
    [[nodiscard]] constexpr bool write(const T& object, SerializationBinaryBufferWriter& buffer)
    {
        constexpr auto flatSchema = Reflection::SchemaTypeErased::compile<T>();

        using VectorVTable = detail::SerializationBinaryTypeErasedArrayAccess::VectorVTable;

        arrayAccess.vectorVtable =
            Span<const VectorVTable>(flatSchema.vtables.vector.values, flatSchema.vtables.vector.size);

        sourceTypes  = Span<const Reflection::TypeInfo>(flatSchema.typeInfos.values, flatSchema.typeInfos.size);
        sourceNames  = Span<const Reflection::TypeStringView>(flatSchema.typeNames.values, flatSchema.typeNames.size);
        sourceObject = sourceObject.reinterpret_object(object);
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

    SerializationBinaryBufferWriter* destination = nullptr;

    Span<const Reflection::TypeInfo>       sourceTypes;
    Span<const Reflection::TypeStringView> sourceNames;

    Span<const char>     sourceObject;
    uint32_t             sourceTypeIndex;
    Reflection::TypeInfo sourceType;

    detail::SerializationBinaryTypeErasedArrayAccess arrayAccess;
};

/// @brief Reads object `T` from a buffer, assuming no versioning changes with data in buffer
struct SerializationBinaryTypeErasedReadExact
{
    template <typename T>
    [[nodiscard]] bool loadExact(T& object, SerializationBinaryBufferReader& buffer)
    {
        constexpr auto flatSchema = Reflection::SchemaTypeErased::compile<T>();

        using VectorVTable = detail::SerializationBinaryTypeErasedArrayAccess::VectorVTable;
        arrayAccess.vectorVtable =
            Span<const VectorVTable>(flatSchema.vtables.vector.values, flatSchema.vtables.vector.size);

        sinkTypes     = Span<const Reflection::TypeInfo>(flatSchema.typeInfos.values, flatSchema.typeInfos.size);
        sinkNames     = Span<const Reflection::TypeStringView>(flatSchema.typeNames.values, flatSchema.typeNames.size);
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

    SerializationBinaryBufferReader* source = nullptr;

    Span<const Reflection::TypeInfo>       sinkTypes;
    Span<const Reflection::TypeStringView> sinkNames;

    Reflection::TypeInfo sinkType;
    uint32_t             sinkTypeIndex = 0;
    Span<char>           sinkObject;

    detail::SerializationBinaryTypeErasedArrayAccess arrayAccess;
};
} // namespace SC
