/**
 * Windows stub for <sys/ioctl.h>.
 *
 * Provides a minimal POSIX-compatible interface for terminal-size queries
 * (struct winsize / TIOCGWINSZ / ioctl) using the Windows Console API.
 * Only included on Windows builds; real POSIX systems use their own header.
 */
#pragma once

#ifdef _WIN32

#include <windows.h>
#include <stdarg.h>

struct winsize {
    unsigned short ws_row;
    unsigned short ws_col;
    unsigned short ws_xpixel; /* unused */
    unsigned short ws_ypixel; /* unused */
};

/* Value matches the Linux constant; the actual numeric value is irrelevant
   because we only dispatch on it inside our own ioctl() shim below. */
#define TIOCGWINSZ 0x5413

static inline int ioctl(int /*fd*/, unsigned long request, ...) {
    if (request == TIOCGWINSZ) {
        va_list args;
        va_start(args, request);
        struct winsize *ws = va_arg(args, struct winsize *);
        va_end(args);
        if (ws) {
            CONSOLE_SCREEN_BUFFER_INFO csbi;
            if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE),
                                           &csbi)) {
                ws->ws_col =
                    (unsigned short)(csbi.srWindow.Right -
                                     csbi.srWindow.Left + 1);
                ws->ws_row =
                    (unsigned short)(csbi.srWindow.Bottom -
                                     csbi.srWindow.Top + 1);
                ws->ws_xpixel = 0;
                ws->ws_ypixel = 0;
                return 0;
            }
        }
        return -1;
    }
    return -1;
}

#endif /* _WIN32 */
