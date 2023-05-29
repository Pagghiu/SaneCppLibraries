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
    using Atom = AtomBase<MetaClassBuilderTemplate>;
    constexpr MetaClassBuilderTemplate(Atom* output = nullptr, const uint32_t capacity = 0)
        : MetaClassBuilder(output, capacity)
    {}
};
using FlatSchemaTemplated = Reflection::FlatSchemaCompiler<MetaClassBuilderTemplate>;

} // namespace Reflection
} // namespace SC
