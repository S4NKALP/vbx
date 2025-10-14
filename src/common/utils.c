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
  if (!safe_snprintf(mute_file, sizeof(mute_file), "%s/keyvibe-mute-%d", rd,
                     (int)getuid()))
    return 0;
  FILE *f = fopen(mute_file, "r");
  if (!f)
    return 0;
  int mute = 0;
  if (fscanf(f, "%d", &mute) == 1) {
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
  if (!safe_snprintf(state_file, sizeof(state_file), "%s/keyvibe-%s-%d", rd,
                     filename_suffix, (int)getuid()))
    return;
  FILE *f = fopen(state_file, "w");
  if (!f)
    return;
  fprintf(f, "%d\n", value);
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
  safe_snprintf(buffer, buflen, "%s/keyvibe-%d.pid", rd, (int)getuid());
}

int read_pidfile(const char *path, pid_t *out_pid) {
  FILE *f = fopen(path, "r");
  if (!f)
    return 0;
  long val = -1;
  if (fscanf(f, "%ld", &val) == 1 && val > 0) {
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
  fprintf(f, "%ld\n", (long)pid);
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
  snprintf(buffer, size, "%d", value);
}

// Safe string duplication
char *xstrdup(const char *s) {
  if (!s)
    return NULL;
  size_t n = strlen(s) + 1;
  char *p = (char *)malloc(n);
  if (!p)
    return NULL;
  memcpy(p, s, n);
  return p;
}

// Validate and clamp volume to 0-100 range
int validate_volume(int volume) {
  if (volume < 0) return 0;
  if (volume > 100) return 100;
  return volume;
}
