###############################################################################
# Dyldo, the dynamic linker
###############################################################################
# dynamic link server
if(BUILD_SYSTEM_SERVERS)
    add_subdirectory(srv)
endif()

# Shared library part of the linker
if(BUILD_USER_LIBS)
    add_subdirectory(lib)
endif()