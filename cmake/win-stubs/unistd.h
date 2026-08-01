/**
 * Windows stub for <unistd.h>.
 *
 * cpppdf's terminal renderer includes <unistd.h> for STDOUT_FILENO (used with
 * the <sys/ioctl.h> shim). MSVC does not ship unistd.h, so we provide the
 * minimal subset required by that translation unit.
 */
#pragma once

#ifdef _WIN32

#include <stdio.h>

#ifndef STDOUT_FILENO
#  define STDOUT_FILENO 1
#endif

#ifndef STDIN_FILENO
#  define STDIN_FILENO 0
#endif

#ifndef STDERR_FILENO
#  define STDERR_FILENO 2
#endif

#endif /* _WIN32 */