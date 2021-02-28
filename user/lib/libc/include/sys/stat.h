#ifndef _SYS_STAT_H_
#define	_SYS_STAT_H_

#include <_libc.h>
#include <sys/types.h>
#include <time.h>

#define    S_ISUID    0004000            /* set user id on execution */
#define    S_ISGID    0002000            /* set group id on execution */
#if __BSD_VISIBLE
#define    S_ISTXT    0001000            /* sticky bit */
#endif

#define    S_IRWXU    0000700            /* RWX mask for owner */
#define    S_IRUSR    0000400            /* R for owner */
#define    S_IWUSR    0000200            /* W for owner */
#define    S_IXUSR    0000100            /* X for owner */

#if __BSD_VISIBLE
#define    S_IREAD        S_IRUSR
#define    S_IWRITE    S_IWUSR
#define    S_IEXEC        S_IXUSR
#endif

#define    S_IRWXG    0000070            /* RWX mask for group */
#define    S_IRGRP    0000040            /* R for group */
#define    S_IWGRP    0000020            /* W for group */
#define    S_IXGRP    0000010            /* X for group */

#define    S_IRWXO    0000007            /* RWX mask for other */
#define    S_IROTH    0000004            /* R for other */
#define    S_IWOTH    0000002            /* W for other */
#define    S_IXOTH    0000001            /* X for other */

#if __XSI_VISIBLE
#define    S_IFMT     0170000        /* type of file mask */
#define    S_IFIFO     0010000        /* named pipe (fifo) */
#define    S_IFCHR     0020000        /* character special */
#define    S_IFDIR     0040000        /* directory */
#define    S_IFBLK     0060000        /* block special */
#define    S_IFREG     0100000        /* regular */
#define    S_IFLNK     0120000        /* symbolic link */
#define    S_IFSOCK 0140000        /* socket */
#define    S_ISVTX     0001000        /* save swapped text even after use */
#endif
#if __BSD_VISIBLE
#define    S_IFWHT  0160000        /* whiteout */
#endif

#define    S_ISDIR(m)    (((m) & 0170000) == 0040000)    /* directory */
#define    S_ISCHR(m)    (((m) & 0170000) == 0020000)    /* char special */
#define    S_ISBLK(m)    (((m) & 0170000) == 0060000)    /* block special */
#define    S_ISREG(m)    (((m) & 0170000) == 0100000)    /* regular file */
#define    S_ISFIFO(m)    (((m) & 0170000) == 0010000)    /* fifo or socket */
#if __POSIX_VISIBLE >= 200112
#define    S_ISLNK(m)    (((m) & 0170000) == 0120000)    /* symbolic link */
#define    S_ISSOCK(m)    (((m) & 0170000) == 0140000)    /* socket */
#endif
#if __BSD_VISIBLE
#define    S_ISWHT(m)    (((m) & 0170000) == 0160000)    /* whiteout */
#endif

#if __BSD_VISIBLE
#define    ACCESSPERMS    (S_IRWXU|S_IRWXG|S_IRWXO)    /* 0777 */
                            /* 7777 */
#define    ALLPERMS    (S_ISUID|S_ISGID|S_ISTXT|S_IRWXU|S_IRWXG|S_IRWXO)
                            /* 0666 */
#define    DEFFILEMODE    (S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH)
#endif

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

LIBC_EXPORT int    chmod(const char *, mode_t);
LIBC_EXPORT int    fchmod(int, mode_t);
LIBC_EXPORT int    fchmodat(int, const char *, mode_t, int);
LIBC_EXPORT int    fstat(int, struct stat *);
LIBC_EXPORT int    fstatat(int, const char *, struct stat *, int);
LIBC_EXPORT int    futimens(int, const struct timespec [2]);
LIBC_EXPORT int    lstat(const char *, struct stat *);
LIBC_EXPORT int    mkdir(const char *, mode_t);
LIBC_EXPORT int    mkdirat(int, const char *, mode_t);
LIBC_EXPORT int    mkfifo(const char *, mode_t);
LIBC_EXPORT int    mkfifoat(int, const char *, mode_t);
LIBC_EXPORT int    mknod(const char *, mode_t, dev_t);
LIBC_EXPORT int    mknodat(int, const char *, mode_t, dev_t);
LIBC_EXPORT int    stat(const char *, struct stat *);
LIBC_EXPORT mode_t umask(mode_t);
LIBC_EXPORT int    utimensat(int, const char *, const struct timespec [2], int);

#ifdef __cplusplus
}
#endif
#endif
