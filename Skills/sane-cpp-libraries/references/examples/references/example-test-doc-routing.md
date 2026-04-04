# Example, Test, And Doc Routing

Use this file to map user requests to the best entrypoints in the repo.

## Start With These High-Value Paths

- `README.md`
- `Documentation/Pages/BuildingUser.md`
- `Documentation/Pages/Examples.md`
- `Tests/Libraries`
- `Examples`

## Route By Request

### "How do I adopt the library?" or "How do I wire this into my build?"

Prioritize:

1. `README.md`
2. `Documentation/Pages/BuildingUser.md`
3. `Documentation/Pages/SingleFileLibs.md`

### "Show me an HTTP server example" or "How do I serve files?"

Prioritize:

1. `Examples/AsyncWebServer/AsyncWebServer.cpp`
2. `Tests/Libraries/Http/HttpAsyncFileServerTest.cpp`
3. `Documentation/Libraries/Http.md`

### "Show me an HTTP client example"

Prioritize:

1. `Examples/SaneHttpGet/SaneHttpGet.cpp`
2. `Tests/Libraries/HttpClient/HttpClientTest.cpp`
3. `Documentation/Libraries/HttpClient.md`

### "Show me async event loop usage"

Prioritize:

1. `Tests/Libraries/Async/AsyncTest.cpp`
2. `Tests/Libraries/Async/AsyncTestLoop.inl`
3. `Documentation/Libraries/Async.md`
4. `Examples/SCExample/SCExample.cpp`

### "Show me plugin hot reload" or "How does SCExample wire plugins?"

Prioritize:

1. `Examples/SCExample/HotReloadSystem.h`
2. `Examples/SCExample/SCExample.cpp`
3. `Tests/Libraries/Plugin/PluginTest.cpp`
4. `Documentation/Libraries/Plugin.md`

### "Show me reflection or serialization examples"

Prioritize:

1. `Tests/Libraries/Reflection/ReflectionTest.cpp`
2. `Tests/Libraries/SerializationBinary/SerializationBinaryTest.cpp`
3. `Tests/Libraries/SerializationText/SerializationJsonTest.cpp`
4. `Documentation/Libraries/Reflection.md`

### "Show me filesystem watching"

Prioritize:

1. `Tests/Libraries/FileSystemWatcher/FileSystemWatcherTest.cpp`
2. `Tests/Libraries/FileSystemWatcher/FileSystemWatcherAsyncTest.cpp`
3. `Documentation/Libraries/FileSystemWatcher.md`
4. `Examples/SCExample/HotReloadSystem.h`

### "Show me how SC::Build or SC::Tools are used in real code"

Prioritize:

1. `Tools/SC-build.cpp`
2. `Tools/SC-package.cpp`
3. `Tools/SC-format.cpp`
4. `Documentation/Pages/Build.md`
5. `Documentation/Pages/Tools.md`

## Curation Rules

- Prefer one runnable example plus one focused test over a giant file dump.
- Use docs first only when the user is still deciding direction.
- Use tests first when the user needs API behavior, edge cases, or a smaller code sample.
- Mention why each path is relevant in one short line.
