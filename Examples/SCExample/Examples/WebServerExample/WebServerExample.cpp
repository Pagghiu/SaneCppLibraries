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
    static constexpr size_t HEADER_SIZE     = 1024 * 8;   // Number of bytes used to hold request and response headers
    static constexpr size_t NUM_FS_THREADS  = 4;          // Number of threads for async file stream operations

    StableArray<char> requestsMemory = {MAX_CONNECTIONS * REQUEST_SIZE}; // Memory sliced into buffers for streaming
    StableArray<char> headersMemory  = {MAX_CONNECTIONS * HEADER_SIZE};  // Memory holding request / response headers

    StableArray<HttpConnection>              connections = {MAX_CONNECTIONS};
    StableArray<AsyncBufferView>             buffers     = {MAX_CONNECTIONS * WRITE_QUEUE}; // WRITE_QUEUE > READ_QUEUE
    StableArray<ReadableFileStream::Request> readQueue   = {MAX_CONNECTIONS * READ_QUEUE};
    StableArray<WritableFileStream::Request> writeQueue  = {MAX_CONNECTIONS * WRITE_QUEUE};
    StableArray<HttpAsyncFileServerStream>   fileStreams = {MAX_CONNECTIONS};

    Result start()
    {
        const size_t numClients = static_cast<size_t>(modelState.maxClients);

        SC_TRY(requestsMemory.resizeWithoutInitializing(numClients * REQUEST_SIZE));
        SC_TRY(headersMemory.resizeWithoutInitializing(numClients * HEADER_SIZE));
        SC_TRY(connections.resize(numClients));
        SC_TRY(buffers.resize(numClients * WRITE_QUEUE));
        SC_TRY(readQueue.resize(numClients * READ_QUEUE));
        SC_TRY(writeQueue.resize(numClients * WRITE_QUEUE));
        SC_TRY(fileStreams.resize(numClients));

        // Slice requests buffer in equal parts to create re-usable sub-buffers to stream files.
        // It's not required to slice the buffer in equal parts, it's just an arbitrary choice.
        SC_TRY(AsyncBuffersPool::sliceInEqualParts(buffers, requestsMemory, numClients * READ_QUEUE));
        buffersPool.buffers = buffers;

        // Optimization: only create a thread pool for FS operations if needed (i.e. when async backend != io_uring)
        if (eventLoop->needsThreadPoolForFileOperations())
        {
            SC_TRY(threadPool.create(NUM_FS_THREADS));
        }
        // Initialize and start the http and the file server
        SC_TRY(httpServer.init(buffersPool, connections, headersMemory, readQueue, writeQueue));
        SC_TRY(fileServer.init(buffersPool, threadPool, modelState.directory.view(), fileStreams));
        SC_TRY(httpServer.start(*eventLoop, modelState.interface.view(), static_cast<uint16_t>(modelState.port)));
        SC_TRY(fileServer.start(httpServer));
        return Result(true);
    }

    Result stop()
    {
        SC_TRY(fileServer.stop());
        SC_TRY(httpServer.stop());
        SC_TRY(fileServer.close());
        SC_TRY(httpServer.close());
        SC_TRY(threadPool.destroy());

        requestsMemory.release();      // Skips invoking destructors, just release virtual memory
        headersMemory.release();       // Skips invoking destructors, just release virtual memory
        connections.clearAndRelease(); // Invokes destructors and releases virtual memory
        buffers.clearAndRelease();     // Invokes destructors and releases virtual memory
        readQueue.clearAndRelease();   // Invokes destructors and releases virtual memory
        writeQueue.clearAndRelease();  // Invokes destructors and releases virtual memory
        fileStreams.clearAndRelease(); // Invokes destructors and releases virtual memory
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
