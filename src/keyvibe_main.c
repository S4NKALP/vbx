#define _POSIX_C_SOURCE 200809L

#include "config.h"
#include "utils.h"
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <json-c/json.h>
#include <pthread.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/inotify.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define MAX_PATH_LENGTH 1024
#define AUDIO_BASE_DIR KeyVibe_DATA_DIR "/soundpacks"
#define KEYBOARD_AUDIO_DIR AUDIO_BASE_DIR "/keyboard"
#define MOUSE_AUDIO_DIR AUDIO_BASE_DIR "/mouse"
#define USER_KEYBOARD_AUDIO_SUBPATH "/.local/share/keyvibe/soundpacks/keyboard"
#define USER_MOUSE_AUDIO_SUBPATH "/.local/share/keyvibe/soundpacks/mouse"
#define USER_CONFIG_FILENAME ".keyvibe.json"

// Global variables for cleanup
pid_t keyboard_pid = 0;
pid_t sound_pid = 0;
static char pidfile_path[MAX_PATH_LENGTH] = {0};
static int is_daemon = 0;
static volatile sig_atomic_t reload_requested = 0;
static volatile sig_atomic_t keyboard_reload_requested = 0;
static volatile sig_atomic_t mouse_reload_requested = 0;
static volatile sig_atomic_t volume_reload_requested = 0;
static int current_keyboard_volume = 50;
static int current_mouse_volume = 50;
static char current_sound_name[MAX_PATH_LENGTH] = {0};
static char current_mouse_sound_name[MAX_PATH_LENGTH] = {0};
static int current_verbose = 0;
static char current_config_path[MAX_PATH_LENGTH] = {0};
static char current_sound_dir[MAX_PATH_LENGTH] = {0};
static char current_mouse_config_path[MAX_PATH_LENGTH] = {0};
static char current_mouse_sound_dir[MAX_PATH_LENGTH] = {0};
static int current_mute = 0;

void print_usage(const char *program_name) {
  printf("KeyVibe - Mechanical Keyboard Sound Simulator\n\n");
  printf("Usage: %s [OPTIONS]\n\n", program_name);
  printf("Options:\n");
  printf("  -S, --sound SOUND_NAME   Select keyboard sound pack (default: eg-oreo)\n");
  printf("  -M, --mouse SOUND_NAME   Select mouse sound pack (default: ping)\n");
  printf("  -V, --volume VOLUME      Set volume [0-100] for both keyboard and mouse (default: 50)\n");
  printf("  -K, --keyboard-volume VOLUME  Set keyboard volume [0-100] (default: 50)\n");
  printf("  -O, --mouse-volume VOLUME     Set mouse volume [0-100] (default: 50)\n");
  printf("  -l, --list               List available sound packs\n");
  printf("  -d, --daemon             Run in background (write PID file)\n");
  printf("  -s, --stop               Stop background daemon\n");
  printf("  -m, --mute               Mute sound\n");
  printf("  -u, --unmute             Unmute sound\n");
  printf("  -h, --help               Show this help message\n");
  printf("  -v, --verbose            Enable verbose output\n");
  printf("  In daemon mode, editing ~/.keyvibe.json will auto-reload.\n");
}

int list_sound_packs() {
  DIR *dir;
  struct dirent *entry;
  char path[MAX_PATH_LENGTH];
  struct stat st;

  // Determine user audio dirs
  char user_keyboard_dir[MAX_PATH_LENGTH] = {0};
  char user_mouse_dir[MAX_PATH_LENGTH] = {0};
  const char *home = getenv("HOME");
  if (!home || strlen(home) == 0) {
    struct passwd *pw = getpwuid(getuid());
    if (pw && pw->pw_dir) home = pw->pw_dir;
  }
  if (home) {
    snprintf(user_keyboard_dir, sizeof(user_keyboard_dir), "%s%s", home, USER_KEYBOARD_AUDIO_SUBPATH);
    snprintf(user_mouse_dir, sizeof(user_mouse_dir), "%s%s", home, USER_MOUSE_AUDIO_SUBPATH);
  }

  printf("Available sound packs:\n");
  printf("======================\n\n");

  printf("Keyboard Sound Packs:\n");
  printf("---------------------\n");

  // List user keyboard packs first (if available)
  if (user_keyboard_dir[0] && (dir = opendir(user_keyboard_dir)) != NULL) {
    while ((entry = readdir(dir)) != NULL) {
      if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
      if ((size_t)snprintf(path, sizeof(path), "%s/%s", user_keyboard_dir, entry->d_name) >= sizeof(path)) {
        fprintf(stderr, "Warning: Path too long, skipping: %s/%s\n", user_keyboard_dir, entry->d_name);
        continue;
      }
      if (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) {
        printf("  %s (user)\n", entry->d_name);
      }
    }
    closedir(dir);
  }

  // Then list system keyboard packs
  dir = opendir(KEYBOARD_AUDIO_DIR);
  if (dir != NULL) {
    while ((entry = readdir(dir)) != NULL) {
      if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
      if ((size_t)snprintf(path, sizeof(path), "%s/%s", KEYBOARD_AUDIO_DIR, entry->d_name) >= sizeof(path)) {
        fprintf(stderr, "Warning: Path too long, skipping: %s/%s\n", KEYBOARD_AUDIO_DIR, entry->d_name);
        continue;
      }
      if (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) {
        printf("  %s (system)\n", entry->d_name);
      }
    }
    closedir(dir);
  }

  printf("\nMouse Sound Packs:\n");
  printf("------------------\n");

  // List user mouse packs first (if available)
  if (user_mouse_dir[0] && (dir = opendir(user_mouse_dir)) != NULL) {
    while ((entry = readdir(dir)) != NULL) {
      if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
      if ((size_t)snprintf(path, sizeof(path), "%s/%s", user_mouse_dir, entry->d_name) >= sizeof(path)) {
        fprintf(stderr, "Warning: Path too long, skipping: %s/%s\n", user_mouse_dir, entry->d_name);
        continue;
      }
      if (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) {
        printf("  %s (user)\n", entry->d_name);
      }
    }
    closedir(dir);
  }

  // Then list system mouse packs
  dir = opendir(MOUSE_AUDIO_DIR);
  if (dir != NULL) {
    while ((entry = readdir(dir)) != NULL) {
      if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
      if ((size_t)snprintf(path, sizeof(path), "%s/%s", MOUSE_AUDIO_DIR, entry->d_name) >= sizeof(path)) {
        fprintf(stderr, "Warning: Path too long, skipping: %s/%s\n", MOUSE_AUDIO_DIR, entry->d_name);
        continue;
      }
      if (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) {
        printf("  %s (system)\n", entry->d_name);
      }
    }
    closedir(dir);
  }

  return 0;
}

static void get_user_keyboard_audio_dir(char *buffer, size_t buflen) {
  buffer[0] = '\0';
  const char *home = getenv("HOME");
  if (!home || strlen(home) == 0) {
    struct passwd *pw = getpwuid(getuid());
    if (pw && pw->pw_dir) home = pw->pw_dir;
  }
  if (home) snprintf(buffer, buflen, "%s%s", home, USER_KEYBOARD_AUDIO_SUBPATH);
}

static void get_user_mouse_audio_dir(char *buffer, size_t buflen) {
  buffer[0] = '\0';
  const char *home = getenv("HOME");
  if (!home || strlen(home) == 0) {
    struct passwd *pw = getpwuid(getuid());
    if (pw && pw->pw_dir) home = pw->pw_dir;
  }
  if (home) snprintf(buffer, buflen, "%s%s", home, USER_MOUSE_AUDIO_SUBPATH);
}

static int resolve_keyboard_sound_base_dir(const char *sound_name, char *out_basedir, size_t out_sz) {
  char user_audio_dir[MAX_PATH_LENGTH];
  get_user_keyboard_audio_dir(user_audio_dir, sizeof(user_audio_dir));
  if (user_audio_dir[0]) {
    char p[MAX_PATH_LENGTH];
    if ((size_t)snprintf(p, sizeof(p), "%s/%s/config.json", user_audio_dir, sound_name) < sizeof(p)
        && access(p, R_OK) == 0) {
      strncpy(out_basedir, user_audio_dir, out_sz - 1); out_basedir[out_sz-1] = '\0';
      return 1;
    }
  }
  // Fallback to system dir
  char p2[MAX_PATH_LENGTH];
  if ((size_t)snprintf(p2, sizeof(p2), "%s/%s/config.json", KEYBOARD_AUDIO_DIR, sound_name) >= sizeof(p2)) {
    return 0; // Path too long
  }
  if (access(p2, R_OK) == 0) {
    strncpy(out_basedir, KEYBOARD_AUDIO_DIR, out_sz - 1); out_basedir[out_sz-1] = '\0';
    return 1;
  }
  return 0;
}

static int resolve_mouse_sound_base_dir(const char *sound_name, char *out_basedir, size_t out_sz) {
  char user_audio_dir[MAX_PATH_LENGTH];
  get_user_mouse_audio_dir(user_audio_dir, sizeof(user_audio_dir));
  if (user_audio_dir[0]) {
    char p[MAX_PATH_LENGTH];
    if ((size_t)snprintf(p, sizeof(p), "%s/%s/config.json", user_audio_dir, sound_name) < sizeof(p)
        && access(p, R_OK) == 0) {
      strncpy(out_basedir, user_audio_dir, out_sz - 1); out_basedir[out_sz-1] = '\0';
      return 1;
    }
  }
  // Fallback to system dir
  char p2[MAX_PATH_LENGTH];
  if ((size_t)snprintf(p2, sizeof(p2), "%s/%s/config.json", MOUSE_AUDIO_DIR, sound_name) >= sizeof(p2)) {
    return 0; // Path too long
  }
  if (access(p2, R_OK) == 0) {
    strncpy(out_basedir, MOUSE_AUDIO_DIR, out_sz - 1); out_basedir[out_sz-1] = '\0';
    return 1;
  }
  return 0;
}

int validate_keyboard_sound_pack(const char *sound_name) {
  char basedir[MAX_PATH_LENGTH];
  if (!resolve_keyboard_sound_base_dir(sound_name, basedir, sizeof(basedir))) {
    fprintf(stderr, "Error: Keyboard sound pack '%s' not found in user or system dirs.\n", sound_name);
    fprintf(stderr, "Use --list to see available sound packs.\n");
    return 0;
  }
  char config_path[MAX_PATH_LENGTH];
  if ((size_t)snprintf(config_path, sizeof(config_path), "%s/%s/config.json", basedir, sound_name) >= sizeof(config_path)) {
    fprintf(stderr, "Error: Sound pack path too long: %s/%s\n", basedir, sound_name);
    return 0;
  }
  if (access(config_path, R_OK) != 0) {
    fprintf(stderr, "Error: Config file not found: %s\n", config_path);
    return 0;
  }
  return 1;
}

int validate_mouse_sound_pack(const char *sound_name) {
  char basedir[MAX_PATH_LENGTH];
  if (!resolve_mouse_sound_base_dir(sound_name, basedir, sizeof(basedir))) {
    fprintf(stderr, "Error: Mouse sound pack '%s' not found in user or system dirs.\n", sound_name);
    fprintf(stderr, "Use --list to see available sound packs.\n");
    return 0;
  }
  char config_path[MAX_PATH_LENGTH];
  if ((size_t)snprintf(config_path, sizeof(config_path), "%s/%s/config.json", basedir, sound_name) >= sizeof(config_path)) {
    fprintf(stderr, "Error: Sound pack path too long: %s/%s\n", basedir, sound_name);
    return 0;
  }
  if (access(config_path, R_OK) != 0) {
    fprintf(stderr, "Error: Config file not found: %s\n", config_path);
    return 0;
  }
  return 1;
}

void cleanup_processes(int sig) {
  (void)sig;
  if (!is_daemon) {
    printf("\nShutting down KeyVibe...\n");
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

static char *get_user_config_path(char *buffer, size_t buflen) {
  const char *home = getenv("HOME");
  if (!home || strlen(home) == 0) {
    struct passwd *pw = getpwuid(getuid());
    if (pw && pw->pw_dir)
      home = pw->pw_dir;
  }
  if (!home)
    return NULL;
  if (!safe_snprintf(buffer, buflen, "%s/%s", home, USER_CONFIG_FILENAME))
    return NULL;
  return buffer;
}


static int build_paths_for_keyboard_sound(const char *sound_name, char *out_config_path,
                                 size_t out_config_sz, char *out_sound_dir,
                                 size_t out_sound_sz) {
  char basedir[MAX_PATH_LENGTH];
  if (!resolve_keyboard_sound_base_dir(sound_name, basedir, sizeof(basedir))) {
    return 0;
  }
  size_t base_len = strlen(basedir);
  size_t name_len = strlen(sound_name);
  const char *cfg_suffix = "/config.json";
  size_t cfg_suffix_len = strlen(cfg_suffix);
  if (base_len + 1 + name_len + cfg_suffix_len + 1 > out_config_sz) {
    fprintf(stderr, "Error: sound pack name too long for path buffer.\n");
    return 0;
  }
  if (base_len + 1 + name_len + 1 > out_sound_sz) {
    fprintf(stderr, "Error: sound pack name too long for directory buffer.\n");
    return 0;
  }
  if (!safe_snprintf(out_config_path, out_config_sz, "%s/%s%s", basedir, sound_name, cfg_suffix))
    return 0;
  if (!safe_snprintf(out_sound_dir, out_sound_sz, "%s/%s", basedir, sound_name))
    return 0;
  return 1;
}

static int build_paths_for_mouse_sound(const char *sound_name, char *out_config_path,
                                 size_t out_config_sz, char *out_sound_dir,
                                 size_t out_sound_sz) {
  char basedir[MAX_PATH_LENGTH];
  if (!resolve_mouse_sound_base_dir(sound_name, basedir, sizeof(basedir))) {
    return 0;
  }
  size_t base_len = strlen(basedir);
  size_t name_len = strlen(sound_name);
  const char *cfg_suffix = "/config.json";
  size_t cfg_suffix_len = strlen(cfg_suffix);
  if (base_len + 1 + name_len + cfg_suffix_len + 1 > out_config_sz) {
    fprintf(stderr, "Error: sound pack name too long for path buffer.\n");
    return 0;
  }
  if (base_len + 1 + name_len + 1 > out_sound_sz) {
    fprintf(stderr, "Error: sound pack name too long for directory buffer.\n");
    return 0;
  }
  if (!safe_snprintf(out_config_path, out_config_sz, "%s/%s%s", basedir, sound_name, cfg_suffix))
    return 0;
  if (!safe_snprintf(out_sound_dir, out_sound_sz, "%s/%s", basedir, sound_name))
    return 0;
  return 1;
}


// Helper: ensure daemon is running, return its pid or print a consistent error
static int require_running_pid(pid_t *out_pid) {
  build_pidfile_path(pidfile_path, sizeof(pidfile_path));
  pid_t running_pid = 0;
  if (!read_pidfile(pidfile_path, &running_pid) || !process_is_running(running_pid)) {
    fprintf(stderr, "KeyVibe: not running.\n");
    return 0;
  }
  *out_pid = running_pid;
  return 1;
}

static void daemonize_self() {
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

static void stop_children() {
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

static int start_children(const char *sound_dir, const char *config_path,
                          int volume, int verbose, int mute,
                          const char *mouse_sound_dir, const char *mouse_config_path,
                          int mouse_volume) {
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
  snprintf(sound_player_path, sizeof(sound_player_path), "%s/keyvibe-audio",
           KeyVibe_BIN_DIR);
  if (sound_pid == 0) {
    close(pipefd[1]);
    dup2(pipefd[0], STDIN_FILENO);
    close(pipefd[0]);
    if (chdir(sound_dir) != 0) {
      perror("chdir");
      exit(1);
    }
    char volume_str[32];
    snprintf(volume_str, sizeof(volume_str), "%d", volume);
    char mouse_volume_str[32];
    snprintf(mouse_volume_str, sizeof(mouse_volume_str), "%d", mouse_volume);
    char verbose_str[8];
    snprintf(verbose_str, sizeof(verbose_str), "%d", verbose);
    char mute_str[8];
    snprintf(mute_str, sizeof(mute_str), "%d", mute);
    execl(sound_player_path, "keyvibe-audio", "config.json", volume_str,
          verbose_str, mute_str, mouse_config_path, mouse_volume_str, (char *)NULL);
    perror("execl keyvibe-audio");
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
           "%s/keyvibe-input", KeyVibe_BIN_DIR);
  if (keyboard_pid == 0) {
    close(pipefd[0]);
    dup2(pipefd[1], STDOUT_FILENO);
    close(pipefd[1]);
    execl(get_key_presses_path, "keyvibe-input", (char *)NULL);
    perror("execl keyvibe-input");
    exit(1);
  }
  close(pipefd[0]);
  close(pipefd[1]);
  return 1;
}

static void handle_sighup(int sig) {
  (void)sig;
  reload_requested = 1;
}

static void write_mute_state(int mute) {
  char mute_file[MAX_PATH_LENGTH];
  const char *rd = get_runtime_dir();
  snprintf(mute_file, sizeof(mute_file), "%s/keyvibe-mute-%d", rd, (int)getuid());
  
  FILE *f = fopen(mute_file, "w");
  if (f) {
    fprintf(f, "%d\n", mute);
    fclose(f);
  }
}


struct inotify_thread_args {
  char path[MAX_PATH_LENGTH];
};

static void *inotify_thread_fn(void *arg) {
  struct inotify_thread_args *a = (struct inotify_thread_args *)arg;
  int fd = inotify_init1(IN_NONBLOCK);
  if (fd < 0) {
    printf("Failed to initialize inotify: %s\n", strerror(errno));
    return NULL;
  }
  int wd = inotify_add_watch(fd, a->path, IN_CLOSE_WRITE | IN_MOVED_TO | IN_ATTRIB | IN_MODIFY);
  if (wd < 0) {
    printf("Failed to add inotify watch for %s: %s\n", a->path, strerror(errno));
    close(fd);
    return NULL;
  }
  printf("Watching config file: %s\n", a->path);
  
  char buf[4096] __attribute__((aligned(__alignof__(struct inotify_event))));
  while (1) {
    ssize_t len = read(fd, buf, sizeof(buf));
    if (len <= 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
      struct timespec ts;
      ts.tv_sec = 0;
        ts.tv_nsec = 100000000L; // 100ms
      nanosleep(&ts, NULL);
      continue;
    }
      printf("inotify read error: %s\n", strerror(errno));
      break;
    }
    printf("Config file changed, requesting reload...\n");
    reload_requested = 1;
  }
  inotify_rm_watch(fd, wd);
  close(fd);
  return NULL;
}

static int write_user_config(const char *path, const char *keyboard_sound, const char *mouse_sound, int keyboard_volume, int mouse_volume) {
  json_object *root = json_object_new_object();
  if (!root)
    return 0;
  
  // Create keyboard section
  json_object *keyboard = json_object_new_object();
  json_object_object_add(keyboard, "keyboard_sound", json_object_new_string(keyboard_sound));
  json_object_object_add(keyboard, "volume", json_object_new_int(keyboard_volume));
  json_object_object_add(root, "keyboard", keyboard);
  
  // Create mouse section
  json_object *mouse = json_object_new_object();
  json_object_object_add(mouse, "mouse_sound", json_object_new_string(mouse_sound));
  json_object_object_add(mouse, "volume", json_object_new_int(mouse_volume));
  json_object_object_add(root, "mouse", mouse);
  
  const char *json_str =
      json_object_to_json_string_ext(root, JSON_C_TO_STRING_PRETTY);
  FILE *f = fopen(path, "w");
  if (!f) {
    json_object_put(root);
    return 0;
  }
  fprintf(f, "%s\n", json_str);
  fclose(f);
  json_object_put(root);
  return 1;
}

static int read_user_config(const char *path, char **out_keyboard_sound, char **out_mouse_sound,
                            int *out_keyboard_volume, int *out_mouse_volume) {
  FILE *f = fopen(path, "r");
  if (!f)
    return 0;
  fseek(f, 0, SEEK_END);
  long sz = ftell(f);
  if (sz < 0) {
    fclose(f);
    return 0;
  }
  errno = 0;
  rewind(f);
  if (errno != 0) {
    fclose(f);
    return 0;
  }
  char *buf = malloc((size_t)sz + 1);
  if (!buf) {
    fclose(f);
    return 0;
  }
  fread(buf, 1, sz, f);
  buf[sz] = '\0';
  fclose(f);
  json_object *root = json_tokener_parse(buf);
  free(buf);
  if (!root)
    return 0;
  
  // Try new format first (keyboard/mouse sections)
  json_object *keyboard_obj, *mouse_obj;
  if (json_object_object_get_ex(root, "keyboard", &keyboard_obj) &&
      json_object_object_get_ex(root, "mouse", &mouse_obj)) {
    // New format
  json_object *o;
    if (json_object_object_get_ex(keyboard_obj, "keyboard_sound", &o)) {
    const char *s = json_object_get_string(o);
    if (s)
        *out_keyboard_sound = strdup(s);
    }
    if (json_object_object_get_ex(mouse_obj, "mouse_sound", &o)) {
      const char *s = json_object_get_string(o);
      if (s)
        *out_mouse_sound = strdup(s);
    }
    
    // Read separate volumes for keyboard and mouse
    if (json_object_object_get_ex(keyboard_obj, "volume", &o)) {
      *out_keyboard_volume = json_object_get_int(o);
    }
    if (json_object_object_get_ex(mouse_obj, "volume", &o)) {
      *out_mouse_volume = json_object_get_int(o);
    }
  } else {
    // Legacy format - try old single sound/volume format
  json_object *o;
  if (json_object_object_get_ex(root, "sound", &o)) {
    const char *s = json_object_get_string(o);
      if (s) {
        *out_keyboard_sound = strdup(s);
        *out_mouse_sound = strdup("ping"); // default mouse sound
      }
  }
  if (json_object_object_get_ex(root, "volume", &o)) {
    int legacy_volume = json_object_get_int(o);
    *out_keyboard_volume = legacy_volume;
    *out_mouse_volume = legacy_volume;
    }
  }
  json_object_put(root);
  return 1;
}

int main(int argc, char *argv[]) {
  char *sound_name = strdup("eg-oreo"); // may be replaced by strdup; track for free
  char *mouse_sound_name = strdup("ping"); // may be replaced by strdup; track for free
  int sound_name_owned = 1;
  int mouse_sound_name_owned = 1;
  int verbose = 0;
  int list_sounds = 0;
  int flag_daemon = 0;
  int flag_stop = 0;
  static struct option long_options[] = {
      {"sound", required_argument, 0, 'S'},
      {"mouse", required_argument, 0, 'M'},
      {"volume", required_argument, 0, 'V'},
      {"keyboard-volume", required_argument, 0, 'K'},
      {"mouse-volume", required_argument, 0, 'O'},
      {"list", no_argument, 0, 'l'},
      {"daemon", no_argument, 0, 'd'},
      {"stop", no_argument, 0, 's'},
      {"mute", no_argument, 0, 'm'},
      {"unmute", no_argument, 0, 'u'},
      {"help", no_argument, 0, 'h'},
      {"verbose", no_argument, 0, 'v'},
      {0, 0, 0, 0}};
  int volume = 50;
  char *cli_sound = NULL;
  char *cli_mouse_sound = NULL;
  int cli_volume = -1;
  int cli_keyboard_volume = -1;
  int cli_mouse_volume = -1;
  char user_cfg_path[MAX_PATH_LENGTH];
  if (get_user_config_path(user_cfg_path, sizeof(user_cfg_path))) {
    if (access(user_cfg_path, R_OK) == 0) {
      char *cfg_keyboard_sound = NULL;
      char *cfg_mouse_sound = NULL;
      int cfg_keyboard_volume = volume;
      int cfg_mouse_volume = volume;
      if (read_user_config(user_cfg_path, &cfg_keyboard_sound, &cfg_mouse_sound, &cfg_keyboard_volume, &cfg_mouse_volume)) {
        if (cfg_keyboard_sound) {
          if (sound_name_owned) { free(sound_name); }
          sound_name = cfg_keyboard_sound; // now owned, must free on exit paths
          sound_name_owned = 1;
        }
        if (cfg_mouse_sound) {
          if (mouse_sound_name_owned) { free(mouse_sound_name); }
          mouse_sound_name = cfg_mouse_sound; // now owned, must free on exit paths
          mouse_sound_name_owned = 1;
        }
        volume = cfg_keyboard_volume; // Use keyboard volume as main volume for CLI compatibility
      }
    }
  }
  for (int i = 1; i < argc; i++) {
    if (argv[i][0] == '-' && argv[i][1] == '-') {
      const char *a = argv[i] + 2; const char *e = strchr(a, '='); size_t n = e ? (size_t)(e - a) : strlen(a);
      if (!((n==5 && strncmp(a, "sound", 5)==0) || (n==5 && strncmp(a, "mouse", 5)==0) || (n==6 && strncmp(a, "volume", 6)==0) || (n==4 && strncmp(a, "list", 4)==0) || (n==6 && strncmp(a, "daemon", 6)==0) || (n==4 && strncmp(a, "stop", 4)==0) || (n==4 && strncmp(a, "mute", 4)==0) || (n==6 && strncmp(a, "unmute", 6)==0) || (n==4 && strncmp(a, "help", 4)==0) || (n==7 && strncmp(a, "verbose", 7)==0))) { fprintf(stderr, "Unknown option: %s (use full option name)\n", argv[i]); return 1; }
    }
  }
  int opt;
  while ((opt = getopt_long(argc, argv, "S:M:V:K:O:lhdsmuv", long_options, NULL)) !=
         -1) {
    switch (opt) {
    case 'd':
      flag_daemon = 1;
      break;
    case 's':
      flag_stop = 1;
      break;
    case 'm': {
      pid_t running_pid = 0;
      if (!require_running_pid(&running_pid)) { if (sound_name_owned) { free(sound_name); } return 1; }
      write_mute_state(1);
      printf("KeyVibe muted.\n");
      if (sound_name_owned) { free(sound_name); }
      return 0;
    }
    case 'u': {
      pid_t running_pid = 0;
      if (!require_running_pid(&running_pid)) { if (sound_name_owned) { free(sound_name); } return 1; }
      write_mute_state(0);
      printf("KeyVibe unmuted.\n");
      if (sound_name_owned) { free(sound_name); }
      return 0;
    }
    case 'S':
      cli_sound = optarg;
      break;
    case 'M':
      cli_mouse_sound = optarg;
      break;
    case 'V':
      cli_volume = atoi(optarg);
      if (cli_volume < 0)
        cli_volume = 0;
      if (cli_volume > 100)
        cli_volume = 100;
      break;
    case 'K':
      cli_keyboard_volume = atoi(optarg);
      if (cli_keyboard_volume < 0)
        cli_keyboard_volume = 0;
      if (cli_keyboard_volume > 100)
        cli_keyboard_volume = 100;
      break;
    case 'O':
      cli_mouse_volume = atoi(optarg);
      if (cli_mouse_volume < 0)
        cli_mouse_volume = 0;
      if (cli_mouse_volume > 100)
        cli_mouse_volume = 100;
      break;
    case 'l':
      list_sounds = 1;
      break;
    /* removed legacy long mute/unmute cases; use -m/-u or --mute/--unmute */
    case 'h':
      print_usage(argv[0]);
      if (sound_name_owned) { free(sound_name); }
      return 0;
    case 'v':
      verbose = 1;
      break;
    default:
      print_usage(argv[0]);
      if (sound_name_owned) { free(sound_name); }
      return 1;
    }
  }
  if (list_sounds) {
    int rc = list_sound_packs();
    if (sound_name_owned) { free(sound_name); }
    return rc;
  }
  build_pidfile_path(pidfile_path, sizeof(pidfile_path));
  if (flag_stop) {
    pid_t running_pid = 0;
    if (!require_running_pid(&running_pid)) return 1;
    if (kill(running_pid, SIGTERM) != 0) return (perror("kill"), 1);
    for (int i = 0; i < 30; i++) {
      if (!process_is_running(running_pid)) {
        unlink(pidfile_path);
        printf("KeyVibe stopped.\n");
        return 0;
      }
      struct timespec ts;
      ts.tv_sec = 0;
      ts.tv_nsec = 100000000L;
      nanosleep(&ts, NULL);
    }
    return errorf("KeyVibe: process did not stop in time\n");
  }
  // Apply CLI options and update config file if needed
  int config_updated = 0;
  if (cli_sound != NULL) {
    if (sound_name_owned) {
      free(sound_name);
    }
      sound_name = cli_sound;
    sound_name_owned = 0; // cli_sound is not owned by us
    config_updated = 1;
  }
  if (cli_mouse_sound != NULL) {
    if (mouse_sound_name_owned) {
      free(mouse_sound_name);
    }
    mouse_sound_name = cli_mouse_sound;
    mouse_sound_name_owned = 0; // cli_mouse_sound is not owned by us
    config_updated = 1;
  }
  if (cli_volume >= 0) {
      volume = cli_volume;
    config_updated = 1;
  }
  if (cli_keyboard_volume >= 0) {
    current_keyboard_volume = cli_keyboard_volume;
    config_updated = 1;
  }
  if (cli_mouse_volume >= 0) {
    current_mouse_volume = cli_mouse_volume;
    config_updated = 1;
  }
  
  // Update config file if CLI options were used
  if (config_updated && get_user_config_path(user_cfg_path, sizeof(user_cfg_path))) {
    if (!write_user_config(user_cfg_path, sound_name, mouse_sound_name, current_keyboard_volume, current_mouse_volume)) {
      fprintf(stderr, "Warning: Failed to update config file %s\n", user_cfg_path);
    } else if (verbose) {
      fprintf(stderr, "Updated config file %s\n", user_cfg_path);
    }
  }
  // Create default config file if it doesn't exist and no CLI options were used
  if (!config_updated && get_user_config_path(user_cfg_path, sizeof(user_cfg_path))) {
    if (access(user_cfg_path, F_OK) != 0) {
      if (!write_user_config(user_cfg_path, sound_name, mouse_sound_name, current_keyboard_volume, current_mouse_volume)) {
        fprintf(stderr, "Warning: Failed to write %s\n", user_cfg_path);
      } else if (verbose) {
        fprintf(stderr, "Created default config %s\n", user_cfg_path);
      }
    }
  }
  if (!validate_keyboard_sound_pack(sound_name)) {
    if (sound_name_owned) { free(sound_name); }
    if (mouse_sound_name_owned) { free(mouse_sound_name); }
    return 1;
  }
  if (!validate_mouse_sound_pack(mouse_sound_name)) {
    if (sound_name_owned) { free(sound_name); }
    if (mouse_sound_name_owned) { free(mouse_sound_name); }
    return 1;
  }
  char get_key_presses_path[MAX_PATH_LENGTH];
  char sound_player_path[MAX_PATH_LENGTH];
  snprintf(get_key_presses_path, sizeof(get_key_presses_path),
           "%s/keyvibe-input", KeyVibe_BIN_DIR);
  snprintf(sound_player_path, sizeof(sound_player_path), "%s/keyvibe-audio",
           KeyVibe_BIN_DIR);
  if (access(get_key_presses_path, X_OK) != 0 || access(sound_player_path, X_OK) != 0)
    return errorf("Error: Cannot find or execute required binaries in %s\n", KeyVibe_BIN_DIR);
  signal(SIGINT, cleanup_processes);
  signal(SIGTERM, cleanup_processes);
  if (flag_daemon) {
    pid_t existing = 0;
    if (read_pidfile(pidfile_path, &existing) && process_is_running(existing)) {
      fprintf(stderr,
              "KeyVibe already running (pid %ld). Use --stop to stop it.\n",
              (long)existing);
      return 1;
    }
    daemonize_self();
    is_daemon = 1;
    write_pidfile(pidfile_path, getpid());
  }
  char config_path[MAX_PATH_LENGTH];
  char sound_dir[MAX_PATH_LENGTH];
  char mouse_config_path[MAX_PATH_LENGTH];
  char mouse_sound_dir[MAX_PATH_LENGTH];
  if (!build_paths_for_keyboard_sound(sound_name, config_path, sizeof(config_path),
                             sound_dir, sizeof(sound_dir))) {
    if (sound_name_owned) { free(sound_name); }
    if (mouse_sound_name_owned) { free(mouse_sound_name); }
    return 1;
  }
  if (!build_paths_for_mouse_sound(mouse_sound_name, mouse_config_path, sizeof(mouse_config_path),
                                   mouse_sound_dir, sizeof(mouse_sound_dir))) {
    if (sound_name_owned) { free(sound_name); }
    if (mouse_sound_name_owned) { free(mouse_sound_name); }
    return 1;
  }
  current_keyboard_volume = volume;
  current_mouse_volume = volume;
  current_verbose = verbose;
  strncpy(current_sound_name, sound_name, sizeof(current_sound_name) - 1);
  current_sound_name[sizeof(current_sound_name) - 1] = '\0';
  strncpy(current_mouse_sound_name, mouse_sound_name, sizeof(current_mouse_sound_name) - 1);
  current_mouse_sound_name[sizeof(current_mouse_sound_name) - 1] = '\0';
  strncpy(current_config_path, config_path, sizeof(current_config_path) - 1);
  current_config_path[sizeof(current_config_path) - 1] = '\0';
  strncpy(current_sound_dir, sound_dir, sizeof(current_sound_dir) - 1);
  current_sound_dir[sizeof(current_sound_dir) - 1] = '\0';
  strncpy(current_mouse_config_path, mouse_config_path, sizeof(current_mouse_config_path) - 1);
  current_mouse_config_path[sizeof(current_mouse_config_path) - 1] = '\0';
  strncpy(current_mouse_sound_dir, mouse_sound_dir, sizeof(current_mouse_sound_dir) - 1);
  current_mouse_sound_dir[sizeof(current_mouse_sound_dir) - 1] = '\0';
  if (verbose && !is_daemon) {
    printf("KeyVibe starting...\n");
    printf("Keyboard sound pack: %s\n", sound_name);
    printf("Mouse sound pack: %s\n", mouse_sound_name);
    printf("Keyboard config file: %s\n", config_path);
    printf("Mouse config file: %s\n", mouse_config_path);
    printf("Keyboard working directory: %s\n", sound_dir);
    printf("Mouse working directory: %s\n", mouse_sound_dir);
    printf("Press Ctrl+C to exit.\n\n");
  } else {
    if (!is_daemon) {
      printf("KeyVibe started with keyboard sound pack: %s, mouse sound pack: %s\n", sound_name, mouse_sound_name);
      printf("Press Ctrl+C to exit.\n");
    }
  }
  if (!start_children(sound_dir, config_path, volume, verbose, current_mute,
                      mouse_sound_dir, mouse_config_path, current_mouse_volume)) {
    if (sound_name_owned) { free(sound_name); }
    return 1;
  }
  int status;
  pid_t finished_pid;
  signal(SIGHUP, handle_sighup);
  pthread_t inotify_thread;
  struct inotify_thread_args in_args;
  // Enable live reloading for both daemon and non-daemon modes
    char user_cfg_path2[MAX_PATH_LENGTH];
    if (get_user_config_path(user_cfg_path2, sizeof(user_cfg_path2))) {
      strncpy(in_args.path, user_cfg_path2, sizeof(in_args.path) - 1);
      in_args.path[sizeof(in_args.path) - 1] = '\0';
    printf("Setting up file watcher for: %s\n", in_args.path);
    
    // Check if the config file exists
    if (access(in_args.path, F_OK) == 0) {
      pthread_create(&inotify_thread, NULL, inotify_thread_fn, &in_args);
      pthread_detach(inotify_thread);
    } else {
      printf("Warning: Config file %s does not exist, file watching disabled\n", in_args.path);
    }
  } else {
    printf("Warning: Could not get user config path for file watching\n");
  }
  while (1) {
    if (reload_requested) {
      printf("Reload requested, processing config changes...\n");
      reload_requested = 0;
      
      char user_cfg_path3[MAX_PATH_LENGTH];
      if (get_user_config_path(user_cfg_path3, sizeof(user_cfg_path3))) {
        char *cfg_keyboard_sound = NULL;
        char *cfg_mouse_sound = NULL;
        int cfg_keyboard_volume = current_keyboard_volume;
        int cfg_mouse_volume = current_mouse_volume;
        
        if (read_user_config(user_cfg_path3, &cfg_keyboard_sound, &cfg_mouse_sound, &cfg_keyboard_volume, &cfg_mouse_volume)) {
          printf("Read config - keyboard:%s mouse:%s kvol:%d mvol:%d (current kvol:%d mvol:%d)\n", 
                 cfg_keyboard_sound ? cfg_keyboard_sound : "NULL",
                 cfg_mouse_sound ? cfg_mouse_sound : "NULL", 
                 cfg_keyboard_volume, cfg_mouse_volume, current_keyboard_volume, current_mouse_volume);
          
          // Check what changed
          int keyboard_changed = 0;
          int mouse_changed = 0;
          int keyboard_volume_changed = 0;
          int mouse_volume_changed = 0;
          
          if (cfg_keyboard_sound && strcmp(current_sound_name, cfg_keyboard_sound) != 0) {
            keyboard_changed = 1;
            strncpy(current_sound_name, cfg_keyboard_sound, sizeof(current_sound_name) - 1);
            current_sound_name[sizeof(current_sound_name) - 1] = '\0';
          }
          free(cfg_keyboard_sound);
          
          if (cfg_mouse_sound && strcmp(current_mouse_sound_name, cfg_mouse_sound) != 0) {
            mouse_changed = 1;
            strncpy(current_mouse_sound_name, cfg_mouse_sound, sizeof(current_mouse_sound_name) - 1);
            current_mouse_sound_name[sizeof(current_mouse_sound_name) - 1] = '\0';
          }
          free(cfg_mouse_sound);
          
          if (cfg_keyboard_volume != current_keyboard_volume) {
            keyboard_volume_changed = 1;
            current_keyboard_volume = cfg_keyboard_volume;
          }
          
          if (cfg_mouse_volume != current_mouse_volume) {
            mouse_volume_changed = 1;
            current_mouse_volume = cfg_mouse_volume;
          }
          
          printf("Config changed - keyboard:%d mouse:%d kvol:%d mvol:%d\n", 
                 keyboard_changed, mouse_changed, keyboard_volume_changed, mouse_volume_changed);
          
          // Handle the changes immediately
          if (keyboard_changed || mouse_changed || keyboard_volume_changed || mouse_volume_changed) {
            // Validate both sound packs
            int keyboard_valid = validate_keyboard_sound_pack(current_sound_name);
            int mouse_valid = validate_mouse_sound_pack(current_mouse_sound_name);
            
            if (keyboard_valid && mouse_valid) {
              // Update keyboard paths
              if (build_paths_for_keyboard_sound(current_sound_name, current_config_path,
                                   sizeof(current_config_path),
                                   current_sound_dir,
                                   sizeof(current_sound_dir))) {
                // Update mouse paths
                if (build_paths_for_mouse_sound(current_mouse_sound_name, current_mouse_config_path,
                                               sizeof(current_mouse_config_path),
                                               current_mouse_sound_dir,
                                               sizeof(current_mouse_sound_dir))) {
                  // Restart children with new config (including volume changes)
        stop_children();
                  start_children(current_sound_dir, current_config_path, current_keyboard_volume,
                                 current_verbose, current_mute, current_mouse_sound_dir, 
                                 current_mouse_config_path, current_mouse_volume);
                  printf("Reloaded successfully: keyboard=%s, mouse=%s, kvol=%d, mvol=%d\n", 
                         current_sound_name, current_mouse_sound_name, current_keyboard_volume, current_mouse_volume);
                } else {
                  printf("Failed to build mouse sound paths\n");
                }
              } else {
                printf("Failed to build keyboard sound paths\n");
              }
            } else {
              if (!keyboard_valid) {
                printf("Reload failed: invalid keyboard sound pack '%s'\n", current_sound_name);
              }
              if (!mouse_valid) {
                printf("Reload failed: invalid mouse sound pack '%s'\n", current_mouse_sound_name);
              }
            }
          }
        } else {
          printf("Failed to read user config\n");
        }
      } else {
        printf("Failed to get user config path\n");
      }
    }
    finished_pid = waitpid(-1, &status, WNOHANG);
    if (finished_pid == 0) {
      struct timespec ts;
      ts.tv_sec = 0;
      ts.tv_nsec = 100000000L;
      nanosleep(&ts, NULL);
      continue;
    } else if (finished_pid > 0) {
      if (finished_pid == keyboard_pid) {
        if (!is_daemon)
          printf("Keyboard listener exited with status %d\n",
                 WEXITSTATUS(status));
        keyboard_pid = 0;
        if (sound_pid > 0)
          kill(sound_pid, SIGTERM);
      } else if (finished_pid == sound_pid) {
        if (!is_daemon)
          printf("Sound player exited with status %d\n", WEXITSTATUS(status));
        sound_pid = 0;
        if (keyboard_pid > 0)
          kill(keyboard_pid, SIGTERM);
      }
      break;
    } else {
      if (errno == ECHILD)
        break;
    }
  }
  if (!is_daemon) {
    printf("KeyVibe exited.\n");
  }
  if (sound_name_owned) { free(sound_name); }
  if (mouse_sound_name_owned) { free(mouse_sound_name); }
  return 0;
}
