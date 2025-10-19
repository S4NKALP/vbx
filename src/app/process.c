#define _POSIX_C_SOURCE 200809L
#include "app/process.h"
#include "common/utils.h"
#include "config.h"
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#define MAX_PATH_LENGTH 1024

pid_t keyboard_pid = 0;
pid_t sound_pid = 0;
char pidfile_path[MAX_PATH_LENGTH] = {0};
int is_daemon = 0;
volatile sig_atomic_t reload_requested = 0;

void cleanup_processes(int sig) {
  (void)sig;
  if (!is_daemon) {
    printf("\nShutting down VBX daemon...\n");
  }
  if (sound_pid > 0) {
    kill(sound_pid, SIGTERM);
    waitpid(sound_pid, NULL, 0);
  }
  if (keyboard_pid > 0) {
    kill(keyboard_pid, SIGTERM);
    waitpid(keyboard_pid, NULL, 0);
  }
  if (pidfile_path[0] != '\0') {
    unlink(pidfile_path);
  }
  exit(0);
}

int require_running_pid(pid_t *out_pid) {
  build_pidfile_path(pidfile_path, sizeof(pidfile_path));
  pid_t running_pid = 0;
  if (!read_pidfile(pidfile_path, &running_pid) ||
      !process_is_running(running_pid)) {
    fprintf(stderr, "VBX daemon is not running.\n");
    return 0;
  }
  *out_pid = running_pid;
  return 1;
}

void daemonize_self(void) {
  pid_t pid = fork();
  if (pid < 0)
    exit(1);
  if (pid > 0)
    exit(0);
  if (setsid() < 0)
    exit(1);
  pid = fork();
  if (pid < 0)
    exit(1);
  if (pid > 0)
    exit(0);
  umask(0);
  chdir("/");
  int fd = open("/dev/null", O_RDWR);
  if (fd >= 0) {
    dup2(fd, STDIN_FILENO);
    dup2(fd, STDOUT_FILENO);
    dup2(fd, STDERR_FILENO);
    if (fd > 2)
      close(fd);
  }
}

void stop_children(void) {
  if (sound_pid > 0) {
    kill(sound_pid, SIGTERM);
    waitpid(sound_pid, NULL, 0);
    sound_pid = 0;
  }
  if (keyboard_pid > 0) {
    kill(keyboard_pid, SIGTERM);
    waitpid(keyboard_pid, NULL, 0);
    keyboard_pid = 0;
  }
}


int start_children(const char *sound_dir, const char *config_path, int volume,
                   int verbose, int mute, const char *mouse_sound_dir,
                   const char *mouse_config_path, int mouse_volume,
                   int keyboard_mute, int mouse_mute, int keyboard_enabled,
                   int mouse_enabled) {
  (void)config_path;
  (void)mouse_sound_dir;
  int pipefd[2];
  if (pipe(pipefd) == -1) {
    perror("pipe");
    return 0;
  }
  sound_pid = fork();
  if (sound_pid == -1) {
    perror("fork");
    close(pipefd[0]);
    close(pipefd[1]);
    return 0;
  }
  char sound_player_path[MAX_PATH_LENGTH];
  snprintf(sound_player_path, sizeof(sound_player_path), "%s/vbx-audio",
           VBX_BIN_DIR);
  if (sound_pid == 0) {
    close(pipefd[1]);
    dup2(pipefd[0], STDIN_FILENO);
    close(pipefd[0]);
    if (chdir(sound_dir) != 0) {
      perror("chdir");
      exit(1);
    }
    char volume_str[32], mouse_volume_str[32], verbose_str[8], mute_str[8];
    char keyboard_mute_str[8], mouse_mute_str[8];
    char keyboard_enabled_str[8], mouse_enabled_str[8];
    
    int_to_str(volume_str, sizeof(volume_str), volume);
    int_to_str(mouse_volume_str, sizeof(mouse_volume_str), mouse_volume);
    int_to_str(verbose_str, sizeof(verbose_str), verbose);
    int_to_str(mute_str, sizeof(mute_str), mute);
    int_to_str(keyboard_mute_str, sizeof(keyboard_mute_str), keyboard_mute);
    int_to_str(mouse_mute_str, sizeof(mouse_mute_str), mouse_mute);
    int_to_str(keyboard_enabled_str, sizeof(keyboard_enabled_str), keyboard_enabled);
    int_to_str(mouse_enabled_str, sizeof(mouse_enabled_str), mouse_enabled);
    
    execl(sound_player_path, "vbx-audio", "config.json", volume_str,
          verbose_str, mute_str, mouse_config_path, mouse_volume_str,
          keyboard_mute_str, mouse_mute_str, keyboard_enabled_str,
          mouse_enabled_str, (char *)NULL);
    perror("execl vbx-audio");
    exit(1);
  }
  keyboard_pid = fork();
  if (keyboard_pid == -1) {
    perror("fork");
    kill(sound_pid, SIGTERM);
    close(pipefd[0]);
    close(pipefd[1]);
    return 0;
  }
  char get_key_presses_path[MAX_PATH_LENGTH];
  snprintf(get_key_presses_path, sizeof(get_key_presses_path),
           "%s/vbx-input", VBX_BIN_DIR);
  if (keyboard_pid == 0) {
    close(pipefd[0]);
    dup2(pipefd[1], STDOUT_FILENO);
    close(pipefd[1]);
    execl(get_key_presses_path, "vbx-input", (char *)NULL);
    perror("execl vbx-input");
    exit(1);
  }
  close(pipefd[0]);
  close(pipefd[1]);
  return 1;
}

void handle_sighup(int sig) {
  (void)sig;
  reload_requested = 1;
}
