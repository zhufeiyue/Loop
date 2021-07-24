cmake_minimum_required (VERSION 3.10)

aux_source_directory(. CommonSourceFile)
aux_source_directory(./sqlite3 CommonSourceFile)

add_definitions( -bigobj)

add_library (common ${CommonSourceFile})
