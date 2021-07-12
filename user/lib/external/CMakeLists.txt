###################################################################################################
# Third party libraries
###################################################################################################
# these have native cmake-based build systems
add_subdirectory(bzip2)
add_subdirectory(lzfse)
add_subdirectory(libpng)
add_subdirectory(libjpeg-turbo)
add_subdirectory(freetype2)
add_subdirectory(pixman)
add_subdirectory(cairo)

###################################################################################################
# string formatting
SET(FMT_OS OFF CACHE BOOL "disable fmtlib OS component" FORCE)
add_subdirectory(fmtlib EXCLUDE_FROM_ALL)

###################################################################################################
# header only libraries
add_subdirectory(HppOnly/tomlplusplus EXCLUDE_FROM_ALL)
add_subdirectory(HppOnly/concurrentqueue EXCLUDE_FROM_ALL)

###################################################################################################
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

###################################################################################################
# zlib
###################################################################################################
add_library(zlib SHARED
    zlib/adler32.c
    zlib/crc32.c
    zlib/deflate.c
    zlib/infback.c
    zlib/inffast.c
    zlib/inflate.c
    zlib/inftrees.c
    zlib/trees.c
    zlib/zutil.c
    zlib/compress.c
    zlib/uncompr.c
    zlib/gzclose.c
    zlib/gzlib.c
    zlib/gzread.c
    zlib/gzwrite.c
)
target_compile_options(zlib PRIVATE -flto -fPIC)
target_compile_options(zlib PRIVATE -Wno-cast-qual -Wno-implicit-fallthrough -Wno-format-nonliteral)
set_target_properties(zlib PROPERTIES OUTPUT_NAME "z")

install(TARGETS zlib LIBRARY)
add_custom_command(TARGET zlib POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/zlib/zlib.h ${SYSROOT_DIR}/usr/include/zlib.h)
add_custom_command(TARGET zlib POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/zlib/zconf.h ${SYSROOT_DIR}/usr/include/zconf.h)

###################################################################################################
# libcrc
###################################################################################################
add_library(libcrc SHARED
    libcrc/src/crc8.c
    libcrc/src/crc16.c
    libcrc/src/crc32.c
    libcrc/src/crcdnp.c
    libcrc/src/crcsick.c
    libcrc/src/crcccitt.c
    libcrc/src/crckrmit.c
    libcrc/src/nmea-chk.c
)
target_compile_options(libcrc PRIVATE -flto -fPIC)
set_target_properties(libcrc PROPERTIES OUTPUT_NAME "crc")
target_include_directories(libcrc PRIVATE libcrc/include)
target_include_directories(libcrc PUBLIC libcrc/include)

install(TARGETS libcrc LIBRARY)
add_custom_command(TARGET libcrc POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/libcrc.so ${SYSROOT_DIR}/boot/lib/libcrc.so)

###################################################################################################
# MessagePack lib
###################################################################################################
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
install(TARGETS mpack_dynamic LIBRARY)
add_library(mpack ALIAS mpack_dynamic)

add_custom_command(TARGET mpack_dynamic POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/libmpack.so ${SYSROOT_DIR}/boot/lib/libmpack.so)

FILE(GLOB MPACK_PUB_HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/mpack/src/mpack/mpack.h")
install(FILES ${MPACK_PUB_HEADERS} DESTINATION ${SYSROOT_DIR}/usr/include/mpack)

