cmake_minimum_required(VERSION 3.26.0 FATAL_ERROR)

set(CMAKE_SYSTEM_VERSION 10.0 CACHE STRING "" FORCE)
set(CMAKE_CXX_STANDARD 20 CACHE STRING "The C++ standard to use")

set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "Minimum OS X deployment version")
set(GAME_STR "MM")
project(2s2h VERSION 5.0.1 LANGUAGES C CXX)
include(CMake/2ship-cvars.cmake)
include(CMake/lus-cvars.cmake)
set(SPDLOG_LEVEL_TRACE 0)
set(SPDLOG_LEVEL_OFF 6)
set(SPDLOG_MIN_CUTOFF SPDLOG_LEVEL_TRACE CACHE STRING "cutoff at trace")
set(PROJECT_BUILD_NAME "Battler Bravo" CACHE STRING "" FORCE)
# 2S2H [Unbound] Bump when cutting a release: the tag is "${PROJECT_VERSION}-${PROJECT_FORK_VERSION}"
set(PROJECT_FORK_VERSION "unbound0.1" CACHE STRING "" FORCE)
set(PROJECT_TEAM "github.com/harbourmasters" CACHE STRING "" FORCE)

execute_process(
    COMMAND git branch --show-current
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_BRANCH
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

set(CMAKE_PROJECT_GIT_BRANCH "${GIT_BRANCH}" CACHE STRING "Git branch" FORCE)

execute_process(
    COMMAND git rev-parse HEAD
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_COMMIT_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

# Get only the first 7 characters of the hash
string(SUBSTRING "${GIT_COMMIT_HASH}" 0 7 SHORT_COMMIT_HASH)

set(CMAKE_PROJECT_GIT_COMMIT_HASH "${SHORT_COMMIT_HASH}" CACHE STRING "Git commit hash" FORCE)

execute_process(
    COMMAND git describe --tags --abbrev=0 --exact-match HEAD
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_COMMIT_TAG
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

if(NOT GIT_COMMIT_TAG)
    set(GIT_COMMIT_TAG "" CACHE STRING "Git commit tag" FORCE)
endif()

set(CMAKE_PROJECT_GIT_COMMIT_TAG "${GIT_COMMIT_TAG}" CACHE STRING "Git commit tag" FORCE)

add_compile_options($<$<CXX_COMPILER_ID:MSVC>:/MP>)
add_compile_options($<$<CXX_COMPILER_ID:MSVC>:/utf-8>)

if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    if (CMAKE_C_COMPILER_LAUNCHER MATCHES "ccache|sccache")
        set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT Embedded)
    endif()
endif()

if (CMAKE_SYSTEM_NAME MATCHES "Windows|Linux")
    if(NOT DEFINED BUILD_CROWD_CONTROL)
        set(BUILD_CROWD_CONTROL OFF)
    endif()
endif()

# Enable the Gfx debugger in LUS to use libgfxd from ZAPDTR
set(GFX_DEBUG_DISASSEMBLER ON)

# Tell LUS we're using F3DEX_GBI_2 (in a way that doesn't break libgfxd)
set(GBI_UCODE F3DEX_GBI_2)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/CMake")

# Enable MPQ and OTR support
set(INCLUDE_MPQ_SUPPORT ON)

################################################################################
# Set target arch type if empty. Visual studio solution generator provides it.
################################################################################
if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    if(NOT CMAKE_VS_PLATFORM_NAME)
        set(CMAKE_VS_PLATFORM_NAME "x64")
    endif()
    message("${CMAKE_VS_PLATFORM_NAME} architecture in use")

    if(NOT ("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "x64"
        OR "${CMAKE_VS_PLATFORM_NAME}" STREQUAL "Win32"))
        message(FATAL_ERROR "${CMAKE_VS_PLATFORM_NAME} arch is not supported!")
    endif()
endif()

################################################################################
# Game module: on desktop the game is a shared library next to a thin launcher, sharing one
# libultraship with its mods and with Fleet, which loads Ship and 2Ship into one process.
################################################################################
if (CMAKE_SYSTEM_NAME MATCHES "Windows|Linux|Darwin")
    option(S2H_SHARED_GAME_MODULE "Build the game as a shared library loaded by a thin launcher" ON)
else()
    set(S2H_SHARED_GAME_MODULE OFF)
endif()
set(LUS_BUILD_SHARED ${S2H_SHARED_GAME_MODULE})
# Static libraries (ZAPD, OTRExporter...) are linked into shared modules.
if (S2H_SHARED_GAME_MODULE)
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
endif()
# The game target keeps the name "2ship" so mods keep linking against 2ship.lib.
if (S2H_SHARED_GAME_MODULE)
    set(S2H_LAUNCHER_TARGET 2ship-launcher)
else()
    set(S2H_LAUNCHER_TARGET 2ship)
endif()
set_property(DIRECTORY ${CMAKE_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${S2H_LAUNCHER_TARGET})

# Modules that exchange C++ objects must share one CRT heap.
if (MSVC AND S2H_SHARED_GAME_MODULE)
    set(S2H_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
    set(CMAKE_MSVC_RUNTIME_LIBRARY ${S2H_MSVC_RUNTIME_LIBRARY})
endif()

# Subprojects that pin the static CRT on their own targets are moved back to the shared one.
function(s2h_use_shared_crt_in directory)
    if (NOT S2H_MSVC_RUNTIME_LIBRARY)
        return()
    endif()
    get_property(targets DIRECTORY "${directory}" PROPERTY BUILDSYSTEM_TARGETS)
    foreach(target IN LISTS targets)
        get_target_property(type ${target} TYPE)
        if (NOT type MATCHES "INTERFACE_LIBRARY|UTILITY")
            set_target_properties(${target} PROPERTIES MSVC_RUNTIME_LIBRARY ${S2H_MSVC_RUNTIME_LIBRARY})
        endif()
    endforeach()
endfunction()

################################################################################
# VCPKG dependencies for Windows
################################################################################
if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
include(CMake/automate-vcpkg.cmake)

# "-static-md": static libraries built against the shared CRT.
if (S2H_SHARED_GAME_MODULE)
    set(S2H_VCPKG_CRT_SUFFIX "-md")
endif()
if ("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "Win32")
	set(VCPKG_TRIPLET x86-windows-static${S2H_VCPKG_CRT_SUFFIX})
	set(VCPKG_TARGET_TRIPLET x86-windows-static${S2H_VCPKG_CRT_SUFFIX})
elseif ("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "x64")
	set(VCPKG_TRIPLET x64-windows-static${S2H_VCPKG_CRT_SUFFIX})
	set(VCPKG_TARGET_TRIPLET x64-windows-static${S2H_VCPKG_CRT_SUFFIX})
endif()

vcpkg_bootstrap()
vcpkg_install_packages(zlib bzip2 libzip libpng sdl2 sdl2-net glew glfw3 nlohmann-json tinyxml2 spdlog libogg libvorbis opus opusfile)
endif()

################################################################################
# Global configuration types
################################################################################
if (CMAKE_SYSTEM_NAME STREQUAL "NintendoSwitch")
    set(CMAKE_C_FLAGS_DEBUG "-g -ffast-math -DDEBUG")
    set(CMAKE_CXX_FLAGS_DEBUG "-g -ffast-math -DDEBUG")
    set(CMAKE_C_FLAGS_RELEASE "-O3 -ffast-math -DNDEBUG")
    set(CMAKE_CXX_FLAGS_RELEASE "-O3 -ffast-math -DNDEBUG")
elseif(MSVC)
    # We need to remove RTC1 as it is not compatible with O2, which we set for some files in DEBUG
    string(REGEX REPLACE "/RTC(su|[1su])" "" CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG}")
    string(REGEX REPLACE "/RTC(su|[1su])" "" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
    set(CMAKE_C_FLAGS_RELEASE "/O2 /DNDEBUG")
    set(CMAKE_CXX_FLAGS_RELEASE "/O2 /DNDEBUG")
else()
    set(CMAKE_C_FLAGS_RELEASE "-O2 -DNDEBUG")
    set(CMAKE_CXX_FLAGS_RELEASE "-O2 -DNDEBUG")
    set(CMAKE_OBJCXX_FLAGS_RELEASE "-O2 -DNDEBUG")
endif()

if(NOT CMAKE_BUILD_TYPE )
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Choose the type of build." FORCE)
endif()

################################################################################
# Common utils
################################################################################
include(CMake/Utils.cmake)

if(CMAKE_SYSTEM_NAME MATCHES "Linux")
    get_linux_lsb_release_information()
    message(STATUS "Linux ${LSB_RELEASE_ID_SHORT} ${LSB_RELEASE_VERSION_SHORT} ${LSB_RELEASE_CODENAME_SHORT}")
else()
    message(STATUS ${CMAKE_SYSTEM_NAME})
endif()

################################################################################
# Additional Global Settings(add specific info there)
################################################################################
include(CMake/GlobalSettingsInclude.cmake OPTIONAL)

################################################################################
# Use solution folders feature
################################################################################
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

################################################################################
# Set CONTROLLERBUTTONS_T
################################################################################
add_compile_definitions(CONTROLLERBUTTONS_T=uint32_t)

# 2S2H [Unbound] float matrices: world positions past +/-32767 need a Mtx that does not wrap
set(GBI_FLOAT_MTX ON)

# 2S2H [Unbound] s32 vertex positions: room meshes draw under the identity matrix, so s16 capped one room at 65535 units
set(GBI_S32_VTX ON)

################################################################################
# Sub-projects
################################################################################
add_subdirectory(libultraship ${CMAKE_BINARY_DIR}/libultraship)
target_compile_definitions(libultraship PUBLIC INCLUDE_MPQ_SUPPORT)

# LUS ships no PCH of its own and these headers dominate its build. Done here rather than in the
# submodule, and PRIVATE, so nothing about its public interface changes.
if (CMAKE_CXX_COMPILER_ID MATCHES "Clang|AppleClang|GNU|MSVC")
    target_precompile_headers(libultraship PRIVATE
        "$<$<COMPILE_LANGUAGE:CXX>:<ship/Context.h$<ANGLE-R>>"
        "$<$<COMPILE_LANGUAGE:CXX>:<ship/resource/ResourceManager.h$<ANGLE-R>>"
        "$<$<COMPILE_LANGUAGE:CXX>:<ship/window/Window.h$<ANGLE-R>>"
        "$<$<COMPILE_LANGUAGE:CXX>:<spdlog/spdlog.h$<ANGLE-R>>"
    )
    # Clang replays a PCH's pending template instantiations in every consuming TU unless told to do
    # the work up front. MSVC's PCH already behaves this way.
    if (CMAKE_CXX_COMPILER_ID MATCHES "Clang|AppleClang")
        target_compile_options(libultraship PRIVATE
            $<$<COMPILE_LANGUAGE:CXX>:-fpch-instantiate-templates>
        )
    endif()
endif()

add_subdirectory(ZAPDTR/ZAPD ${CMAKE_BINARY_DIR}/ZAPD)
s2h_use_shared_crt_in(ZAPDTR/ZAPD)
add_subdirectory(OTRExporter)
s2h_use_shared_crt_in(OTRExporter/OTRExporter)
# OTRExporter's archive headers #undef _DLL, which stamps their objects as static CRT (LNK2038).
# Including the CRT first fixes the runtime choice before that line runs.
if (S2H_MSVC_RUNTIME_LIBRARY)
    target_compile_options(OTRExporter PRIVATE /FIyvals.h)
endif()
add_subdirectory(mm)

set_property(TARGET ${S2H_LAUNCHER_TARGET} PROPERTY APPIMAGE_DESKTOP_FILE_TERMINAL YES)
set_property(TARGET ${S2H_LAUNCHER_TARGET} PROPERTY APPIMAGE_DESKTOP_FILE "${CMAKE_SOURCE_DIR}/mm/linux/2s2h.desktop")
set_property(TARGET ${S2H_LAUNCHER_TARGET} PROPERTY APPIMAGE_ICON_FILE "${CMAKE_BINARY_DIR}/2s2hIcon.png")

if("${CMAKE_SYSTEM_NAME}" STREQUAL "Linux")
install(FILES "${CMAKE_BINARY_DIR}/mm/2ship.o2r" DESTINATION . COMPONENT ship)
install(TARGETS ZAPD DESTINATION ./assets/extractor COMPONENT extractor)
install(DIRECTORY "${CMAKE_SOURCE_DIR}/mm/assets/extractor/" DESTINATION ./assets/ COMPONENT extractor)
install(DIRECTORY "${CMAKE_SOURCE_DIR}/mm/assets/xml/" DESTINATION ./assets/xml COMPONENT extractor)
endif()

if ("${CMAKE_SYSTEM_NAME}" STREQUAL "Windows")
install(DIRECTORY "${CMAKE_SOURCE_DIR}/mm/assets/extractor/" DESTINATION ./assets/ COMPONENT 2s2h)
install(DIRECTORY "${CMAKE_SOURCE_DIR}/mm/assets/xml/" DESTINATION ./assets/xml COMPONENT 2s2h)
endif()

find_package(Python3 COMPONENTS Interpreter)

# Target to generate OTRs
add_custom_target(
    ExtractAssets
    # CMake versions prior to 3.17 do not have the rm command, use remove instead for older versions
    COMMAND ${CMAKE_COMMAND} -E $<IF:$<VERSION_LESS:${CMAKE_VERSION},3.17>,remove,rm> -f mm.zip 2ship.o2r
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/OTRExporter/extract_assets.py -z "$<TARGET_FILE:ZAPD>" --non-interactive --xml-root ../mm/assets/xml --custom-otr-file 2ship.o2r "--custom-assets-path" ${CMAKE_CURRENT_SOURCE_DIR}/mm/assets/custom --port-ver "${CMAKE_PROJECT_VERSION}"
    COMMAND ${CMAKE_COMMAND} -DSYSTEM_NAME=${CMAKE_SYSTEM_NAME} -DTARGET_DIR="$<TARGET_FILE_DIR:ZAPD>" -DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR} -DBINARY_DIR=${CMAKE_BINARY_DIR} -P ${CMAKE_CURRENT_SOURCE_DIR}/CMake/copy-existing-otrs.cmake
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/mm
    COMMENT "Running asset extraction..."
    DEPENDS ZAPD
    BYPRODUCTS mm.o2r ${CMAKE_SOURCE_DIR}/mm.o2r ${CMAKE_SOURCE_DIR}/2ship.o2r
)

# Target to generate headers
add_custom_target(
    ExtractAssetHeaders
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/OTRExporter/extract_assets.py -z "$<TARGET_FILE:ZAPD>" --non-interactive --xml-root ../mm/assets/xml --gen-headers
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/mm
    COMMENT "Generating asset headers..."
    DEPENDS ZAPD
)

# Target to generate only 2ship.o2r
add_custom_target(
    Generate2ShipOtr
    # CMake versions prior to 3.17 do not have the rm command, use remove instead for older versions
    COMMAND ${CMAKE_COMMAND} -E $<IF:$<VERSION_LESS:${CMAKE_VERSION},3.17>,remove,rm> -f 2ship.o2r
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/OTRExporter/extract_assets.py -z "$<TARGET_FILE:ZAPD>" --norom --custom-otr-file 2ship.o2r "--custom-assets-path" ${CMAKE_CURRENT_SOURCE_DIR}/mm/assets/custom --port-ver "${CMAKE_PROJECT_VERSION}"
    COMMAND ${CMAKE_COMMAND} -DSYSTEM_NAME=${CMAKE_SYSTEM_NAME} -DTARGET_DIR="$<TARGET_FILE_DIR:ZAPD>" -DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR} -DBINARY_DIR=${CMAKE_BINARY_DIR} -DONLY2SHIPOTR=On -P ${CMAKE_CURRENT_SOURCE_DIR}/CMake/copy-existing-otrs.cmake
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/mm
    COMMENT "Generating 2ship.o2r..."
    DEPENDS ZAPD
)

if(CMAKE_SYSTEM_NAME MATCHES "Linux")
file(COPY ${CMAKE_SOURCE_DIR}/mm/linux/2s2hIcon.png DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
endif()

if(CMAKE_SYSTEM_NAME MATCHES "Darwin")
add_custom_target(CreateOSXIcons
   COMMAND mkdir -p ${CMAKE_BINARY_DIR}/macosx/2s2h.iconset
   COMMAND sips -z 16 16     mm/macosx/2s2hIcon.png --out ${CMAKE_BINARY_DIR}/macosx/2s2h.iconset/icon_16x16.png
   COMMAND sips -z 32 32     mm/macosx/2s2hIcon.png --out ${CMAKE_BINARY_DIR}/macosx/2s2h.iconset/icon_16x16@2x.png
   COMMAND sips -z 32 32     mm/macosx/2s2hIcon.png --out ${CMAKE_BINARY_DIR}/macosx/2s2h.iconset/icon_32x32.png
   COMMAND sips -z 64 64     mm/macosx/2s2hIcon.png --out ${CMAKE_BINARY_DIR}/macosx/2s2h.iconset/icon_32x32@2x.png
   COMMAND sips -z 128 128   mm/macosx/2s2hIcon.png --out ${CMAKE_BINARY_DIR}/macosx/2s2h.iconset/icon_128x128.png
   COMMAND sips -z 256 256   mm/macosx/2s2hIcon.png --out ${CMAKE_BINARY_DIR}/macosx/2s2h.iconset/icon_128x128@2x.png
   COMMAND sips -z 256 256   mm/macosx/2s2hIcon.png --out ${CMAKE_BINARY_DIR}/macosx/2s2h.iconset/icon_256x256.png
   COMMAND sips -z 512 512   mm/macosx/2s2hIcon.png --out ${CMAKE_BINARY_DIR}/macosx/2s2h.iconset/icon_256x256@2x.png
   COMMAND sips -z 512 512   mm/macosx/2s2hIcon.png --out ${CMAKE_BINARY_DIR}/macosx/2s2h.iconset/icon_512x512.png
   COMMAND cp                mm/macosx/2s2hIcon.png ${CMAKE_BINARY_DIR}/macosx/2s2h.iconset/icon_512x512@2x.png
   COMMAND iconutil -c icns -o ${CMAKE_BINARY_DIR}/macosx/2s2h.icns ${CMAKE_BINARY_DIR}/macosx/2s2h.iconset
   WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
   COMMENT "Creating OSX icons ..."
   )
add_dependencies(${S2H_LAUNCHER_TARGET} CreateOSXIcons)

install(TARGETS ZAPD DESTINATION ${CMAKE_BINARY_DIR}/assets/extractor)

set(PROGRAM_PERMISSIONS_EXECUTE OWNER_EXECUTE OWNER_WRITE OWNER_READ GROUP_EXECUTE GROUP_READ WORLD_EXECUTE WORLD_READ)

install(DIRECTORY "${CMAKE_SOURCE_DIR}/mm/assets/extractor/" DESTINATION ./assets/)
install(DIRECTORY "${CMAKE_SOURCE_DIR}/mm/assets/xml/" DESTINATION ./assets/xml)

# Rename the installed soh binary to drop the macos suffix
INSTALL(CODE "FILE(RENAME \${CMAKE_INSTALL_PREFIX}/../MacOS/2s2h-macos \${CMAKE_INSTALL_PREFIX}/../MacOS/2s2h)")
install(CODE "
   include(BundleUtilities)
  fixup_bundle(\"\${CMAKE_INSTALL_PREFIX}/../MacOS/2s2h\" \"\" \"${dirs}\")
   ")

endif()

if (MSVC AND S2H_SHARED_GAME_MODULE)
    # The shared CRT ships beside the game so players need no Visual C++ redistributable.
    set(CMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION .)
    set(CMAKE_INSTALL_SYSTEM_RUNTIME_COMPONENT 2s2h)
    include(InstallRequiredSystemLibraries)
endif()

if(CMAKE_SYSTEM_NAME MATCHES "Windows|NintendoSwitch|CafeOS")
install(FILES ${CMAKE_SOURCE_DIR}/README.md DESTINATION . COMPONENT 2s2h RENAME readme.txt )
install(CODE "file(MAKE_DIRECTORY \"\${CMAKE_INSTALL_PREFIX}/mods\")" COMPONENT 2s2h)
install(CODE "file(TOUCH \"\${CMAKE_INSTALL_PREFIX}/mods/custom_mod_files_go_here.txt\")" COMPONENT 2s2h)
endif()

if(CMAKE_SYSTEM_NAME MATCHES "Linux")
    set(CPACK_GENERATOR "External")
elseif(CMAKE_SYSTEM_NAME MATCHES "Windows|NintendoSwitch|CafeOS")
    set(CPACK_GENERATOR "ZIP")
elseif(CMAKE_SYSTEM_NAME MATCHES "Darwin")
    set(CPACK_GENERATOR "Bundle")
endif()

set(CPACK_PROJECT_CONFIG_FILE ${CMAKE_SOURCE_DIR}/CMake/Packaging-2.cmake)
include(CMake/Packaging.cmake)
