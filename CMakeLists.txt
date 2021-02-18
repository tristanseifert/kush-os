###############################################################################
# Primary CMakeLists for the kush-os project.
#
# Automatically builds kernel and userland components.
###############################################################################
cmake_minimum_required(VERSION 3.7 FATAL_ERROR)
project(kush VERSION 0.1 LANGUAGES ASM C CXX)

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake;${CMAKE_MODULE_PATH}")

##### Sysroot directory: we install libraries and so forth there
set(SYSROOT_DIR ${CMAKE_CURRENT_BINARY_DIR}/../sysroot CACHE FILEPATH "Sysroot directory" FORCE)
message("Sysroot at ${SYSROOT_DIR}")

##### Version
include(GetGitRevisionDescription)
git_describe(VERSION --tags --dirty=-d)

get_git_head_revision(GIT_REFSPEC GIT_HASH)

string(REGEX REPLACE "^v([0-9]+)\\..*" "\\1" VERSION_MAJOR "${VERSION}")
string(REGEX REPLACE "^v[0-9]+\\.([0-9]+).*" "\\1" VERSION_MINOR "${VERSION}")
string(REGEX REPLACE "^v[0-9]+\\.[0-9]+\\.([0-9]+).*" "\\1" VERSION_PATCH "${VERSION}")
string(REGEX REPLACE "^v[0-9]+\\.[0-9]+\\.[0-9]+(.*)" "\\1" VERSION_SHA1 "${VERSION}")
set(VERSION_SHORT "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}")

execute_process(COMMAND git describe --tags --dirty=d OUTPUT_VARIABLE VERSION_TAG OUTPUT_STRIP_TRAILING_WHITESPACE)

##### Options for what to be compiled
option(BUILD_KERNEL "Build the kernel" ON)
option(BUILD_USER_LIBS "Build userspace libraries" ON)
option(BUILD_SYSTEM_SERVERS "Build core system servers" ON)

##### General defines
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

#set(CMAKE_VERBOSE_MAKEFILE ON)

# configure warnings
add_compile_options(-Wall -Wno-format -Wmissing-declarations -Wformat=2 -fdiagnostics-color=always -ftls-model=initial-exec -Wundef -Wcast-qual -Wwrite-strings)

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
add_compile_options(-Werror -Wimplicit-fallthrough -Wno-deprecated-copy -Wno-address-of-packed-member -Wno-expansion-to-defined)
add_compile_options(-Wno-unused-private-field)
endif()

####################
##### Use cross toolchain for all targets below

##### Specify options for kernel, free-standing userspace components
add_compile_options(-g1 -fstack-protector-strong -Wundef -Wcast-qual -Wwrite-strings)

##### Kernel
if(BUILD_KERNEL)
    add_subdirectory(kernel)
endif()

##### Userspace libraries
if(BUILD_USER_LIBS)
    add_subdirectory(user/libc)
endif()

##### System servers (init, drivers, core system services)
if(BUILD_SYSTEM_SERVERS)
    add_subdirectory(user/rootsrv)
endif()
