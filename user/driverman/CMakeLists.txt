###############################################################################
# Driver manager
#
# Handles discovery of devices on the system, loading drivers for them, and
# maintains an in-memory tree of their relationships as the system runs.
###############################################################################
add_executable(driverman
    src/main.cpp
    src/Log.cpp
    src/experts/Expert.cpp
    src/forest/Forest.cpp
    src/forest/DriverInstance.cpp
    src/forest/Device.cpp
    src/rpc/Server.cpp
    src/rpc/Server_Driverman.cpp
    src/rpc/Driverman.capnp.c++
    src/db/Driver.cpp
    src/db/DriverDb.cpp
    src/db/DbParser.cpp
    src/db/DeviceMatch.cpp
)

if(${KERNEL_ARCH} STREQUAL "x86")
target_sources(driverman PRIVATE
    src/experts/X86PcExpert.cpp
)
elseif(${KERNEL_ARCH} STREQUAL "x86_64")
target_sources(driverman PRIVATE
    src/experts/Amd64PcExpert.cpp
)
install(FILES ${CMAKE_CURRENT_LIST_DIR}/dist/amd64/BootDriverDb.toml DESTINATION
    ${SYSROOT_DIR}/boot/config RENAME DriverDb.toml)
endif()

#####
# allow link time optimization
target_compile_options(driverman PRIVATE -flto)

# search our codebase for includes
target_include_directories(driverman PRIVATE include)
target_include_directories(driverman PRIVATE src)

# config parsing library
target_link_libraries(driverman PRIVATE tomlplusplus::tomlplusplus)

# required libraries
add_dependencies(driverman c_dynamic system_dynamic)
target_link_libraries(driverman PRIVATE capnp rpc driver mpack_dynamic)

# install it to boot directory in sysroot
install(TARGETS driverman RUNTIME DESTINATION ${SYSROOT_DIR}/sbin)
install(TARGETS driverman RUNTIME DESTINATION ${SYSROOT_DIR}/boot/sbin)

