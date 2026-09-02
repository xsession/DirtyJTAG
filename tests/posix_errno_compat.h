#ifndef DIRTYJTAG_TEST_POSIX_ERRNO_COMPAT_H
#define DIRTYJTAG_TEST_POSIX_ERRNO_COMPAT_H

/* MinGW's errno.h omits values that Zephyr and POSIX hosts provide. */
#ifndef ENOTSUP
#define ENOTSUP 134
#endif

#ifndef ETIMEDOUT
#define ETIMEDOUT 116
#endif

#ifndef EBADMSG
#define EBADMSG 74
#endif

#ifndef EMSGSIZE
#define EMSGSIZE 90
#endif

#endif
