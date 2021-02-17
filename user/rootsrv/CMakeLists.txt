###############################################################################
# Root server (init task)
###############################################################################
add_executable(rootsrv
    src/main.c
    src/log.c
    src/init/Init.cpp
)

#####
add_subdirectory(lib/lzfse)

target_link_libraries(rootsrv PRIVATE rootsrv_lzfse)

# allow link time optimization (strips C library functions we don't use)
if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    target_compile_options(rootsrv PRIVATE "-flto")
endif()

# include C library
target_link_libraries(rootsrv PUBLIC "--whole-archive" c)

# set its name
#set_target_properties(rootsrv PROPERTIES OUTPUT_NAME "rootsrv")
set_target_properties(rootsrv PROPERTIES SUFFIX "${PLATFORM_KERNEL_EXTENSION}")

# use the correct linker script
set_target_properties(rootsrv PROPERTIES LINK_DEPENDS ${C_STATIC_LINKER_SCRIPT})
target_link_options(rootsrv PUBLIC "-T${C_STATIC_LINKER_SCRIPT}")
