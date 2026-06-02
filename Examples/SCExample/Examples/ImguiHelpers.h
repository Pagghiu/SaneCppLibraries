// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "Libraries/Common/Result.h"
#include "Libraries/Containers/Vector.h"
#include "Libraries/Memory/String.h"
#include "Libraries/Strings/StringView.h"
#include "imgui.h"

namespace SC
{
namespace Internal
{
struct InputTextFuncs
{
    static int MyResizeCallback(ImGuiInputTextCallbackData* data)
    {
        if (data->EventFlag == ImGuiInputTextFlags_CallbackResize)
        {
            Buffer& str = *reinterpret_cast<Buffer*>(data->UserData);
            if (not str.resize(static_cast<size_t>(data->BufSize)))
            {
                return 0;
            }
            data->Buf = str.data();
        }
        return 0;
    }
};

inline Result PrepareInputTextBuffer(Buffer& buffer, String& str)
{
    if (str.view().isEmpty())
    {
        buffer.clear();
        SC_TRY(buffer.resize(1));
    }
    else
    {
        buffer.clear();
        SC_TRY(buffer.append(str.view().toCharSpan()));
        SC_TRY(buffer.push_back(0));
    }
    return Result(true);
}
} // namespace Internal

inline Result InputText(const char* name, Buffer& buffer, String& str, bool& modified)
{
    SC_TRY(Internal::PrepareInputTextBuffer(buffer, str));
    if (ImGui::InputText(name, buffer.data(), buffer.size(), ImGuiInputTextFlags_CallbackResize,
                         Internal::InputTextFuncs::MyResizeCallback, &buffer))
    {
        modified = true;
        SC_TRY(str.assign(StringView::fromNullTerminated(buffer.data(), StringEncoding::Utf8)));
    }
    return Result(true);
}

inline Result InputTextMultiline(const char* name, Buffer& buffer, String& str, ImVec2 size, bool& modified)
{
    SC_TRY(Internal::PrepareInputTextBuffer(buffer, str));
    if (ImGui::InputTextMultiline(name, buffer.data(), buffer.size(), size, ImGuiInputTextFlags_CallbackResize,
                                  Internal::InputTextFuncs::MyResizeCallback, &buffer))
    {
        modified = true;
        SC_TRY(str.assign(StringView::fromNullTerminated(buffer.data(), StringEncoding::Utf8)));
    }
    return Result(true);
}
} // namespace SC
