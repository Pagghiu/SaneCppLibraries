// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT

// Description: Simple integration of SC::AsyncEventLoop within macOS, windows and linux native GUI event loop.

#include "Libraries/Async/Async.h"
#include "Libraries/Foundation/Deferred.h"
#include "Libraries/Strings/String.h"
#include "Libraries/Strings/StringBuilder.h"
#include "Libraries/Time/Time.h"
#include "SCExampleSokol.h"
#include "imgui.h"
#define SOKOL_IMGUI_IMPL
#include "util/sokol_imgui.h"

namespace SC
{
struct ApplicationState
{
    static constexpr int NumPauseFrames = 2;

    int pausedCounter        = 0;
    int continueDrawingForMs = 500;
    int timeoutOccursEveryMs = 2000;
    int numberOfFrames       = 0;

    // EventLoop
    String loopMessage  = "Waiting for first timeout...";
    int    loopTimeouts = 1;

    Time::Milliseconds loopTime;
};

struct ApplicationSystem
{
    ApplicationState state;

    Result create()
    {
        currentThreadID = Thread::CurrentThreadID();
        lastEventTime.snap();

        SC_TRY(eventLoop.create());
        timeout.callback = [this](AsyncLoopTimeout::Result& result) { onTimeout(result); };
        SC_TRY(timeout.start(eventLoop, Time::Milliseconds(state.timeoutOccursEveryMs)));
        eventLoopMonitor.onNewEventsAvailable = [this]() { sokol_wake_up(); };
        SC_TRY(eventLoopMonitor.create(eventLoop));
        return Result(true);
    }

    void resetLastEventTime() { lastEventTime.snap(); }

    // This is called during "frame callback" and it will either quickly execute or block when it's time to sleep
    Result runLoopStepInsideSokolApp()
    {
        // Update loop time, mainly to display it in the GUI
        state.loopTime = eventLoop.getLoopTime().getRelative().inRoundedUpperMilliseconds();

        // Check if enough time has passed since last user input event
        const Time::Relative sinceLastEvent = Time::HighResolutionCounter().snap().subtractApproximate(lastEventTime);
        if (sinceLastEvent.inRoundedUpperMilliseconds() > Time::Milliseconds(state.continueDrawingForMs))
        {
            // Enough time has passed such that we need to pause execution to avoid unnecessary cpu usage
            if (state.pausedCounter < ApplicationState::NumPauseFrames)
            {
                state.pausedCounter++;
                return Result(true); // Additional frames are needed to draw "paused" before entering sleep
            }
            // If we are here we really want to sleep the app until a new input event OR an I/O event arrives
            // We implement this logic by:
            // 1. Starting to monitor the event loop for IO in a different thread
            // 2. Blocking on sokol native gui event loop (sokol_sleep)
            //  2a. If input from user occurs, sokol_sleep() will unblock itself
            //  2b. If I/O event occurs, calling sokol_wake_up() from the monitoring thread will unblock sokol_sleep
            // 3. After returning from sokol_sleep, we make sure to dispatch callbacks for all ready completions
            auto resetLastEventTime = MakeDeferred([this] { lastEventTime.snap(); });
            SC_TRY(eventLoopMonitor.startMonitoring());
            sokol_sleep();
            return eventLoopMonitor.stopMonitoringAndDispatchCompletions();
        }
        else
        {
            // Keep the application running, but use the occasion to check if some I/O event has been queued by the OS.
            // This also updates the loop time, that is needed to fire AsyncLoopTimeouts events with decent precision.
            state.pausedCounter = 0;
            return eventLoop.runNoWait();
        }
    }

    Result close()
    {
        SC_TRY(eventLoopMonitor.close())
        SC_TRY(eventLoop.close());
        return Result(true);
    }

  private:
    AsyncEventLoop              eventLoop;
    AsyncEventLoopMonitor       eventLoopMonitor;
    Time::HighResolutionCounter lastEventTime;
    AsyncLoopTimeout            timeout;

    uint64_t currentThreadID = 0;

    void onTimeout(AsyncLoopTimeout::Result& result)
    {
        // The entire point of runStep is to run this callback in the main thread, so let's assert that
        SC_ASSERT_RELEASE(currentThreadID == Thread::CurrentThreadID());
        (void)StringBuilder(state.loopMessage).format("I/O WakeUp {}", state.loopTimeouts);
        state.loopTimeouts++;
        result.getAsync().relativeTimeout = Time::Milliseconds(state.timeoutOccursEveryMs);
        result.reactivateRequest(true);
    }
};

struct ApplicationView
{
    ApplicationSystem& system;
    ApplicationState&  state;
    ApplicationView(ApplicationSystem& system, ApplicationState& data) : system(system), state(data) {}

    void draw()
    {
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(ImVec2(350, ImGui::GetIO().DisplaySize.y));
        if (ImGui::Begin("Test", nullptr,
                         ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar))
        {
            ImGui::Text("SCExample");
            if (ImGui::CollapsingHeader("SC::Async Integration", ImGuiTreeNodeFlags_DefaultOpen))
            {
                drawSCAsync();
            }
        }
        ImGui::End();
    }

    void drawSCAsync()
    {
        if (state.pausedCounter == 0)
        {
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Running");
        }
        else
        {
            ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Paused");
        }
        ImGui::Text("%s", state.loopMessage.view().bytesIncludingTerminator());
        ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate,
                    ImGui::GetIO().Framerate);
        ImGui::Text("Frame %d", state.numberOfFrames++);
        ImGui::Text("Time %.3f", state.loopTime.ms / 1000.0f);
        ImGui::PushItemWidth(100);
        ImGui::InputInt("Continue drawing for (ms)", &state.continueDrawingForMs);
        ImGui::InputInt("Timeout occurs every (ms)", &state.timeoutOccursEveryMs);
        ImGui::PopItemWidth();
        state.continueDrawingForMs = max(0, state.continueDrawingForMs);
        state.timeoutOccursEveryMs = max(0, state.timeoutOccursEveryMs);
    }
};
} // namespace SC

SC::ApplicationSystem* gModelSystem = nullptr;

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
        gModelSystem = new ApplicationSystem();
        if (not gModelSystem->create())
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
        ApplicationView applicationView(*gModelSystem, gModelSystem->state);
        applicationView.draw();

        sg_pass pass   = {};
        pass.action    = gGlobalPassAction;
        pass.swapchain = sglue_swapchain();
        sg_begin_pass(&pass);
        simgui_render();
        sg_end_pass();
        sg_commit();

        if (not gModelSystem->runLoopStepInsideSokolApp())
        {
            sapp_quit();
        }
    };

    desc.cleanup_cb = []()
    {
        if (not gModelSystem->close())
        {
            sapp_quit();
        }
        delete gModelSystem;
        gModelSystem = nullptr;
        simgui_shutdown();
        sg_shutdown();
    };

    desc.event_cb = [](const sapp_event* ev)
    {
        simgui_handle_event(ev);
        // Reset redraw time counter
        gModelSystem->resetLastEventTime();
    };
    return desc;
}
