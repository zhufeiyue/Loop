cmake_minimum_required (VERSION 3.10)

include_directories(${OpenSSLPath}/include)
link_directories(${OpenSSLPath}/lib)

add_executable (Loop  "Loop.cpp")

target_link_libraries(Loop torrent)
target_link_libraries(Loop common)
target_link_libraries(Loop ${Boost_LIBRARIES})
target_link_libraries(Loop libcrypto libssl)