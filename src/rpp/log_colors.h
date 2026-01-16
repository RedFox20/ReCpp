#pragma once

/**
 * Colors macros to wrap raw string literals with ANSI color codes
 */
#ifndef LOG_COLORS
#define LOG_COLORS
    // https://gist.github.com/iamnewton/8754917
    #define COLOR_RESET        "\x1b[0m"

    // Regular Colors (Dark)
    #define COLOR_BLACK        "\x1b[0;30m" // for text this looks like dark gray on black background
    #define COLOR_DARK_RED     "\x1b[0;31m"
    #define COLOR_DARK_GREEN   "\x1b[0;32m"
    #define COLOR_DARK_YELLOW  "\x1b[0;33m"
    #define COLOR_DARK_BLUE    "\x1b[0;34m"
    #define COLOR_DARK_MAGENTA "\x1b[0;35m"
    #define COLOR_DARK_CYAN    "\x1b[0;36m"
    #define COLOR_DARK_WHITE   "\x1b[0;37m" // looks like white to me
    #define BLACK(text)        COLOR_BLACK        text  COLOR_RESET
    #define DARK_RED(text)     COLOR_DARK_RED     text  COLOR_RESET
    #define DARK_GREEN(text)   COLOR_DARK_GREEN   text  COLOR_RESET
    #define DARK_YELLOW(text)  COLOR_DARK_YELLOW  text  COLOR_RESET
    #define DARK_BLUE(text)    COLOR_DARK_BLUE    text  COLOR_RESET
    #define DARK_MAGENTA(text) COLOR_DARK_MAGENTA text  COLOR_RESET
    #define DARK_CYAN(text)    COLOR_DARK_CYAN    text  COLOR_RESET
    #define DARK_WHITE(text)   COLOR_DARK_WHITE   text  COLOR_RESET

    // Bold (Dark)
    #define COLOR_BOLD_DARK_BLACK        "\x1b[1;30m"
    #define COLOR_BOLD_DARK_RED          "\x1b[1;31m"
    #define COLOR_BOLD_DARK_GREEN        "\x1b[1;32m"
    #define COLOR_BOLD_DARK_YELLOW       "\x1b[1;33m"
    #define COLOR_BOLD_DARK_BLUE         "\x1b[1;34m"
    #define COLOR_BOLD_DARK_MAGENTA      "\x1b[1;35m"
    #define COLOR_BOLD_DARK_CYAN         "\x1b[1;36m"
    #define COLOR_BOLD_DARK_WHITE        "\x1b[1;37m"
    #define BOLD_DARK_BLACK(text)        COLOR_BOLD_DARK_BLACK        text  COLOR_RESET
    #define BOLD_DARK_RED(text)          COLOR_BOLD_DARK_RED          text  COLOR_RESET
    #define BOLD_DARK_GREEN(text)        COLOR_BOLD_DARK_GREEN        text  COLOR_RESET
    #define BOLD_DARK_YELLOW(text)       COLOR_BOLD_DARK_YELLOW       text  COLOR_RESET
    #define BOLD_DARK_BLUE(text)         COLOR_BOLD_DARK_BLUE         text  COLOR_RESET
    #define BOLD_DARK_MAGENTA(text)      COLOR_BOLD_DARK_MAGENTA      text  COLOR_RESET
    #define BOLD_DARK_CYAN(text)         COLOR_BOLD_DARK_CYAN         text  COLOR_RESET
    #define BOLD_DARK_WHITE(text)        COLOR_BOLD_DARK_WHITE        text  COLOR_RESET

    // Underline (Dark)
    #define COLOR_ULINE_DARK_GRAY   "\x1b[4;30m" // visually it is dark gray
    #define COLOR_ULINE_RED         "\x1b[4;31m"
    #define COLOR_ULINE_GREEN       "\x1b[4;32m"
    #define COLOR_ULINE_YELLOW      "\x1b[4;33m"
    #define COLOR_ULINE_BLUE        "\x1b[4;34m"
    #define COLOR_ULINE_MAGENTA     "\x1b[4;35m"
    #define COLOR_ULINE_CYAN        "\x1b[4;36m"
    #define COLOR_ULINE_WHITE       "\x1b[4;37m"
    #define ULINE_DARK_GRAY(text)   COLOR_ULINE_DARK_GRAY   text  COLOR_RESET
    #define ULINE_RED(text)         COLOR_ULINE_RED     text  COLOR_RESET
    #define ULINE_GREEN(text)       COLOR_ULINE_GREEN   text  COLOR_RESET
    #define ULINE_YELLOW(text)      COLOR_ULINE_YELLOW  text  COLOR_RESET
    #define ULINE_BLUE(text)        COLOR_ULINE_BLUE    text  COLOR_RESET
    #define ULINE_MAGENTA(text)     COLOR_ULINE_MAGENTA text  COLOR_RESET
    #define ULINE_CYAN(text)        COLOR_ULINE_CYAN    text  COLOR_RESET
    #define ULINE_WHITE(text)       COLOR_ULINE_WHITE   text  COLOR_RESET

    // Background (Dark)
    #define COLOR_BKG_BLACK           "\x1b[40m" // dark black is just black
    #define COLOR_BKG_DARK_RED        "\x1b[41m"
    #define COLOR_BKG_DARK_GREEN      "\x1b[42m"
    #define COLOR_BKG_DARK_YELLOW     "\x1b[43m"
    #define COLOR_BKG_DARK_BLUE       "\x1b[44m"
    #define COLOR_BKG_DARK_MAGENTA    "\x1b[45m"
    #define COLOR_BKG_DARK_CYAN       "\x1b[46m"
    #define COLOR_BKG_LIGHT_GRAY      "\x1b[47m" // Dark White is actually light gray
    #define BKG_BLACK(text)           COLOR_BKG_BLACK      text  COLOR_RESET
    #define BKG_DARK_RED(text)        COLOR_BKG_DARK_RED        text  COLOR_RESET
    #define BKG_DARK_GREEN(text)      COLOR_BKG_DARK_GREEN      text  COLOR_RESET
    #define BKG_DARK_YELLOW(text)     COLOR_BKG_DARK_YELLOW     text  COLOR_RESET
    #define BKG_DARK_BLUE(text)       COLOR_BKG_DARK_BLUE       text  COLOR_RESET
    #define BKG_DARK_MAGENTA(text)    COLOR_BKG_DARK_MAGENTA    text  COLOR_RESET
    #define BKG_DARK_CYAN(text)       COLOR_BKG_DARK_CYAN       text  COLOR_RESET
    #define BKG_LIGHT_GRAY(text)      COLOR_BKG_LIGHT_GRAY      text  COLOR_RESET

    // High Intensity (bright)
    #define COLOR_DARK_GRAY "\x1b[90m" // black bright is just dark gray
    #define COLOR_RED       "\x1b[91m"
    #define COLOR_GREEN     "\x1b[92m"
    #define COLOR_YELLOW    "\x1b[93m"
    #define COLOR_BLUE      "\x1b[94m"
    #define COLOR_MAGENTA   "\x1b[95m"
    #define COLOR_CYAN      "\x1b[96m"
    #define COLOR_WHITE     "\x1b[97m"
    #define DARK_GRAY(text) COLOR_DARK_GRAY text  COLOR_RESET
    #define RED(text)       COLOR_RED       text  COLOR_RESET
    #define GREEN(text)     COLOR_GREEN     text  COLOR_RESET
    #define ORANGE(text)    COLOR_YELLOW    text  COLOR_RESET
    #define BLUE(text)      COLOR_BLUE      text  COLOR_RESET
    #define MAGENTA(text)   COLOR_MAGENTA   text  COLOR_RESET
    #define CYAN(text)      COLOR_CYAN      text  COLOR_RESET
    #define WHITE(text)     COLOR_WHITE     text  COLOR_RESET

    // Bold High Intensity
    #define COLOR_BOLD_DARK_GRAY  "\x1b[1;90m"
    #define COLOR_BOLD_RED        "\x1b[1;91m"
    #define COLOR_BOLD_GREEN      "\x1b[1;92m"
    #define COLOR_BOLD_YELLOW     "\x1b[1;93m"
    #define COLOR_BOLD_BLUE       "\x1b[1;94m"
    #define COLOR_BOLD_MAGENTA    "\x1b[1;95m"
    #define COLOR_BOLD_CYAN       "\x1b[1;96m"
    #define COLOR_BOLD_WHITE      "\x1b[1;97m"
    #define BOLD_DARK_GRAY(text)  COLOR_BOLD_DARK_GRAY  text  COLOR_RESET
    #define BOLD_RED(text)        COLOR_BOLD_RED        text  COLOR_RESET
    #define BOLD_GREEN(text)      COLOR_BOLD_GREEN      text  COLOR_RESET
    #define BOLD_YELLOW(text)     COLOR_BOLD_YELLOW     text  COLOR_RESET
    #define BOLD_BLUE(text)       COLOR_BOLD_BLUE       text  COLOR_RESET
    #define BOLD_MAGENTA(text)    COLOR_BOLD_MAGENTA    text  COLOR_RESET
    #define BOLD_CYAN(text)       COLOR_BOLD_CYAN       text  COLOR_RESET
    #define BOLD_WHITE(text)      COLOR_BOLD_WHITE      text  COLOR_RESET

    // High Intensity Backgrounds
    #define COLOR_BKG_DARK_GRAY   "\x1b[0;100m"
    #define COLOR_BKG_RED         "\x1b[0;101m"
    #define COLOR_BKG_GREEN       "\x1b[0;102m"
    #define COLOR_BKG_YELLOW      "\x1b[0;103m"
    #define COLOR_BKG_BLUE        "\x1b[0;104m"
    #define COLOR_BKG_MAGENTA     "\x1b[0;105m"
    #define COLOR_BKG_CYAN        "\x1b[0;106m"
    #define COLOR_BKG_WHITE       "\x1b[0;107m"
    #define BKG_DARK_GRAY(text)   COLOR_BKG_DARK_GRAY   text  COLOR_RESET
    #define BKG_RED(text)         COLOR_BKG_RED         text  COLOR_RESET
    #define BKG_GREEN(text)       COLOR_BKG_GREEN       text  COLOR_RESET
    #define BKG_YELLOW(text)      COLOR_BKG_YELLOW      text  COLOR_RESET
    #define BKG_BLUE(text)        COLOR_BKG_BLUE        text  COLOR_RESET
    #define BKG_MAGENTA(text)     COLOR_BKG_MAGENTA     text  COLOR_RESET
    #define BKG_CYAN(text)        COLOR_BKG_CYAN        text  COLOR_RESET
    #define BKG_WHITE(text)       COLOR_BKG_WHITE       text  COLOR_RESET

#endif // LOG_COLORS
