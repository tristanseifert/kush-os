###############################################################################
# Root server (init task)
###############################################################################
# compile all Cap'n Proto schemas
add_executable(rootsrv
    src/main.cpp
    src/log.c
    src/init/Init.cpp
    src/init/Bundle.cpp
    src/init/ScriptParser.cpp
    src/init/BundleFileRpcHandler.cpp
    src/task/Task.cpp
    src/task/Registry.cpp
    src/task/RpcHandler.cpp
    src/task/TaskEndpoint.capnp.c++
    src/task/InfoPage.cpp
    src/task/loader/ElfCommon.cpp
    src/task/loader/Elf32.cpp
    src/task/loader/Elf64.cpp
    src/dispensary/Dispensary.cpp
    src/dispensary/Registry.cpp
    src/dispensary/RpcHandler.cpp
)

#### arch specific ELF loader code
if(${KERNEL_ARCH} STREQUAL "x86")
    # install the boot config
    install(FILES ${CMAKE_CURRENT_LIST_DIR}/data/x86_pc.init DESTINATION ${SYSROOT_DIR}/boot/config
        RENAME default.init)
elseif(${KERNEL_ARCH} STREQUAL "x86_64")
    # install the boot config
    install(FILES ${CMAKE_CURRENT_LIST_DIR}/data/amd64_pc.init
        DESTINATION ${SYSROOT_DIR}/boot/config RENAME default.init)
else()
    message(FATAL_ERROR "rootsrv does not have binary loader for arch '${KERNEL_ARCH}'")
endif()

#####
# allow link time optimization (strips C library functions we don't use)
target_compile_options(rootsrv PRIVATE -flto -fno-rtti)
target_link_options(rootsrv PRIVATE -s)

# copy the bundle types include file
file(COPY ${CMAKE_CURRENT_LIST_DIR}/../../tools/mkinit/src/BundleTypes.h DESTINATION ${CMAKE_CURRENT_LIST_DIR}/src/init)

# search our codebase for includes
target_include_directories(rootsrv PRIVATE include)
target_include_directories(rootsrv PRIVATE src)
target_include_directories(rootsrv PRIVATE rpc)

# required libraries
target_link_libraries(rootsrv PRIVATE c_static system_static rpc_static)
target_link_libraries(rootsrv PRIVATE lzfse_static fmt::fmt-header-only capnp)

# we have to compile the root server as standalone; pull in required runtime libs
target_link_options(rootsrv PRIVATE -static -nostdlib++ -lopenlibm)
target_link_libraries(rootsrv PRIVATE unwind c++ c++abi)

# use the correct linker script
set_target_properties(rootsrv PROPERTIES LINK_DEPENDS ${C_STATIC_LINKER_SCRIPT})
target_link_options(rootsrv PUBLIC "-Wl,-T${C_STATIC_LINKER_SCRIPT}")
#target_link_options(rootsrv PRIVATE -v)

# install it to boot directory in sysroot
install(TARGETS rootsrv RUNTIME DESTINATION ${SYSROOT_DIR}/boot)

# copy the rpc headers
FILE(GLOB ROOTSRV_RPC_HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/include/rpc/*.hpp")
install(FILES ${ROOTSRV_RPC_HEADERS} DESTINATION ${SYSROOT_DIR}/usr/include/rpc)
