###############################################################################
# Filesystem driver
#
# Implements support for all filesystems that the system supports. This driver
# is automatically attached to all discovered disks.
###############################################################################
add_executable(fsdrv
    src/main.cpp
    src/Log.cpp
    src/FilesystemRegistry.cpp
    src/fs/Supported.cpp
    # partition table support
    src/partition/GPT.cpp
    # FAT
    src/fs/fat/FAT.cpp
    src/fs/fat/FAT32.cpp
    src/fs/fat/Directory.cpp
    src/fs/fat/File.cpp
    # various testing filesystems
    src/fs/test/SectorTest.cpp
    # automounting
    src/auto/Automount.cpp
    # RPC service
    src/rpc/MessageLoop.cpp
    src/rpc/LegacyIo.cpp
    rpc/Server_Filesystem.cpp
    # Helpers
    src/util/Path.cpp
    src/util/String.cpp
)

# allow link time optimization
target_compile_options(fsdrv PRIVATE -flto -fno-rtti -fno-exceptions)
target_link_options(fsdrv PRIVATE -s)

# search our codebase for includes
target_include_directories(fsdrv PRIVATE include)
target_include_directories(fsdrv PRIVATE src)

# required libraries
add_dependencies(fsdrv c_dynamic system_dynamic)
target_link_libraries(fsdrv PRIVATE tomlplusplus::tomlplusplus)
target_link_libraries(fsdrv PRIVATE libcrc driver DriverSupport::Disk rpc)

# copy the example automount configuration file
install(FILES ${CMAKE_CURRENT_LIST_DIR}/dist/Automount.toml DESTINATION
    ${SYSROOT_DIR}/boot/config)
install(FILES ${CMAKE_CURRENT_LIST_DIR}/dist/Automount.toml DESTINATION
    ${SYSROOT_DIR}/config)

# install it to boot directory in sysroot
install(TARGETS fsdrv RUNTIME DESTINATION ${SYSROOT_DIR}/sbin)
install(TARGETS fsdrv RUNTIME DESTINATION ${SYSROOT_DIR}/boot/sbin)

