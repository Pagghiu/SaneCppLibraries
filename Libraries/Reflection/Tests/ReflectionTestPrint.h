// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#include "../../Strings/String.h"
#include "../../Strings/StringBuilder.h"
#include "../../System/Console.h"
#include "../Reflection.h"

namespace SC
{
inline const StringView typeCategoryToStringView(Reflection::TypeCategory type)
{
    switch (type)
    {
    case Reflection::TypeCategory::TypeInvalid: return "TypeInvalid ";
    case Reflection::TypeCategory::TypeUINT8: return "TypeUINT8   ";
    case Reflection::TypeCategory::TypeUINT16: return "TypeUINT16  ";
    case Reflection::TypeCategory::TypeUINT32: return "TypeUINT32  ";
    case Reflection::TypeCategory::TypeUINT64: return "TypeUINT64  ";
    case Reflection::TypeCategory::TypeINT8: return "TypeINT8    ";
    case Reflection::TypeCategory::TypeINT16: return "TypeINT16   ";
    case Reflection::TypeCategory::TypeINT32: return "TypeINT32   ";
    case Reflection::TypeCategory::TypeINT64: return "TypeINT64   ";
    case Reflection::TypeCategory::TypeFLOAT32: return "TypeFLOAT32 ";
    case Reflection::TypeCategory::TypeDOUBLE64: return "TypeDOUBLE64";
    case Reflection::TypeCategory::TypeStruct: return "TypeStruct  ";
    case Reflection::TypeCategory::TypeArray: return "TypeArray   ";
    case Reflection::TypeCategory::TypeVector: return "TypeVector  ";
    }
    Assert::unreachable();
}

template <int NUM_TYPES>
inline void printFlatSchema(Console& console, const Reflection::TypeInfo (&type)[NUM_TYPES],
                            const Reflection::TypeStringView (&names)[NUM_TYPES])
{
    String buffer(StringEncoding::Ascii);
    int    typeIndex = 0;
    while (typeIndex < NUM_TYPES)
    {
        StringBuilder builder(buffer, StringBuilder::Clear);
        typeIndex += printTypes(builder, typeIndex, type + typeIndex, names + typeIndex) + 1;
        console.print(buffer.view());
    }
}

inline int printTypes(StringBuilder& builder, int typeIndex, const Reflection::TypeInfo* types,
                      const Reflection::TypeStringView* typeNames)
{
    SC_COMPILER_WARNING_PUSH_UNUSED_RESULT;
    const StringView typeName(typeNames[0].data, typeNames[0].length, false, StringEncoding::Ascii);
    builder.append("[{:02}] {}", typeIndex, typeName);
    switch (types[0].type)
    {
    case Reflection::TypeCategory::TypeStruct:
        builder.append(" (Struct with {} members - Packed = {})", types[0].getNumberOfChildren(),
                       types[0].structInfo.isPacked ? "true" : "false");
        break;
    case Reflection::TypeCategory::TypeArray:
        builder.append(" (Array of size {} with {} children - Packed = {})", types[0].arrayInfo.numElements,
                       types[0].getNumberOfChildren(), types[0].arrayInfo.isPacked ? "true" : "false");
        break;
    case Reflection::TypeCategory::TypeVector:
        builder.append(" (Vector with {} children)", types[0].getNumberOfChildren());
        break;
    default: break;
    }
    builder.append("\n{\n");
    for (int idx = 0; idx < types[0].getNumberOfChildren(); ++idx)
    {
        const Reflection::TypeInfo& field = types[idx + 1];
        builder.append("[{:02}] ", typeIndex + idx + 1);

        const StringView fieldName(typeNames[idx + 1].data, typeNames[idx + 1].length, false, StringEncoding::Ascii);
        if (types[0].type == Reflection::TypeCategory::TypeStruct)
        {
            builder.append("Type={}\tOffset={}\tSize={}\tName={}", typeCategoryToStringView(field.type),
                           field.memberInfo.offsetInBytes, field.sizeInBytes, fieldName);
        }
        else
        {
            builder.append("Type={}\t         \tSize={}\tName={}", typeCategoryToStringView(field.type),
                           field.sizeInBytes, fieldName);
        }
        if (field.hasValidLinkIndex())
        {
            builder.append("\t[LinkIndex={}]", field.getLinkIndex());
        }
        builder.append("\n");
    }

    builder.append("}\n");
    SC_COMPILER_WARNING_POP;
    return types[0].getNumberOfChildren();
}
} // namespace SC
