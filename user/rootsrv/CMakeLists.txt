###############################################################################
# Root server (init task)
###############################################################################
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
    src/task/InfoPage.cpp
    src/task/DyldoPipe.cpp
    src/task/loader/Elf32.cpp
    src/dispensary/Dispensary.cpp
    src/dispensary/Registry.cpp
    src/dispensary/RpcHandler.cpp
)

#####
# allow link time optimization (strips C library functions we don't use)
target_compile_options(rootsrv PRIVATE -flto)

# copy the bundle types include file
file(COPY ${CMAKE_CURRENT_LIST_DIR}/../../tools/mkinit/src/BundleTypes.h DESTINATION ${CMAKE_CURRENT_LIST_DIR}/src/init)
# copy the dynamic linker RPC headers
file(COPY ${CMAKE_CURRENT_LIST_DIR}/../dyldo/srv/include/rpc/LoaderPort.h DESTINATION ${CMAKE_CURRENT_LIST_DIR}/include/rpc)

# search our codebase for includes
target_include_directories(rootsrv PRIVATE include)
target_include_directories(rootsrv PRIVATE src)

# required libraries
add_dependencies(rootsrv c_static)
target_link_libraries(rootsrv PRIVATE c_static)
target_link_libraries(rootsrv PRIVATE system_static rpc_static lzfse_static cista)
target_link_libraries(rootsrv PRIVATE fmt::fmt-header-only)

# we have to compile the root server as standalone
target_link_options(rootsrv PRIVATE -static)

# use the correct linker script
set_target_properties(rootsrv PROPERTIES LINK_DEPENDS ${C_STATIC_LINKER_SCRIPT})
target_link_options(rootsrv PUBLIC "-Wl,-T${C_STATIC_LINKER_SCRIPT}")
#target_link_options(rootsrv PRIVATE -v)

# install it to boot directory in sysroot
install(TARGETS rootsrv RUNTIME DESTINATION ${SYSROOT_DIR}/boot)

# copy the rpc headers
FILE(GLOB ROOTSRV_RPC_HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/include/rpc/*.hpp")
install(FILES ${ROOTSRV_RPC_HEADERS} DESTINATION ${SYSROOT_DIR}/usr/include/rpc)

# copy some data files as well
install(FILES data/x86_pc.init DESTINATION ${SYSROOT_DIR}/etc RENAME default.init)
