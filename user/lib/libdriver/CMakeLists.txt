###############################################################################
# Driver library
#
# Helpers for writing driver servers.
###############################################################################
add_library(driver SHARED
    # RPC implementations
    src/rpc/Client_Driverman.cpp
    src/rpc/Client.cpp
    # serialization helpers
    src/serialize/base85.c
    # DMA support
    src/dma/BufferPool.cpp
    src/dma/ScatterGatherBuffer.cpp
)

# allow the library to have link time optimization
target_compile_options(driver PRIVATE -flto -fno-rtti -fno-exceptions)
target_link_options(driver PRIVATE -flto -fno-rtti -fno-exceptions)

# specify the include directories
target_include_directories(driver PRIVATE ${CMAKE_CURRENT_LIST_DIR}/src)
target_include_directories(driver PUBLIC ${CMAKE_CURRENT_LIST_DIR}/include)

target_link_libraries(driver PUBLIC mpack_dynamic)

# install library
install(TARGETS driver LIBRARY)
add_custom_command(TARGET driver POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_BINARY_DIR}/libdriver.so ${SYSROOT_DIR}/boot/lib/libdriver.so)

