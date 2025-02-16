// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#pragma once
#include "Libraries/Containers/Vector.h"
#include "Libraries/Foundation/Result.h"
#include "Libraries/Strings/String.h"
#include "imgui.h"

namespace SC
{
inline Result InputText(const char* name, Buffer& buffer, String& str, bool& modified)
{
    struct Funcs
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
    if (ImGui::InputText(name, buffer.data(), buffer.size(), ImGuiInputTextFlags_CallbackResize,
                         Funcs::MyResizeCallback, &buffer))
    {
        modified = true;
        SC_TRY(str.assign(StringView::fromNullTerminated(buffer.data(), StringEncoding::Ascii)));
    }
    return Result(true);
}
} // namespace SC
