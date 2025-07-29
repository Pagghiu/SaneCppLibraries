@page page_dependencies Dependencies
This file describes what each library depends on. It also lists the number of lines of code (LOC) for each library, both including and excluding comments.

# [Algorithms](@ref library_algorithms)
- Direct dependencies: [Foundation](@ref library_foundation)
- All dependencies: [Foundation](@ref library_foundation)
- Lines of code (excluding comments): 102
- Lines of code (including comments): 170

# [Async](@ref library_async)
- Direct dependencies: [File](@ref library_file), [FileSystem](@ref library_file_system), [Foundation](@ref library_foundation), [Socket](@ref library_socket), [Threading](@ref library_threading), [Time](@ref library_time)
- All dependencies: [File](@ref library_file), [FileSystem](@ref library_file_system), [Foundation](@ref library_foundation), [Socket](@ref library_socket), [Threading](@ref library_threading), [Time](@ref library_time)
- Lines of code (excluding comments): 5661
- Lines of code (including comments): 7502

# [AsyncStreams](@ref library_async_streams)
- Direct dependencies: [Async](@ref library_async), [Foundation](@ref library_foundation)
- All dependencies: [Async](@ref library_async), [File](@ref library_file), [FileSystem](@ref library_file_system), [Foundation](@ref library_foundation), [Socket](@ref library_socket), [Threading](@ref library_threading), [Time](@ref library_time)
- Lines of code (excluding comments): 2013
- Lines of code (including comments): 2558

# [Build](@ref library_build)
- Direct dependencies: [Algorithms](@ref library_algorithms), [Containers](@ref library_containers), [FileSystem](@ref library_file_system), [FileSystemIterator](@ref library_file_system_iterator), [Foundation](@ref library_foundation), [Hashing](@ref library_hashing), [Process](@ref library_process), [Strings](@ref library_strings)
- All dependencies: [Algorithms](@ref library_algorithms), [Containers](@ref library_containers), [File](@ref library_file), [FileSystem](@ref library_file_system), [FileSystemIterator](@ref library_file_system_iterator), [Foundation](@ref library_foundation), [Hashing](@ref library_hashing), [Memory](@ref library_memory), [Process](@ref library_process), [Strings](@ref library_strings), [Time](@ref library_time)
- Lines of code (excluding comments): 4094
- Lines of code (including comments): 4751

# [Containers](@ref library_containers)
- Direct dependencies: [Algorithms](@ref library_algorithms), [Foundation](@ref library_foundation), [Memory](@ref library_memory)
- All dependencies: [Algorithms](@ref library_algorithms), [Foundation](@ref library_foundation), [Memory](@ref library_memory)
- Lines of code (excluding comments): 801
- Lines of code (including comments): 1111

# [File](@ref library_file)
- Direct dependencies: [Foundation](@ref library_foundation)
- All dependencies: [Foundation](@ref library_foundation)
- Lines of code (excluding comments): 700
- Lines of code (including comments): 931

# [FileSystem](@ref library_file_system)
- Direct dependencies: [File](@ref library_file), [Foundation](@ref library_foundation), [Time](@ref library_time)
- All dependencies: [File](@ref library_file), [Foundation](@ref library_foundation), [Time](@ref library_time)
- Lines of code (excluding comments): 1323
- Lines of code (including comments): 1752

# [FileSystemIterator](@ref library_file_system_iterator)
- Direct dependencies: [Foundation](@ref library_foundation)
- All dependencies: [Foundation](@ref library_foundation)
- Lines of code (excluding comments): 417
- Lines of code (including comments): 541

# [FileSystemWatcher](@ref library_file_system_watcher)
- Direct dependencies: [Foundation](@ref library_foundation), [Threading](@ref library_threading)
- All dependencies: [Foundation](@ref library_foundation), [Threading](@ref library_threading)
- Lines of code (excluding comments): 1319
- Lines of code (including comments): 1668

# [FileSystemWatcherAsync](@ref library_file_system_watcher_async)
- Direct dependencies: [Async](@ref library_async), [FileSystemWatcher](@ref library_file_system_watcher), [Foundation](@ref library_foundation)
- All dependencies: [Async](@ref library_async), [File](@ref library_file), [FileSystem](@ref library_file_system), [FileSystemWatcher](@ref library_file_system_watcher), [Foundation](@ref library_foundation), [Socket](@ref library_socket), [Threading](@ref library_threading), [Time](@ref library_time)
- Lines of code (excluding comments): 113
- Lines of code (including comments): 166

# [Foundation](@ref library_foundation)
- Direct dependencies: *(none)*
- All dependencies: *(none)*
- Lines of code (excluding comments): 1215
- Lines of code (including comments): 1842

# [Hashing](@ref library_hashing)
- Direct dependencies: [Foundation](@ref library_foundation)
- All dependencies: [Foundation](@ref library_foundation)
- Lines of code (excluding comments): 359
- Lines of code (including comments): 490

# [Http](@ref library_http)
- Direct dependencies: [Async](@ref library_async), [Containers](@ref library_containers), [FileSystem](@ref library_file_system), [Foundation](@ref library_foundation), [Memory](@ref library_memory), [Socket](@ref library_socket), [Strings](@ref library_strings)
- All dependencies: [Algorithms](@ref library_algorithms), [Async](@ref library_async), [Containers](@ref library_containers), [File](@ref library_file), [FileSystem](@ref library_file_system), [Foundation](@ref library_foundation), [Memory](@ref library_memory), [Socket](@ref library_socket), [Strings](@ref library_strings), [Threading](@ref library_threading), [Time](@ref library_time)
- Lines of code (excluding comments): 1299
- Lines of code (including comments): 1639

# [Memory](@ref library_memory)
- Direct dependencies: [Foundation](@ref library_foundation)
- All dependencies: [Foundation](@ref library_foundation)
- Lines of code (excluding comments): 1257
- Lines of code (including comments): 1679

# [Plugin](@ref library_plugin)
- Direct dependencies: [Algorithms](@ref library_algorithms), [Containers](@ref library_containers), [FileSystem](@ref library_file_system), [FileSystemIterator](@ref library_file_system_iterator), [Foundation](@ref library_foundation), [Memory](@ref library_memory), [Process](@ref library_process), [Strings](@ref library_strings), [Threading](@ref library_threading), [Time](@ref library_time)
- All dependencies: [Algorithms](@ref library_algorithms), [Containers](@ref library_containers), [File](@ref library_file), [FileSystem](@ref library_file_system), [FileSystemIterator](@ref library_file_system_iterator), [Foundation](@ref library_foundation), [Memory](@ref library_memory), [Process](@ref library_process), [Strings](@ref library_strings), [Threading](@ref library_threading), [Time](@ref library_time)
- Lines of code (excluding comments): 1464
- Lines of code (including comments): 1852

# [Process](@ref library_process)
- Direct dependencies: [File](@ref library_file), [Foundation](@ref library_foundation)
- All dependencies: [File](@ref library_file), [Foundation](@ref library_foundation)
- Lines of code (excluding comments): 1318
- Lines of code (including comments): 1828

# [Reflection](@ref library_reflection)
- Direct dependencies: [Algorithms](@ref library_algorithms), [Containers](@ref library_containers), [Foundation](@ref library_foundation), [Memory](@ref library_memory), [Strings](@ref library_strings)
- All dependencies: [Algorithms](@ref library_algorithms), [Containers](@ref library_containers), [Foundation](@ref library_foundation), [Memory](@ref library_memory), [Strings](@ref library_strings)
- Lines of code (excluding comments): 700
- Lines of code (including comments): 994

# [SerializationBinary](@ref library_serialization_binary)
- Direct dependencies: [Foundation](@ref library_foundation), [Memory](@ref library_memory), [Reflection](@ref library_reflection)
- All dependencies: [Algorithms](@ref library_algorithms), [Containers](@ref library_containers), [Foundation](@ref library_foundation), [Memory](@ref library_memory), [Reflection](@ref library_reflection), [Strings](@ref library_strings)
- Lines of code (excluding comments): 594
- Lines of code (including comments): 856

# [SerializationText](@ref library_serialization_text)
- Direct dependencies: [Foundation](@ref library_foundation), [Reflection](@ref library_reflection), [Strings](@ref library_strings)
- All dependencies: [Algorithms](@ref library_algorithms), [Containers](@ref library_containers), [Foundation](@ref library_foundation), [Memory](@ref library_memory), [Reflection](@ref library_reflection), [Strings](@ref library_strings)
- Lines of code (excluding comments): 661
- Lines of code (including comments): 853

# [Socket](@ref library_socket)
- Direct dependencies: [Foundation](@ref library_foundation)
- All dependencies: [Foundation](@ref library_foundation)
- Lines of code (excluding comments): 889
- Lines of code (including comments): 1179

# [Strings](@ref library_strings)
- Direct dependencies: [Foundation](@ref library_foundation), [Memory](@ref library_memory)
- All dependencies: [Foundation](@ref library_foundation), [Memory](@ref library_memory)
- Lines of code (excluding comments): 3387
- Lines of code (including comments): 4950

# [Testing](@ref library_testing)
- Direct dependencies: [Foundation](@ref library_foundation), [Memory](@ref library_memory), [Strings](@ref library_strings)
- All dependencies: [Foundation](@ref library_foundation), [Memory](@ref library_memory), [Strings](@ref library_strings)
- Lines of code (excluding comments): 343
- Lines of code (including comments): 440

# [Threading](@ref library_threading)
- Direct dependencies: [Foundation](@ref library_foundation)
- All dependencies: [Foundation](@ref library_foundation)
- Lines of code (excluding comments): 895
- Lines of code (including comments): 1178

# [Time](@ref library_time)
- Direct dependencies: [Foundation](@ref library_foundation)
- All dependencies: [Foundation](@ref library_foundation)
- Lines of code (excluding comments): 349
- Lines of code (including comments): 514

---
# Project Total
- Total lines of code (excluding comments): 31273
- Total lines of code (including comments): 41444

