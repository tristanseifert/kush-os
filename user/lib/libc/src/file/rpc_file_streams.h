#ifndef FILE_RPC_FILE_STREAMS_H
#define FILE_RPC_FILE_STREAMS_H

#include <stdio.h>

/**
 * RPC handler open routine
 */
FILE *__libc_rpc_file_open(const char *path, const char *mode);

#endif
