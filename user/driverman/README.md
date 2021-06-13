# Driver Manager
The driver manager holds a representation of all hardware available on the machine, and matches drivers to it.

## RPC Protocol
Compile the RPC protocol `Driverman.idl` in the `src/rpc` directory if it changes. The generated code should be checked in to git, and should not be moved from this location as it is referred to by `libdriver` for the client part of the code.
