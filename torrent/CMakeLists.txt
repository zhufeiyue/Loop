cmake_minimum_required (VERSION 3.10)

aux_source_directory(.  TorrentSourceFile)

add_library (torrent ${TorrentSourceFile})
