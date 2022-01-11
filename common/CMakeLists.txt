cmake_minimum_required (VERSION 3.10)

aux_source_directory(. CommonSourceFile)
aux_source_directory(./sqlite3 CommonSourceFile)

if(MSVC)
	add_definitions( -bigobj)
endif()

include_directories(${OpenSSLPath}/include)

add_library (common ${CommonSourceFile})
