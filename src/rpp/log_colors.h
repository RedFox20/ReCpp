#pragma once

/**
 * Colors macros to wrap raw string literals with ANSI color codes
 */
#ifndef LOG_COLORS
#define LOG_COLORS
    #define DARK_RED(text)     "\x1b[0;31m" text "\x1b[0m"
    #define DARK_GREEN(text)   "\x1b[0;32m" text "\x1b[0m"
    #define DARK_YELLOW(text)  "\x1b[0;33m" text "\x1b[0m"
    #define DARK_BLUE(text)    "\x1b[0;34m" text "\x1b[0m"
    #define DARK_MAGENTA(text) "\x1b[0;35m" text "\x1b[0m"
    #define DARK_CYAN(text)    "\x1b[0;36m" text "\x1b[0m"
    #define DARK_WHITE(text)   "\x1b[0;37m" text "\x1b[0m"

    #define ULINE_RED(text)     "\x1b[4;31m" text "\x1b[0m"
    #define ULINE_GREEN(text)   "\x1b[4;32m" text "\x1b[0m"
    #define ULINE_YELLOW(text)  "\x1b[4;33m" text "\x1b[0m"
    #define ULINE_BLUE(text)    "\x1b[4;34m" text "\x1b[0m"
    #define ULINE_MAGENTA(text) "\x1b[4;35m" text "\x1b[0m"
    #define ULINE_CYAN(text)    "\x1b[4;36m" text "\x1b[0m"
    #define ULINE_WHITE(text)   "\x1b[4;37m" text "\x1b[0m"

    #define DARK_GRAY(text) "\x1b[90m" text "\x1b[0m"
    #define RED(text)     "\x1b[91m" text "\x1b[0m"
    #define GREEN(text)   "\x1b[92m" text "\x1b[0m"
    #define ORANGE(text)  "\x1b[93m" text "\x1b[0m"
    #define BLUE(text)    "\x1b[94m" text "\x1b[0m"
    #define MAGENTA(text) "\x1b[95m" text "\x1b[0m"
    #define CYAN(text)    "\x1b[96m" text "\x1b[0m"
    #define WHITE(text)   "\x1b[97m" text "\x1b[0m"
#endif // LOG_COLORS
