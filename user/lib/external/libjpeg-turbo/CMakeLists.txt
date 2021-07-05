###################################################################################################
# Include the libjpeg-turbo CMake file, after changing some settings
###################################################################################################
set(SKIP_INSTALL_PROGRAMS ON)
set(ENABLE_STATIC OFF)

# force set the processor (for some reason, we don't get this correctly from the toolchain...)
set(CMAKE_SYSTEM_PROCESSOR "x86_64")

# hacks to get NASM to work right
set(CMAKE_ASM_FLAGS "")
set(CMAKE_ASM_NASM_FLAGS "")

set(CMAKE_ASM_NASM_COMPILE_OBJECT "<CMAKE_ASM_NASM_COMPILER> <INCLUDES> -felf64 -g -DELF -D__x86_64__ -DPIC -o <OBJECT> <SOURCE>")

# we do not care about the docs. at all
set(CMAKE_INSTALL_DOCDIR ${CMAKE_CURRENT_BINARY_DIR}/doc)

# disable some more agressive warnings/errors
add_compile_options(-Wno-implicit-fallthrough -Wno-cast-qual -Wno-format-nonliteral -Wno-incompatible-pointer-types-discards-qualifiers)

add_subdirectory(source)
