cmake_minimum_required(VERSION 3.24)
project(TwoShipMods LANGUAGES C CXX)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/S2HModSdk.cmake")

option(S2H_BUILD_TEMPLATES "Build the SDK templates next to the mods" ON)
set(S2H_MODS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/mods" CACHE PATH "Folder whose mod subfolders are built")

if(S2H_BUILD_TEMPLATES)
    s2h_add_mods_in("${CMAKE_CURRENT_SOURCE_DIR}/templates")
endif()
s2h_add_mods_in("${S2H_MODS_DIR}")
