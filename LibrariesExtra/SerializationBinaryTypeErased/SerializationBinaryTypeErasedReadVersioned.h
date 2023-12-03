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

/// @brief Holds Schema of serialized binary data
struct VersionSchema
{
    Span<const Reflection::TypeInfo> sourceTypes;

    /// @brief Controls compatibility options for versioned deserialization
    struct Options
    {
        bool allowFloatToIntTruncation    = true; ///< allow truncating a float to get an integer value
        bool allowDropEccessArrayItems    = true; ///< drop array items in source data if destination array is smaller
        bool allowDropEccessStructMembers = true; ///< drop fields that have no matching memberTag in destination struct
    };
    Options options; ///< Options for versioned deserialization
};

/// @brief De-serializes binary data with its associated schema into object `T`
struct ReadVersioned
{
    /// @brief Deserialize object `T` from a Binary stream with a reflection schema not matching `T` schema
    /// @tparam T Type of object to be dserialized
    /// @param object The object to deserialize
    /// @param source The stream holding the bytes to be used for deserialization
    /// @param schema The schema used to serialize data in the stream
    /// @return `true` if deserialization succeded
    template <typename T>
    [[nodiscard]] bool readVersioned(T& object, SerializationBinary::Buffer& source, VersionSchema& schema)
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

    detail::ArrayAccess arrayAccess;

    VersionSchema::Options options;

    Span<const Reflection::TypeInfo> sinkTypes;
    Span<uint8_t>                    sinkObject;
    Reflection::TypeInfo             sinkType;
    uint32_t                         sinkTypeIndex = 0;

    Span<const Reflection::TypeInfo> sourceTypes;
    SerializationBinary::Buffer*     sourceObject = nullptr;
    Reflection::TypeInfo             sourceType;
    uint32_t                         sourceTypeIndex = 0;

    [[nodiscard]] bool read();
    [[nodiscard]] bool readStruct();
    [[nodiscard]] bool readArrayVector();
    [[nodiscard]] bool skipCurrent();
};
} // namespace SerializationBinaryTypeErased
} // namespace SC
