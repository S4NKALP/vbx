#define _POSIX_C_SOURCE 200809L

#include "app/cli.h"
#include "app/process.h"
#include "app/reload.h"
#include "app/watch.h"
#include "common/mute.h"
#include "common/utils.h"
#include "config.h"
#include "user_config.h"
#include "soundpacks.h"
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
#define AUDIO_BASE_DIR KeyVibe_DATA_DIR "/soundpacks"
#define KEYBOARD_AUDIO_DIR AUDIO_BASE_DIR "/keyboard"
#define MOUSE_AUDIO_DIR AUDIO_BASE_DIR "/mouse"
#define USER_KEYBOARD_AUDIO_SUBPATH "/.local/share/keyvibe/soundpacks/keyboard"
#define USER_MOUSE_AUDIO_SUBPATH "/.local/share/keyvibe/soundpacks/mouse"
#define USER_CONFIG_FILENAME ".keyvibe.json"

extern pid_t keyboard_pid;
extern pid_t sound_pid;
extern char pidfile_path[];
extern int is_daemon;
extern volatile sig_atomic_t reload_requested;
static int current_keyboard_volume = 50;
static int current_mouse_volume = 50;
static char current_sound_name[MAX_PATH_LENGTH] = {0};
static char current_mouse_sound_name[MAX_PATH_LENGTH] = {0};
static int current_verbose = 0;
static int current_keyboard_mute = 0;
static int current_mouse_mute = 0;
static char current_config_path[MAX_PATH_LENGTH] = {0};
static char current_sound_dir[MAX_PATH_LENGTH] = {0};
static char current_mouse_config_path[MAX_PATH_LENGTH] = {0};
static char current_mouse_sound_dir[MAX_PATH_LENGTH] = {0};
static int current_mute = 0;

int main(int argc, char *argv[]) {
  char *sound_name = strdup("eg-oreo");
  char *mouse_sound_name = strdup("ping");
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
      {"mute", optional_argument, 0, 'm'},
      {"unmute", optional_argument, 0, 'u'},
      {"list", no_argument, 0, 'l'},
      {"daemon", no_argument, 0, 'd'},
      {"stop", no_argument, 0, 's'},
      {"help", no_argument, 0, 'h'},
      {"verbose", no_argument, 0, 'v'},
      {0, 0, 0, 0}};
  int volume = 50;
  char *cli_sound = NULL;
  char *cli_mouse_sound = NULL;
  int cli_volume = -1;
  int cli_keyboard_volume = -1;
  int cli_mouse_volume = -1;
  int cli_keyboard_mute = -1;
  int cli_mouse_mute = -1;
  char user_cfg_path[MAX_PATH_LENGTH];
  if (get_user_config_path(user_cfg_path, sizeof(user_cfg_path))) {
    if (access(user_cfg_path, R_OK) == 0) {
      char *cfg_keyboard_sound = NULL;
      char *cfg_mouse_sound = NULL;
      int cfg_keyboard_volume = volume;
      int cfg_mouse_volume = volume;
      if (read_user_config(user_cfg_path, &cfg_keyboard_sound, &cfg_mouse_sound,
                           &cfg_keyboard_volume, &cfg_mouse_volume)) {
        if (cfg_keyboard_sound) {
          if (sound_name_owned) {
            free(sound_name);
          }
          sound_name = cfg_keyboard_sound;
          sound_name_owned = 1;
        }
        if (cfg_mouse_sound) {
          if (mouse_sound_name_owned) {
            free(mouse_sound_name);
          }
          mouse_sound_name = cfg_mouse_sound;
          mouse_sound_name_owned = 1;
        }
        volume = cfg_keyboard_volume;
      }
    }
  }
  for (int i = 1; i < argc; i++) {
    if (argv[i][0] == '-' && argv[i][1] == '-') {
      const char *a = argv[i] + 2;
      const char *e = strchr(a, '=');
      size_t n = e ? (size_t)(e - a) : strlen(a);
      if (!((n == 5 && strncmp(a, "sound", 5) == 0) ||
            (n == 5 && strncmp(a, "mouse", 5) == 0) ||
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
  while ((opt = getopt_long(argc, argv, "S:M:V:K:O:lhdsm::u::v", long_options,
                            NULL)) != -1) {
    switch (opt) {
    case 'd':
      flag_daemon = 1;
      break;
    case 's':
      flag_stop = 1;
      break;
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
    case 'm':
      if (optarg == NULL) {
        cli_keyboard_mute = 1;
        cli_mouse_mute = 1;
        current_mute = 1; // global mute
      } else if (strcmp(optarg, "keyboard") == 0) {
        cli_keyboard_mute = 1;
        write_runtime_keyboard_mute_file(1);
      } else if (strcmp(optarg, "mouse") == 0) {
        cli_mouse_mute = 1;
        write_runtime_mouse_mute_file(1);
      } else if (strcmp(optarg, "both") == 0) {
        cli_keyboard_mute = 1;
        cli_mouse_mute = 1;
        current_mute = 1; // global mute
      } else {
        fprintf(stderr,
                "Invalid mute option: %s. Use 'keyboard', 'mouse', or 'both'\n",
                optarg);
        return 1;
      }
      break;
    case 'u':
      if (optarg == NULL) {
        cli_keyboard_mute = 0;
        cli_mouse_mute = 0;
        current_mute = 0; // clear global mute
      } else if (strcmp(optarg, "keyboard") == 0) {
        cli_keyboard_mute = 0;
        write_runtime_keyboard_mute_file(0);
      } else if (strcmp(optarg, "mouse") == 0) {
        cli_mouse_mute = 0;
        write_runtime_mouse_mute_file(0);
      } else if (strcmp(optarg, "both") == 0) {
        cli_keyboard_mute = 0;
        cli_mouse_mute = 0;
        current_mute = 0; // clear global mute
      } else {
        fprintf(
            stderr,
            "Invalid unmute option: %s. Use 'keyboard', 'mouse', or 'both'\n",
            optarg);
        return 1;
      }
      break;
    case 'l':
      list_sounds = 1;
      break;
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
  build_pidfile_path(pidfile_path, 1024);
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
  current_mute = read_runtime_mute_file();

  int config_updated = 0;
  if (cli_sound != NULL) {
    if (sound_name_owned) {
      free(sound_name);
    }
    sound_name = cli_sound;
    sound_name_owned = 0;
    config_updated = 1;
  }
  if (cli_mouse_sound != NULL) {
    if (mouse_sound_name_owned) {
      free(mouse_sound_name);
    }
    mouse_sound_name = cli_mouse_sound;
    mouse_sound_name_owned = 0;
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
  if (cli_keyboard_mute >= 0) {
    current_keyboard_mute = cli_keyboard_mute;
    config_updated = 1;
  }
  if (cli_mouse_mute >= 0) {
    current_mouse_mute = cli_mouse_mute;
    config_updated = 1;
  }

  write_runtime_mute_file(current_mute);
  write_runtime_keyboard_mute_file(current_keyboard_mute);
  write_runtime_mouse_mute_file(current_mouse_mute);

  if (config_updated && !flag_daemon && !list_sounds && !flag_stop) {
    if (sound_name_owned) {
      free(sound_name);
    }
    if (mouse_sound_name_owned) {
      free(mouse_sound_name);
    }
    return 0;
  }

  if (config_updated &&
      get_user_config_path(user_cfg_path, sizeof(user_cfg_path))) {
    if (!write_user_config(user_cfg_path, sound_name, mouse_sound_name,
                           current_keyboard_volume, current_mouse_volume)) {
      fprintf(stderr, "Warning: Failed to update config file %s\n",
              user_cfg_path);
    } else if (verbose) {
      fprintf(stderr, "Updated config file %s\n", user_cfg_path);
    }
  }

  if (!config_updated &&
      get_user_config_path(user_cfg_path, sizeof(user_cfg_path))) {
    if (access(user_cfg_path, F_OK) != 0) {
      if (!write_user_config(user_cfg_path, sound_name, mouse_sound_name,
                             current_keyboard_volume, current_mouse_volume)) {
        fprintf(stderr, "Warning: Failed to write %s\n", user_cfg_path);
      } else if (verbose) {
        fprintf(stderr, "Created default config %s\n", user_cfg_path);
      }
    }
  }
  if (!validate_keyboard_sound_pack(sound_name)) {
    if (sound_name_owned) {
      free(sound_name);
    }
    if (mouse_sound_name_owned) {
      free(mouse_sound_name);
    }
    return 1;
  }
  if (!validate_mouse_sound_pack(mouse_sound_name)) {
    if (sound_name_owned) {
      free(sound_name);
    }
    if (mouse_sound_name_owned) {
      free(mouse_sound_name);
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
  char mouse_config_path[MAX_PATH_LENGTH];
  char mouse_sound_dir[MAX_PATH_LENGTH];
  if (!build_paths_for_keyboard_sound(sound_name, config_path,
                                      sizeof(config_path), sound_dir,
                                      sizeof(sound_dir))) {
    if (sound_name_owned) {
      free(sound_name);
    }
    if (mouse_sound_name_owned) {
      free(mouse_sound_name);
    }
    return 1;
  }
  if (!build_paths_for_mouse_sound(mouse_sound_name, mouse_config_path,
                                   sizeof(mouse_config_path), mouse_sound_dir,
                                   sizeof(mouse_sound_dir))) {
    if (sound_name_owned) {
      free(sound_name);
    }
    if (mouse_sound_name_owned) {
      free(mouse_sound_name);
    }
    return 1;
  }
  current_keyboard_volume = volume;
  current_mouse_volume = volume;
  current_verbose = verbose;
  strncpy(current_sound_name, sound_name, sizeof(current_sound_name) - 1);
  current_sound_name[sizeof(current_sound_name) - 1] = '\0';
  strncpy(current_mouse_sound_name, mouse_sound_name,
          sizeof(current_mouse_sound_name) - 1);
  current_mouse_sound_name[sizeof(current_mouse_sound_name) - 1] = '\0';
  strncpy(current_config_path, config_path, sizeof(current_config_path) - 1);
  current_config_path[sizeof(current_config_path) - 1] = '\0';
  strncpy(current_sound_dir, sound_dir, sizeof(current_sound_dir) - 1);
  current_sound_dir[sizeof(current_sound_dir) - 1] = '\0';
  strncpy(current_mouse_config_path, mouse_config_path,
          sizeof(current_mouse_config_path) - 1);
  current_mouse_config_path[sizeof(current_mouse_config_path) - 1] = '\0';
  strncpy(current_mouse_sound_dir, mouse_sound_dir,
          sizeof(current_mouse_sound_dir) - 1);
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
      printf("KeyVibe started with keyboard sound pack: %s, mouse sound pack: "
             "%s\n",
             sound_name, mouse_sound_name);
      printf("Press Ctrl+C to exit.\n");
    }
  }
  if (!start_children(sound_dir, config_path, volume, verbose, current_mute,
                      mouse_sound_dir, mouse_config_path, current_mouse_volume,
                      current_keyboard_mute, current_mouse_mute)) {
    if (sound_name_owned) {
      free(sound_name);
    }
    return 1;
  }
  int status;
  pid_t finished_pid;
  signal(SIGHUP, handle_sighup);

  char user_cfg_path2[MAX_PATH_LENGTH];
  if (get_user_config_path(user_cfg_path2, sizeof(user_cfg_path2))) {
    printf("Setting up file watcher for: %s\n", user_cfg_path2);
    start_config_watcher(user_cfg_path2);
  } else {
    printf("Warning: Could not get user config path for file watching\n");
  }
  while (1) {
    if (reload_requested) {
      printf("Reload requested, processing config changes...\n");
      reload_requested = 0;
      char user_cfg_path3[MAX_PATH_LENGTH];
      if (get_user_config_path(user_cfg_path3, sizeof(user_cfg_path3))) {
        handle_reload(
            user_cfg_path3, current_sound_name, current_mouse_sound_name,
            &current_keyboard_volume, &current_mouse_volume,
            current_config_path, current_sound_dir, current_mouse_config_path,
            current_mouse_sound_dir, current_mute, current_verbose,
            current_mouse_volume, current_keyboard_mute, current_mouse_mute);
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
  if (sound_name_owned) {
    free(sound_name);
  }
  if (mouse_sound_name_owned) {
    free(mouse_sound_name);
  }
  return 0;
}
