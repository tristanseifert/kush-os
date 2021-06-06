###############################################################################
# Userspace
###############################################################################

# Libraries, if enabled
if(BUILD_USER_LIBS)
    add_subdirectory(lib)
endif()

# system servers
if(BUILD_SYSTEM_SERVERS)
    add_subdirectory(rootsrv)
    add_subdirectory(driverman)
    add_subdirectory(acpi)
    #add_subdirectory(drivers)
endif()

add_subdirectory(dyldo)
