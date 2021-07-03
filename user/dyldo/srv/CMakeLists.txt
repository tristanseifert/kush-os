###############################################################################
# Dynamic link server
#
# This is the second (and last) statically linked server. It keeps track of
# loaded shared libraries so it can vend out shareable segments to tasks. It
# works in conjunction with the shared library part that's mapped into each
# task, and executes before the entry point.
###############################################################################
add_executable(dyldosrv
    src/main.cpp
    src/Log.cpp
    src/MessageLoop.cpp
)

#####
target_compile_options(dyldosrv PRIVATE -flto -fno-exceptions -fno-rtti)

target_include_directories(dyldosrv PRIVATE include)
target_include_directories(dyldosrv PRIVATE src)

target_link_libraries(dyldosrv PRIVATE system_static c_crt0T c_static rpc_static_small)
target_link_options(dyldosrv PRIVATE -s -nostdlib -lopenlibm)

# use the correct linker script
set_target_properties(dyldosrv PROPERTIES LINK_DEPENDS ${C_STATIC_LINKER_SCRIPT})
target_link_options(dyldosrv PUBLIC "-Wl,-T${C_STATIC_LINKER_SCRIPT}")

# install it to boot directory in sysroot
install(TARGETS dyldosrv RUNTIME DESTINATION ${SYSROOT_DIR}/boot/sbin)
install(TARGETS dyldosrv RUNTIME DESTINATION ${SYSROOT_DIR}/sbin)

