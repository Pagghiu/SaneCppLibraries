@page page_examples Examples
# Examples

The examples are complete programs built from the same library sources as the test suite. Use them to see how the
libraries fit together, then follow the links to the source when you need the implementation details.

## See the examples in action

These recordings show the larger examples running before you explore their source or the focused command-line programs
below.

### Applications and runtime behavior

\htmlonly
<div class="example-video-grid">
  <article class="example-video-card example-video-featured">
    <h3>AsyncWebServer</h3>
    <p>Static files, streamed uploads, and a shared WebSocket canvas running in two browser windows.</p>
    <video controls playsinline preload="none" poster="/images/examples/async-web-server.jpg" style="display:block; width:100%; max-width:960px; height:auto;">
      <source src="https://github.com/user-attachments/assets/d6ad7ecf-8c98-430d-84e0-93ebbfb05dc1" type="video/mp4">
      Your browser cannot play this video. <a href="https://github.com/user-attachments/assets/d6ad7ecf-8c98-430d-84e0-93ebbfb05dc1">Open the AsyncWebServer video</a>.
    </video>
  </article>
  <article class="example-video-card">
    <h3>Serialization and state</h3>
    <p>Reflected application state moving between binary and JSON representations.</p>
    <video controls playsinline preload="none" poster="/images/examples/serialization-and-state.jpg" style="display:block; width:100%; max-width:960px; height:auto;">
      <source src="https://github.com/user-attachments/assets/2a38310c-6a28-4f86-a0f3-665dc15b126d" type="video/mp4">
      Your browser cannot play this video. <a href="https://github.com/user-attachments/assets/2a38310c-6a28-4f86-a0f3-665dc15b126d">Open the serialization example video</a>.
    </video>
  </article>
  <article class="example-video-card">
    <h3>Plugin loading and hot reload</h3>
    <p>SCExample loading native plugins and replacing them while the host keeps running.</p>
    <video controls playsinline preload="none" poster="/images/examples/plugin-hot-reload.jpg" style="display:block; width:100%; max-width:960px; height:auto;">
      <source src="https://github.com/Pagghiu/SaneCppLibraries/assets/5406873/5c7d4036-6e0c-4262-ad57-9ef84c214717" type="video/mp4">
      Your browser cannot play this video. <a href="https://github.com/Pagghiu/SaneCppLibraries/assets/5406873/5c7d4036-6e0c-4262-ad57-9ef84c214717">Open the plugin hot-reload video</a>.
    </video>
  </article>
</div>
\endhtmlonly

### SC::Build workflows

\htmlonly
<div class="example-video-grid">
  <article class="example-video-card">
    <h3>Native standalone build</h3>
    <p>The native backend compiling and linking the repository directly, without generating an IDE project first.</p>
    <video controls playsinline preload="none" poster="/images/examples/build-standalone.jpg" style="display:block; width:100%; max-width:960px; height:auto;">
      <source src="https://github.com/user-attachments/assets/8b0cf915-6a26-4e17-b774-7b2f44dc1d5c" type="video/mp4">
      Your browser cannot play this video. <a href="https://github.com/user-attachments/assets/8b0cf915-6a26-4e17-b774-7b2f44dc1d5c">Open the native SC::Build video</a>.
    </video>
  </article>
  <article class="example-video-card">
    <h3>External project bootstrap</h3>
    <p>A small external project fetching the launcher with curl and building from its own <code>SC-build.cpp</code>.</p>
    <video controls playsinline preload="none" poster="/images/examples/build-external.jpg" style="display:block; width:100%; max-width:960px; height:auto;">
      <source src="https://github.com/user-attachments/assets/0b3ca57f-6c48-4931-bdef-c1e97201c970" type="video/mp4">
      Your browser cannot play this video. <a href="https://github.com/user-attachments/assets/0b3ca57f-6c48-4931-bdef-c1e97201c970">Open the external SC::Build video</a>.
    </video>
  </article>
</div>
\endhtmlonly

## Quick start

Build and run an example from the repository root:

```bash
./SC.sh build compile AwaitEcho Debug
./SC.sh build run AwaitEcho Debug
```

Use `SC.bat` instead of `./SC.sh` on Windows. Project generation, IDE setup, and platform prerequisites are covered in
[Building (Contributor)](@ref page_building_contributor).

## HTTP and networking

- [ApiServer](https://github.com/Pagghiu/SaneCppLibraries/tree/main/Examples/ApiServer) is a small routed HTTP API with
  health, query, and streaming echo endpoints.
- [SaneHttpGet](https://github.com/Pagghiu/SaneCppLibraries/tree/main/Examples/SaneHttpGet) is the shortest blocking
  `HttpClient` example.
- [HttpClientPollSession](https://github.com/Pagghiu/SaneCppLibraries/tree/main/Examples/HttpClientPollSession) shows a
  poll-driven client session with caller-owned storage.
- [HttpClientAsyncGet](https://github.com/Pagghiu/SaneCppLibraries/tree/main/Examples/HttpClientAsyncGet) connects
  `HttpClient` to `AsyncStreams` and an `AsyncEventLoop`.
- [AsyncWebServer](https://github.com/Pagghiu/SaneCppLibraries/tree/main/Examples/AsyncWebServer) is the advanced HTTP
  laboratory for static files, uploads, WebSockets, configurable connection storage, and Linux backend selection.

To serve a directory with `AsyncWebServer`, pass an absolute path after `--`:

```bash
./SC.sh build compile AsyncWebServer Debug
./SC.sh build run AsyncWebServer Debug -- --directory /absolute/path/to/site
```

## Await cookbook

> **Warning:** `Await` is currently experimental. These examples are complete programs for exploring the coroutine API,
> not a stable compatibility promise.

### Task ownership and coordination

- `AwaitBackgroundJobs` manages detached work in fixed caller-owned registry slots.
- `AwaitDeadline` applies a deadline and cooperative cancellation to a child task.
- `AwaitFirstResponse` races two jobs and cancels the slower one.

### Networking

- `AwaitEcho` is the baseline TCP client/server conversation.
- `AwaitDatagramPing` demonstrates a bounded UDP request and reply.
- `AwaitLineProtocol` builds a small CRLF-framed protocol.

### File I/O

- `AwaitConfigReload` composes a child task that reads a configuration file.
- `AwaitFilePatch` performs explicit offset writes and reads.
- `AwaitManifestPreview` reads into a bounded preview buffer until full or EOF.
- `AwaitTaskGroupFiles` reads multiple files with a task group.
- `AwaitFileCourier` combines file operations with file-to-socket transfer.

### System integration

- `AwaitBackgroundDigest` moves bounded CPU work to a thread pool.
- `AwaitCallbackBridge` runs callback-style `Async` and coroutine-style `Await` on the same event loop.
- `AwaitProcessExitCodes` waits for multiple child processes.
- `AwaitThreadWakeUp` delivers another thread's signal to an Await coroutine.

### Composed workflow

- `AwaitServiceProbe` combines networking, task groups, background work, timeouts, and explicit shutdown.

All Await sources are under the
[Examples directory](https://github.com/Pagghiu/SaneCppLibraries/tree/main/Examples) and use their directory name as the
build target.

## Fibers

- [FibersDemo](https://github.com/Pagghiu/SaneCppLibraries/tree/main/Examples/FibersDemo) demonstrates CPU fibers,
  `FibersAsync` sleeps, and worker-pool work in three focused sections.
- [FibersBenchmark](https://github.com/Pagghiu/SaneCppLibraries/tree/main/Examples/FibersBenchmark) is a maintainer
  benchmark for scheduler, contention, and sustained micro-task workloads.
- [FibersStackGrowthPrototype](https://github.com/Pagghiu/SaneCppLibraries/tree/main/Examples/FibersStackGrowthPrototype)
  is a maintainer prototype for stack-growth behavior.

Benchmark results depend on the machine and build configuration; they are development measurements rather than portable
performance claims.

## Native integration and hot reload

[SCExample](https://github.com/Pagghiu/SaneCppLibraries/tree/main/Examples/SCExample) is the native Sokol and Dear ImGui
host used to exercise `Async`, plugins, file watching, state handoff, and hot reload. Its plugins are:

- `CollaborativeCanvasExample`, which hosts a browser canvas through HTTP and WebSockets;
- `SerializationExample`, which demonstrates reflected state and binary/JSON serialization;
- `WebServerExample`, which configures a static file server from the native UI.

SCExample is an integration testbed rather than the shortest introduction to any individual library. Start with a
focused console example when learning one API.

## More source-backed examples

The library pages contain compiled snippets for individual APIs. The tests provide broader executable reference code,
including error paths and platform-specific behavior. See [Tests](@ref page_tests) and the
[library catalog](@ref libraries) when an example above is broader than the question you are trying to answer.
