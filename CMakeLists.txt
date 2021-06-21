cmake_minimum_required (VERSION 3.10)

project ("Loop")

set(CMAKE_CXX_STANDARD 17)

set(OpenSSLPath $ENV{OPENSSL64})
set(BoostPath $ENV{BOOSTLIB})

set(CMAKE_PREFIX_PATH ${BoostPath})
find_package(Boost REQUIRED date_time regex COMPONENTS  REQUIRED)
#for header only lib
include_directories(${Boost_INCLUDE_DIRS})
message(${Boost_VERSION})

include_directories(${PROJECT_SOURCE_DIR})
include_directories(${OpenSSLPath}/include)

add_subdirectory ("common")
add_subdirectory ("torrent")
add_subdirectory ("Loop")