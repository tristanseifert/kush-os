###############################################################################
# Driver library
#
# Helpers for writing driver servers.
###############################################################################
add_library(driver STATIC
    # RPC implementations
    src/rpc/Driverman.capnp.c++
    src/rpc/Client_Driverman.cpp
    src/rpc/Client.cpp
    # RPC user clients
    src/clients/pci/PciDriverUser.capnp.c++
    src/clients/pci/Client_PciDriverUser.cpp
    src/clients/pci/Client.cpp
    src/clients/pci/Device.cpp
    src/clients/pci/Helpers.cpp
    # serialization helpers
    src/serialize/base85.c
)

# allow the library to have link time optimization
target_compile_options(driver PRIVATE -flto)

# specify the include directories
target_include_directories(driver PRIVATE ${CMAKE_CURRENT_LIST_DIR}/src)
target_include_directories(driver PUBLIC ${CMAKE_CURRENT_LIST_DIR}/include)

target_link_libraries(driver PRIVATE mpack_dynamic)

# install the library to the sysroot
set_target_properties(driver PROPERTIES OUTPUT_NAME "driver")
install(TARGETS driver LIBRARY)

# copy the driver headers
FILE(GLOB LIBDRIVER_PUB_HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/include/driver/*.h*")
install(FILES ${LIBDRIVER_PUB_HEADERS} DESTINATION ${SYSROOT_DIR}/usr/include/driver)

