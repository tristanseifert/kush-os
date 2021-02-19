# Host-side tools
Tools in this directory are compiled on the host that builds the system. They provide various utilities to work with the system's file formats, file systems, and so forth.

## mkinit
Builds an init bundle, which encapsulates all the resources (libraries, configuration, server binaries) to bootstrap the system to the point where it can access the file system and load additional data from there.
