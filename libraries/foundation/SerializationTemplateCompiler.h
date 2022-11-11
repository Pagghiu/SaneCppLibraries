#pragma once
#include "Reflection.h"
namespace SC
{
namespace Reflection
{
struct FlatSchemaTemplated
{
    struct EmptyPayload
    {
    };
    struct MetaClassBuilderTemplate : public MetaClassBuilder<MetaClassBuilderTemplate>
    {
        typedef AtomBase<MetaClassBuilderTemplate> Atom;
        EmptyPayload                               payload;
        constexpr MetaClassBuilderTemplate(Atom* output = nullptr, const int capacity = 0)
            : MetaClassBuilder(output, capacity)
        {}
    };
    typedef FlatSchemaCompiler::FlatSchemaCompiler<MetaProperties, MetaClassBuilderTemplate::Atom,
                                                   MetaClassBuilderTemplate>
        FlatSchemaBase;

    // You can customize:
    // - MAX_LINK_BUFFER_SIZE: maximum number of "complex types" (anything that is not a primitive) that can be built
    // - MAX_TOTAL_ATOMS: maximum number of atoms (struct members). When using constexpr it will trim it to actual size.
    template <typename T, int MAX_LINK_BUFFER_SIZE = 20, int MAX_TOTAL_ATOMS = 100>
    static constexpr auto compile()
    {
        constexpr auto schema = FlatSchemaBase::compileAllAtomsFor<MAX_LINK_BUFFER_SIZE, MAX_TOTAL_ATOMS>(
            &MetaClass<T>::template build<MetaClassBuilderTemplate>);
        static_assert(schema.atoms.size > 0, "Something failed in compileAllAtomsFor");
        FlatSchemaBase::FlatSchema<schema.atoms.size> result;
        for (int i = 0; i < schema.atoms.size; ++i)
        {
            result.properties.values[i] = schema.atoms.values[i].properties;
            result.names.values[i]      = schema.atoms.values[i].name;
        }
        result.properties.size = schema.atoms.size;
        result.names.size      = schema.atoms.size;
        return result;
    }
};

} // namespace Reflection
} // namespace SC
