cmake_minimum_required (VERSION 3.12)

add_executable (Loop  "Loop.cpp")

target_link_libraries(Loop torrent)
target_link_libraries(Loop common)
target_link_libraries(Loop ${Boost_LIBRARIES})