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

/**
 * Closes a previously open file.
 *
 * The caller is responsible for ensuring there is no outstanding IO remaining on the file; any
 * buffers are discarded.
 *
 * @param file File handle from an earlier call to FileOpen()
 * @return 0 on success, error code otherwise.
 */
int FileClose(const uintptr_t file);

/**
 * Reads from the file.
 *
 * This may generate more than one message to the file IO server, if the requested read length is
 * longer than what can be supported in one message.
 *
 * @return Actual number of bytes returned, or a negative error code.
 */
int FileRead(const uintptr_t file, const uint64_t offset, const size_t length, void * _Nonnull buf);

#ifdef __cplusplus
}
#endif

#endif
