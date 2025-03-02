@page page_examples Examples
# Examples

@note All of the examples assume build configure to be already run, as described in [Building Contributor](@ref page_building_contributor):
- Posix: `./SC.sh build configure`
- Windows: `SC.bat build configure`
Projects will be generated in `_Build/_Projects` for Visual Studio, Xcode and Makefiles

## SCExample

| Property      | Value                                                                                                                             |
|:--------------|:----------------------------------------------------------------------------------------------------------------------------------|
| Location      | `Examples/SCExample`                                                                                                              |
| Description   | Simple immediate mode gui application using sokol and dear imgui libraries pausing in absence of inputs and I/O to save CPU time  |
| Purposes      | Use [SC::Build](@ref library_build) to build on macOS, windows and linux and [SC::Async](@ref library_async) as I/O event Loop    |
|               | Use [Plugin](@ref library_plugin) and [FileSystemWatcher](@ref library_file_system_watcher) libraries implementing hot-reload     |
| Prerequisites | Linux - Fedora: `sudo dnf install mesa-libGL-devel mesa-libEGL-devel libX11-devel libXi-devel libXcursor-devel`                   |
|               | Linux - Ubuntu: `sudo apt-get install libgles2-mesa-dev libx11-dev libxi-dev libxcursor-dev`                                      |
|               | Windows: None                                                                                                                     |
|               | macOS: None                                                                                                                       |
| Dependencies  | `sokol` and `dear-imgui` are automatically downloaded during `./SC.sh build configure` or  `SC.bat build configure`               |
| Run (Posix)   | `./SC.sh build run SCExample`                                                                                                     |
| Run (Windows) | `SC.bat build run SCExample`                                                                                                      |
| Debug (VSCode)| Select correct `SCExample $ARCH ($PLATFORM)` for your system and press `Start Debugging` (F5)                                     |
| Debug (XCode) | Open `_Build/_Projects/XCode/SCExample.xcodeproj` and choose `Product` --> `Run`                                                  |
| Debug (VS2022)| Open `_Build/_Projects/VisualStudio2022/SCExample.sln` and start Debugging (F5)                                                   |

# Blog

Some relevant blog posts are:

- [June 2024 Update](https://pagghiu.github.io/site/blog/2024-06-30-SaneCppLibrariesUpdate.html)
- [July 2024 Update](https://pagghiu.github.io/site/blog/2024-07-31-SaneCppLibrariesUpdate.html)
- [August 2024 Update](https://pagghiu.github.io/site/blog/2024-08-30-SaneCppLibrariesUpdate.html)

### Examples

| Example       | Description                                                                                                                       |
|:--------------|:----------------------------------------------------------------------------------------------------------------------------------|
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

