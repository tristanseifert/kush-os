###################################################################################################
# Include the Freetype2 CMake file
###################################################################################################
# force some libraries on
set(FT_WITH_ZLIB ON)
set(ZLIB_LIBRARY "${SYSROOT_DIR}/lib/libz.so")

set(FT_WITH_BZIP2 ON)
set(BZIP2_LIBRARIES "${SYSROOT_DIR}/lib/libbzip2.so")
set(BZIP2_INCLUDE_DIR "${SYSROOT_DIR}/usr/include/compress")

set(FT_WITH_PNG ON)
set(PNG_LIBRARY "${SYSROOT_DIR}/lib/libpng.so")

# build a shared lib
set(BUILD_SHARED_LIBS ON)

# disable some more agressive warnings/errors
add_compile_options(-Wno-implicit-fallthrough -Wno-cast-qual -Wno-format-nonliteral -Wno-incompatible-pointer-types-discards-qualifiers)

# XXX: freetype source should be fixed to remove uninitialized issues
add_compile_options(-Wno-sometimes-uninitialized)

# then include the FreeType source
add_subdirectory(source)
