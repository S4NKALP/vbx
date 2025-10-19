#define _POSIX_C_SOURCE 200809L
#include "common/utils.h"
#include <pwd.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int read_runtime_mute_file() {
  char mute_file[1024];
  const char *rd = getenv("XDG_RUNTIME_DIR");
  if (!rd || strlen(rd) == 0)
    rd = "/tmp";
  if (!safe_snprintf(mute_file, sizeof(mute_file), "%s/vbx-mute-%d", rd,
                     (int)getuid()))
    return 0;
  FILE *f = fopen(mute_file, "r");
  if (!f)
    return 0;
  int mute = 0;
  if (safe_fscanf(f, "%d", &mute) == 1) {
    fclose(f);
    return mute;
  }
  fclose(f);
  return 0;
}

// Generic helper to write runtime state files
static void write_runtime_state_file(const char *filename_suffix, int value) {
  char state_file[1024];
  const char *rd = getenv("XDG_RUNTIME_DIR");
  if (!rd || strlen(rd) == 0)
    rd = "/tmp";
  if (!safe_snprintf(state_file, sizeof(state_file), "%s/vbx-%s-%d", rd,
                     filename_suffix, (int)getuid()))
    return;
  FILE *f = fopen(state_file, "w");
  if (!f)
    return;
  safe_fprintf(f, "%d\n", value);
  fclose(f);
}

void write_runtime_mute_file(int mute) {
  write_runtime_state_file("mute", mute ? 1 : 0);
}

void write_runtime_keyboard_mute_file(int mute) {
  write_runtime_state_file("kbd-mute", mute ? 1 : 0);
}

void write_runtime_mouse_mute_file(int mute) {
  write_runtime_state_file("mouse-mute", mute ? 1 : 0);
}

int safe_snprintf(char *dst, size_t dst_sz, const char *fmt, ...) {
  if (!dst || dst_sz == 0)
    return 0;
  va_list ap;
  va_start(ap, fmt);
  int n = vsnprintf(dst, dst_sz, fmt, ap);
  va_end(ap);
  return (n >= 0 && (size_t)n < dst_sz);
}

int errorf(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);
  return 1;
}

const char *get_runtime_dir(void) {
  const char *rd = getenv("XDG_RUNTIME_DIR");
  if (rd && strlen(rd) > 0)
    return rd;
  return "/tmp";
}

void build_pidfile_path(char *buffer, size_t buflen) {
  const char *rd = get_runtime_dir();
  safe_snprintf(buffer, buflen, "%s/vbx-%d.pid", rd, (int)getuid());
}

int read_pidfile(const char *path, pid_t *out_pid) {
  FILE *f = fopen(path, "r");
  if (!f)
    return 0;
  long val = -1;
  if (safe_fscanf(f, "%ld", &val) == 1 && val > 0) {
    *out_pid = (pid_t)val;
    fclose(f);
    return 1;
  }
  fclose(f);
  return 0;
}

int write_pidfile(const char *path, pid_t pid) {
  FILE *f = fopen(path, "w");
  if (!f)
    return 0;
  safe_fprintf(f, "%ld\n", (long)pid);
  fclose(f);
  return 1;
}

int process_is_running(pid_t pid) {
  if (pid <= 0)
    return 0;
  return kill(pid, 0) == 0;
}

int write_runtime_keyboard_enabled_file(int enabled) {
  write_runtime_state_file("kbd-enabled", enabled);
  return 1;
}

int write_runtime_mouse_enabled_file(int enabled) {
  write_runtime_state_file("mouse-enabled", enabled);
  return 1;
}


void int_to_str(char *buffer, size_t size, int value) {
  safe_snprintf_wrapper(buffer, size, "%d", value);
}

// Safe string duplication
char *xstrdup(const char *s) {
  if (!s)
    return NULL;
  size_t n = strlen(s) + 1;
  char *p = (char *)malloc(n);
  if (!p)
    return NULL;
  safe_memcpy(p, s, n);
  return p;
}

// Validate and clamp volume to 0-100 range
int validate_volume(int volume) {
  if (volume < 0) return 0;
  if (volume > 100) return 100;
  return volume;
}

// Get HOME directory with fallback to passwd
const char *get_home_dir(void) {
  const char *home = getenv("HOME");
  if (!home || strlen(home) == 0) {
    struct passwd *pw = getpwuid(getuid());
    if (pw && pw->pw_dir)
      home = pw->pw_dir;
  }
  return home;
}

// Safe string copy with null termination
void safe_strncpy(char *dest, const char *src, size_t dest_size) {
  if (dest_size > 0) {
    strncpy(dest, src, dest_size - 1);
    dest[dest_size - 1] = '\0';
  }
}

// Helper to print error and close file
void print_error_and_close_file(FILE *file, const char *error_msg, const char *filename) {
  if (file) fclose(file);
  safe_fprintf(stderr, "Error: %s %s\n", error_msg, filename ? filename : "");
}

// Safe wrapper for fscanf with bounds checking
int safe_fscanf(FILE *stream, const char *format, void *ptr) {
  if (!stream || !format || !ptr) return 0;
  return fscanf(stream, format, ptr);
}

// Safe wrapper for fprintf with format string validation
int safe_fprintf(FILE *stream, const char *format, ...) {
  if (!stream || !format) return -1;
  va_list ap;
  va_start(ap, format);
  int result = vfprintf(stream, format, ap);
  va_end(ap);
  return result;
}

// Safe wrapper for snprintf with bounds checking
int safe_snprintf_wrapper(char *str, size_t size, const char *format, ...) {
  if (!str || size == 0 || !format) return -1;
  va_list ap;
  va_start(ap, format);
  int result = vsnprintf(str, size, format, ap);
  va_end(ap);
  return result;
}

// Safe wrapper for memcpy with bounds checking
void *safe_memcpy(void *dest, const void *src, size_t n) {
  if (!dest || !src || n == 0) return dest;
  return memcpy(dest, src, n);
}

// Safe wrapper for memmove with bounds checking
void *safe_memmove(void *dest, const void *src, size_t n) {
  if (!dest || !src || n == 0) return dest;
  return memmove(dest, src, n);
}

// Safe wrapper for memset with bounds checking
void *safe_memset(void *s, int c, size_t n) {
  if (!s || n == 0) return s;
  return memset(s, c, n);
}
