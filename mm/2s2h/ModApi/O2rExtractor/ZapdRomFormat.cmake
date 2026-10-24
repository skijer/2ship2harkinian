# Keep the bundled extractor source intact while selecting MM YAR archives by ROM,
# rather than by the game which linked ZAPD. OoT uses these DMA entries as raw files.
get_target_property(_zapd_rom_sources ZAPDLib SOURCES)
list(FIND _zapd_rom_sources "ZRom.cpp" _zapd_rom_index)
if(_zapd_rom_index LESS 0)
    message(FATAL_ERROR "ZAPD ROM source layout changed; review the runtime ROM format patch")
endif()
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/../ZAPDTR/ZAPD/ZRom.cpp" _zapd_rom_source)
set(_zapd_yar_original "#ifdef GAME_MM\n\t\tif ((i >= 15 && i <= 20) || i == 22)\n\t\t{\n\t\t\tyarCompressed = true;\n\t\t}\n#endif")
string(FIND "${_zapd_rom_source}" "${_zapd_yar_original}" _zapd_yar_index)
if(_zapd_yar_index LESS 0)
    message(FATAL_ERROR "ZAPD YAR format guard changed; review the runtime ROM format patch")
endif()
string(REPLACE "${_zapd_yar_original}"
    "if ((version.crc == MM_NTSC_10 || version.crc == MM_NTSC_10_UNCOMPRESSED || version.crc == MM_NTSC_GC || version.crc == MM_NTSC_JP_GC) && ((i >= 15 && i <= 20) || i == 22)) { yarCompressed = true; }"
    _zapd_rom_source "${_zapd_rom_source}")
set(_zapd_rom_generated "${CMAKE_CURRENT_BINARY_DIR}/ZRomRuntime.cpp")
file(WRITE "${_zapd_rom_generated}" "${_zapd_rom_source}")
list(REMOVE_ITEM _zapd_rom_sources "ZRom.cpp")
list(APPEND _zapd_rom_sources "${_zapd_rom_generated}")
set_property(TARGET ZAPDLib PROPERTY SOURCES "${_zapd_rom_sources}")
target_include_directories(ZAPDLib PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../ZAPDTR/ZAPD")
