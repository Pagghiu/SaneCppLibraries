@page page_examples Examples
# Examples

@note All of the examples assume build configure to be already run, as described in [Building Contributor](@ref page_building_contributor):
- Windows: `./SC.sh build configure`
- Posix: `SC.bat build configure`

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

https://github.com/Pagghiu/SaneCppLibraries/assets/5406873/4ad12058-6bc2-4316-90f8-4ba4c05e28de

\htmlonly
<iframe width="700" height="400" src="https://github.com/Pagghiu/SaneCppLibraries/assets/5406873/4ad12058-6bc2-4316-90f8-4ba4c05e28de" frameborder="0" allowfullscreen>
\endhtmlonly

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
