###############################################################################
# Dynamic linker shared library
#
# This is mapped into every task at startup, and performs relocations and
# symbol resolution.
#
# Additionally, it provides runtime services exposed by the C runtime.
###############################################################################

add_library(dyldo SHARED
    src/lib_init.c
)

# this is a shared library but really only in spirit lol
target_compile_options(dyldo PRIVATE -fno-rtti -fno-exceptions)
target_link_options(dyldo PRIVATE -nostartfiles)

# set required compiler flags
target_compile_definitions(dyldo PRIVATE BUILDING_LIBDYLDO=1)

# specify the include directories
target_include_directories(dyldo PRIVATE ${CMAKE_CURRENT_LIST_DIR}/src)
target_include_directories(dyldo PUBLIC ${CMAKE_CURRENT_LIST_DIR}/include)

# install the library to the sysroot
install(TARGETS dyldo LIBRARY)

FILE(GLOB LIBDYLDO_PUB_HEADERS "${CMAKE_CURRENT_LIST_DIR}/include/*.h")
install(FILES ${LIBDYLDO_PUB_HEADERS} DESTINATION ${SYSROOT_DIR}/usr/include/dyldo)
