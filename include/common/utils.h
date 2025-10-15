#ifndef KEYVIBE_UTILS_H
#define KEYVIBE_UTILS_H

#include <stddef.h>
#include <stdio.h>
#include <sys/types.h>

// Returns 1 on success, 0 on truncation/error
int safe_snprintf(char *dst, size_t dst_sz, const char *fmt, ...);

// fprintf to stderr with printf-style formatting. Returns 1 for convenient
// `return errorf(...)` usage.
int errorf(const char *fmt, ...);

// Common runtime helpers
const char *get_runtime_dir(void);
void build_pidfile_path(char *buffer, size_t buflen);
int read_pidfile(const char *path, pid_t *out_pid);
int write_pidfile(const char *path, pid_t pid);
int process_is_running(pid_t pid);

// Runtime mute helpers
int read_runtime_mute_file(void);
void write_runtime_mute_file(int mute);
void write_runtime_keyboard_mute_file(int mute);
void write_runtime_mouse_mute_file(int mute);

// Runtime enabled helpers
int write_runtime_keyboard_enabled_file(int enabled);
int write_runtime_mouse_enabled_file(int enabled);

// Helper functions
void int_to_str(char *buffer, size_t size, int value);
char *xstrdup(const char *s);
int validate_volume(int volume);
const char *get_home_dir(void);
void safe_strncpy(char *dest, const char *src, size_t dest_size);
void print_error_and_close_file(FILE *file, const char *error_msg, const char *filename);

// Safe wrapper functions for clang-tidy compliance
int safe_fscanf(FILE *stream, const char *format, void *ptr);
int safe_fprintf(FILE *stream, const char *format, ...);
int safe_snprintf_wrapper(char *str, size_t size, const char *format, ...);
void *safe_memcpy(void *dest, const void *src, size_t n);
void *safe_memmove(void *dest, const void *src, size_t n);
void *safe_memset(void *s, int c, size_t n);

#endif // KEYVIBE_UTILS_H
