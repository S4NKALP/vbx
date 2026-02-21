#ifndef VBX_LOG_H
#define VBX_LOG_H

#include <stdio.h>
#include <time.h>
#include <errno.h>
#include <string.h>

// Assuming g_verbose is defined globally or we pass it
extern int g_verbose;
extern int is_daemon;

#define LOG_LEVEL_DEBUG 0
#define LOG_LEVEL_INFO  1
#define LOG_LEVEL_WARN  2
#define LOG_LEVEL_ERROR 3

#define LOG_MACRO(level, prefix, out_stream, ...) do { \
    if (level == LOG_LEVEL_DEBUG && !g_verbose) break; \
    flockfile(out_stream); \
    fprintf(out_stream, "[VBX-%s] ", prefix); \
    fprintf(out_stream, __VA_ARGS__); \
    fprintf(out_stream, "\n"); \
    fflush(out_stream); \
    funlockfile(out_stream); \
} while(0)

#define LOG_DEBUG(...)  LOG_MACRO(LOG_LEVEL_DEBUG, "DEBUG", stdout, __VA_ARGS__)
#define LOG_INFO(...)   LOG_MACRO(LOG_LEVEL_INFO,  "INFO",  stdout, __VA_ARGS__)
#define LOG_WARN(...)   LOG_MACRO(LOG_LEVEL_WARN,  "WARN",  stderr, __VA_ARGS__)
#define LOG_ERROR(...)  LOG_MACRO(LOG_LEVEL_ERROR, "ERROR", stderr, __VA_ARGS__)
#define LOG_PERROR(msg) LOG_MACRO(LOG_LEVEL_ERROR, "ERROR", stderr, "%s: %s", msg, strerror(errno))

#endif // VBX_LOG_H
