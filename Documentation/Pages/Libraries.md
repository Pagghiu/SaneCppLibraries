@page libraries Libraries

This is the list of all libraries, whose internal dependencies are documented at [Dependencies](@ref page_dependencies):

Library                                     | Description                                   | Header LOC | Source LOC | Standalone LOC
:-------------------------------------------|:----------------------------------------------|-----------:|-----------:|---------------:
@subpage library_async                  | @copybrief library_async                  | 2121 | 6094 | 23222
@subpage library_async_fibers           | @copybrief library_async_fibers           | 206 | 939 | 36572
@subpage library_async_streams          | @copybrief library_async_streams          | 1219 | 1847 | 4224
@subpage library_await                  | @copybrief library_await                  | 1316 | 3265 | 28629
@subpage library_containers             | @copybrief library_containers             | 1007 | 3 | 4514
@subpage library_containers_reflection  | @copybrief library_containers_reflection  | 231 | 0 | 5695
@subpage library_cryptography           | @copybrief library_cryptography           | 1048 | 2778 | 4120
@subpage library_fibers                 | @copybrief library_fibers                 | 1637 | 8620 | 11374
@subpage library_file                   | @copybrief library_file                   | 496 | 1724 | 3883
@subpage library_file_system            | @copybrief library_file_system            | 494 | 2080 | 4139
@subpage library_file_system_iterator   | @copybrief library_file_system_iterator   | 247 | 488 | 2549
@subpage library_file_system_watcher    | @copybrief library_file_system_watcher    | 729 | 1200 | 3589
@subpage library_foundation             | @copybrief library_foundation             | 27 | 64 | 1866
@subpage library_hashing                | @copybrief library_hashing                | 95 | 293 | 777
@subpage library_http                   | @copybrief library_http                   | 2500 | 8403 | 39697
@subpage library_http_client            | @copybrief library_http_client            | 1696 | 5049 | 7826
@subpage library_memory                 | @copybrief library_memory                 | 554 | 1053 | 2802
@subpage library_plugin                 | @copybrief library_plugin                 | 1293 | 1509 | 11996
@subpage library_process                | @copybrief library_process                | 585 | 1196 | 7571
@subpage library_reflection             | @copybrief library_reflection             | 616 | 0 | 930
@subpage library_serial_port            | @copybrief library_serial_port            | 230 | 634 | 5754
@subpage library_serialization_binary   | @copybrief library_serialization_binary   | 593 | 0 | 1975
@subpage library_serialization_text     | @copybrief library_serialization_text     | 632 | 471 | 2835
@subpage library_socket                 | @copybrief library_socket                 | 549 | 1405 | 3262
@subpage library_strings                | @copybrief library_strings                | 1093 | 3145 | 5662
@subpage library_testing                | @copybrief library_testing                | 155 | 796 | 2679
@subpage library_threading              | @copybrief library_threading              | 565 | 952 | 2610
@subpage library_time                   | @copybrief library_time                   | 133 | 208 | 627
Common source fragments                    | Shared source fragments, not a library       | 1871 | 644 | -

LOC metric                         | Header | Source | Sum
:-----------------------------------|-------:|-------:|----:
Library source                      | 22067 | 54216 | 76283
Common source fragments             | 1871 | 644 | 2515

All LOC counts in the tables above exclude comments.





















![Dependency Graph](https://pagghiu.github.io/images/dependencies/SaneCppLibrariesDependencies.svg)

Some libraries have [C Bindings](@ref group_c_bindings):

Library                                     | Description
:-------------------------------------------|:-----------------------------------------------
@ref group_sc_hashing                       | @copybrief group_sc_hashing
