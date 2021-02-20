#ifndef _SYS_STAT_H_
#define	_SYS_STAT_H_

#include <sys/types.h>
#include <time.h>

/*
 * Info structure for a file
 */
struct stat {
    /// device id of the device that holds this file
    dev_t                               st_dev;
    /// file serial number
    ino_t                               st_ino;
    /// file mode
    mode_t                              st_mode;
    /// Number of hard links to the file
    nlink_t                             st_nlink;

    /// owner user id
    uid_t                               st_uid;
    /// owner group id
    gid_t                               st_gid;

    /// device ID (if this is a character/special file)
    dev_t                               st_rdev;

    /// size of the file in bytes (for regular files)
    off_t                               st_size;

    /// last access timestamp
    struct timespec                     st_atim;
    /// last modification timestamp
    struct timespec                     st_mtim;
    /// last status change timestamp
    struct timespec                     st_ctim;

    /// FS specific preferred block size
    blksize_t                           st_blksize;
    /// number of FS specific blocks allocated for the object
    blksize_t                           st_blocks;
};

#ifdef __cplusplus
extern "C" {
#endif

int    chmod(const char *, mode_t);
int    fchmod(int, mode_t);
int    fchmodat(int, const char *, mode_t, int);
int    fstat(int, struct stat *);
int    fstatat(int, const char *, struct stat *, int);
int    futimens(int, const struct timespec [2]);
int    lstat(const char *, struct stat *);
int    mkdir(const char *, mode_t);
int    mkdirat(int, const char *, mode_t);
int    mkfifo(const char *, mode_t);
int    mkfifoat(int, const char *, mode_t);
int    mknod(const char *, mode_t, dev_t);
int    mknodat(int, const char *, mode_t, dev_t);
int    stat(const char *, struct stat *);
mode_t umask(mode_t);
int    utimensat(int, const char *, const struct timespec [2], int);

#ifdef __cplusplus
}
#endif
#endif
