include_guard(GLOBAL)

get_filename_component(S2H_MOD_SDK_DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
if(NOT S2H_ROOT)
    get_filename_component(S2H_ROOT "${S2H_MOD_SDK_DIRECTORY}/.." ABSOLUTE)
endif()
set(S2H_ROOT "${S2H_ROOT}" CACHE PATH "2Ship source checkout the mods compile against")
set(S2H_IMPORT_LIBRARY "${S2H_ROOT}/build/x64/mm/$<CONFIG>/2ship.lib" CACHE STRING
    "Windows only: 2ship.lib, the import library of the 2ship.exe the mods will load into")
set(S2H_GAME_DIR "" CACHE PATH "Game folder; when set, every built mod is copied into its mods/ folder")

find_package(Python3 REQUIRED COMPONENTS Interpreter)
file(STRINGS "${S2H_ROOT}/mm/2s2h/ModApi/ModApi.h" modApiMajorLine REGEX "^#define S2H_MOD_API_MAJOR [0-9]+$")
string(REGEX REPLACE "^#define S2H_MOD_API_MAJOR " "" S2H_MODAPI_MAJOR "${modApiMajorLine}")
if(NOT S2H_MODAPI_MAJOR MATCHES "^[0-9]+$")
    message(FATAL_ERROR "ModApi.h does not define S2H_MOD_API_MAJOR")
endif()

if(WIN32)
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(S2H_MOD_PLATFORM windows_x64)
    else()
        set(S2H_MOD_PLATFORM windows_x86)
    endif()
elseif(APPLE)
    set(S2H_MOD_PLATFORM darwin)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|amd64|AMD64)$")
    set(S2H_MOD_PLATFORM linux_x64)
else()
    message(FATAL_ERROR "Unsupported native mod target: ${CMAKE_SYSTEM_NAME}/${CMAKE_SYSTEM_PROCESSOR}")
endif()

if(NOT TARGET s2h_mod_sdk)
    add_library(s2h_mod_sdk INTERFACE)
    target_include_directories(s2h_mod_sdk INTERFACE
        "${S2H_ROOT}/mm"
        "${S2H_ROOT}/mm/2s2h"
        "${S2H_ROOT}/mm/include"
        "${S2H_ROOT}/mm/src"
        "${S2H_ROOT}/mm/assets"
        "${S2H_ROOT}/libultraship/include"
        "${S2H_ROOT}/libultraship/include/libultraship"
    )
    # Every add_compile_definitions of the game build that changes a shared struct belongs here too, or the mod
    # sees another layout (CONTROLLERBUTTONS_T once left every mod reading the stick out of the button mask).
    target_compile_definitions(s2h_mod_sdk INTERFACE
        F3DEX_GBI_2
        NOMINMAX
        S2H_MOD
        CONTROLLERBUTTONS_T=uint32_t
        GBI_FLOAT_MTX=1
        GBI_S32_VTX=1
    )
    target_compile_features(s2h_mod_sdk INTERFACE c_std_11 cxx_std_20)
    if(MSVC)
        target_compile_options(s2h_mod_sdk INTERFACE $<$<COMPILE_LANGUAGE:CXX>:/Zc:preprocessor> /permissive- /bigobj)
    else()
        target_compile_options(s2h_mod_sdk INTERFACE
            $<$<COMPILE_LANGUAGE:C>:-Werror=implicit-function-declaration>
            $<$<COMPILE_LANGUAGE:C>:-Wno-int-conversion>
            $<$<COMPILE_LANGUAGE:C>:-Wno-incompatible-function-pointer-types>
            $<$<COMPILE_LANGUAGE:CXX>:-fpermissive>
        )
    endif()
    if(WIN32)
        target_link_libraries(s2h_mod_sdk INTERFACE "${S2H_IMPORT_LIBRARY}")
    elseif(APPLE)
        target_link_options(s2h_mod_sdk INTERFACE "LINKER:-undefined,dynamic_lookup")
    endif()

    file(GLOB helperDirectories LIST_DIRECTORIES true "${S2H_MOD_SDK_DIRECTORY}/include/*")
    set(S2H_MOD_HELPERS "")
    foreach(helperDirectory IN LISTS helperDirectories)
        if(IS_DIRECTORY "${helperDirectory}")
            get_filename_component(helper "${helperDirectory}" NAME)
            add_library(${helper} INTERFACE)
            target_link_libraries(${helper} INTERFACE s2h_mod_sdk)
            target_include_directories(${helper} INTERFACE "${helperDirectory}")
            list(APPEND S2H_MOD_HELPERS ${helper})
        endif()
    endforeach()
    set(S2H_MOD_HELPERS "${S2H_MOD_HELPERS}" CACHE INTERNAL "Header-only helper libraries of the SDK")
endif()

function(s2h_add_mod name)
    cmake_parse_arguments(MOD "" "DIRECTORY" "SOURCES;LIBRARIES" ${ARGN})
    if(NOT MOD_DIRECTORY OR NOT MOD_SOURCES)
        message(FATAL_ERROR "s2h_add_mod(${name}) requires DIRECTORY and SOURCES")
    endif()
    if(NOT EXISTS "${MOD_DIRECTORY}/manifest.json")
        message(FATAL_ERROR "s2h_add_mod(${name}): ${MOD_DIRECTORY}/manifest.json is missing")
    endif()

    set(package "${CMAKE_BINARY_DIR}/packages/$<CONFIG>/${name}.o2r")
    add_library(${name} MODULE ${MOD_SOURCES})
    target_link_libraries(${name} PRIVATE s2h_mod_sdk ${MOD_LIBRARIES})
    set_target_properties(${name} PROPERTIES PREFIX "" C_VISIBILITY_PRESET hidden CXX_VISIBILITY_PRESET hidden)
    add_custom_command(TARGET ${name} POST_BUILD
        COMMAND Python3::Interpreter "${S2H_MOD_SDK_DIRECTORY}/tools/pack_mod.py"
            --mod-directory "${MOD_DIRECTORY}"
            --binary "$<TARGET_FILE:${name}>"
            --platform "${S2H_MOD_PLATFORM}"
            --output "${package}"
            --modapi-major "${S2H_MODAPI_MAJOR}"
        VERBATIM
    )
    if(S2H_GAME_DIR)
        add_custom_command(TARGET ${name} POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E make_directory "${S2H_GAME_DIR}/mods"
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${package}" "${S2H_GAME_DIR}/mods/${name}.o2r"
            VERBATIM
        )
    endif()
endfunction()

function(s2h_add_mod_directory directory)
    get_filename_component(name "${directory}" NAME)
    file(GLOB_RECURSE sources CONFIGURE_DEPENDS "${directory}/*.c" "${directory}/*.cpp")
    list(FILTER sources EXCLUDE REGEX "/assets/")
    list(FILTER sources EXCLUDE REGEX "\\.inc\\.c$")
    if(NOT sources)
        message(FATAL_ERROR "Mod ${name} in ${directory} has no .c or .cpp sources")
    endif()
    s2h_add_mod(${name} DIRECTORY "${directory}" SOURCES ${sources} LIBRARIES ${S2H_MOD_HELPERS})
    # translate_mod.py copies the OoT headers MM lacks here.
    if(IS_DIRECTORY "${directory}/oot_include")
        target_include_directories(${name} PRIVATE "${directory}/oot_include")
    endif()
endfunction()

function(s2h_add_mods_in parent)
    file(GLOB children LIST_DIRECTORIES true "${parent}/*")
    foreach(child IN LISTS children)
        if(EXISTS "${child}/CMakeLists.txt")
            get_filename_component(childName "${child}" NAME)
            add_subdirectory("${child}" "${CMAKE_BINARY_DIR}/mods/${childName}")
        elseif(EXISTS "${child}/manifest.json")
            s2h_add_mod_directory("${child}")
        endif()
    endforeach()
endfunction()
