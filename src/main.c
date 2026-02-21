#define _POSIX_C_SOURCE 200809L

#include "app/cli.h"
#include "app/process.h"
#include "app/reload.h"
#include "app/watch.h"
#include "common/log.h"
#include "common/mute.h"
#include "common/utils.h"
#include "config.h"
#include "soundpacks.h"
#include "user_config.h"
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
#define AUDIO_BASE_DIR VBX_DATA_DIR "/soundpacks"
#define KEYBOARD_AUDIO_DIR AUDIO_BASE_DIR "/keyboard"
#define MOUSE_AUDIO_DIR AUDIO_BASE_DIR "/mouse"
#define USER_KEYBOARD_AUDIO_SUBPATH "/.local/share/vbx/soundpacks/keyboard"
#define USER_MOUSE_AUDIO_SUBPATH "/.local/share/vbx/soundpacks/mouse"
#define USER_CONFIG_FILENAME ".vbx.json"

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
static int current_keyboard_enabled = 1;
static int current_mouse_enabled = 1;
static int current_system_volume_following = 0;
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
  CliOptions cli_opts;
  char user_cfg_path[MAX_PATH_LENGTH];
  if (get_user_config_path(user_cfg_path, sizeof(user_cfg_path))) {
    if (access(user_cfg_path, R_OK) == 0) {
      LOG_DEBUG("Loading configuration from: %s", user_cfg_path);
      char *cfg_keyboard_sound = NULL;
      char *cfg_mouse_sound = NULL;
      int cfg_keyboard_volume = current_keyboard_volume;
      int cfg_mouse_volume = current_mouse_volume;
      int cfg_keyboard_enabled = 1;
      int cfg_mouse_enabled = 1;
      int cfg_system_volume_following = 0;
      if (read_user_config(user_cfg_path, &cfg_keyboard_sound, &cfg_mouse_sound,
                           &cfg_keyboard_volume, &cfg_mouse_volume,
                           &cfg_keyboard_enabled, &cfg_mouse_enabled,
                           &cfg_system_volume_following)) {
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
        current_keyboard_volume = cfg_keyboard_volume;
        current_mouse_volume = cfg_mouse_volume;
        current_keyboard_enabled = cfg_keyboard_enabled;
        current_mouse_enabled = cfg_mouse_enabled;
        current_system_volume_following = cfg_system_volume_following;
      }
    }
  }

  int parse_result = parse_cli(argc, argv, &cli_opts);
  if (parse_result == 1) {
    if (sound_name_owned) {
      free(sound_name);
    }
    if (mouse_sound_name_owned) {
      free(mouse_sound_name);
    }
    return 1;
  } else if (parse_result == 2) {
    if (sound_name_owned) {
      free(sound_name);
    }
    if (mouse_sound_name_owned) {
      free(mouse_sound_name);
    }
    return 0;
  }

  verbose = cli_opts.verbose;
  list_sounds = cli_opts.list_flag;
  flag_daemon = cli_opts.daemon_flag;
  flag_stop = cli_opts.stop_flag;
  if (list_sounds) {
    int rc = list_sound_packs();
    if (sound_name_owned) {
      free(sound_name);
    }
    if (mouse_sound_name_owned) {
      free(mouse_sound_name);
    }
    return rc;
  }

  // Show welcome message for first-time users
  if (access(user_cfg_path, F_OK) != 0 && !flag_daemon) {
    LOG_INFO("Welcome to VBX!");
    LOG_INFO("This appears to be your first time running VBX.");
    LOG_INFO("A default configuration will be created at: %s", user_cfg_path);
  }
  build_pidfile_path(pidfile_path, 1024);
  if (flag_stop) {
    pid_t running_pid = 0;
    if (!require_running_pid(&running_pid)) {
      if (sound_name_owned) {
        free(sound_name);
      }
      if (mouse_sound_name_owned) {
        free(mouse_sound_name);
      }
      return 1;
    }
    if (kill(running_pid, SIGTERM) != 0) {
      if (sound_name_owned) {
        free(sound_name);
      }
      if (mouse_sound_name_owned) {
        free(mouse_sound_name);
      }
      LOG_PERROR("kill");
      return 1;
    }
    for (int i = 0; i < 30; i++) {
      if (!process_is_running(running_pid)) {
        unlink(pidfile_path);
        LOG_INFO("VBX daemon stopped successfully.");
        if (sound_name_owned) {
          free(sound_name);
        }
        if (mouse_sound_name_owned) {
          free(mouse_sound_name);
        }
        return 0;
      }
      struct timespec ts;
      ts.tv_sec = 0;
      ts.tv_nsec = 100000000L;
      nanosleep(&ts, NULL);
    }
    if (sound_name_owned) {
      free(sound_name);
    }
    if (mouse_sound_name_owned) {
      free(mouse_sound_name);
    }
    LOG_ERROR("VBX: daemon did not stop in time. Try running 'vbx --stop' again.");
    return 1;
  }
  current_mute = read_runtime_mute_file();

  int config_updated = 0;
  if (cli_opts.sound != NULL) {
    if (sound_name_owned) {
      free(sound_name);
    }
    sound_name = cli_opts.sound;
    sound_name_owned = 0;
    config_updated = 1;
  }
  if (cli_opts.mouse_sound != NULL) {
    if (mouse_sound_name_owned) {
      free(mouse_sound_name);
    }
    mouse_sound_name = cli_opts.mouse_sound;
    mouse_sound_name_owned = 0;
    config_updated = 1;
  }
  if (cli_opts.volume >= 0) {
    current_keyboard_volume = cli_opts.volume;
    current_mouse_volume = cli_opts.volume;
    config_updated = 1;
  }
  if (cli_opts.keyboard_volume >= 0) {
    current_keyboard_volume = cli_opts.keyboard_volume;
    config_updated = 1;
  }
  if (cli_opts.mouse_volume >= 0) {
    current_mouse_volume = cli_opts.mouse_volume;
    config_updated = 1;
  }
  if (cli_opts.keyboard_mute >= 0) {
    current_keyboard_mute = cli_opts.keyboard_mute;
    config_updated = 1;
    if (current_keyboard_mute) {
      LOG_INFO("Keyboard sounds muted.");
    } else {
      LOG_INFO("Keyboard sounds unmuted.");
    }
  }
  if (cli_opts.mouse_mute >= 0) {
    current_mouse_mute = cli_opts.mouse_mute;
    config_updated = 1;
    if (current_mouse_mute) {
      LOG_INFO("Mouse sounds muted.");
    } else {
      LOG_INFO("Mouse sounds unmuted.");
    }
  }
  if (cli_opts.keyboard_enabled >= 0) {
    current_keyboard_enabled = cli_opts.keyboard_enabled;
    config_updated = 1;
    if (current_keyboard_enabled) {
      LOG_INFO("Keyboard sounds enabled.");
    } else {
      LOG_INFO("Keyboard sounds disabled.");
    }
  }
  if (cli_opts.mouse_enabled >= 0) {
    current_mouse_enabled = cli_opts.mouse_enabled;
    config_updated = 1;
    if (current_mouse_enabled) {
      LOG_INFO("Mouse sounds enabled.");
    } else {
      LOG_INFO("Mouse sounds disabled.");
    }
  }

  write_runtime_mute_file(current_mute);
  write_runtime_keyboard_mute_file(current_keyboard_mute);
  write_runtime_mouse_mute_file(current_mouse_mute);
  write_runtime_keyboard_enabled_file(current_keyboard_enabled);
  write_runtime_mouse_enabled_file(current_mouse_enabled);
  write_runtime_system_volume_following_file(current_system_volume_following);

  if (config_updated &&
      get_user_config_path(user_cfg_path, sizeof(user_cfg_path))) {
    if (!write_user_config(user_cfg_path, sound_name, mouse_sound_name,
                           current_keyboard_volume, current_mouse_volume,
                           current_keyboard_enabled, current_mouse_enabled,
                           current_system_volume_following)) {
      LOG_WARN("Failed to update config file %s", user_cfg_path);
      LOG_WARN("Your settings will not be saved for next time.");
    } else {
      LOG_DEBUG("Updated config file %s", user_cfg_path);
    }
  }

  if (config_updated && !flag_daemon && !list_sounds && !flag_stop) {
    if (sound_name_owned) {
      free(sound_name);
    }
    if (mouse_sound_name_owned) {
      free(mouse_sound_name);
    }
    return 0;
  }

  if (!config_updated &&
      get_user_config_path(user_cfg_path, sizeof(user_cfg_path))) {
    if (access(user_cfg_path, F_OK) != 0) {
      if (!write_user_config(user_cfg_path, sound_name, mouse_sound_name,
                             current_keyboard_volume, current_mouse_volume,
                             current_keyboard_enabled, current_mouse_enabled,
                             current_system_volume_following)) {
        LOG_WARN("Failed to write %s", user_cfg_path);
      } else if (verbose) {
        LOG_INFO("Created default configuration: %s", user_cfg_path);
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
  safe_snprintf_wrapper(get_key_presses_path, sizeof(get_key_presses_path),
                        "%s/vbx-input", VBX_BIN_DIR);
  safe_snprintf_wrapper(sound_player_path, sizeof(sound_player_path),
                        "%s/vbx-audio", VBX_BIN_DIR);
  if (access(get_key_presses_path, X_OK) != 0 ||
      access(sound_player_path, X_OK) != 0) {
    if (sound_name_owned) {
      free(sound_name);
    }
    if (mouse_sound_name_owned) {
      free(mouse_sound_name);
    }
    LOG_ERROR("Error: Cannot find or execute required binaries in %s", VBX_BIN_DIR);
    return 1;
  }
  signal(SIGINT, cleanup_processes);
  signal(SIGTERM, cleanup_processes);
  if (flag_daemon) {
    pid_t existing = 0;
    if (read_pidfile(pidfile_path, &existing) && process_is_running(existing)) {
      if (sound_name_owned) {
        free(sound_name);
      }
      if (mouse_sound_name_owned) {
        free(mouse_sound_name);
      }
      LOG_WARN("VBX daemon is already running (PID: %ld). Use 'vbx --stop' to stop it.", (long)existing);
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
  current_verbose = verbose;
  safe_strncpy(current_sound_name, sound_name, sizeof(current_sound_name));
  safe_strncpy(current_mouse_sound_name, mouse_sound_name,
               sizeof(current_mouse_sound_name));
  safe_strncpy(current_config_path, config_path, sizeof(current_config_path));
  safe_strncpy(current_sound_dir, sound_dir, sizeof(current_sound_dir));
  safe_strncpy(current_mouse_config_path, mouse_config_path,
               sizeof(current_mouse_config_path));
  safe_strncpy(current_mouse_sound_dir, mouse_sound_dir,
               sizeof(current_mouse_sound_dir));
  if (verbose && !is_daemon) {
    LOG_INFO("Starting VBX daemon...");
    LOG_INFO("Keyboard sound pack: %s", sound_name);
    LOG_INFO("Mouse sound pack: %s", mouse_sound_name);
    LOG_INFO("Keyboard config file: %s", config_path);
    LOG_INFO("Mouse config file: %s", mouse_config_path);
    LOG_INFO("Keyboard working directory: %s", sound_dir);
    LOG_INFO("Mouse working directory: %s", mouse_sound_dir);
    LOG_DEBUG("Press Ctrl+C to exit.");
  } else {
    if (!is_daemon) {
      LOG_INFO("VBX daemon started successfully!");
      LOG_INFO("  Keyboard: %s (volume: %d%%)", sound_name, current_keyboard_volume);
      LOG_INFO("  Mouse: %s (volume: %d%%)", mouse_sound_name, current_mouse_volume);
      LOG_INFO("  Config: ~/.vbx.json (auto-reload enabled)");
      LOG_INFO("Use 'vbx --stop' to stop the daemon.");
      LOG_INFO("Press Ctrl+C to exit.");
    }
  }
  if (!start_children(sound_dir, config_path, current_keyboard_volume, verbose, current_mute,
                      mouse_sound_dir, mouse_config_path, current_mouse_volume,
                      current_keyboard_mute, current_mouse_mute,
                      current_keyboard_enabled, current_mouse_enabled,
                      current_system_volume_following)) {
    if (sound_name_owned) {
      free(sound_name);
    }
    if (mouse_sound_name_owned) {
      free(mouse_sound_name);
    }
    return 1;
  }
  int status;
  pid_t finished_pid;
  signal(SIGHUP, handle_sighup);

  char user_cfg_path2[MAX_PATH_LENGTH];
  if (get_user_config_path(user_cfg_path2, sizeof(user_cfg_path2))) {
    LOG_INFO("Setting up file watcher for: %s", user_cfg_path2);
    start_config_watcher(user_cfg_path2);
  } else {
    LOG_WARN("Could not get user config path for file watching");
  }
  while (1) {
    if (reload_requested) {
      LOG_INFO("Reload requested, processing config changes...");
      reload_requested = 0;
      char user_cfg_path3[MAX_PATH_LENGTH];
      if (get_user_config_path(user_cfg_path3, sizeof(user_cfg_path3))) {
        handle_reload(
            user_cfg_path3, current_sound_name, current_mouse_sound_name,
            &current_keyboard_volume, &current_mouse_volume,
            current_config_path, current_sound_dir, current_mouse_config_path,
            current_mouse_sound_dir, current_mute, current_verbose,
            current_keyboard_mute, current_mouse_mute,
            &current_keyboard_enabled, &current_mouse_enabled,
            &current_system_volume_following);
      } else {
        LOG_ERROR("Failed to get user config path");
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
          LOG_WARN("Keyboard listener exited with status %d", WEXITSTATUS(status));
        keyboard_pid = 0;
        if (sound_pid > 0)
          kill(sound_pid, SIGTERM);
      } else if (finished_pid == sound_pid) {
        if (!is_daemon)
          LOG_WARN("Sound player exited with status %d", WEXITSTATUS(status));
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
    LOG_INFO("VBX daemon exited.");
  }
  if (sound_name_owned) {
    free(sound_name);
  }
  if (mouse_sound_name_owned) {
    free(mouse_sound_name);
  }
  return 0;
}
