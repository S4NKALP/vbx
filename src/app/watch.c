#include "app/watch.h"
#include "app/process.h"
#include "common/log.h"
#include "common/utils.h"
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/inotify.h>
#include <time.h>
#include <unistd.h>

struct inotify_thread_args {
  char path[1024];
};

static void *inotify_thread_fn(void *arg) {
  struct inotify_thread_args *a = (struct inotify_thread_args *)arg;
  int fd = inotify_init1(IN_NONBLOCK);
  if (fd < 0) {
    LOG_PERROR("Failed to initialize inotify");
    return NULL;
  }
  int wd = inotify_add_watch(
      fd, a->path, IN_CLOSE_WRITE | IN_MOVED_TO | IN_ATTRIB | IN_MODIFY);
  if (wd < 0) {
    LOG_PERROR("Failed to add inotify watch");
    close(fd);
    return NULL;
  }
  LOG_INFO("Watching config file: %s", a->path);
  char buf[4096] __attribute__((aligned(__alignof__(struct inotify_event))));
  while (1) {
    ssize_t len = read(fd, buf, sizeof(buf));
    if (len <= 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        struct timespec ts;
        ts.tv_sec = 0;
        ts.tv_nsec = 100000000L;
        nanosleep(&ts, NULL);
        continue;
      }
      LOG_PERROR("inotify read error");
      break;
    }
    LOG_INFO("Config file changed, requesting reload...");
    handle_sighup(0);
  }
  inotify_rm_watch(fd, wd);
  close(fd);
  return NULL;
}

int start_config_watcher(const char *path) {
  static pthread_t inotify_thread;
  static struct inotify_thread_args in_args;
  safe_strncpy(in_args.path, path, sizeof(in_args.path));
  if (access(in_args.path, F_OK) == 0) {
    pthread_create(&inotify_thread, NULL, inotify_thread_fn, &in_args);
    pthread_detach(inotify_thread);
    return 1;
  } else {
    LOG_WARN("Warning: Config file %s does not exist, file watching disabled",
           in_args.path);
    return 0;
  }
}
