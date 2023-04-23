#pragma once
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include "../Foundation/String.h"

namespace SC
{
struct UtilityWindows
{
    [[nodiscard]] static ReturnCode formatWindowsError(int errorNumber, String& buffer)
    {
        LPWSTR messageBuffer = nullptr;
        size_t size          = FormatMessageW(
            FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL,
            errorNumber, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), reinterpret_cast<LPWSTR>(&messageBuffer), 0, NULL);
        auto deferFree = MakeDeferred([&]() { LocalFree(messageBuffer); });

        const StringView sv =
            StringView(Span<const wchar_t>(messageBuffer, size * sizeof(wchar_t)), true, StringEncoding::Utf16);
        if (not buffer.assign(sv))
        {
            return "UtilityWindows::formatWindowsError - returned error"_a8;
        }
        return true;
    }
};

} // namespace SC
