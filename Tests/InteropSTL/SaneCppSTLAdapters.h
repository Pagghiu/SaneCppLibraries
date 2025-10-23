// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT

// Remember to compile this with SC_COMPILER_ENABLE_STD_CPP=1, and possibly exceptions and RTTI enabled

#include "Libraries/Foundation/Internal/IGrowableBuffer.h"
#include "Libraries/Memory/String.h"

#include <string>
#include <string_view>
#include <vector>

namespace SC
{
// From std:: --> Sane C++ Libraries
inline StringSpan asSane(const std::string_view str)
{
    return {{str.data(), str.size()}, false, StringEncoding::Utf8}; // false == not null terminated
}
inline StringSpan asSane(const std::string& str)
{
    return {{str.c_str(), str.size()}, true, StringEncoding::Utf8}; // true == null terminated
}
#if SC_PLATFORM_WINDOWS
inline StringSpan asSane(const std::wstring& str)
{
    return {{str.c_str(), str.size()}, true, StringEncoding::Utf16}; // true == null terminated
}
#endif
// From Sane C++ Libraries --> std::
inline std::string asStdString(SC::StringSpan s) { return {s.bytesWithoutTerminator(), s.sizeInBytes()}; }
inline std::string asStdString(const SC::String& str) { return asStdString(str.view()); }

inline std::string_view asStd(SC::StringSpan s) { return {s.bytesWithoutTerminator(), s.sizeInBytes()}; }
inline std::string_view asStd(const SC::String& str) { return asStd(str.view()); }

// A common GrowableBuffer implementation for basic_string<char / wchar_t> and vector<char/wchar_t>
template <typename Container>
struct GrowableBufferSTL : public IGrowableBuffer
{
    using T = typename Container::value_type;
    Container& str;
    GrowableBufferSTL(Container& str) : IGrowableBuffer(&GrowableBufferSTL::tryGrowTo), str(str)
    {
        // C++ STL doesn't provide any way of doing an uninitialized_resize for strings
        // so best we can do is to preventively resize to capacity, even if this will
        // just be very inefficient, as it will memset the entire capacity of the string.
        // Of course if one wants a solution, he could just use Sane C++ Strings / containers
        // or just any Sane C++ style container (there are many projects you can find on github).
        // If you're really brave to stick with the STL (or crazy, it depends) you could also do something like
        // https://raw.githubusercontent.com/facebook/folly/refs/heads/main/folly/memory/UninitializedMemoryHacks.h
        // If you have an uninitializedResize operation then you can remove this resize and just just use it
        // instead of the other resize in tryGrowTo
        const size_t oldSize = str.size();
        str.resize(str.capacity());
        IGrowableBuffer::directAccess = {oldSize * sizeof(T), str.capacity() * sizeof(T),
                                         str.empty() ? nullptr : &str[0]};
    }

    ~GrowableBufferSTL() noexcept { finalize(); }

    void finalize() noexcept
    {
        if (directAccess.sizeInBytes == 0)
        {
            str.clear();
        }
        else if ((directAccess.sizeInBytes / sizeof(T)) < str.size())
        {
            // drop any excess byte, resize(newSize) with newSize < str.size() cannot throw
            str.resize(directAccess.sizeInBytes / sizeof(T));
        }
    }

    static bool tryGrowTo(IGrowableBuffer& gb, size_t newSizeInBytes) noexcept
    {
        GrowableBufferSTL& self = static_cast<GrowableBufferSTL&>(gb);
        try
        {
            auto& str = self.str;
            if (newSizeInBytes > str.capacity())
            {
                str.resize(newSizeInBytes / sizeof(T));
                str.resize(str.capacity());
            }
            self.directAccess = {newSizeInBytes, str.capacity() * sizeof(T), str.empty() ? nullptr : &str[0]};
        }
        catch (...)
        {
            return false;
        }
        return true;
    }
};

template <typename T>
struct GrowableBuffer<std::basic_string<T>> : public GrowableBufferSTL<std::basic_string<T>>
{
    using GrowableBufferSTL<std::basic_string<T>>::GrowableBufferSTL;
    // We just hardcode Utf8 for string and utf16 for wstrings (on windows)
    static StringEncoding getEncodingFor(const std::basic_string<char>&) { return StringEncoding::Utf8; }
#if SC_PLATFORM_WINDOWS
    static_assert(sizeof(wchar_t) == 2, "UTF16");
    static StringEncoding getEncodingFor(const std::basic_string<wchar_t>&) { return StringEncoding::Utf16; }
#endif
};

template <typename T>
struct GrowableBuffer<std::vector<T>> : public GrowableBufferSTL<std::vector<T>>
{
    using GrowableBufferSTL<std::vector<T>>::GrowableBufferSTL;
    // We just hardcode Utf8 for string and utf16 for wstrings (on windows)
    static StringEncoding getEncodingFor(const std::vector<char>&) { return StringEncoding::Utf8; }
#if SC_PLATFORM_WINDOWS
    static_assert(sizeof(wchar_t) == 2, "UTF16");
    static StringEncoding getEncodingFor(const std::vector<wchar_t>&) { return StringEncoding::Utf16; }
#endif
};

} // namespace SC
