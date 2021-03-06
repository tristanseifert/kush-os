###############################################################################
# Primary CMakeLists for the kush-os host side tools.
#
# This will probably only work when compiling with clang.
###############################################################################
cmake_minimum_required(VERSION 3.7 FATAL_ERROR)
project(kush-tools VERSION 0.1 LANGUAGES C CXX)

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/../cmake;${CMAKE_MODULE_PATH}")

# figure out the directory for tool binaries
SET(TOOLS_BIN_DIR ${CMAKE_CURRENT_LIST_DIR}/bin)
get_filename_component(TOOLS_BIN_DIR ${TOOLS_BIN_DIR} ABSOLUTE)

message("Tool binaries installed to: ${TOOLS_BIN_DIR}")

##### Get version info via git
include(GetGitRevisionDescription)
git_describe(VERSION --tags --dirty=-d)

get_git_head_revision(GIT_REFSPEC GIT_HASH)

string(REGEX REPLACE "^v([0-9]+)\\..*" "\\1" VERSION_MAJOR "${VERSION}")
string(REGEX REPLACE "^v[0-9]+\\.([0-9]+).*" "\\1" VERSION_MINOR "${VERSION}")
string(REGEX REPLACE "^v[0-9]+\\.[0-9]+\\.([0-9]+).*" "\\1" VERSION_PATCH "${VERSION}")
string(REGEX REPLACE "^v[0-9]+\\.[0-9]+\\.[0-9]+(.*)" "\\1" VERSION_SHA1 "${VERSION}")
set(VERSION_SHORT "${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}")

execute_process(COMMAND git describe --tags --dirty=d OUTPUT_VARIABLE VERSION_TAG OUTPUT_STRIP_TRAILING_WHITESPACE)

# C++17 standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

# enable all of the warnings
add_compile_options(-Wall -Wno-format -Wmissing-declarations -Wformat=2 -fdiagnostics-color=always -ftls-model=initial-exec -Wundef -Wcast-qual -Wwrite-strings)

if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
add_compile_options(-Werror -Wimplicit-fallthrough -Wno-deprecated-copy -Wno-address-of-packed-member -Wno-expansion-to-defined)
add_compile_options(-Wno-unused-private-field)
endif()

# include the tools' binaries
add_subdirectory(mkinit)
add_subdirectory(idlc)
