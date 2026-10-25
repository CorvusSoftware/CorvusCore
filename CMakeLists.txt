# CorvusCore - CorvusSoftware
# Copyright (C) 2026 CorvusSoftware
# SPDX-License-Identifier: AGPL-3.0-only

cmake_minimum_required(VERSION 3.22.1)
project(CorvusCore VERSION 0.14.0 LANGUAGES CXX)

# An unoptimized emulator is unusable: default to Release when this repository
# is the main project and no build type was requested.
get_property(CORVUSCORE_MULTI_CONFIG GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(PROJECT_IS_TOP_LEVEL AND NOT CORVUSCORE_MULTI_CONFIG AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(CORVUSCORE_BUILD_LIBRETRO "Build the three Libretro frontend adapters" ON)
option(CORVUSCORE_BUILD_CORVUSAPI "Build CorvusAPI shared core modules" OFF)
option(CORVUSCORE_INSTALL "Install the engines, the C API and the CMake package" ${PROJECT_IS_TOP_LEVEL})
include(CTest)

add_subdirectory(cores)
add_subdirectory(sdk)


# The public MIT C ABI is independent of CorvusCore's existing SDK.
# Each core is its own shared module and exports corvus_get_api.
if(CORVUSCORE_BUILD_CORVUSAPI)
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/third_party/CorvusAPI/include/corvus/api.h")
        message(FATAL_ERROR "CorvusAPI submodule missing: git submodule update --init --recursive")
    endif()
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
    add_subdirectory(third_party/CorvusAPI)
    foreach(system gb gbc gba)
        if(system STREQUAL "gb")
            set(system_id 0)
        elseif(system STREQUAL "gbc")
            set(system_id 1)
        else()
            set(system_id 2)
        endif()
        add_library(corvus_${system} SHARED sdk/src/corvus_api_adapter.cpp)
        target_compile_definitions(corvus_${system} PRIVATE CORVUS_ADAPTER_SYSTEM=${system_id})
        target_link_libraries(corvus_${system} PRIVATE corvuscore_c_api CorvusAPI::CorvusAPI)
        set_target_properties(corvus_${system} PROPERTIES
            CXX_VISIBILITY_PRESET hidden VISIBILITY_INLINES_HIDDEN YES
            LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
        target_compile_options(corvus_${system} PRIVATE ${CORVUSCORE_WARNING_OPTIONS})
    endforeach()
endif()

if(CORVUSCORE_BUILD_LIBRETRO)
    add_subdirectory(libretro)
endif()

if(CORVUSCORE_INSTALL)
    # find_package(CorvusCore) then link CorvusCore::corvuscore (C++)
    # or CorvusCore::corvuscore_c_api (C ABI). Libretro cores are not installed:
    # they belong in the frontend's own directories.
    include(GNUInstallDirs)
    include(CMakePackageConfigHelpers)
    set(CORVUSCORE_PACKAGE_DIR "${CMAKE_INSTALL_LIBDIR}/cmake/CorvusCore")

    install(TARGETS
            corvuscore_options corvuscore_common gbc_hardware gb_corvus_core gbc_corvus_core
            gba_corvus_core corvuscore corvuscore_c_api
        EXPORT CorvusCoreTargets
        ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        INCLUDES DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
    install(DIRECTORY
            cores/common/include/ cores/gb/include/ cores/gbc/include/ cores/gba/include/
            sdk/include/
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
    install(EXPORT CorvusCoreTargets
        NAMESPACE CorvusCore::
        DESTINATION "${CORVUSCORE_PACKAGE_DIR}")

    configure_package_config_file(cmake/CorvusCoreConfig.cmake.in
        "${CMAKE_CURRENT_BINARY_DIR}/CorvusCoreConfig.cmake"
        INSTALL_DESTINATION "${CORVUSCORE_PACKAGE_DIR}")
    # Pre-1.0: the C ABI and save states may change between minor versions.
    write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/CorvusCoreConfigVersion.cmake"
        COMPATIBILITY SameMinorVersion)
    install(FILES
            "${CMAKE_CURRENT_BINARY_DIR}/CorvusCoreConfig.cmake"
            "${CMAKE_CURRENT_BINARY_DIR}/CorvusCoreConfigVersion.cmake"
        DESTINATION "${CORVUSCORE_PACKAGE_DIR}")
endif()
