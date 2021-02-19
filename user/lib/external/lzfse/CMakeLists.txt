###############################################################################
# lzfse: Energy efficient compression algorithm
#
# See https://github.com/lzfse/lzfse
###############################################################################
add_library(lzfse STATIC
    src/lzfse_decode_base.c
    src/lzfse_decode.c
    src/lzfse_encode_base.c
    src/lzfse_encode.c
    src/lzfse_fse.c
    src/lzvn_decode_base.c
    src/lzvn_encode_base.c)
target_include_directories(lzfse PRIVATE src)
target_include_directories(lzfse PUBLIC include)

target_compile_options(lzfse PRIVATE "-Wno-implicit-fallthrough")

# allow link time optimization
if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    target_compile_options(lzfse PRIVATE "-flto")
endif()

# installation
install(TARGETS lzfse LIBRARY DESTINATION)

FILE(GLOB LZFSE_HEADERS "${CMAKE_CURRENT_LIST_DIR}/include/*.h")
install(FILES ${LZFSE_HEADERS} DESTINATION ${SYSROOT_DIR}/usr/include/compress)

