﻿cmake_minimum_required (VERSION 3.10)

message(${CMAKE_BUILD_TYPE})

# CMAKE_PREFIX_PATH default is empty
set(CMAKE_PREFIX_PATH ${QtPath})
find_package(Qt5 COMPONENTS Core Gui Widgets Network WebSockets REQUIRED)

# Find includes in corresponding build directories
set(CMAKE_INCLUDE_CURRENT_DIR ON)
# Instruct CMake to run moc automatically when needed
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

include_directories(${FFmpegPath}/include)
link_directories(${FFmpegPath}/lib)

set(PlayerSource main.cpp)
list(APPEND PlayerSource FFmpegDemuxer.cpp FFmpegDemuxer.h FFmpegMuxer.cpp FFmpegMuxer.h)
list(APPEND PlayerSource Player.h Player.cpp DecodeFile.h DecodeFile.cpp IDecoder.cpp)

#aux_source_directory(./view MainQtSource)
set(PlayerQtSource RenderGraphicsView.cpp RenderGraphicsView.h )

# WIN32(gui program;need WinMain entry point)
add_executable (player WIN32 ${PlayerSource} ${PlayerQtSource})

target_link_libraries(player common)
target_link_libraries(player Qt5::Core Qt5::Widgets Qt5::Gui Qt5::WebSockets)
target_link_libraries(player avformat avutil.lib avcodec.lib swscale.lib swresample.lib)
target_link_libraries(player ${Boost_LIBRARIES})