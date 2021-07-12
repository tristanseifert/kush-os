###################################################################################################
# Builds the various components responsible for providing the graphical user interface.
###################################################################################################


if(BUILD_SYSTEM_SERVERS)
    add_subdirectory(windowserver)
endif()

if(BUILD_USER_LIBS)
    add_subdirectory(lib)
endif()
