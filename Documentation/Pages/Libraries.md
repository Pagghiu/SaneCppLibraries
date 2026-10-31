@page libraries Libraries

This is the list of all libraries, whose internal dependencies are documented at [Dependencies](@ref page_dependencies):

Library                                     | Description                                   | Header LOC | Source LOC | Standalone LOC
:-------------------------------------------|:----------------------------------------------|-----------:|-----------:|---------------:
@subpage library_async                  | @copybrief library_async                  | 2129 | 6219 | 23345
@subpage library_async_fibers           | @copybrief library_async_fibers           | 208 | 775 | 36900
@subpage library_async_streams          | @copybrief library_async_streams          | 1219 | 1849 | 4226
@subpage library_await                  | @copybrief library_await                  | 1316 | 3265 | 28752
@subpage library_containers             | @copybrief library_containers             | 1007 | 3 | 4514
@subpage library_containers_reflection  | @copybrief library_containers_reflection  | 231 | 0 | 5695
@subpage library_cryptography           | @copybrief library_cryptography           | 1048 | 2888 | 4230
@subpage library_fibers                 | @copybrief library_fibers                 | 1664 | 8960 | 11741
@subpage library_file                   | @copybrief library_file                   | 496 | 1680 | 3839
@subpage library_file_system            | @copybrief library_file_system            | 494 | 2080 | 4139
@subpage library_file_system_iterator   | @copybrief library_file_system_iterator   | 247 | 488 | 2549
@subpage library_file_system_watcher    | @copybrief library_file_system_watcher    | 751 | 1224 | 3635
@subpage library_foundation             | @copybrief library_foundation             | 27 | 66 | 1868
@subpage library_hashing                | @copybrief library_hashing                | 95 | 295 | 779
@subpage library_http                   | @copybrief library_http                   | 2550 | 8499 | 39968
@subpage library_http_client            | @copybrief library_http_client            | 1717 | 5086 | 7884
@subpage library_memory                 | @copybrief library_memory                 | 554 | 1053 | 2802
@subpage library_plugin                 | @copybrief library_plugin                 | 1303 | 1530 | 11986
@subpage library_process                | @copybrief library_process                | 585 | 1199 | 7530
@subpage library_reflection             | @copybrief library_reflection             | 616 | 0 | 930
@subpage library_serial_port            | @copybrief library_serial_port            | 230 | 634 | 5710
@subpage library_serialization_binary   | @copybrief library_serialization_binary   | 593 | 0 | 1975
@subpage library_serialization_text     | @copybrief library_serialization_text     | 632 | 471 | 2835
@subpage library_socket                 | @copybrief library_socket                 | 549 | 1405 | 3262
@subpage library_strings                | @copybrief library_strings                | 1093 | 3131 | 5648
@subpage library_testing                | @copybrief library_testing                | 173 | 796 | 2697
@subpage library_threading              | @copybrief library_threading              | 566 | 967 | 2626
@subpage library_time                   | @copybrief library_time                   | 133 | 208 | 627
Common source fragments                    | Shared source fragments, not a library       | 1871 | 644 | -

LOC metric                         | Header | Source | Sum
:-----------------------------------|-------:|-------:|----:
Library source                      | 22226 | 54771 | 76997
Common source fragments             | 1871 | 644 | 2515

All LOC counts in the tables above exclude comments.






















![Dependency Graph](https://pagghiu.github.io/images/dependencies/SaneCppLibrariesDependencies.svg)

Some libraries have [C Bindings](@ref group_c_bindings):

Library                                     | Description
:-------------------------------------------|:-----------------------------------------------
@ref group_sc_hashing                       | @copybrief group_sc_hashing
