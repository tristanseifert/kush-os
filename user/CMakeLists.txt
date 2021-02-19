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
endif()