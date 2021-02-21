###############################################################################
# Third party libraries
###############################################################################

# these have native cmake-based build systems
add_subdirectory(bzip2)
add_subdirectory(lzfse)

# Cista is used rather extensively for RPC serialization
add_subdirectory(cista)

# string formatting
SET(FMT_OS OFF CACHE BOOL "disable fmtlib OS component" FORCE)
add_subdirectory(fmtlib)

# OpenLibm (math library)
ExternalProject_Add(openlibm
    SOURCE_DIR ${CMAKE_CURRENT_LIST_DIR}/openlibm
    INSTALL_DIR ${SYSROOT_DIR}
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ${CMAKE_COMMAND} -E env
        AR=${CMAKE_AR}
        CC=${CMAKE_C_COMPILER}
        LD=${CMAKE_C_COMPILER}
        LDFLAGS="--target=i386-pc-kush-elf --sysroot=${CMAKE_SYSROOT}"
        CFLAGS="--target=i386-pc-kush-elf --sysroot=${CMAKE_SYSROOT}"
        SFLAGS="--target=i386-pc-kush-elf --sysroot=${CMAKE_SYSROOT}"
        make -C ${CMAKE_CURRENT_SOURCE_DIR}/openlibm
    BUILD_IN_SOURCE TRUE
    INSTALL_COMMAND make DESTDIR=${CMAKE_SYSROOT} libdir=/lib includedir=/usr/include install
)
