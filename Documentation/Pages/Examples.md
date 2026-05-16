@page page_examples Examples
# Examples

## Building

@note Examples now build directly through native `build compile` / `build run` without a prior `configure`.
Use `build configure` only when you explicitly want generated Visual Studio, XCode, or Make projects.

@note Projects will be generated in `_Build/_Projects`.  
After compiling (`SC build compile`) executables will be at `_Build/_Outputs/${platform}-${arch}-${build}-${compiler}-${config}/${EXAMPLE_NAME}`.

## SCExample

| Property      | Value                                                                                                                             |
|:--------------|:----------------------------------------------------------------------------------------------------------------------------------|
| Location      | `Examples/SCExample`                                                                                                              |
| Description   | Simple immediate mode gui application using sokol and dear imgui libraries pausing in absence of inputs and I/O to save CPU time  |
| Purposes      | Use [SC::Build](@ref page_build) to build on macOS, windows and linux and [SC::Async](@ref library_async) as I/O event Loop       |
|               | Use [Plugin](@ref library_plugin) and [FileSystemWatcher](@ref library_file_system_watcher) libraries implementing hot-reload     |
| Prerequisites | Linux - Fedora: `sudo dnf install mesa-libGL-devel mesa-libEGL-devel libX11-devel libXi-devel libXcursor-devel`                   |
|               | Linux - Ubuntu: `sudo apt-get install libgles2-mesa-dev libx11-dev libxi-dev libxcursor-dev`                                      |
|               | Windows: None                                                                                                                     |
|               | macOS: None                                                                                                                       |
| Dependencies  | `sokol` and `dear-imgui` are automatically downloaded during the first `build compile` / `build run`, or during `build configure` if you are generating projects explicitly |
| Run (Posix)   | `./SC.sh build run SCExample` (will also compile before running)                                                                  |
| Run (Windows) | `SC.bat build run SCExample`  (will also compile before running)                                                                  |
| Run (Native)  | `./SC.sh build run SCExample --config Debug` or `SC.bat build run SCExample --config Debug`                                       |
| Debug (VSCode)| Select correct `SCExample $ARCH ($PLATFORM)` for your system and press `Start Debugging` (F5)                                     |
| Debug (XCode) | Run `./SC.sh build configure`, then open `_Build/_Projects/XCode/SCWorkspace/SCWorkspace.xcworkspace` and choose the `SCExample` scheme |
| Debug (VS2022)| Run `SC.bat build configure`, then open `_Build/_Projects/VisualStudio2022/SCWorkspace/SCWorkspace.sln` and start Debugging (F5) |

## AsyncWebServerExample

| Property      | Value                                                                                                                             |
|:--------------|:----------------------------------------------------------------------------------------------------------------------------------|
| Location      | `Examples/AsyncWebServer`                                                                                                         |
| Description   | Simple Http server listening on port 8090 by default, serving a folder for a static website.                                      |
| Purposes      | Show how to create an http server with a runtime configurable set of buffers                                                      |
| Prerequisites | Linux: None                                                                                                                       |
|               | Windows: None                                                                                                                     |
|               | macOS: None                                                                                                                       |
| Dependencies  | None                                                                                                                              |
| Run (Posix)   | `Examples/AsyncWebServer/BuildAndRun.sh` (will also compile before running)                                                       |
| Run (Windows) | `Examples/AsyncWebServer/BuildAndRun.bat` (will also compile before running)                                                      |
| Parameters    | All parameters can be added after `BuildAndRun` for example `BuildAndRun.sh --directory /somedir`:                                |
|               | `--directory /path/to/dir`: Serves the given folder                                                                               |
| Debug (XCode) | Run `./SC.sh build configure`, then open `_Build/_Projects/XCode/SCWorkspace/SCWorkspace.xcworkspace` and choose the `AsyncWebServer` scheme |
| Debug (VS2022)| Run `SC.bat build configure`, then open `_Build/_Projects/VisualStudio2022/SCWorkspace/SCWorkspace.sln` and start Debugging (F5) |

# Blog

Some relevant blog posts are:

- [June 2024 Update](https://pagghiu.github.io/site/blog/2024-06-30-SaneCppLibrariesUpdate.html)
- [July 2024 Update](https://pagghiu.github.io/site/blog/2024-07-31-SaneCppLibrariesUpdate.html)
- [August 2024 Update](https://pagghiu.github.io/site/blog/2024-08-30-SaneCppLibrariesUpdate.html)

### Examples

| Example       | Description                                                                                                                       |
|:--------------|:----------------------------------------------------------------------------------------------------------------------------------|
| AwaitCallbackBridge | Shows callback-style `Async` and coroutine-style `Await` sharing one caller-owned event loop                                |
| AwaitDatagramPing | Shows UDP request/reply with `AwaitEventLoop::sendTo()` and `receiveFrom()`                                                   |
| AwaitEcho     | Shows a tiny TCP echo conversation with `AwaitTaskGroup` and caller-owned coroutine arena                                        |
| AwaitFileCourier | Shows file copy followed by `AwaitEventLoop::fileSend()` over a socket                                                     |
| AwaitLineProtocol | Shows a tiny CRLF text protocol with `receiveLine()`, `sendAll()`, and caller-owned coroutine arena                       |
| AwaitTaskGroupFiles | Shows `AwaitTaskGroup` fan-out over two file reads with caller-owned task storage                                        |
| Serialization | Use [Reflection](@ref library_reflection) and [Serialization](@ref library_serialization_binary) to persist application state     |
| WebServer     | Use [Http](@ref library_http) to statically host a website from a specified directory                                             |

# Where can I find more examples?

- The test suite is the closest thing to additional examples you can find in this project.
- The second best thing to do is looking at [SC::Tools](@ref page_tools) implementation.
- Documentation for each library has some examples and / or code snippets that you can look at.


# Where can I learn more?

There are many way to learn about the library:
- Read the [introductory blog post](https://pagghiu.github.io/site/blog/2023-12-23-SaneCppLibrariesRelease.html)
- Take a look at videos from [Youtube Channel](https://www.youtube.com/@Pagghiu)
- Read and / or step through the extensive set of unit tests (current test code coverage is > 90%).
- Ask in the [Discord](https://discord.gg/tyBfFp33Z6)

https://github.com/user-attachments/assets/2a38310c-6a28-4f86-a0f3-665dc15b126d
https://github.com/Pagghiu/SaneCppLibraries/assets/5406873/5c7d4036-6e0c-4262-ad57-9ef84c214717

\htmlonly
<iframe width="700" height="400" src="https://github.com/user-attachments/assets/2a38310c-6a28-4f86-a0f3-665dc15b126d" frameborder="0" allowfullscreen>
</iframe>
\endhtmlonly

\htmlonly
<iframe width="700" height="400" src="https://github.com/Pagghiu/SaneCppLibraries/assets/5406873/5c7d4036-6e0c-4262-ad57-9ef84c214717" frameborder="0" allowfullscreen>
</iframe>
\endhtmlonly
