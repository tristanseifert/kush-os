###############################################################################
# bzip2: used for compression of init bundle data
###############################################################################
add_library(rootsrv_bzip2 OBJECT
    src/blocksort.c
    src/bzlib.c
    src/compress.c
    src/crctable.c
    src/decompress.c
    src/huffman.c
    src/randtable.c)
target_include_directories(rootsrv_bzip2 PRIVATE src)
target_include_directories(rootsrv_bzip2 PUBLIC include)
target_compile_definitions(rootsrv_bzip2 PRIVATE BZ_NO_STDIO=1)

target_compile_options(rootsrv_bzip2 PRIVATE "-Wno-implicit-fallthrough")

target_link_libraries(rootsrv_bzip2 PUBLIC c)

# allow link time optimization
if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    target_compile_options(rootsrv_bzip2 PRIVATE "-flto")
endif()

