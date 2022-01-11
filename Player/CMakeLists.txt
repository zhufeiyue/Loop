cmake_minimum_required (VERSION 3.10)

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

set(CMAKE_PREFIX_PATH ${OpenALPath})
find_package(OpenAL REQUIRED)
if(OPENAL_FOUND)
	message("Using " ${OPENAL_INCLUDE_DIR})
	message( "Using " ${OPENAL_LIBRARY})
	include_directories(${OPENAL_INCLUDE_DIR})
endif()

set(PlayerSource main.cpp)
list(APPEND PlayerSource FFmpegDemuxer.cpp FFmpegDemuxer.h FFmpegMuxer.cpp FFmpegMuxer.h FFmpegFilter.h FFmpegFilter.cpp)
list(APPEND PlayerSource DecodeFile.h DecodeFile.cpp IDecoder.cpp IRender.h)
list(APPEND PlayerSource RenderOpenAL.h RenderOpenAL.cpp WavDemuxer.h WavDemuxer.cpp)
list(APPEND PlayerSource AVSync.h AVSync.cpp)
list(APPEND PlayerSource Player.h Player.cpp)
list(APPEND PlayerSource PlayerTimer.h PlayerTimer.cpp)

#aux_source_directory(./view MainQtSource)
set(PlayerQtSource RenderGraphicsView.cpp RenderGraphicsView.h )
list(APPEND PlayerQtSource RenderOpenGLWidget.h RenderOpenGLWidget.cpp)

# WIN32(gui program;need WinMain entry point)
add_executable (player WIN32 ${PlayerSource} ${PlayerQtSource})

target_link_libraries(player common)
target_link_libraries(player Qt5::Core Qt5::Widgets Qt5::Gui Qt5::WebSockets)
target_link_libraries(player avformat avutil avcodec swscale swresample avfilter)
target_link_libraries(player ${Boost_LIBRARIES})
target_link_libraries(player ${OPENAL_LIBRARY})