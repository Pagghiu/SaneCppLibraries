// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
#include "Libraries/Foundation/Deferred.h"
#include "Libraries/Foundation/Result.h"
#include "Libraries/Time/Time.h"
#include "SCExampleSokol.h"
#include "imgui.h"
#define SOKOL_IMGUI_IMPL
#include "util/sokol_imgui.h"

namespace SC
{
struct ModelData
{
    static constexpr int NumPauseFrames = 2;

    int pausedCounter        = 0;
    int continueDrawingForMs = 500;
    int numberOfFrames       = 0;
};

struct ModelBehaviour
{
    ModelData modelData;

    Result create()
    {
        lastEventTime.snap();
        return Result(true);
    }

    void resetLastEventTime() { lastEventTime.snap(); }

    // This is called during "frame callback" and it will either quickly execute or block when it's time to sleep
    Result runLoopStepInsideSokolApp()
    {
        // Check if we need to pause execution
        const Time::Relative sinceLastEvent = Time::HighResolutionCounter().snap().subtractApproximate(lastEventTime);
        if (sinceLastEvent.inRoundedUpperMilliseconds() > Time::Milliseconds(modelData.continueDrawingForMs))
        {
            // Enough time has passed such that we need to pause execution to avoid unnecessary cpu usage
            if (modelData.pausedCounter < ModelData::NumPauseFrames)
            {
                modelData.pausedCounter++;
                return Result(true); // one more frame is needed to draw "paused" before pausing for real
            }
            auto resetLastEventTime = MakeDeferred([this] { lastEventTime.snap(); });
            sokol_sleep();
        }
        else
        {
            modelData.pausedCounter = 0;
        }
        return Result(true);
    }

    Result close() { return Result(true); }

  private:
    Time::HighResolutionCounter lastEventTime;
};

struct ApplicationView
{
    ModelData& modelData;
    ApplicationView(ModelData& model) : modelData(model) {}

    void draw()
    {
        if (ImGui::Begin("Test"))
        {
            ImGui::Text("SCExample");
            if (modelData.pausedCounter == 0)
            {
                ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Running");
            }
            else
            {
                ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Paused");
            }
            ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate,
                        ImGui::GetIO().Framerate);
            ImGui::Text("Frame %d", modelData.numberOfFrames++);
            ImGui::PushItemWidth(100);
            ImGui::InputInt("Continue drawing for (ms)", &modelData.continueDrawingForMs);
            ImGui::PopItemWidth();
            modelData.continueDrawingForMs = max(0, modelData.continueDrawingForMs);
        }
        ImGui::End();
    }
};
} // namespace SC

SC::ModelBehaviour* gModelBehaviour = nullptr;

static sg_pass_action gGlobalPassAction;

sapp_desc sokol_main(int, char*[])
{
    using namespace SC;
    sapp_desc desc = {};

    desc.window_title     = "SCExample";
    desc.high_dpi         = true;
    desc.enable_clipboard = true;

    desc.init_cb = []()
    {
        gModelBehaviour = new ModelBehaviour();
        if (not gModelBehaviour->create())
        {
            sapp_quit();
        }
        sg_desc desc = {};

        desc.environment = sglue_environment();
        sg_setup(&desc);

        simgui_desc_t simgui_desc = {};
        simgui_setup(&simgui_desc);

        gGlobalPassAction.colors[0].load_action = SG_LOADACTION_CLEAR;
        gGlobalPassAction.colors[0].clear_value = {0.0f, 0.5f, 0.7f, 1.0f};
    };

    desc.frame_cb = []()
    {
        simgui_new_frame({sapp_width(), sapp_height(), sapp_frame_duration(), sapp_dpi_scale()});
        ApplicationView applicationView(gModelBehaviour->modelData);
        applicationView.draw();
        sg_pass pass   = {};
        pass.action    = gGlobalPassAction;
        pass.swapchain = sglue_swapchain();
        sg_begin_pass(&pass);
        simgui_render();
        sg_end_pass();
        sg_commit();

        if (not gModelBehaviour->runLoopStepInsideSokolApp())
        {
            sapp_quit();
        }
    };

    desc.cleanup_cb = []()
    {
        if (not gModelBehaviour->close())
        {
            sapp_quit();
        }
        delete gModelBehaviour;
        gModelBehaviour = nullptr;
        simgui_shutdown();
        sg_shutdown();
    };

    desc.event_cb = [](const sapp_event* ev)
    {
        simgui_handle_event(ev);
        // Reset redraw time counter
        gModelBehaviour->resetLastEventTime();
    };
    return desc;
}
