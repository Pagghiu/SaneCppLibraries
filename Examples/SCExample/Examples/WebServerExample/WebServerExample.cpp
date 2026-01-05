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

#include "Libraries/ContainersReflection/ContainersReflection.h"
#include "Libraries/ContainersReflection/MemorySerialization.h"
#include "Libraries/Http/HttpAsyncFileServer.h"
#include "Libraries/Http/HttpAsyncServer.h"
#include "Libraries/Plugin/PluginMacros.h"
#include "Libraries/SerializationBinary/SerializationBinary.h"
#include "Libraries/Threading/ThreadPool.h"

#include "../ISCExample.h"
#include "StableArray.h"

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
    String  interface  = "127.0.0.1";
    int32_t port       = 8090;
    int32_t maxClients = 32;

    HttpAsyncConnectionBase::Configuration asyncConfiguration;
};

SC_REFLECT_STRUCT_VISIT(SC::WebServerExampleModelState)
SC_REFLECT_STRUCT_FIELD(0, directory)
SC_REFLECT_STRUCT_FIELD(1, interface)
SC_REFLECT_STRUCT_FIELD(2, port)
SC_REFLECT_STRUCT_FIELD(3, maxClients)
SC_REFLECT_STRUCT_LEAVE()

struct SC::WebServerExampleViewState
{
    Buffer inputTextBuffer;

    bool needsRestart = false;
    bool needsResize  = false;
};
SC_REFLECT_STRUCT_VISIT(SC::WebServerExampleViewState)
SC_REFLECT_STRUCT_FIELD(0, needsRestart)
SC_REFLECT_STRUCT_LEAVE()

struct SC::WebServerExampleModel
{
    WebServerExampleModelState modelState;

    AsyncEventLoop* eventLoop = nullptr;
    //! [WebServerExampleSnippet]

    HttpAsyncServer     httpServer;
    HttpAsyncFileServer fileServer;

    ThreadPool threadPool;

    static constexpr size_t MAX_CONNECTIONS  = 1000000;     // Reserve space for max 1 million connections
    static constexpr size_t MAX_READ_QUEUE   = 10;          // Max number of read queue buffers for each connection
    static constexpr size_t MAX_WRITE_QUEUE  = 10;          // Max number of write queue buffers for each connection
    static constexpr size_t MAX_BUFFERS      = 10;          // Max number of write queue buffers for each connection
    static constexpr size_t MAX_REQUEST_SIZE = 1024 * 1024; // Max number of bytes to stream data for each connection
    static constexpr size_t MAX_HEADER_SIZE  = 32 * 1024;   // Max number of bytes to hold request and response headers
    static constexpr size_t NUM_FS_THREADS   = 4;           // Number of threads for async file stream operations

    StableArray<HttpAsyncConnectionBase> clients = {MAX_CONNECTIONS};

    // For simplicity just hardcode a read queue of 3 for file streams
    StableArray<HttpAsyncFileServer::StreamQueue<3>> fileStreams = {MAX_CONNECTIONS};

    StableArray<AsyncReadableStream::Request> allReadQueues  = {MAX_CONNECTIONS * MAX_READ_QUEUE};
    StableArray<AsyncWritableStream::Request> allWriteQueues = {MAX_CONNECTIONS * MAX_WRITE_QUEUE};
    StableArray<AsyncBufferView>              allBuffers     = {MAX_CONNECTIONS * MAX_BUFFERS};
    StableArray<char>                         allHeaders     = {MAX_CONNECTIONS * MAX_HEADER_SIZE};
    StableArray<char>                         allStreams     = {MAX_CONNECTIONS * MAX_REQUEST_SIZE};

    Result start()
    {
        SC_TRY(assignConnectionMemory(static_cast<size_t>(modelState.maxClients)));
        // Optimization: only create a thread pool for FS operations if needed (i.e. when async backend != io_uring)
        if (eventLoop->needsThreadPoolForFileOperations())
        {
            SC_TRY(threadPool.create(NUM_FS_THREADS));
        }
        // Initialize and start http and file servers, delegating requests to the latter in order to serve files
        SC_TRY(httpServer.init(clients.toSpan()));
        SC_TRY(httpServer.start(*eventLoop, modelState.interface.view(), static_cast<uint16_t>(modelState.port)));
        SC_TRY(fileServer.init(threadPool, *eventLoop, modelState.directory.view()));
        httpServer.onRequest = [&](HttpConnection& connection)
        {
            HttpAsyncFileServer::Stream& stream = fileStreams.toSpan()[connection.getConnectionID().getIndex()];
            SC_ASSERT_RELEASE(fileServer.handleRequest(stream, connection));
        };
        return Result(true);
    }

    Result assignConnectionMemory(size_t numClients)
    {
        SC_TRY(clients.resize(numClients));
        SC_TRY(fileStreams.resize(numClients));
        SC_TRY(allReadQueues.resize(numClients * modelState.asyncConfiguration.readQueueSize));
        SC_TRY(allWriteQueues.resize(numClients * modelState.asyncConfiguration.writeQueueSize));
        SC_TRY(allBuffers.resize(numClients * modelState.asyncConfiguration.buffersQueueSize));
        SC_TRY(allHeaders.resize(numClients * modelState.asyncConfiguration.headerBytesLength));
        SC_TRY(allStreams.resize(numClients * modelState.asyncConfiguration.streamBytesLength));
        HttpAsyncConnectionBase::Memory memory;
        memory.allBuffers    = allBuffers;
        memory.allReadQueue  = allReadQueues;
        memory.allWriteQueue = allWriteQueues;
        memory.allHeaders    = allHeaders;
        memory.allStreams    = allStreams;
        SC_TRY(memory.assignTo(modelState.asyncConfiguration, clients.toSpan()));
        return Result(true);
    }

    Result runtimeResize()
    {
        const size_t numClients =
            max(static_cast<size_t>(modelState.maxClients), httpServer.getConnections().getHighestActiveConnection());
        SC_TRY(assignConnectionMemory(numClients));
        SC_TRY(httpServer.resize(clients.toSpan()));
        return Result(true);
    }
    //! [WebServerExampleSnippet]

    Result stop()
    {
        SC_TRY(httpServer.stop());
        SC_TRY(fileServer.close());
        SC_TRY(httpServer.close());
        SC_TRY(threadPool.destroy());
        SC_TRY(clients.clearAndShrinkToFit()); // Invokes destructors and de-commits virtual memory
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

    bool InputSizeT(const char* name, size_t* num)
    {
        int intNum = static_cast<int>(*num);
        if (ImGui::InputInt(name, &intNum))
        {
            if (intNum < 0)
                intNum = 0;
            *num = static_cast<size_t>(intNum);
            return true;
        }
        return false;
    }

    Result draw(WebServerExampleModel& model)
    {
        auto& buffer = viewState.inputTextBuffer;
        SC_TRY(InputText("Interface", buffer, model.modelState.interface, viewState.needsRestart));
        SC_TRY(InputText("Directory", buffer, model.modelState.directory, viewState.needsRestart));
        ImGui::PushItemWidth(130);
        viewState.needsRestart |= ImGui::InputInt("Port", &model.modelState.port);
        viewState.needsResize |= ImGui::InputInt("Max Clients", &model.modelState.maxClients);
        ImGui::Text("Per connection quantities (need restart)");
        auto& configuration = model.modelState.asyncConfiguration;
        viewState.needsRestart |= InputSizeT("Read Queue (items)", &configuration.readQueueSize);
        viewState.needsRestart |= InputSizeT("Write Queue (items)", &configuration.writeQueueSize);
        viewState.needsRestart |= InputSizeT("Buffers Queue (items)", &configuration.buffersQueueSize);
        viewState.needsRestart |= InputSizeT("Header buffer (bytes)", &configuration.headerBytesLength);
        viewState.needsRestart |= InputSizeT("Streams buffer (bytes)", &configuration.streamBytesLength);
        ImGui::PopItemWidth();
        ImGui::Text("Total Connections : %zu", model.httpServer.getConnections().getNumTotalConnections());
        ImGui::Text("Active Connections: %zu", model.httpServer.getConnections().getNumActiveConnections());
        ImGui::Text("Highest Active Idx: %zu", model.httpServer.getConnections().getHighestActiveConnection());

        if (not model.httpServer.isStarted())
        {
            viewState.needsRestart = false;
        }

        if (viewState.needsResize)
        {
            ImGui::BeginDisabled(not model.canBeStarted());
            if (ImGui::Button("Resize"))
            {
                (void)model.runtimeResize();
                viewState.needsResize = false;
            }
            ImGui::EndDisabled();
        }
        else if (viewState.needsRestart)
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

    [[nodiscard]] bool close() { return model.stop(); }

    SC::Result initAsync(SC::AsyncEventLoop& eventLoop)
    {
        model.eventLoop = &eventLoop;
        return SC::Result(true);
    }

    SC::Result closeAsync(SC::AsyncEventLoop&) { return model.stop(); }

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
