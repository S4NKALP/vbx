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
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/inotify.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define MAX_PATH_LENGTH 1024
#define AUDIO_BASE_DIR KeyVibe_DATA_DIR "/audio"
#define USER_AUDIO_SUBPATH "/.local/share/keyvibe/audio"
#define USER_CONFIG_FILENAME ".keyvibe.json"

// Global variables for cleanup
pid_t keyboard_pid = 0;
pid_t sound_pid = 0;
static char pidfile_path[MAX_PATH_LENGTH] = {0};
static int is_daemon = 0;
static volatile sig_atomic_t reload_requested = 0;
static int current_volume = 50;
static char current_sound_name[MAX_PATH_LENGTH] = {0};
static int current_verbose = 0;
static char current_config_path[MAX_PATH_LENGTH] = {0};
static char current_sound_dir[MAX_PATH_LENGTH] = {0};
static int current_mute = 0;

void print_usage(const char *program_name) {
  printf("KeyVibe - Mechanical Keyboard Sound Simulator\n\n");
  printf("Usage: %s [OPTIONS]\n\n", program_name);
  printf("Options:\n");
  printf("  -S, --sound SOUND_NAME   Select sound pack (default: eg-oreo)\n");
  printf("  -V, --volume VOLUME      Set volume [0-100] (default: 50)\n");
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

  // Determine user audio dir
  char user_audio_dir[MAX_PATH_LENGTH] = {0};
  const char *home = getenv("HOME");
  if (!home || strlen(home) == 0) {
    struct passwd *pw = getpwuid(getuid());
    if (pw && pw->pw_dir)
      home = pw->pw_dir;
  }
  if (home) {
    snprintf(user_audio_dir, sizeof(user_audio_dir), "%s%s", home,
             USER_AUDIO_SUBPATH);
  }

  printf("Available sound packs:\n");
  printf("======================\n");

  // List user packs first (if available)
  if (user_audio_dir[0] && (dir = opendir(user_audio_dir)) != NULL) {
    while ((entry = readdir(dir)) != NULL) {
      if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
        continue;
      if ((size_t)snprintf(path, sizeof(path), "%s/%s", user_audio_dir,
                           entry->d_name) >= sizeof(path)) {
        fprintf(stderr, "Warning: Path too long, skipping: %s/%s\n",
                user_audio_dir, entry->d_name);
        continue;
      }
      if (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) {
        printf("  %s (user)\n", entry->d_name);
      }
    }
    closedir(dir);
  }

  // Then list system packs
  dir = opendir(AUDIO_BASE_DIR);
  if (dir != NULL) {
    while ((entry = readdir(dir)) != NULL) {
      if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
        continue;
      if ((size_t)snprintf(path, sizeof(path), "%s/%s", AUDIO_BASE_DIR,
                           entry->d_name) >= sizeof(path)) {
        fprintf(stderr, "Warning: Path too long, skipping: %s/%s\n",
                AUDIO_BASE_DIR, entry->d_name);
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

static void get_user_audio_dir(char *buffer, size_t buflen) {
  buffer[0] = '\0';
  const char *home = getenv("HOME");
  if (!home || strlen(home) == 0) {
    struct passwd *pw = getpwuid(getuid());
    if (pw && pw->pw_dir)
      home = pw->pw_dir;
  }
  if (home)
    snprintf(buffer, buflen, "%s%s", home, USER_AUDIO_SUBPATH);
}

static int resolve_sound_base_dir(const char *sound_name, char *out_basedir,
                                  size_t out_sz) {
  char user_audio_dir[MAX_PATH_LENGTH];
  get_user_audio_dir(user_audio_dir, sizeof(user_audio_dir));
  if (user_audio_dir[0]) {
    char p[MAX_PATH_LENGTH];
    if ((size_t)snprintf(p, sizeof(p), "%s/%s/config.json", user_audio_dir,
                         sound_name) < sizeof(p) &&
        access(p, R_OK) == 0) {
      strncpy(out_basedir, user_audio_dir, out_sz - 1);
      out_basedir[out_sz - 1] = '\0';
      return 1;
    }
  }
  // Fallback to system dir
  char p2[MAX_PATH_LENGTH];
  if ((size_t)snprintf(p2, sizeof(p2), "%s/%s/config.json", AUDIO_BASE_DIR,
                       sound_name) >= sizeof(p2)) {
    return 0; // Path too long
  }
  if (access(p2, R_OK) == 0) {
    strncpy(out_basedir, AUDIO_BASE_DIR, out_sz - 1);
    out_basedir[out_sz - 1] = '\0';
    return 1;
  }
  return 0;
}

int validate_sound_pack(const char *sound_name) {
  char basedir[MAX_PATH_LENGTH];
  if (!resolve_sound_base_dir(sound_name, basedir, sizeof(basedir))) {
    fprintf(stderr,
            "Error: Sound pack '%s' not found in user or system dirs.\n",
            sound_name);
    fprintf(stderr, "Use --list to see available sound packs.\n");
    return 0;
  }
  char config_path[MAX_PATH_LENGTH];
  if ((size_t)snprintf(config_path, sizeof(config_path), "%s/%s/config.json",
                       basedir, sound_name) >= sizeof(config_path)) {
    fprintf(stderr, "Error: Sound pack path too long: %s/%s\n", basedir,
            sound_name);
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

static int build_paths_for_sound(const char *sound_name, char *out_config_path,
                                 size_t out_config_sz, char *out_sound_dir,
                                 size_t out_sound_sz) {
  char basedir[MAX_PATH_LENGTH];
  if (!resolve_sound_base_dir(sound_name, basedir, sizeof(basedir))) {
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
  if (!safe_snprintf(out_config_path, out_config_sz, "%s/%s%s", basedir,
                     sound_name, cfg_suffix))
    return 0;
  if (!safe_snprintf(out_sound_dir, out_sound_sz, "%s/%s", basedir, sound_name))
    return 0;
  return 1;
}

// Helper: ensure daemon is running, return its pid or print a consistent error
static int require_running_pid(pid_t *out_pid) {
  build_pidfile_path(pidfile_path, sizeof(pidfile_path));
  pid_t running_pid = 0;
  if (!read_pidfile(pidfile_path, &running_pid) ||
      !process_is_running(running_pid)) {
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
                          int volume, int verbose, int mute) {
  (void)config_path;
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
    char verbose_str[8];
    snprintf(verbose_str, sizeof(verbose_str), "%d", verbose);
    char mute_str[8];
    snprintf(mute_str, sizeof(mute_str), "%d", mute);
    execl(sound_player_path, "keyvibe-audio", "config.json", volume_str,
          verbose_str, mute_str, (char *)NULL);
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
  snprintf(mute_file, sizeof(mute_file), "%s/keyvibe-mute-%d", rd,
           (int)getuid());

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
  if (fd < 0)
    return NULL;
  int wd =
      inotify_add_watch(fd, a->path, IN_CLOSE_WRITE | IN_MOVED_TO | IN_ATTRIB);
  if (wd < 0) {
    close(fd);
    return NULL;
  }
  char buf[4096] __attribute__((aligned(__alignof__(struct inotify_event))));
  while (1) {
    ssize_t len = read(fd, buf, sizeof(buf));
    if (len <= 0) {
      struct timespec ts;
      ts.tv_sec = 0;
      ts.tv_nsec = 200000000L;
      nanosleep(&ts, NULL);
      continue;
    }
    reload_requested = 1;
  }
  inotify_rm_watch(fd, wd);
  close(fd);
  return NULL;
}

static int write_user_config(const char *path, const char *sound, int volume) {
  json_object *root = json_object_new_object();
  if (!root)
    return 0;
  json_object_object_add(root, "sound", json_object_new_string(sound));
  json_object_object_add(root, "volume", json_object_new_int(volume));
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

static int read_user_config(const char *path, char **out_sound,
                            int *out_volume) {
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
  json_object *o;
  if (json_object_object_get_ex(root, "sound", &o)) {
    const char *s = json_object_get_string(o);
    if (s)
      *out_sound = strdup(s);
  }
  if (json_object_object_get_ex(root, "volume", &o)) {
    *out_volume = json_object_get_int(o);
  }
  json_object_put(root);
  return 1;
}

int main(int argc, char *argv[]) {
  char *sound_name = "eg-oreo"; // may be replaced by strdup; track for free
  int sound_name_owned = 0;
  int verbose = 0;
  int list_sounds = 0;
  int flag_daemon = 0;
  int flag_stop = 0;
  static struct option long_options[] = {{"sound", required_argument, 0, 'S'},
                                         {"volume", required_argument, 0, 'V'},
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
  int cli_volume = -1;
  char user_cfg_path[MAX_PATH_LENGTH];
  if (get_user_config_path(user_cfg_path, sizeof(user_cfg_path))) {
    if (access(user_cfg_path, R_OK) == 0) {
      char *cfg_sound = NULL;
      int cfg_volume = volume;
      if (read_user_config(user_cfg_path, &cfg_sound, &cfg_volume)) {
        if (cfg_sound) {
          sound_name = cfg_sound; // now owned, must free on exit paths
          sound_name_owned = 1;
        }
        volume = cfg_volume;
      }
    }
  }
  for (int i = 1; i < argc; i++) {
    if (argv[i][0] == '-' && argv[i][1] == '-') {
      const char *a = argv[i] + 2;
      const char *e = strchr(a, '=');
      size_t n = e ? (size_t)(e - a) : strlen(a);
      if (!((n == 5 && strncmp(a, "sound", 5) == 0) ||
            (n == 6 && strncmp(a, "volume", 6) == 0) ||
            (n == 4 && strncmp(a, "list", 4) == 0) ||
            (n == 6 && strncmp(a, "daemon", 6) == 0) ||
            (n == 4 && strncmp(a, "stop", 4) == 0) ||
            (n == 4 && strncmp(a, "mute", 4) == 0) ||
            (n == 6 && strncmp(a, "unmute", 6) == 0) ||
            (n == 4 && strncmp(a, "help", 4) == 0) ||
            (n == 7 && strncmp(a, "verbose", 7) == 0))) {
        fprintf(stderr, "Unknown option: %s (use full option name)\n", argv[i]);
        return 1;
      }
    }
  }
  int opt;
  while ((opt = getopt_long(argc, argv, "S:V:lhdsmuv", long_options, NULL)) !=
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
      if (!require_running_pid(&running_pid)) {
        if (sound_name_owned) {
          free(sound_name);
        }
        return 1;
      }
      write_mute_state(1);
      printf("KeyVibe muted.\n");
      if (sound_name_owned) {
        free(sound_name);
      }
      return 0;
    }
    case 'u': {
      pid_t running_pid = 0;
      if (!require_running_pid(&running_pid)) {
        if (sound_name_owned) {
          free(sound_name);
        }
        return 1;
      }
      write_mute_state(0);
      printf("KeyVibe unmuted.\n");
      if (sound_name_owned) {
        free(sound_name);
      }
      return 0;
    }
    case 'S':
      cli_sound = optarg;
      break;
    case 'V':
      cli_volume = atoi(optarg);
      if (cli_volume < 0)
        cli_volume = 0;
      if (cli_volume > 100)
        cli_volume = 100;
      break;
    case 'l':
      list_sounds = 1;
      break;
    /* removed legacy long mute/unmute cases; use -m/-u or --mute/--unmute */
    case 'h':
      print_usage(argv[0]);
      if (sound_name_owned) {
        free(sound_name);
      }
      return 0;
    case 'v':
      verbose = 1;
      break;
    default:
      print_usage(argv[0]);
      if (sound_name_owned) {
        free(sound_name);
      }
      return 1;
    }
  }
  if (list_sounds) {
    int rc = list_sound_packs();
    if (sound_name_owned) {
      free(sound_name);
    }
    return rc;
  }
  build_pidfile_path(pidfile_path, sizeof(pidfile_path));
  if (flag_stop) {
    pid_t running_pid = 0;
    if (!require_running_pid(&running_pid))
      return 1;
    if (kill(running_pid, SIGTERM) != 0)
      return (perror("kill"), 1);
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
  if (cli_volume >= 0) {
    volume = cli_volume;
    config_updated = 1;
  }

  // Update config file if CLI options were used
  if (config_updated &&
      get_user_config_path(user_cfg_path, sizeof(user_cfg_path))) {
    if (!write_user_config(user_cfg_path, sound_name, volume)) {
      fprintf(stderr, "Warning: Failed to update config file %s\n",
              user_cfg_path);
    } else if (verbose) {
      fprintf(stderr, "Updated config file %s\n", user_cfg_path);
    }
  }
  // Create default config file if it doesn't exist and no CLI options were used
  if (!config_updated &&
      get_user_config_path(user_cfg_path, sizeof(user_cfg_path))) {
    if (access(user_cfg_path, F_OK) != 0) {
      if (!write_user_config(user_cfg_path, sound_name, volume)) {
        fprintf(stderr, "Warning: Failed to write %s\n", user_cfg_path);
      } else if (verbose) {
        fprintf(stderr, "Created default config %s\n", user_cfg_path);
      }
    }
  }
  if (!validate_sound_pack(sound_name)) {
    if (sound_name_owned) {
      free(sound_name);
    }
    return 1;
  }
  char get_key_presses_path[MAX_PATH_LENGTH];
  char sound_player_path[MAX_PATH_LENGTH];
  snprintf(get_key_presses_path, sizeof(get_key_presses_path),
           "%s/keyvibe-input", KeyVibe_BIN_DIR);
  snprintf(sound_player_path, sizeof(sound_player_path), "%s/keyvibe-audio",
           KeyVibe_BIN_DIR);
  if (access(get_key_presses_path, X_OK) != 0 ||
      access(sound_player_path, X_OK) != 0)
    return errorf("Error: Cannot find or execute required binaries in %s\n",
                  KeyVibe_BIN_DIR);
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
  if (!build_paths_for_sound(sound_name, config_path, sizeof(config_path),
                             sound_dir, sizeof(sound_dir))) {
    return 1;
  }
  current_volume = volume;
  current_verbose = verbose;
  strncpy(current_sound_name, sound_name, sizeof(current_sound_name) - 1);
  current_sound_name[sizeof(current_sound_name) - 1] = '\0';
  strncpy(current_config_path, config_path, sizeof(current_config_path) - 1);
  current_config_path[sizeof(current_config_path) - 1] = '\0';
  strncpy(current_sound_dir, sound_dir, sizeof(current_sound_dir) - 1);
  current_sound_dir[sizeof(current_sound_dir) - 1] = '\0';
  if (verbose && !is_daemon) {
    printf("KeyVibe starting...\n");
    printf("Sound pack: %s\n", sound_name);
    printf("Config file: %s\n", config_path);
    printf("Working directory: %s\n", sound_dir);
    printf("Press Ctrl+C to exit.\n\n");
  } else {
    if (!is_daemon) {
      printf("KeyVibe started with sound pack: %s\n", sound_name);
      printf("Press Ctrl+C to exit.\n");
    }
  }
  if (!start_children(sound_dir, config_path, volume, verbose, current_mute)) {
    if (sound_name_owned) {
      free(sound_name);
    }
    return 1;
  }
  int status;
  pid_t finished_pid;
  signal(SIGHUP, handle_sighup);
  pthread_t inotify_thread;
  struct inotify_thread_args in_args;
  if (is_daemon) {
    char user_cfg_path2[MAX_PATH_LENGTH];
    if (get_user_config_path(user_cfg_path2, sizeof(user_cfg_path2))) {
      strncpy(in_args.path, user_cfg_path2, sizeof(in_args.path) - 1);
      in_args.path[sizeof(in_args.path) - 1] = '\0';
      pthread_create(&inotify_thread, NULL, inotify_thread_fn, &in_args);
      pthread_detach(inotify_thread);
    }
  }
  while (1) {
    if (reload_requested) {
      reload_requested = 0;
      char user_cfg_path3[MAX_PATH_LENGTH];
      if (get_user_config_path(user_cfg_path3, sizeof(user_cfg_path3))) {
        char *cfg_sound = NULL;
        int cfg_volume = current_volume;
        if (read_user_config(user_cfg_path3, &cfg_sound, &cfg_volume)) {
          if (cfg_sound) {
            strncpy(current_sound_name, cfg_sound,
                    sizeof(current_sound_name) - 1);
            free(cfg_sound);
          }
          current_volume = cfg_volume;
        }
      }
      if (validate_sound_pack(current_sound_name)) {
        if (!build_paths_for_sound(current_sound_name, current_config_path,
                                   sizeof(current_config_path),
                                   current_sound_dir,
                                   sizeof(current_sound_dir))) {
          continue;
        }
        stop_children();
        start_children(current_sound_dir, current_config_path, current_volume,
                       current_verbose, current_mute);
      }
      continue;
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
  if (sound_name_owned) {
    free(sound_name);
  }
  return 0;
}
