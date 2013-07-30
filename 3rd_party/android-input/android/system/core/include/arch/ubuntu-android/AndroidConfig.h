/*
 * Copyright (C) 2005 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 * Android config -- "Ubuntu".  Used for Ubuntu desktop x86 Linux.
 */
#ifndef _ANDROID_CONFIG_H
#define _ANDROID_CONFIG_H

/*
 * ===========================================================================
 *                              !!! IMPORTANT !!!
 * ===========================================================================
 *
 * This file is included by ALL C/C++ source files.  Don't put anything in
 * here unless you are absolutely certain it can't go anywhere else.
 *
 * Any C++ stuff must be wrapped with "#ifdef __cplusplus".  Do not use "//"
 * comments.
 */

/*
 * Threading model.  Choose one:
 *
 * HAVE_PTHREADS - use the pthreads library.
 * HAVE_WIN32_THREADS - use Win32 thread primitives.
 *  -- combine HAVE_CREATETHREAD, HAVE_CREATEMUTEX, and HAVE__BEGINTHREADEX
 */
#define HAVE_PTHREADS

/*
 * Define this if you have <termio.h>
 */
#define  HAVE_TERMIO_H 1

/*
 * Define this if you have <sys/sendfile.h>
 */
#define  HAVE_SYS_SENDFILE_H 1

/*
 * Define this if you build against MSVCRT.DLL
 */
/* #define HAVE_MS_C_RUNTIME */

/*
 * Define this if you have sys/uio.h
 */
#define  HAVE_SYS_UIO_H 1

/*
 * Define this if we have localtime_r().
 */
#define HAVE_LOCALTIME_R 1

/*
 * Define this if we have gethostbyname_r().
 */
#define HAVE_GETHOSTBYNAME_R

/*
 * Define this if we want to use WinSock.
 */
/* #define HAVE_WINSOCK */

/*
 * Define this if have clock_gettime() and friends
 *
 * Desktop Linux has this in librt, but it's broken in goobuntu, yielding
 * mildly or wildly inaccurate results.
 */
/*#define HAVE_POSIX_CLOCKS*/

/*
 * Define this if we have pthread_cond_timedwait_monotonic() and
 * clock_gettime(CLOCK_MONOTONIC).
 */
/* #define HAVE_TIMEDWAIT_MONOTONIC */

/*
 * We need to choose between 32-bit and 64-bit off_t.  All of our code should
 * agree on the same size.  For desktop systems, use 64-bit values,
 * because some of our libraries (e.g. wxWidgets) expect to be built that way.
 */
#define _FILE_OFFSET_BITS 64
#define _LARGEFILE_SOURCE 1

/*
 * Define if platform has off64_t (and lseek64 and other xxx64 functions)
 */
#define HAVE_OFF64_T

/*
 * Defined if we have the cxxabi.h header for demangling C++ symbols.  If
 * not defined, stack crawls will be displayed with raw mangled symbols
 */
#define HAVE_CXXABI 0

/*
 * Defined if we have the gettid() system call.
 */
/* #define HAVE_GETTID */

/*
 * Add any extra platform-specific defines here.
 */

/*
 * Define if we have Linux-style non-filesystem Unix Domain Sockets
 */

/*
 * What CPU architecture does this platform use?
 */
#define ARCH_X86


/*
 * Define if we have Linux's inotify in <sys/inotify.h>.
 */
/*#define HAVE_INOTIFY 1*/

/*
 * Define if we have madvise() in <sys/mman.h>
 */
#define HAVE_MADVISE 1

/*
 * Define if tm struct has tm_gmtoff field
 */
#define HAVE_TM_GMTOFF 1

/*
 * Define if dirent struct has d_type field
 */
#define HAVE_DIRENT_D_TYPE 1

/*
 * Define if libc includes Android system properties implementation.
 */
/* #define HAVE_LIBC_SYSTEM_PROPERTIES */

/*
 * Define if system provides a system property server (should be
 * mutually exclusive with HAVE_LIBC_SYSTEM_PROPERTIES).
 */
#define HAVE_SYSTEM_PROPERTY_SERVER

/*
 * sprintf() format string for shared library naming.
 */
#define OS_SHARED_LIB_FORMAT_STR    "lib%s.so"

/*
 * The default path separator for the platform
 */
#define OS_PATH_SEPARATOR '/'

/*
 * Define if <sys/socket.h> exists.
 */
#define HAVE_SYS_SOCKET_H 1

/*
 * Define if the strlcpy() function exists on the system.
 */
/* #define HAVE_STRLCPY 1 */

/*
 * Define if the open_memstream() function exists on the system.
 */
#define HAVE_OPEN_MEMSTREAM 1

/*
 * Define if the BSD funopen() function exists on the system.
 */
/* #define HAVE_FUNOPEN 1 */

/*
 * Define if prctl() exists
 */
#define HAVE_PRCTL 1

/*
 * Define if writev() exists
 */
#define HAVE_WRITEV 1

/*
 * Define if <stdint.h> exists.
 */
#define HAVE_STDINT_H 1

/*
 * Define if <stdbool.h> exists.
 */
#define HAVE_STDBOOL_H 1

/*
 * Define if <sched.h> exists.
 */
#define HAVE_SCHED_H 1

/*
 * Define if pread() exists
 */
#define HAVE_PREAD 1

/*
 * Define if we have st_mtim in struct stat
 */
#define HAVE_STAT_ST_MTIM 1

/*
 * Define if printf() supports %zd for size_t arguments
 */
#define HAVE_PRINTF_ZD 1

/*
 * Define to 1 if <stdlib.h> provides qsort_r() with a BSD style function prototype.
 */
#define HAVE_BSD_QSORT_R 0

/*
 * Define to 1 if <stdlib.h> provides qsort_r() with a GNU style function prototype.
 */
#if __GLIBC__ > 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 8)
#define HAVE_GNU_QSORT_R 1
#else
#define HAVE_GNU_QSORT_R 0
#endif

#endif /*_ANDROID_CONFIG_H*/
