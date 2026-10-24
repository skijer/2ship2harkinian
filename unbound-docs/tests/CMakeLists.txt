cmake_minimum_required(VERSION 3.20)
project(unbound_save_compat LANGUAGES CXX)
get_filename_component(S2H_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
add_executable(unbound_save_compat save_compat.cpp)
target_compile_features(unbound_save_compat PRIVATE cxx_std_20)
target_compile_definitions(unbound_save_compat PRIVATE GBI_FLOAT_MTX GBI_S32_VTX FMT_HEADER_ONLY)
target_include_directories(unbound_save_compat PRIVATE
    "${S2H_ROOT}/mm" "${S2H_ROOT}/mm/include" "${S2H_ROOT}/mm/2s2h"
    "${S2H_ROOT}/libultraship/include" "${S2H_ROOT}/libultraship/include/libultraship"
    "${S2H_ROOT}/libultraship/libgfxd" "${S2H_ROOT}/libultraship/libgfxd/include"
    "${S2H_ROOT}/build/x64/vcpkg/installed/x64-windows-static/include")
if(MSVC)
    target_compile_options(unbound_save_compat PRIVATE /bigobj /Zc:preprocessor /utf-8)
endif()
enable_testing()
add_test(NAME save_compat COMMAND unbound_save_compat)
