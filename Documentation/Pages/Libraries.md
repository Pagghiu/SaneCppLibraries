@page libraries Libraries

This is the list of all libraries, whose internal dependencies are documented at [Dependencies](@ref page_dependencies):

Library                                     | Description                                   | Header LOC | Source LOC | Standalone LOC
:-------------------------------------------|:----------------------------------------------|-----------:|-----------:|---------------:
@subpage library_async                  | @copybrief library_async                  | 1926 | 5842 | 19469
@subpage library_async_streams          | @copybrief library_async_streams          | 924 | 1798 | 3755
@subpage library_await                  | @copybrief library_await                  | 1199 | 3231 | 24431
@subpage library_containers             | @copybrief library_containers             | 1007 | 3 | 4428
@subpage library_containers_reflection  | @copybrief library_containers_reflection  | 231 | 0 | 5609
@subpage library_file                   | @copybrief library_file                   | 200 | 1364 | 3018
@subpage library_file_system            | @copybrief library_file_system            | 188 | 1858 | 3403
@subpage library_file_system_iterator   | @copybrief library_file_system_iterator   | 92 | 402 | 2098
@subpage library_file_system_watcher    | @copybrief library_file_system_watcher    | 511 | 1122 | 3145
@subpage library_foundation             | @copybrief library_foundation             | 27 | 64 | 1813
@subpage library_hashing                | @copybrief library_hashing                | 95 | 292 | 776
@subpage library_http                   | @copybrief library_http                   | 1861 | 7587 | 33872
@subpage library_http_client            | @copybrief library_http_client            | 1188 | 4754 | 6876
@subpage library_memory                 | @copybrief library_memory                 | 554 | 1053 | 2749
@subpage library_plugin                 | @copybrief library_plugin                 | 865 | 1322 | 9892
@subpage library_process                | @copybrief library_process                | 408 | 1103 | 6227
@subpage library_reflection             | @copybrief library_reflection             | 616 | 0 | 930
@subpage library_serial_port            | @copybrief library_serial_port            | 55 | 599 | 4532
@subpage library_serialization_binary   | @copybrief library_serialization_binary   | 593 | 0 | 1943
@subpage library_serialization_text     | @copybrief library_serialization_text     | 632 | 471 | 2783
@subpage library_socket                 | @copybrief library_socket                 | 185 | 894 | 2238
@subpage library_strings                | @copybrief library_strings                | 1056 | 3143 | 5475
@subpage library_testing                | @copybrief library_testing                | 155 | 791 | 2560
@subpage library_threading              | @copybrief library_threading              | 433 | 926 | 2190
@subpage library_time                   | @copybrief library_time                   | 133 | 208 | 541
Common source fragments                    | Shared source fragments, not a library       | 1676 | 582 | -

LOC metric                         | Header | Source | Sum
:-----------------------------------|-------:|-------:|----:
Library source                      | 15134 | 38827 | 53961
Common source fragments             | 1676 | 582 | 2258

All LOC counts in the tables above exclude comments.













![Dependency Graph](https://pagghiu.github.io/images/dependencies/SaneCppLibrariesDependencies.svg)

Some libraries have [C Bindings](@ref group_c_bindings):

Library                                     | Description
:-------------------------------------------|:-----------------------------------------------
@ref group_sc_hashing                       | @copybrief group_sc_hashing
