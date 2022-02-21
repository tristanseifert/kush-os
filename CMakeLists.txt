###############################################################################
# Primary CMakeLists for the kush-os project.
#
# Automatically builds kernel and userland components.
###############################################################################
cmake_minimum_required(VERSION 3.7 FATAL_ERROR)
project(kush VERSION 2.0 LANGUAGES ASM C CXX)

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake;${CMAKE_MODULE_PATH}")

### required modules
include(ExternalProject)

##### Sysroot directory: we install libraries and so forth there
set(SYSROOT_DIR ${CMAKE_CURRENT_LIST_DIR}/sysroot CACHE FILEPATH "Sysroot directory" FORCE)
message("Sysroot at ${SYSROOT_DIR}")

set(CMAKE_INSTALL_PREFIX ${SYSROOT_DIR})
set(CMAKE_INSTALL_INCLUDEDIR "/Local/Includes")

##### Get build version (based on git tags)
execute_process(
    COMMAND git rev-parse --abbrev-ref HEAD
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_COMMIT_BRANCH
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
execute_process(
    COMMAND git log -1 --format=%h
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_COMMIT_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE
)
cmake_host_system_information(RESULT BUILD_HOSTNAME QUERY FQDN)
set(BUILD_USERNAME $ENV{USER})

##### Top level options for what to be compiled
option(BUILD_CORE "Build core system" ON)

##### General defines
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# configure warnings
add_compile_options(-Wall -Wno-format -Wmissing-declarations -Wformat=2 -fdiagnostics-color=always -ftls-model=initial-exec -Wundef -Wcast-qual -Wwrite-strings)

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    add_compile_options(-Werror -Wimplicit-fallthrough -Wno-deprecated-copy -Wno-address-of-packed-member -Wno-expansion-to-defined -Wno-undef)
add_compile_options(-Wno-unused-private-field)
endif()

####################
##### Specify options for kernel, free-standing userspace components
add_compile_options(-g1 -fstack-protector-strong -Wundef -Wcast-qual -Wwrite-strings)

if(BUILD_CORE)
    add_subdirectory(Core)
endif()
