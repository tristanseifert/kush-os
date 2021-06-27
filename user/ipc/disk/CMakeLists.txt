###############################################################################
# Disk driver support library
#
# Implements the protocol stubs for drivers that expose a disk device
###############################################################################
add_library(libdiskdriver STATIC
    src/Server_DiskDriver.cpp
    src/Client_DiskDriver.cpp
    src/Client.cpp
)
set_target_properties(libdiskdriver PROPERTIES OUTPUT_NAME "diskdriver")

target_include_directories(libdiskdriver PUBLIC include)
target_include_directories(libdiskdriver PRIVATE src)

target_compile_options(libdiskdriver PRIVATE -flto -fno-rtti -fno-exceptions)
target_link_libraries(libdiskdriver PUBLIC driver rpc mpack)

# export it under the DriverSupport space
add_library(DriverSupport::Disk ALIAS libdiskdriver)
