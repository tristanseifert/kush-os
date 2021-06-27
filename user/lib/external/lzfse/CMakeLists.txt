###############################################################################
# lzfse: Energy efficient compression algorithm
#
# See https://github.com/lzfse/lzfse
###############################################################################
add_library(lzfse_objs OBJECT
    src/lzfse_decode_base.c
    src/lzfse_decode.c
    src/lzfse_encode_base.c
    src/lzfse_encode.c
    src/lzfse_fse.c
    src/lzvn_decode_base.c
    src/lzvn_encode_base.c)
target_include_directories(lzfse_objs PRIVATE src)
target_include_directories(lzfse_objs PUBLIC include)

target_compile_options(lzfse_objs PRIVATE -fPIC -O3)
target_compile_options(lzfse_objs PRIVATE "-Wno-implicit-fallthrough")

# create the static and dynamic targets
add_library(lzfse_static STATIC $<TARGET_OBJECTS:lzfse_objs>)
set_target_properties(lzfse_static PROPERTIES OUTPUT_NAME "lzfse")
target_include_directories(lzfse_static PUBLIC include)

add_library(lzfse_dynamic SHARED $<TARGET_OBJECTS:lzfse_objs>)
set_target_properties(lzfse_dynamic PROPERTIES OUTPUT_NAME "lzfse")
target_include_directories(lzfse_dynamic PUBLIC include)

# installation
install(TARGETS lzfse_dynamic LIBRARY DESTINATION)
add_library(lzfse ALIAS lzfse_dynamic)

FILE(GLOB LZFSE_HEADERS "${CMAKE_CURRENT_LIST_DIR}/include/*.h")
install(FILES ${LZFSE_HEADERS} DESTINATION ${SYSROOT_DIR}/usr/include/compress)

