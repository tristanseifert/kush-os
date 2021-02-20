###############################################################################
# bzip2: used for compression of init bundle data
###############################################################################
add_library(bzip2 SHARED
    src/blocksort.c
    src/bzlib.c
    src/compress.c
    src/crctable.c
    src/decompress.c
    src/huffman.c
    src/randtable.c)
target_compile_options(bzip2 PRIVATE -fPIC)
target_include_directories(bzip2 PRIVATE src)
target_include_directories(bzip2 PUBLIC include)
target_compile_definitions(bzip2 PRIVATE BZ_NO_STDIO=1)

target_compile_options(bzip2 PRIVATE "-Wno-implicit-fallthrough")

# installation
install(TARGETS bzip2 LIBRARY)

FILE(GLOB BZIP2_HEADERS "${CMAKE_CURRENT_LIST_DIR}/include/*.h")
install(FILES ${BZIP2_HEADERS} DESTINATION ${SYSROOT_DIR}/usr/include/compress)


