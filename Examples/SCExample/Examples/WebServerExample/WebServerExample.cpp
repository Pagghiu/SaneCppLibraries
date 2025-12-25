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
};
SC_REFLECT_STRUCT_VISIT(SC::WebServerExampleViewState)
SC_REFLECT_STRUCT_FIELD(0, needsRestart)
SC_REFLECT_STRUCT_LEAVE()

struct SC::WebServerExampleModel
{
    WebServerExampleModelState modelState;

    AsyncEventLoop*  eventLoop = nullptr;
    AsyncBuffersPool buffersPool;

    HttpAsyncServer     httpServer;
    HttpAsyncFileServer fileServer;

    ThreadPool threadPool;

    static constexpr size_t MAX_CONNECTIONS = 1000000;    // Reserve space for max 1 million connections
    static constexpr size_t READ_QUEUE      = 2;          // Number of read queue buffers for each connection
    static constexpr size_t WRITE_QUEUE     = 3;          // Number of write queue buffers for each connection
    static constexpr size_t REQUEST_SIZE    = 512 * 1024; // Number of bytes used to stream data for each connection
    static constexpr size_t HEADER_SIZE     = 8 * 1024;   // Number of bytes used to hold request and response headers
    static constexpr size_t NUM_FS_THREADS  = 4;          // Number of threads for async file stream operations

    // In this simple setup we have a single file stream, with fixed read / write queues and a fixed header buffer
    struct HttpCustomClient : HttpAsyncConnection<READ_QUEUE, WRITE_QUEUE, HEADER_SIZE>
    {
        HttpAsyncFileServer::StreamQueue<READ_QUEUE> fileStream; // Store a file stream inline with the client
    };

    StableArray<HttpCustomClient> clients  = {MAX_CONNECTIONS};
    StableArray<AsyncBufferView>  buffers  = {MAX_CONNECTIONS * (WRITE_QUEUE + READ_QUEUE)};
    StableArray<char>             requests = {MAX_CONNECTIONS * REQUEST_SIZE};

    Result start()
    {
        const size_t numClients = static_cast<size_t>(modelState.maxClients);

        SC_TRY(requests.resizeWithoutInitializing(numClients * REQUEST_SIZE));
        SC_TRY(clients.resize(numClients));
        SC_TRY(buffers.resize(numClients * WRITE_QUEUE));

        // Slice requests buffer in equal parts to create re-usable sub-buffers to stream files.
        // It's not required to slice the buffer in equal parts, it's just an arbitrary choice.
        SC_TRY(AsyncBuffersPool::sliceInEqualParts(buffers, requests, numClients * READ_QUEUE));
        buffersPool.setBuffers(buffers);

        // Optimization: only create a thread pool for FS operations if needed (i.e. when async backend != io_uring)
        if (eventLoop->needsThreadPoolForFileOperations())
        {
            SC_TRY(threadPool.create(NUM_FS_THREADS));
        }
        // Initialize and start the http server
        SC_TRY(httpServer.init(buffersPool, clients.toSpan()));
        SC_TRY(httpServer.start(*eventLoop, modelState.interface.view(), static_cast<uint16_t>(modelState.port)));

        // Init the file server and setup the httpServer onRequest to serve files
        SC_TRY(fileServer.init(buffersPool, threadPool, *eventLoop, modelState.directory.view()));
        httpServer.onRequest = [&](HttpConnection& connection)
        {
            HttpAsyncFileServer::Stream& stream = clients.toSpan()[connection.getConnectionID().getIndex()].fileStream;
            SC_ASSERT_RELEASE(fileServer.serveFile(stream, connection));
        };
        return Result(true);
    }

    Result stop()
    {
        SC_TRY(httpServer.stop());
        SC_TRY(fileServer.close());
        SC_TRY(httpServer.close());
        SC_TRY(threadPool.destroy());

        requests.release();        // Skips invoking destructors, just release virtual memory
        clients.clearAndRelease(); // Invokes destructors and releases virtual memory
        buffers.clearAndRelease(); // Invokes destructors and releases virtual memory
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
