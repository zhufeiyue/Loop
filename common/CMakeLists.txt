cmake_minimum_required (VERSION 3.10)

include_directories(${OpenSSLPath}/include)

aux_source_directory(. CommonSourceFile)

add_definitions( -bigobj)

add_library (common ${CommonSourceFile})
