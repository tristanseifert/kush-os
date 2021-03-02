###############################################################################
# Driver library
#
# Helpers for writing driver servers.
###############################################################################
add_library(driver STATIC
    # serialization helpers
    src/serialize/base85.c
    # message implementations
    src/messages/Send.cpp
    src/messages/Messages.cpp
    src/messages/Discovery.cpp
    src/messages/Pci.cpp
)

# allow the library to have link time optimization
target_compile_options(driver PRIVATE -flto)

# specify the include directories
target_include_directories(driver PRIVATE ${CMAKE_CURRENT_LIST_DIR}/src)
target_include_directories(driver PUBLIC ${CMAKE_CURRENT_LIST_DIR}/include)

# for rpc stuff we need Cista
target_link_libraries(driver PRIVATE mpack_dynamic)

# install the library to the sysroot
set_target_properties(driver PROPERTIES OUTPUT_NAME "driver")
install(TARGETS driver LIBRARY)

# copy the driver headers
FILE(GLOB LIBDRIVER_PUB_HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/include/driver/*.h*")
install(FILES ${LIBDRIVER_PUB_HEADERS} DESTINATION ${SYSROOT_DIR}/usr/include/driver)

