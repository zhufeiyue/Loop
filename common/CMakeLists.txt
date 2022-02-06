cmake_minimum_required (VERSION 3.12)

aux_source_directory(. CommonSourceFile)
aux_source_directory(./sqlite3 CommonSourceFile)

if(MSVC)
	add_definitions( -bigobj)
endif()

set(CMAKE_PREFIX_PATH ${OpenSSLPath})
find_package(OpenSSL REQUIRED)
if( OPENSSL_FOUND )
    include_directories(${OPENSSL_INCLUDE_DIR})
    message(STATUS "Using OpenSSL ${OPENSSL_VERSION}")
endif()

add_library (common ${CommonSourceFile})
target_link_libraries(common ${OPENSSL_LIBRARIES})
