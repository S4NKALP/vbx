
#ifndef KEYVIBE_UTILS_H
#define KEYVIBE_UTILS_H

#include <stddef.h>
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

#endif // KEYVIBE_UTILS_H
