# Example, Test, And Doc Routing

Use this file to map user requests to the best entrypoints in the repo.

## Start With These High-Value Paths

- `/Users/stefano/Developer/Projects/SC-skills/SC-skills/README.md`
- `/Users/stefano/Developer/Projects/SC-skills/SC-skills/Documentation/Pages/BuildingUser.md`
- `/Users/stefano/Developer/Projects/SC-skills/SC-skills/Documentation/Pages/Examples.md`
- `/Users/stefano/Developer/Projects/SC-skills/SC-skills/Tests/Libraries`
- `/Users/stefano/Developer/Projects/SC-skills/SC-skills/Examples`

## Route By Request

### "How do I adopt the library?" or "How do I wire this into my build?"

Prioritize:

1. `/Users/stefano/Developer/Projects/SC-skills/SC-skills/README.md`
2. `/Users/stefano/Developer/Projects/SC-skills/SC-skills/Documentation/Pages/BuildingUser.md`
3. `/Users/stefano/Developer/Projects/SC-skills/SC-skills/Documentation/Pages/SingleFileLibs.md`

### "Show me an HTTP server example" or "How do I serve files?"

Prioritize:

1. `/Users/stefano/Developer/Projects/SC-skills/SC-skills/Examples/AsyncWebServer/AsyncWebServer.cpp`
2. `/Users/stefano/Developer/Projects/SC-skills/SC-skills/Tests/Libraries/Http/HttpAsyncFileServerTest.cpp`
3. `/Users/stefano/Developer/Projects/SC-skills/SC-skills/Documentation/Libraries/Http.md`

### "Show me an HTTP client example"

Prioritize:

1. `/Users/stefano/Developer/Projects/SC-skills/SC-skills/Examples/SaneHttpGet/SaneHttpGet.cpp`
2. `/Users/stefano/Developer/Projects/SC-skills/SC-skills/Tests/Libraries/HttpClient/HttpClientTest.cpp`
3. `/Users/stefano/Developer/Projects/SC-skills/SC-skills/Documentation/Libraries/HttpClient.md`

### "Show me async event loop usage"

Prioritize:

1. `/Users/stefano/Developer/Projects/SC-skills/SC-skills/Tests/Libraries/Async/AsyncTest.cpp`
2. `/Users/stefano/Developer/Projects/SC-skills/SC-skills/Tests/Libraries/Async/AsyncTestLoop.inl`
3. `/Users/stefano/Developer/Projects/SC-skills/SC-skills/Documentation/Libraries/Async.md`
4. `/Users/stefano/Developer/Projects/SC-skills/SC-skills/Examples/SCExample/SCExample.cpp`

### "Show me plugin hot reload" or "How does SCExample wire plugins?"

Prioritize:

1. `/Users/stefano/Developer/Projects/SC-skills/SC-skills/Examples/SCExample/HotReloadSystem.h`
2. `/Users/stefano/Developer/Projects/SC-skills/SC-skills/Examples/SCExample/SCExample.cpp`
3. `/Users/stefano/Developer/Projects/SC-skills/SC-skills/Tests/Libraries/Plugin/PluginTest.cpp`
4. `/Users/stefano/Developer/Projects/SC-skills/SC-skills/Documentation/Libraries/Plugin.md`

### "Show me reflection or serialization examples"

Prioritize:

1. `/Users/stefano/Developer/Projects/SC-skills/SC-skills/Tests/Libraries/Reflection/ReflectionTest.cpp`
2. `/Users/stefano/Developer/Projects/SC-skills/SC-skills/Tests/Libraries/SerializationBinary/SerializationBinaryTest.cpp`
3. `/Users/stefano/Developer/Projects/SC-skills/SC-skills/Tests/Libraries/SerializationText/SerializationJsonTest.cpp`
4. `/Users/stefano/Developer/Projects/SC-skills/SC-skills/Documentation/Libraries/Reflection.md`

### "Show me filesystem watching"

Prioritize:

1. `/Users/stefano/Developer/Projects/SC-skills/SC-skills/Tests/Libraries/FileSystemWatcher/FileSystemWatcherTest.cpp`
2. `/Users/stefano/Developer/Projects/SC-skills/SC-skills/Tests/Libraries/FileSystemWatcher/FileSystemWatcherAsyncTest.cpp`
3. `/Users/stefano/Developer/Projects/SC-skills/SC-skills/Documentation/Libraries/FileSystemWatcher.md`
4. `/Users/stefano/Developer/Projects/SC-skills/SC-skills/Examples/SCExample/HotReloadSystem.h`

### "Show me how SC::Build or SC::Tools are used in real code"

Prioritize:

1. `/Users/stefano/Developer/Projects/SC-skills/SC-skills/Tools/SC-build.cpp`
2. `/Users/stefano/Developer/Projects/SC-skills/SC-skills/Tools/SC-package.cpp`
3. `/Users/stefano/Developer/Projects/SC-skills/SC-skills/Tools/SC-format.cpp`
4. `/Users/stefano/Developer/Projects/SC-skills/SC-skills/Documentation/Pages/Build.md`
5. `/Users/stefano/Developer/Projects/SC-skills/SC-skills/Documentation/Pages/Tools.md`

## Curation Rules

- Prefer one runnable example plus one focused test over a giant file dump.
- Use docs first only when the user is still deciding direction.
- Use tests first when the user needs API behavior, edge cases, or a smaller code sample.
- Mention why each path is relevant in one short line.
