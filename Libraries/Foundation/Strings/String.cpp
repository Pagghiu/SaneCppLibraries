// Copyright (c) 2022-2023, Stefano Cristiano
//
// All Rights Reserved. Reproduction is not allowed.
#include "String.h"

SC::String::String(StringEncoding encoding) : encoding(encoding)
{
    SC_COMPILER_WARNING_PUSH_OFFSETOF;
    static_assert(alignof(SegmentHeader) == alignof(uint64_t), "alignof(segmentheader)");
    static_assert(SC_COMPILER_OFFSETOF(SmallString<1>, buffer) - SC_COMPILER_OFFSETOF(String, data) ==
                      alignof(SegmentHeader),
                  "Wrong alignment");
    SC_COMPILER_WARNING_POP;
}

SC::String::String(StringView sv) { SC_ASSERT_RELEASE(assign(sv)); }

#if SC_PLATFORM_WINDOWS
wchar_t* SC::String::nativeWritableBytesIncludingTerminator()
{
    SC_ASSERT_RELEASE(encoding == StringEncoding::Utf16);
    return reinterpret_cast<wchar_t*>(data.data());
}
#else
char* SC::String::nativeWritableBytesIncludingTerminator()
{
    SC_ASSERT_RELEASE(encoding < StringEncoding::Utf16);
    return data.data();
}
#endif

bool SC::String::assign(StringView sv)
{
    const size_t length    = sv.sizeInBytes();
    encoding               = sv.getEncoding();
    const uint32_t numZero = StringEncodingGetSize(encoding);
    const bool     result  = data.resizeWithoutInitializing(length + numZero);
    if (sv.isNullTerminated())
    {
        memcpy(data.items, sv.bytesWithoutTerminator(), length + numZero);
    }
    else
    {
        memcpy(data.items, sv.bytesWithoutTerminator(), length);
        for (uint32_t idx = 0; idx < numZero; ++idx)
        {
            data.items[length + idx] = 0;
        }
    }
    return result;
}

SC::StringView SC::String::view() const
{
    if (data.isEmpty())
    {
        const char* cnull = nullptr;
        return StringView(cnull, 0, false, encoding);
    }
    else
    {
        return StringView(data.items, data.size() - StringEncodingGetSize(encoding), true, encoding);
    }
}

bool SC::String::addZeroTerminatorIfNeeded()
{
    const int numZeros = static_cast<int>(StringEncodingGetSize(encoding));
    SC_TRY(data.size() == 0 or data.size() >= static_cast<size_t>(numZeros));
    if (data.size() >= static_cast<size_t>(numZeros))
    {
        for (int idx = 0; idx < numZeros; ++idx)
        {
            (&data.back())[-idx] = 0;
        }
    }
    return true;
}
