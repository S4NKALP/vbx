#define _XOPEN_SOURCE 500
#include "audio/playback.h"
#include "audio/types.h"
#include "common/utils.h"
#include "common/ipc.h"
#include "common/log.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/time.h>
#include <unistd.h>
#include <pthread.h>

int g_keyboard_enabled = 1;
int g_mouse_enabled = 1;
int g_system_volume_following = 0;
float g_system_volume_multiplier = 1.0f;

int load_sound_config(const char *config_path);

// Generic function to read runtime state files
static int read_runtime_state(const char *filename_suffix, int default_value) {
  char state_file[1024];
  const char *rd = getenv("XDG_RUNTIME_DIR");
  if (!rd || strlen(rd) == 0) {
    rd = "/tmp";
  }
  if (!safe_snprintf(state_file, sizeof(state_file), "%s/vbx-%s-%d", rd,
                     filename_suffix, (int)getuid())) {
    return default_value;
  }
  FILE *f = fopen(state_file, "r");
  if (!f) {
    return default_value;
  }
  int state = default_value;
  if (safe_fscanf(f, "%d", &state) == 1) {
    fclose(f);
    return state;
  }
  fclose(f);
  return default_value;
}

static int read_mute_state() {
  return read_runtime_state("mute", 0);
}

static int read_keyboard_mute_state() {
  return read_runtime_state("kbd-mute", 0);
}

static int read_mouse_mute_state() {
  return read_runtime_state("mouse-mute", 0);
}

static int read_keyboard_enabled_state() {
  return read_runtime_state("kbd-enabled", 1);
}

static int read_mouse_enabled_state() {
  return read_runtime_state("mouse-enabled", 1);
}

static int read_system_volume_following_state() {
  return read_runtime_state("sysvol-following", 0);
}

void *system_volume_poller_thread(void *arg) {
  (void)arg;
  while (1) {
    g_mute = read_mute_state();
    g_keyboard_mute = read_keyboard_mute_state();
    g_mouse_mute = read_mouse_mute_state();
    g_keyboard_enabled = read_keyboard_enabled_state();
    g_mouse_enabled = read_mouse_enabled_state();
    g_system_volume_following = read_system_volume_following_state();

    if (g_system_volume_following) {
      char buffer[128];
      FILE *fp = popen("wpctl get-volume @DEFAULT_AUDIO_SINK@", "r");
      if (fp != NULL) {
        if (fgets(buffer, sizeof(buffer), fp) != NULL) {
          float vol = 1.0f;
          if (sscanf(buffer, "Volume: %f", &vol) == 1) {
            g_system_volume_multiplier = vol;
          }
        }
        pclose(fp);
      }
    } else {
      g_system_volume_multiplier = 1.0f;
    }
    usleep(500000); // 500ms
  }
  return NULL;
}

int main(int argc, char *argv[]) {
  if (argc < 2 || argc > 12) {
    safe_fprintf(stderr,
            "Usage: %s <config.json> [volume] [verbose] [mute] [mouse_config] "
            "[mouse_volume] [keyboard_mute] [mouse_mute] [keyboard_enabled] "
            "[mouse_enabled] [system_volume_following]\n",
            argv[0]);
    safe_fprintf(stderr, "  volume: 0-100 (default: 50)\n");
    safe_fprintf(stderr, "  verbose: 1 to enable verbose output (default: 0)\n");
    safe_fprintf(stderr, "  mute: 1 to mute all sound (default: 0)\n");
    safe_fprintf(stderr, "  mouse_config: path to mouse config.json (optional)\n");
    safe_fprintf(stderr, "  mouse_volume: 0-100 for mouse volume (optional)\n");
    safe_fprintf(stderr, "  keyboard_mute: 1 to mute keyboard sounds (optional)\n");
    safe_fprintf(stderr, "  mouse_mute: 1 to mute mouse sounds (optional)\n");
    safe_fprintf(stderr,
            "  keyboard_enabled: 1 to enable keyboard sounds (optional)\n");
    safe_fprintf(stderr, "  mouse_enabled: 1 to enable mouse sounds (optional)\n");
    return 1;
  }
  if (argc >= 3) {
    int volume_percent = validate_volume(atoi(argv[2]));
    g_volume = volume_percent / 100.0f;
    if (g_verbose)
      printf("Volume set to: %d%%\n", volume_percent);
  } else {
    if (g_verbose)
      printf("Volume set to: 50%% (default)\n");
  }
  if (argc >= 4) {
    g_verbose = atoi(argv[3]);
    if (g_verbose) {
      printf("Verbose mode enabled\n");
    }
  }
  if (argc >= 5) {
    g_mute = atoi(argv[4]);
    if (g_mute) {
      printf("Sound muted\n");
    }
  }
  if (argc >= 6) {
    if (load_sound_config(argv[5]) != 0) {
      safe_fprintf(stderr, "Failed to load mouse sound configuration\n");
      return 1;
    }
    g_mouse_sound_pack = g_sound_pack;
    if (g_verbose) {
      printf("Mouse sound pack loaded from: %s\n", argv[5]);
    }
  }
  if (argc >= 7) {
    int mouse_volume_percent = validate_volume(atoi(argv[6]));
    g_mouse_volume = mouse_volume_percent / 100.0f;
    if (g_verbose)
      printf("Mouse volume set to: %d%%\n", mouse_volume_percent);
  }
  if (argc >= 8) {
    g_keyboard_mute = atoi(argv[7]);
    if (g_verbose)
      printf("Keyboard mute: %s\n", g_keyboard_mute ? "enabled" : "disabled");
  }
  if (argc >= 9) {
    g_mouse_mute = atoi(argv[8]);
    if (g_verbose)
      printf("Mouse mute: %s\n", g_mouse_mute ? "enabled" : "disabled");
  }
  if (argc >= 10) {
    g_keyboard_enabled = atoi(argv[9]);
    if (g_verbose)
      printf("Keyboard enabled: %s\n", g_keyboard_enabled ? "yes" : "no");
  }
  if (argc >= 11) {
    g_mouse_enabled = atoi(argv[10]);
    if (g_verbose)
      printf("Mouse enabled: %s\n", g_mouse_enabled ? "yes" : "no");
  }
  if (argc >= 12) {
    g_system_volume_following = atoi(argv[11]);
    if (g_verbose)
      printf("System volume following: %s\n", g_system_volume_following ? "yes" : "no");
  }
  if (load_sound_config(argv[1]) != 0) {
    safe_fprintf(stderr, "Failed to load keyboard sound configuration\n");
    return 1;
  }
  if (init_audio() != 0) {
    safe_fprintf(stderr, "Failed to initialize audio\n");
    return 1;
  }
  
  // Read states initially before starting threads and loops
  g_mute = read_mute_state();
  g_keyboard_mute = read_keyboard_mute_state();
  g_mouse_mute = read_mouse_mute_state();
  g_keyboard_enabled = read_keyboard_enabled_state();
  g_mouse_enabled = read_mouse_enabled_state();
  g_system_volume_following = read_system_volume_following_state();

  pthread_t poller_thread;
  pthread_create(&poller_thread, NULL, system_volume_poller_thread, NULL);
  pthread_detach(poller_thread);

  fd_set readfds;
  struct timeval timeout;
  while (1) {
    FD_ZERO(&readfds);
    FD_SET(STDIN_FILENO, &readfds);
    timeout.tv_sec = 1;
    timeout.tv_usec = 0;
    int ready = select(STDIN_FILENO + 1, &readfds, NULL, NULL, &timeout);
    if (ready == -1) {
      if (errno == EINTR) {
        continue;
      }
      perror("select");
      break;
    } else if (ready == 0) {
      if (g_verbose) {
        printf("Waiting for input...\n");
      }
      continue;
    }
    if (FD_ISSET(STDIN_FILENO, &readfds)) {
      VbxEvent ev;
      if (fread(&ev, sizeof(VbxEvent), 1, stdin) != 1) {
        if (feof(stdin)) {
          LOG_DEBUG("EOF reached on stdin");
        } else {
          perror("fread");
        }
        break;
      }
      play_sound_segment(ev.key_code, ev.is_pressed);
    }
  }
  return 0;
}
