#ifndef _SYS_TYPES_H_
#define	_SYS_TYPES_H_

// define off_t
#ifndef __off_t_defined
#ifndef __USE_FILE_OFFSET64
typedef int off_t;
# else
typedef long long off_t;
#endif
#define __off_t_defined
#endif

// POSIX types
typedef unsigned int pid_t;

typedef unsigned int uid_t;
typedef unsigned int gid_t;

typedef unsigned int mode_t;

typedef unsigned int dev_t;
typedef unsigned int ino_t;
typedef unsigned int nlink_t;

// fs types
typedef unsigned int blksize_t;
typedef unsigned int blkcnt_t;

#endif
