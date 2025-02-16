// Copyright (c) Stefano Cristiano
// SPDX-License-Identifier: MIT
// --------------------------------------------------------------------------------------------------
// SC_BEGIN_PLUGIN
//
// Name:          WebServer
// Version:       1
// Description:   Creates an http server serving a website from the specified directory
// Category:      Generic
// Build:         libc
// Dependencies:
//
// SC_END_PLUGIN
// --------------------------------------------------------------------------------------------------
#include "../ImguiHelpers.h"
#include "imgui.h"

#include "Libraries/Http/HttpWebServer.h"
#include "Libraries/Plugin/PluginMacros.h"
#include "Libraries/SerializationBinary/SerializationBinary.h"

#include "../ISCExample.h"

namespace SC
{
struct WebServerExampleModel;
struct WebServerExampleView;
struct WebServerExampleSystem;
struct WebServerExampleModelState;
struct WebServerExampleViewState;
} // namespace SC

struct SC::WebServerExampleModelState
{
    String  directory;
    String  interface             = "127.0.0.1";
    int32_t port                  = 8090;
    int32_t maxConcurrentRequests = 16;
};

SC_REFLECT_STRUCT_VISIT(SC::WebServerExampleModelState)
SC_REFLECT_STRUCT_FIELD(0, directory)
SC_REFLECT_STRUCT_FIELD(1, interface)
SC_REFLECT_STRUCT_FIELD(2, port)
SC_REFLECT_STRUCT_FIELD(3, maxConcurrentRequests)
SC_REFLECT_STRUCT_LEAVE()

struct SC::WebServerExampleViewState
{
    Buffer inputTextBuffer;

    bool needsRestart = false;
};
SC_REFLECT_STRUCT_VISIT(SC::WebServerExampleViewState)
SC_REFLECT_STRUCT_FIELD(0, needsRestart)
SC_REFLECT_STRUCT_LEAVE()

struct SC::WebServerExampleModel
{
    WebServerExampleModelState modelState;

    AsyncEventLoop* eventLoop = nullptr;

    HttpServer    httpServer;
    HttpWebServer httpWebServer;

    Result start()
    {
        httpServer.onRequest.bind<HttpWebServer, &HttpWebServer::serveFile>(httpWebServer);
        SC_TRY(httpServer.start(*eventLoop, static_cast<uint32_t>(modelState.maxConcurrentRequests),
                                modelState.interface.view(), static_cast<uint16_t>(modelState.port)));
        SC_TRY(httpWebServer.init(modelState.directory.view()));
        return Result(true);
    }

    Result stop()
    {
        SC_TRY(httpWebServer.stopAsync());
        SC_TRY(httpServer.stopSync());
        return Result(true);
    }

    bool canBeStarted() const
    {
        return eventLoop and modelState.port != 0 and not modelState.interface.isEmpty() and
               not modelState.directory.isEmpty();
    }

    Result saveToBinary(Buffer& modelStateBuffer)
    {
        return Result(SC::SerializationBinary::writeWithSchema(modelState, modelStateBuffer));
    }

    Result loadFromBinary(Span<const char> modelStateSpan)
    {
        return Result(SC::SerializationBinary::loadVersionedWithSchema(modelState, modelStateSpan));
    }
};

struct SC::WebServerExampleView
{
    WebServerExampleViewState viewState;

    Result init() { return Result(true); }

    Result saveToBinary(Buffer& viewStateBuffer)
    {
        return Result(SC::SerializationBinary::writeWithSchema(viewState, viewStateBuffer));
    }

    Result loadFromBinary(Span<const char> viewStateSpan)
    {
        return Result(SC::SerializationBinary::loadVersionedWithSchema(viewState, viewStateSpan));
    }

    Result draw(WebServerExampleModel& model)
    {
        auto& buffer = viewState.inputTextBuffer;
        SC_TRY(InputText("Interface", buffer, model.modelState.interface, viewState.needsRestart));
        SC_TRY(InputText("Directory", buffer, model.modelState.directory, viewState.needsRestart));
        viewState.needsRestart |= ImGui::InputInt("Port", &model.modelState.port);

        if (not model.httpServer.isStarted())
        {
            viewState.needsRestart = false;
        }

        if (viewState.needsRestart)
        {
            ImGui::BeginDisabled(not model.canBeStarted());
            if (ImGui::Button("Apply Changes"))
            {
                (void)model.stop();
                (void)model.start();
                viewState.needsRestart = false;
            }
            ImGui::EndDisabled();
        }
        else
        {
            ImGui::BeginDisabled(not model.canBeStarted() or model.httpServer.isStarted());
            if (ImGui::Button("Start"))
            {
                (void)model.start();
            }
            ImGui::EndDisabled();
        }

        ImGui::SameLine();

        ImGui::BeginDisabled(not model.httpServer.isStarted());
        if (ImGui::Button("Stop"))
        {
            (void)model.stop();
        }
        ImGui::EndDisabled();
        return Result(true);
    }
};

struct WebServerExample : public SC::ISCExample
{
    SC::WebServerExampleModel model;
    SC::WebServerExampleView  view;

    WebServerExample()
    {
        ISCExample::onDraw.bind<WebServerExample, &WebServerExample::draw>(*this);
        ISCExample::serialize.bind<WebServerExample, &WebServerExample::serialize>(*this);
        ISCExample::deserialize.bind<WebServerExample, &WebServerExample::deserialize>(*this);
        ISCExample::initAsync.bind<WebServerExample, &WebServerExample::initAsync>(*this);
        ISCExample::closeAsync.bind<WebServerExample, &WebServerExample::closeAsync>(*this);
    }

    [[nodiscard]] bool init() { return view.init(); }

    [[nodiscard]] bool close() { return true; }

    [[nodiscard]] SC::Result initAsync(SC::AsyncEventLoop& eventLoop)
    {
        model.eventLoop = &eventLoop;
        return SC::Result(true);
    }

    [[nodiscard]] SC::Result closeAsync(SC::AsyncEventLoop&) { return model.stop(); }

    void draw() { (void)view.draw(model); }

    SC::Result serialize(SC::Buffer& modelStateBuffer, SC::Buffer& viewStateBuffer)
    {
        SC_TRY(model.saveToBinary(modelStateBuffer));
        SC_TRY(view.saveToBinary(viewStateBuffer));
        return SC::Result(true);
    }

    SC::Result deserialize(SC::Span<const char> modelStateBuffer, SC::Span<const char> viewStateBuffer)
    {
        SC_TRY(model.loadFromBinary(modelStateBuffer));
        SC_TRY(view.loadFromBinary(viewStateBuffer));
        return SC::Result(true);
    }
};

SC_PLUGIN_DEFINE(WebServerExample)
SC_PLUGIN_EXPORT_INTERFACES(WebServerExample, SC::ISCExample)
