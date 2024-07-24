// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
// --------------------------------------------------------------------------------------------------
// SC_BEGIN_PLUGIN
//
// Name:          Serialization
// Version:       1
// Description:   Showcase binary and json serialization of model and view state
// Category:      Generic
// Build:         libc
// Dependencies:
//
// SC_END_PLUGIN
// --------------------------------------------------------------------------------------------------
#include "imgui.h"

// --------------------------------------------------------------------------------------------------
// Instead of using SC::Vector<T> to hold data to serialize, just for the sake of it, let's describe
// a custom templated vector type (ImVector<T>) to SC::Reflection, SC::Serialization{Binary|Text}.
//
// - Reflect<ImVector<T>> informs SC::Reflect that ImVector<T> is a "vector-like" type
// - ExtendedTypeInfo<ImVector<T>> knows how to resize and access custom vector contents
// - SerializerBinaryRead* informs SC::SerializationBinary that ImVector<T> is a "vector-like" type
// - SerializerTextRead* informs SC::SerializationText that ImVector<T> is a "vector-like" type
// --------------------------------------------------------------------------------------------------
#include "Libraries/Reflection/Reflection.h"
#include "Libraries/SerializationBinary/Internal/SerializationBinaryReadVersioned.h"
#include "Libraries/SerializationBinary/Internal/SerializationBinaryReadWriteExact.h"
#include "Libraries/SerializationText/Internal/SerializationTextReadVersioned.h"

namespace SC
{
namespace Reflection
{

template <typename T>
struct Reflect<ImVector<T>>
{
    static constexpr TypeCategory getCategory() { return TypeCategory::TypeVector; }

    template <typename MemberVisitor>
    [[nodiscard]] static constexpr bool build(MemberVisitor& builder)
    {
        // Add Vector type
        if (not builder.addType(MemberVisitor::Type::template createArray<ImVector<T>>("ImVector", 1, {false, 0})))
            return false;

        // Add dependent item type
        return builder.addType(MemberVisitor::Type::template createGeneric<T>());
    }
};

template <typename T>
struct ExtendedTypeInfo<ImVector<T>>
{
    static constexpr bool IsPacked = false;

    [[nodiscard]] static int  size(const ImVector<T>& object) { return object.size(); }
    [[nodiscard]] static T*   data(ImVector<T>& object) { return object.begin(); }
    [[nodiscard]] static bool resizeWithoutInitializing(ImVector<T>& container, size_t newSize)
    {
        container.resize(static_cast<int>(newSize));
        return true;
    }
    [[nodiscard]] static bool resize(ImVector<T>& object, size_t newSize)
    {
        object.resize(static_cast<int>(newSize));
        return true;
    }
};
} // namespace Reflection

namespace Serialization
{
// clang-format off
template <typename BinaryStream, typename T>
struct SerializerBinaryReadVersioned<BinaryStream, ImVector<T>> : public SerializationBinaryVersionedVector<BinaryStream, ImVector<T>, T, 0xffffffff> { };

template <typename BinaryStream, typename T>
struct SerializerBinaryReadWriteExact<BinaryStream, ImVector<T>> : public SerializerBinaryExactVector<BinaryStream, ImVector<T>, T> { };

template <typename TextStream, typename T>
struct SerializationTextReadWriteExact<TextStream, ImVector<T>> : public SerializationTextExactVector<TextStream, ImVector<T>, T> { };

template <typename TextStream, typename T>
struct SerializationTextReadVersioned<TextStream, ImVector<T>> : public SerializationTextVersionedVector<TextStream, ImVector<T>, T> { };
// clang-format on

} // namespace Serialization
} // namespace SC

#include "Libraries/Containers/SmallVector.h"
#include "Libraries/FileSystem/FileSystem.h"
#include "Libraries/FileSystem/FileSystemDirectories.h"
#include "Libraries/FileSystem/Path.h"
#include "Libraries/Plugin/PluginMacros.h"
#include "Libraries/Process/Process.h"
#include "Libraries/SerializationBinary/SerializationBinary.h"
#include "Libraries/SerializationText/SerializationJson.h"
#include "Libraries/Strings/String.h"
#include "Libraries/Strings/StringBuilder.h"

#include "../ISCExample.h"
#include <math.h>
namespace SC
{
struct SerializationExampleModel;
struct SerializationExampleView;
struct SerializationExampleModelState;
struct SerializationExampleViewState;
} // namespace SC

SC_REFLECT_STRUCT_VISIT(ImVec2)
SC_REFLECT_STRUCT_FIELD(0, x)
SC_REFLECT_STRUCT_FIELD(1, y)
SC_REFLECT_STRUCT_LEAVE()

struct SC::SerializationExampleModelState
{
    ImVector<ImVec2> points;
};

SC_REFLECT_STRUCT_VISIT(SC::SerializationExampleModelState)
SC_REFLECT_STRUCT_FIELD(0, points)
SC_REFLECT_STRUCT_LEAVE()

struct SC::SerializationExampleViewState
{
    bool addingLine        = false;
    bool enableGrid        = true;
    bool enableContextMenu = true;

    ImVec2 scrolling = {0, 0};

    String jsonSerializationPath;
    String binarySerializationPath;
};
SC_REFLECT_STRUCT_VISIT(SC::SerializationExampleViewState)
SC_REFLECT_STRUCT_FIELD(0, addingLine)
SC_REFLECT_STRUCT_FIELD(1, enableGrid)
SC_REFLECT_STRUCT_FIELD(2, enableContextMenu)
SC_REFLECT_STRUCT_FIELD(3, scrolling)
SC_REFLECT_STRUCT_FIELD(4, jsonSerializationPath)
SC_REFLECT_STRUCT_FIELD(5, binarySerializationPath)
SC_REFLECT_STRUCT_LEAVE()

struct SC::SerializationExampleModel
{
    SerializationExampleModelState modelState;

    Result saveToBinary(Vector<uint8_t>& modelStateBuffer)
    {
        return Result(SC::SerializationBinary::writeWithSchema(modelState, modelStateBuffer));
    }

    Result loadFromBinary(Span<const uint8_t> modelStateSpan)
    {
        return Result(SC::SerializationBinary::loadVersionedWithSchema(modelState, modelStateSpan));
    }

    Result saveToBinaryFile(const StringView fileName)
    {
        Vector<uint8_t> buffer;
        SC_TRY(saveToBinary(buffer));
        return FileSystem().write(fileName, buffer.toSpanConst());
    }

    Result loadFromBinaryFile(const StringView fileName)
    {
        Vector<uint8_t> buffer;
        SC_TRY(FileSystem().read(fileName, buffer));
        return loadFromBinary(buffer.toSpanConst());
    }

    Result saveToJSONFile(StringView jsonPath)
    {
        Vector<char>       buffer;
        StringFormatOutput output(StringEncoding::Ascii, buffer);
        SC_TRY(SC::SerializationJson::write(modelState, output));
        Span<const char> jsonSpan;
        SC_TRY(buffer.toSpanConst().sliceStartLength(0, buffer.size() - 1, jsonSpan));
        return FileSystem().writeString(jsonPath, {jsonSpan, false, StringEncoding::Ascii});
    }

    Result loadFromJSONFile(StringView jsonPath)
    {
        String buffer;
        SC_TRY(FileSystem().read(jsonPath, buffer, StringEncoding::Ascii));
        return Result(SC::SerializationJson::loadVersioned(modelState, buffer.view()));
    }
};

struct SC::SerializationExampleView
{
    SerializationExampleViewState viewState;

    Result init()
    {
        FileSystemDirectories directories;
        SC_TRY(directories.init());
        const StringView appPath = SC::Path::dirname(directories.getApplicationPath(), SC::Path::Type::AsNative);
        SC_TRY(SC::Path::join(viewState.jsonSerializationPath, {appPath, "state.json"}));
        SC_TRY(SC::Path::join(viewState.binarySerializationPath, {appPath, "state.binary"}));
        return Result(true);
    }

    Result saveToBinary(Vector<uint8_t>& viewStateBuffer)
    {
        return Result(SC::SerializationBinary::writeWithSchema(viewState, viewStateBuffer));
    }

    Result loadFromBinary(Span<const uint8_t> viewStateSpan)
    {
        return Result(SC::SerializationBinary::loadVersionedWithSchema(viewState, viewStateSpan));
    }

    void draw(SerializationExampleModel& model)
    {
        if (ImGui::Button("Show##1"))
        {
            (void)showJSONInFinder(viewState.jsonSerializationPath.view());
        }
        ImGui::SameLine();
        ImGui::Text("%s", viewState.jsonSerializationPath.view().bytesIncludingTerminator());
        if (ImGui::Button("Show##2"))
        {
            (void)showJSONInFinder(viewState.binarySerializationPath.view());
        }
        ImGui::SameLine();
        ImGui::Text("%s", viewState.binarySerializationPath.view().bytesIncludingTerminator());
        if (ImGui::Button("Load from Binary"))
        {
            (void)model.loadFromBinaryFile(viewState.binarySerializationPath.view());
        }
        ImGui::SameLine();
        if (ImGui::Button("Save to Binary"))
        {
            (void)model.saveToBinaryFile(viewState.binarySerializationPath.view());
        }
        ImGui::SameLine();
        if (ImGui::Button("Load from JSON"))
        {
            (void)model.loadFromJSONFile(viewState.jsonSerializationPath.view());
        }
        ImGui::SameLine();
        if (ImGui::Button("Save to JSON"))
        {
            (void)model.saveToJSONFile(viewState.jsonSerializationPath.view());
        }
        drawCanvas(model);
    }

    void drawCanvas(SerializationExampleModel& model)
    {
        ImVector<ImVec2>& points = model.modelState.points;

        ImVec2& scrolling               = viewState.scrolling;
        bool&   adding_line             = viewState.addingLine;
        bool&   opt_enable_grid         = viewState.enableGrid;
        bool&   opt_enable_context_menu = viewState.enableContextMenu;

        // -------------------------------------------------------------
        // The following code has been copied from imgui_demo.cpp
        // -------------------------------------------------------------
        SC_COMPILER_WARNING_PUSH_UNUSED_RESULT;
        ImGui::Separator();
        ImGui::Text("** The canvas control below has been copy/pasted from dear-imgui demo file **");
        ImGui::Separator();
        ImGui::Checkbox("Enable grid", &opt_enable_grid);
        ImGui::Checkbox("Enable context menu", &opt_enable_context_menu);
        ImGui::Text("Mouse Left: drag to add lines,\nMouse Right: drag to scroll, click for context menu.");

        // Using InvisibleButton() as a convenience 1) it will advance the layout cursor and 2) allows us to use
        // IsItemHovered()/IsItemActive()
        ImVec2 canvas_p0 = ImGui::GetCursorScreenPos();    // ImDrawList API uses screen coordinates!
        ImVec2 canvas_sz = ImGui::GetContentRegionAvail(); // Resize canvas to what's available
        if (canvas_sz.x < 50.0f)
            canvas_sz.x = 50.0f;
        if (canvas_sz.y < 50.0f)
            canvas_sz.y = 50.0f;
        ImVec2 canvas_p1 = ImVec2(canvas_p0.x + canvas_sz.x, canvas_p0.y + canvas_sz.y);

        // Draw border and background color
        ImGuiIO&    io        = ImGui::GetIO();
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        draw_list->AddRectFilled(canvas_p0, canvas_p1, IM_COL32(50, 50, 50, 255));
        draw_list->AddRect(canvas_p0, canvas_p1, IM_COL32(255, 255, 255, 255));

        // This will catch our interactions
        ImGui::InvisibleButton("canvas", canvas_sz,
                               ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
        const bool   is_hovered = ImGui::IsItemHovered();                          // Hovered
        const bool   is_active  = ImGui::IsItemActive();                           // Held
        const ImVec2 origin(canvas_p0.x + scrolling.x, canvas_p0.y + scrolling.y); // Lock scrolled origin
        const ImVec2 mouse_pos_in_canvas(io.MousePos.x - origin.x, io.MousePos.y - origin.y);

        // Add first and second point
        if (is_hovered && !adding_line && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
        {
            points.push_back(mouse_pos_in_canvas);
            points.push_back(mouse_pos_in_canvas);
            adding_line = true;
        }
        if (adding_line)
        {
            points.back() = mouse_pos_in_canvas;
            if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
                adding_line = false;
        }

        // Pan (we use a zero mouse threshold when there's no context menu)
        // You may decide to make that threshold dynamic based on whether the mouse is hovering something etc.
        const float mouse_threshold_for_pan = opt_enable_context_menu ? -1.0f : 0.0f;
        if (is_active && ImGui::IsMouseDragging(ImGuiMouseButton_Right, mouse_threshold_for_pan))
        {
            scrolling.x += io.MouseDelta.x;
            scrolling.y += io.MouseDelta.y;
        }

        // Context menu (under default mouse threshold)
        ImVec2 drag_delta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Right);
        if (opt_enable_context_menu && drag_delta.x == 0.0f && drag_delta.y == 0.0f)
            ImGui::OpenPopupOnItemClick("context", ImGuiPopupFlags_MouseButtonRight);
        if (ImGui::BeginPopup("context"))
        {
            if (adding_line)
                points.resize(points.size() - 2);
            adding_line = false;
            if (ImGui::MenuItem("Remove one", NULL, false, not points.empty()))
            {
                points.resize(points.size() - 2);
            }
            if (ImGui::MenuItem("Remove all", NULL, false, not points.empty()))
            {
                points.clear();
            }
            ImGui::EndPopup();
        }

        // Draw grid + all lines in the canvas
        draw_list->PushClipRect(canvas_p0, canvas_p1, true);
        if (opt_enable_grid)
        {
            const float GRID_STEP = 64.0f;
            for (float x = fmodf(scrolling.x, GRID_STEP); x < canvas_sz.x; x += GRID_STEP)
                draw_list->AddLine(ImVec2(canvas_p0.x + x, canvas_p0.y), ImVec2(canvas_p0.x + x, canvas_p1.y),
                                   IM_COL32(200, 200, 200, 40));
            for (float y = fmodf(scrolling.y, GRID_STEP); y < canvas_sz.y; y += GRID_STEP)
                draw_list->AddLine(ImVec2(canvas_p0.x, canvas_p0.y + y), ImVec2(canvas_p1.x, canvas_p0.y + y),
                                   IM_COL32(200, 200, 200, 40));
        }
        for (decltype(points.size()) n = 0; n < points.size(); n += 2)
            draw_list->AddLine(ImVec2(origin.x + points[n].x, origin.y + points[n].y),
                               ImVec2(origin.x + points[n + 1].x, origin.y + points[n + 1].y),
                               IM_COL32(255, 255, 0, 255), 2.0f);
        draw_list->PopClipRect();
        // -------------------------------------------------------------
        // End of code copied from imgui_demo.cpp
        // -------------------------------------------------------------
        SC_COMPILER_WARNING_POP;
    }

    Result showJSONInFinder(const StringView jsonPath)
    {
        SC::Process process;
        switch (SC::HostPlatform)
        {
        case SC::Platform::Windows: {
            StringNative<128> command;
            SC_TRY(StringBuilder(command).format("/select,\"{}\"", jsonPath));
            return process.exec({"explorer", command.view()});
        }
        default: {
            return process.exec({"open", "-R", jsonPath});
        }
        }
    }
};

struct SerializationExample : public SC::ISCExample
{
    SC::SerializationExampleModel model;
    SC::SerializationExampleView  view;

    SerializationExample()
    {
        ISCExample::onDraw.bind<SerializationExample, &SerializationExample::draw>(*this);
        ISCExample::serialize.bind<SerializationExample, &SerializationExample::serialize>(*this);
        ISCExample::deserialize.bind<SerializationExample, &SerializationExample::deserialize>(*this);
    }

    [[nodiscard]] bool init() { return view.init(); }

    [[nodiscard]] bool close() { return true; }

    void draw() { view.draw(model); }

    SC::Result serialize(SC::Vector<SC::uint8_t>& modelStateBuffer, SC::Vector<SC::uint8_t>& viewStateBuffer)
    {
        SC_TRY(model.saveToBinary(modelStateBuffer));
        SC_TRY(view.saveToBinary(viewStateBuffer));
        return SC::Result(true);
    }

    SC::Result deserialize(SC::Span<const SC::uint8_t> modelStateBuffer, SC::Span<const SC::uint8_t> viewStateBuffer)
    {
        SC_TRY(model.loadFromBinary(modelStateBuffer));
        SC_TRY(view.loadFromBinary(viewStateBuffer));
        return SC::Result(true);
    }
};

SC_PLUGIN_DEFINE(SerializationExample)
SC_PLUGIN_EXPORT_INTERFACES(SerializationExample, SC::ISCExample)
