cmake_minimum_required(VERSION 3.24)
project(S2HSignedMods LANGUAGES C CXX)

set(S2H_SIGNED_MODS_DIR "" CACHE PATH "Folder whose subfolders are the mods submitted for signing")
if(NOT IS_DIRECTORY "${S2H_SIGNED_MODS_DIR}")
    message(FATAL_ERROR "S2H_SIGNED_MODS_DIR must name the submitted mods folder")
endif()

include("${S2H_ROOT}/mod-sdk/cmake/S2HModSdk.cmake")

# Never add_subdirectory: a submitted mod must not run CMake code of its own while it is built for signing.
file(GLOB children LIST_DIRECTORIES true "${S2H_SIGNED_MODS_DIR}/*")
foreach(child IN LISTS children)
    if(EXISTS "${child}/manifest.json")
        s2h_add_mod_directory("${child}")
    endif()
endforeach()
