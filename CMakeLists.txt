cmake_minimum_required (VERSION 3.12)

project ("Loop")

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_PREFIX_PATH ${BoostPath})
find_package(Boost REQUIRED date_time regex COMPONENTS  REQUIRED)
message(${Boost_VERSION})
include_directories(${Boost_INCLUDE_DIRS})

include_directories(${PROJECT_SOURCE_DIR})

add_subdirectory ("common")
add_subdirectory ("torrent")
add_subdirectory ("Loop")
add_subdirectory ("Player")