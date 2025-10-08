#define _POSIX_C_SOURCE 200809L
#include "utils.h"
#include "config.h"
#include <pwd.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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
