###############################################################################
# Root server (init task)
###############################################################################
add_executable(rootsrv
    src/main.cpp
    src/log.c
    src/init/Init.cpp
)

#####
# allow link time optimization (strips C library functions we don't use)
if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    target_compile_options(rootsrv PRIVATE "-flto")
endif()

# required libraries
target_link_libraries(rootsrv PUBLIC system)
target_link_libraries(rootsrv PRIVATE lzfse)

# we have to compile the root server as standalone
#target_link_options(rootsrv PRIVATE -static -ffreestanding -nodefaultlibs -nostdlib -fno-exceptions -fno-rtti)
#target_link_options(rootsrv PRIVATE -static -fno-exceptions -fno-rtti -nostdlib)
target_link_options(rootsrv PRIVATE -static)

# set its name
#set_target_properties(rootsrv PROPERTIES OUTPUT_NAME "rootsrv")
set_target_properties(rootsrv PROPERTIES SUFFIX "${PLATFORM_KERNEL_EXTENSION}")

# use the correct linker script
set_target_properties(rootsrv PROPERTIES LINK_DEPENDS ${C_STATIC_LINKER_SCRIPT})
#target_link_options(rootsrv PUBLIC "-v")
target_link_options(rootsrv PUBLIC "-Wl,-T${C_STATIC_LINKER_SCRIPT}")

# install it to boot directory in sysroot
install(TARGETS rootsrv RUNTIME DESTINATION ${SYSROOT_DIR}/boot)

# copy some data files as well
install(FILES data/x86_pc.init DESTINATION ${SYSROOT_DIR}/etc RENAME default.init)
