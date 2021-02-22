#ifndef LIBRPC_RPC_FILE_H
#define LIBRPC_RPC_FILE_H

#include <stdint.h>

/// Open a file for reading
#define FILE_OPEN_READ          (1 << 0)
/// Open a file for writing
#define FILE_OPEN_WRITE         (1 << 1)
/// Create the file if it doesn't exist already.
#define FILE_OPEN_CREATE        (1 << 8)

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Attempts to open a file by name.
 *
 * @param path File path
 * @param mode A combination of file mode flags, defined above.
 * @param outHandle Variable where the file handle is written on success.
 * @param outLength Where to store the length of the file, if the caller is interested.
 * @return A negative error code, or 0 on success.
 */
int FileOpen(const char * _Nonnull path, const uintptr_t flags, uintptr_t * _Nonnull outHandle,
        uint64_t * _Nullable outLength);


#ifdef __cplusplus
}
#endif

#endif
