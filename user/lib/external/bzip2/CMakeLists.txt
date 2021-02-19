###############################################################################
# bzip2: used for compression of init bundle data
###############################################################################
add_library(bzip2 STATIC
    src/blocksort.c
    src/bzlib.c
    src/compress.c
    src/crctable.c
    src/decompress.c
    src/huffman.c
    src/randtable.c)
target_include_directories(bzip2 PRIVATE src)
target_include_directories(bzip2 PUBLIC include)
target_compile_definitions(bzip2 PRIVATE BZ_NO_STDIO=1)

target_compile_options(bzip2 PRIVATE "-Wno-implicit-fallthrough")

# allow link time optimization
if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    target_compile_options(bzip2 PRIVATE "-flto")
endif()

# installation
install(TARGETS bzip2 LIBRARY)

FILE(GLOB BZIP2_HEADERS "${CMAKE_CURRENT_LIST_DIR}/include/*.h")
install(FILES ${BZIP2_HEADERS} DESTINATION ${SYSROOT_DIR}/usr/include/compress)


