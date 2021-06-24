# Filesystem Server
This driver implements the core of the vfs layer of the system. It reads partition tables from disks and launches the appropriate file system drivers (in process) and registers them on the appropriate vfs path.

Mount points for automatically mounted disks can be configured via a configuration file; see `dist/Automount.toml` file for an example. This file must be accessible at boot time to configure mounting of the system's root volume.
