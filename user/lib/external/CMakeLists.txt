###############################################################################
# Third party libraries
###############################################################################

# these have native cmake-based build systems
add_subdirectory(bzip2)
add_subdirectory(lzfse)

# Cap'n Proto: build a minimal library (without reflection) for now
set(EXTERNAL_CAPNP ON CACHE BOOL "" FORCE)
set(CAPNP_LITE ON CACHE BOOL "" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
add_subdirectory(capnproto/c++)

# string formatting
SET(FMT_OS OFF CACHE BOOL "disable fmtlib OS component" FORCE)
add_subdirectory(fmtlib EXCLUDE_FROM_ALL)

# header only libraries
add_subdirectory(HppOnly/tomlplusplus EXCLUDE_FROM_ALL)

# OpenLibm (math library)
ExternalProject_Add(openlibm
    SOURCE_DIR ${CMAKE_CURRENT_LIST_DIR}/openlibm
    INSTALL_DIR ${SYSROOT_DIR}
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ${CMAKE_COMMAND} -E env
        AR=${CMAKE_AR}
        CC=${CMAKE_C_COMPILER}
        LD=${CMAKE_C_COMPILER}
        LDFLAGS="--target=${TARGET_TRIPLE} --sysroot=${CMAKE_SYSROOT}"
        CFLAGS="--target=${TARGET_TRIPLE} --sysroot=${CMAKE_SYSROOT}"
        SFLAGS="--target=${TARGET_TRIPLE} --sysroot=${CMAKE_SYSROOT}"
        make -C ${CMAKE_CURRENT_SOURCE_DIR}/openlibm
    BUILD_IN_SOURCE TRUE
    INSTALL_COMMAND make DESTDIR=${CMAKE_SYSROOT} libdir=/lib includedir=/usr/include install
)

# MessagePack lib
add_library(mpack_objs OBJECT
    mpack/src/mpack/mpack-common.c
    mpack/src/mpack/mpack-expect.c
    mpack/src/mpack/mpack-node.c
    mpack/src/mpack/mpack-platform.c
    mpack/src/mpack/mpack-reader.c
    mpack/src/mpack/mpack-writer.c
)
target_include_directories(mpack_objs PRIVATE mpack/src/mpack)
target_compile_options(mpack_objs PRIVATE -flto -fPIC)


add_library(mpack_static STATIC $<TARGET_OBJECTS:mpack_objs>)
set_target_properties(mpack_static PROPERTIES OUTPUT_NAME "mpack")
target_include_directories(mpack_static PUBLIC mpack/src/)

add_library(mpack_dynamic SHARED $<TARGET_OBJECTS:mpack_objs>)
set_target_properties(mpack_dynamic PROPERTIES OUTPUT_NAME "mpack")
target_include_directories(mpack_dynamic PUBLIC mpack/src/)

# install the library to the sysroot
install(TARGETS mpack_static LIBRARY)
install(TARGETS mpack_dynamic LIBRARY)

add_custom_command(TARGET mpack_dynamic POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/libmpack.so ${SYSROOT_DIR}/boot/lib/libmpack.so)

FILE(GLOB MPACK_PUB_HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/mpack/src/mpack/mpack.h")
install(FILES ${MPACK_PUB_HEADERS} DESTINATION ${SYSROOT_DIR}/usr/include/mpack)

