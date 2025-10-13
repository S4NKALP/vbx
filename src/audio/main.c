#include "audio/playback.h"
#include "audio/types.h"
#include "common/utils.h"
#include <errno.h>
#include <json-c/json.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/time.h>
#include <unistd.h>

// Global variables for enabled state
int g_keyboard_enabled = 1;
int g_mouse_enabled = 1;

int load_sound_config(const char *config_path);

static int read_mute_state() {
  char mute_file[1024];
  const char *rd = getenv("XDG_RUNTIME_DIR");
  if (!rd || strlen(rd) == 0) {
    rd = "/tmp";
  }
  if (!safe_snprintf(mute_file, sizeof(mute_file), "%s/keyvibe-mute-%d", rd,
                     (int)getuid())) {
    return 0;
  }
  FILE *f = fopen(mute_file, "r");
  if (!f) {
    return 0;
  }
  int mute = 0;
  if (fscanf(f, "%d", &mute) == 1) {
    fclose(f);
    return mute;
  }
  fclose(f);
  return 0;
}

static int read_keyboard_mute_state() {
  char mute_file[1024];
  const char *rd = getenv("XDG_RUNTIME_DIR");
  if (!rd || strlen(rd) == 0) {
    rd = "/tmp";
  }
  if (!safe_snprintf(mute_file, sizeof(mute_file), "%s/keyvibe-kbd-mute-%d", rd,
                     (int)getuid())) {
    return 0;
  }
  FILE *f = fopen(mute_file, "r");
  if (!f) {
    return 0;
  }
  int mute = 0;
  if (fscanf(f, "%d", &mute) == 1) {
    fclose(f);
    return mute;
  }
  fclose(f);
  return 0;
}

static int read_mouse_mute_state() {
  char mute_file[1024];
  const char *rd = getenv("XDG_RUNTIME_DIR");
  if (!rd || strlen(rd) == 0) {
    rd = "/tmp";
  }
  if (!safe_snprintf(mute_file, sizeof(mute_file), "%s/keyvibe-mouse-mute-%d",
                     rd, (int)getuid())) {
    return 0;
  }
  FILE *f = fopen(mute_file, "r");
  if (!f) {
    return 0;
  }
  int mute = 0;
  if (fscanf(f, "%d", &mute) == 1) {
    fclose(f);
    return mute;
  }
  fclose(f);
  return 0;
}

static int read_keyboard_enabled_state() {
  char enabled_file[1024];
  const char *rd = getenv("XDG_RUNTIME_DIR");
  if (!rd || strlen(rd) == 0) {
    rd = "/tmp";
  }
  if (!safe_snprintf(enabled_file, sizeof(enabled_file), "%s/keyvibe-kbd-enabled-%d", rd,
                     (int)getuid())) {
    return 1; // default enabled
  }
  FILE *f = fopen(enabled_file, "r");
  if (!f) {
    return 1; // default enabled
  }
  int enabled = 1;
  if (fscanf(f, "%d", &enabled) == 1) {
    fclose(f);
    return enabled;
  }
  fclose(f);
  return 1; // default enabled
}

static int read_mouse_enabled_state() {
  char enabled_file[1024];
  const char *rd = getenv("XDG_RUNTIME_DIR");
  if (!rd || strlen(rd) == 0) {
    rd = "/tmp";
  }
  if (!safe_snprintf(enabled_file, sizeof(enabled_file), "%s/keyvibe-mouse-enabled-%d",
                     rd, (int)getuid())) {
    return 1; // default enabled
  }
  FILE *f = fopen(enabled_file, "r");
  if (!f) {
    return 1; // default enabled
  }
  int enabled = 1;
  if (fscanf(f, "%d", &enabled) == 1) {
    fclose(f);
    return enabled;
  }
  fclose(f);
  return 1; // default enabled
}

int main(int argc, char *argv[]) {
  if (argc < 2 || argc > 9) {
    fprintf(stderr,
            "Usage: %s <config.json> [volume] [verbose] [mute] [mouse_config] "
            "[mouse_volume] [keyboard_mute] [mouse_mute]\n",
            argv[0]);
    fprintf(stderr, "  volume: 0-100 (default: 50)\n");
    fprintf(stderr, "  verbose: 1 to enable verbose output (default: 0)\n");
    fprintf(stderr, "  mute: 1 to mute all sound (default: 0)\n");
    fprintf(stderr, "  mouse_config: path to mouse config.json (optional)\n");
    fprintf(stderr, "  mouse_volume: 0-100 for mouse volume (optional)\n");
    fprintf(stderr, "  keyboard_mute: 1 to mute keyboard sounds (optional)\n");
    fprintf(stderr, "  mouse_mute: 1 to mute mouse sounds (optional)\n");
    return 1;
  }
  if (argc >= 3) {
    int volume_percent = atoi(argv[2]);
    if (volume_percent < 0)
      volume_percent = 0;
    if (volume_percent > 100)
      volume_percent = 100;
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
      fprintf(stderr, "Failed to load mouse sound configuration\n");
      return 1;
    }
    g_mouse_sound_pack = g_sound_pack;
    if (g_verbose) {
      printf("Mouse sound pack loaded from: %s\n", argv[5]);
    }
  }
  if (argc >= 7) {
    int mouse_volume_percent = atoi(argv[6]);
    if (mouse_volume_percent < 0)
      mouse_volume_percent = 0;
    if (mouse_volume_percent > 100)
      mouse_volume_percent = 100;
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
  if (load_sound_config(argv[1]) != 0) {
    fprintf(stderr, "Failed to load keyboard sound configuration\n");
    return 1;
  }
  if (init_audio() != 0) {
    fprintf(stderr, "Failed to initialize audio\n");
    return 1;
  }
  fd_set readfds;
  struct timeval timeout;
  char line[1024];
  while (1) {
    g_mute = read_mute_state();
    g_keyboard_mute = read_keyboard_mute_state();
    g_mouse_mute = read_mouse_mute_state();
    
    // Check enabled state from runtime files
    g_keyboard_enabled = read_keyboard_enabled_state();
    g_mouse_enabled = read_mouse_enabled_state();

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
      if (fgets(line, sizeof(line), stdin) == NULL) {
        if (feof(stdin)) {
          if (g_verbose)
            printf("EOF reached on stdin\n");
        } else {
          perror("fgets");
        }
        break;
      }
      int key_code, is_pressed;
      if (parse_keyboard_event(line, &key_code, &is_pressed) == 0) {
        play_sound_segment(key_code, is_pressed);
      }
    }
  }
  return 0;
}
