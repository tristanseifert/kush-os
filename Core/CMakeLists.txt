###################################################################################################
# Core system CMake list
#
# This serves as a sort of umbrella include for all of the core components, gated behind the
# appropriate build flags.
###################################################################################################

##### Define options for what is to be built
option(BUILD_KERNEL "Build kernel" ON)

##### Include kernel components
if(BUILD_KERNEL)
    add_subdirectory(Kernel)
endif()
