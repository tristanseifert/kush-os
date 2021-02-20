###############################################################################
# Dynamic link server
#
# This is the second (and last) statically linked server. It keeps track of
# loaded shared libraries so it can vend out their text segments to tasks. It
# works in conjunction with the shared library part that's mapped into each
# task, and executes before the entry point.
###############################################################################
add_executable(dyldosrv
    src/main.cpp
)

#####
# allow link time optimization
target_compile_options(dyldosrv PRIVATE "-flto")

# search our codebase for includes
target_include_directories(dyldosrv PRIVATE include)
target_include_directories(dyldosrv PRIVATE src)

# required libraries
add_dependencies(dyldosrv c_static system_static)
target_link_libraries(dyldosrv PRIVATE system_static cista)

# build a static binary
target_link_options(dyldosrv PRIVATE -static)
target_compile_options(dyldosrv PRIVATE -fno-exceptions -fno-rtti)

# use the correct linker script
set_target_properties(dyldosrv PROPERTIES LINK_DEPENDS ${C_STATIC_LINKER_SCRIPT})
target_link_options(dyldosrv PUBLIC "-Wl,-T${C_STATIC_LINKER_SCRIPT}")

# install it to boot directory in sysroot
install(TARGETS dyldosrv RUNTIME DESTINATION ${SYSROOT_DIR}/sbin)

