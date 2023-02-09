// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#pragma once
#include "../Reflection/Reflection.h"
#include "../Reflection/ReflectionFlatSchemaCompiler.h"
namespace SC
{
namespace Reflection
{
struct MetaClassBuilderTemplate : public MetaClassBuilder<MetaClassBuilderTemplate>
{
    typedef AtomBase<MetaClassBuilderTemplate> Atom;
    struct EmptyPayload
    {
    };
    EmptyPayload payload;
    constexpr MetaClassBuilderTemplate(Atom* output = nullptr, const int capacity = 0)
        : MetaClassBuilder(output, capacity)
    {}
};
using FlatSchemaTemplated = Reflection::FlatSchemaCompiler<MetaClassBuilderTemplate>;

} // namespace Reflection
} // namespace SC
