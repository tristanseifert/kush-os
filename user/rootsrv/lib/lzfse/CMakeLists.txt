###############################################################################
# lzfse: used for compression of init bundle data
###############################################################################
add_library(rootsrv_lzfse OBJECT
    src/lzfse_decode_base.c
    src/lzfse_decode.c
    src/lzfse_encode_base.c
    src/lzfse_encode.c
    src/lzfse_fse.c
    src/lzvn_decode_base.c
    src/lzvn_encode_base.c)
target_include_directories(rootsrv_lzfse PRIVATE src)
target_include_directories(rootsrv_lzfse PUBLIC include)

target_compile_options(rootsrv_lzfse PRIVATE "-Wno-implicit-fallthrough")

target_link_libraries(rootsrv_lzfse PUBLIC c)

# allow link time optimization
if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    target_compile_options(rootsrv_lzfse PRIVATE "-flto")
endif()

