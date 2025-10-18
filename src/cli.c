#include "app/cli.h"
#include "common/utils.h"
#include "config.h"
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Helper function to parse device-specific arguments
static int parse_device_argument(const char *optarg, const char *action, 
                                int *keyboard_value, int *mouse_value, int value) {
  if (optarg == NULL || strcmp(optarg, "both") == 0) {
    *keyboard_value = value;
    *mouse_value = value;
  } else if (strcmp(optarg, "keyboard") == 0) {
    *keyboard_value = value;
  } else if (strcmp(optarg, "mouse") == 0) {
    *mouse_value = value;
  } else {
    safe_fprintf(stderr, "Error: Invalid device '%s' for %s\n", optarg, action);
    safe_fprintf(stderr, "Valid options: keyboard, mouse, or both\n");
    return 1;
  }
  return 0;
}

// Helper function to validate and set volume
static void set_volume(int *volume_ptr, int value) {
  *volume_ptr = validate_volume(value);
}

void print_usage(const char *program_name) {
  printf("VBX - Mechanical Keyboard Sound Simulator\n");
  printf("Version: %s\n\n", PROJECT_VERSION);

  printf("DESCRIPTION:\n");
  printf("  VBX brings realistic mechanical keyboard sounds to every keystroke.\n");
  printf("  Run interactively or as a background daemon with live config reload.\n\n");

  printf("USAGE:\n");
  printf("  %s [OPTIONS]\n\n", program_name);

  printf("SOUND PACKS:\n");
  printf("  -S, --sound PACK         Choose keyboard sound pack\n");
  printf("  -M, --mouse PACK         Choose mouse sound pack\n");
  printf("  -l, --list               Show available sound packs\n\n");

  printf("VOLUME CONTROL:\n");
  printf("  -V, --volume LEVEL       Set volume for both devices [0-100]\n");
  printf("  -K, --keyboard-volume    Set keyboard volume only [0-100]\n");
  printf("  -O, --mouse-volume       Set mouse volume only [0-100]\n\n");

  printf("AUDIO CONTROL:\n");
  printf("  -m, --mute[=DEVICE]      Mute audio (keyboard|mouse|both)\n");
  printf("  -u, --unmute[=DEVICE]    Unmute audio (keyboard|mouse|both)\n");
  printf("  --enable[=DEVICE]        Enable device sounds (keyboard|mouse|both)\n");
  printf("  --disable[=DEVICE]       Disable device sounds (keyboard|mouse|both)\n\n");

  printf("DAEMON MODE:\n");
  printf("  -d, --daemon             Run in background with auto-reload\n");
  printf("  -s, --stop               Stop background daemon\n\n");

  printf("OTHER OPTIONS:\n");
  printf("  -v, --verbose            Show detailed output\n");
  printf("  -h, --help               Show this help message\n\n");

  printf("QUICK EXAMPLES:\n");
  printf("  %s --list                                    # Browse sound packs\n",
         program_name);
  printf("  %s -S cherrymx-blue -V 75                   # Cherry MX Blue at 75%%\n",
         program_name);
  printf("  %s --daemon                                  # Start background service\n",
         program_name);
  printf("  %s --mute keyboard                          # Mute keyboard only\n",
         program_name);
  printf("  %s --disable mouse                          # Disable mouse sounds\n",
         program_name);
  printf("  %s --keyboard-volume 80 --mouse-volume 60    # Different volumes\n",
         program_name);
  printf("\n");
  printf("CONFIGURATION:\n");
  printf("  Settings are saved to ~/.vbx.json and auto-reload in daemon mode.\n");
  printf("  Use --list to see available sound packs and their sources.\n");
}

int parse_cli(int argc, char **argv, CliOptions *out) {
  safe_memset(out, 0, sizeof(*out));
  out->volume = -1;
  out->keyboard_volume = -1;
  out->mouse_volume = -1;
  out->keyboard_mute = -1;
  out->mouse_mute = -1;
  out->keyboard_enabled = -1;
  out->mouse_enabled = -1;
  static struct option long_options[] = {
      {"sound", required_argument, 0, 'S'},
      {"mouse", required_argument, 0, 'M'},
      {"volume", required_argument, 0, 'V'},
      {"keyboard-volume", required_argument, 0, 'K'},
      {"mouse-volume", required_argument, 0, 'O'},
      {"mute", optional_argument, 0, 'm'},
      {"unmute", optional_argument, 0, 'u'},
      {"enable", optional_argument, 0, 1000},
      {"disable", optional_argument, 0, 1001},
      {"list", no_argument, 0, 'l'},
      {"daemon", no_argument, 0, 'd'},
      {"stop", no_argument, 0, 's'},
      {"help", no_argument, 0, 'h'},
      {"verbose", no_argument, 0, 'v'},
      {0, 0, 0, 0}};
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
            (n == 6 && strncmp(a, "enable", 6) == 0) ||
            (n == 7 && strncmp(a, "disable", 7) == 0) ||
            (n == 4 && strncmp(a, "help", 4) == 0) ||
            (n == 7 && strncmp(a, "verbose", 7) == 0))) {
        safe_fprintf(stderr, "Error: Unknown option '%s'\n", argv[i]);
        safe_fprintf(stderr, "Run '%s --help' to see available options.\n", argv[0]);
        return 1;
      }
    }
  }
  optind = 1;
  int opt;
  while ((opt = getopt_long(argc, argv, "S:M:V:K:O:lhdsm::u::v", long_options,
                            NULL)) != -1) {
    switch (opt) {
    case 'd':
      out->daemon_flag = 1;
      break;
    case 's':
      out->stop_flag = 1;
      break;
    case 'S':
      out->sound = optarg;
      break;
    case 'M':
      out->mouse_sound = optarg;
      break;
    case 'V':
      set_volume(&out->volume, atoi(optarg));
      break;
    case 'K':
      set_volume(&out->keyboard_volume, atoi(optarg));
      break;
    case 'O':
      set_volume(&out->mouse_volume, atoi(optarg));
      break;
    case 'm':
      if (parse_device_argument(optarg, "mute", &out->keyboard_mute, &out->mouse_mute, 1) != 0) {
        return 1;
      }
      break;
    case 'u':
      if (parse_device_argument(optarg, "unmute", &out->keyboard_mute, &out->mouse_mute, 0) != 0) {
        return 1;
      }
      break;
    case 1000: // --enable
    case 1001: // --disable
      {
        int enable_value = (opt == 1000) ? 1 : 0;
        const char *action = (opt == 1000) ? "enable" : "disable";
        
        if (optarg == NULL) {
          // Handle space-separated argument: --enable mouse
          if (optind < argc && argv[optind][0] != '-') {
            optarg = argv[optind++];
          } else {
            optarg = "both"; // default to both if no argument
          }
        }
        if (parse_device_argument(optarg, action, &out->keyboard_enabled, &out->mouse_enabled, enable_value) != 0) {
          return 1;
        }
      }
      break;
    case 'l':
      out->list_flag = 1;
      break;
    case 'h':
      print_usage(argv[0]);
      return 2; // signal printed help
    case 'v':
      out->verbose = 1;
      break;
    default:
      print_usage(argv[0]);
      return 1;
    }
  }
  return 0;
}
