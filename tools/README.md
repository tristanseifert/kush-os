# Host-side tools
Tools in this directory are compiled on the host that builds the system. They provide various utilities to work with the system's file formats, file systems, and so forth.

## mkinit
Builds an init bundle, which encapsulates all the resources (libraries, configuration, server binaries) to bootstrap the system to the point where it can access the file system and load additional data from there. This is used for the i386 port.

## ildc
Code generator for the RPC IDL. It takes in an IDL file that describes one or more RPC interfaces, and outputs some C++ code -- both the server and client stubs -- as well as some Cap'n Proto files that need to be compiled alongside the C++ files to actually define the wire format of the messages.
