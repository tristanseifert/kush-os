#ifndef LIBRPC_RPC_DISPENSARY_H
#define LIBRPC_RPC_DISPENSARY_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Attempts to resolve a name into a port.
 *
 * @param name Service name to look up; this is a zero-terminated UTF-8 string.
 * @param outPort If a port is found, its handle is written to this variable.
 *
 * @return 0 if the request was completed, but the port was not found; 1 if the port was found; or
 * a negative error code.
 */
int LookupService(const char * _Nonnull name, uintptr_t * _Nonnull outPort);

#ifdef __cplusplus
}
#endif

#endif
