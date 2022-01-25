cmake_minimum_required (VERSION 3.12)

set(CMAKE_PREFIX_PATH ${OpenSSLPath})
find_package(OpenSSL REQUIRED)
if( OPENSSL_FOUND )
    include_directories(${OPENSSL_INCLUDE_DIRS})
    message(STATUS "Using OpenSSL ${OPENSSL_VERSION}")
endif()

add_executable (Loop  "Loop.cpp")

target_link_libraries(Loop torrent)
target_link_libraries(Loop common)
target_link_libraries(Loop ${Boost_LIBRARIES})
target_link_libraries(Loop ${OPENSSL_LIBRARIES})