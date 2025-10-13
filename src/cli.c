#include "app/cli.h"
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void print_usage(const char *program_name) {
  printf("KeyVibe - Mechanical Keyboard Sound Simulator\n\n");
  printf("Usage: %s [OPTIONS]\n\n", program_name);
  printf("Options:\n");
  printf("  -S, --sound SOUND_NAME   Select keyboard sound pack (default: "
         "eg-oreo)\n");
  printf(
      "  -M, --mouse SOUND_NAME   Select mouse sound pack (default: ping)\n");
  printf("  -V, --volume VOLUME      Set volume [0-100] for both keyboard and "
         "mouse (default: 50)\n");
  printf("  -K, --keyboard-volume VOLUME  Set keyboard volume [0-100] "
         "(default: 50)\n");
  printf("  -O, --mouse-volume VOLUME     Set mouse volume [0-100] (default: "
         "50)\n");
  printf("  -l, --list               List available sound packs\n");
  printf("  -d, --daemon             Run in background (write PID file)\n");
  printf("  -s, --stop               Stop background daemon\n");
  printf("  -m, --mute[=DEVICE]      Mute sounds (keyboard|mouse|both, "
         "default: both)\n");
  printf("  -u, --unmute[=DEVICE]    Unmute sounds (keyboard|mouse|both, "
         "default: both)\n");
  printf("  -h, --help               Show this help message\n");
  printf("  -v, --verbose            Enable verbose output\n");
  printf("  In daemon mode, editing ~/.keyvibe.json will auto-reload.\n");
}

int parse_cli(int argc, char **argv, CliOptions *out) {
  memset(out, 0, sizeof(*out));
  out->volume = -1;
  out->keyboard_volume = -1;
  out->mouse_volume = -1;
  out->keyboard_mute = -1;
  out->mouse_mute = -1;
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
      out->volume = atoi(optarg);
      if (out->volume < 0)
        out->volume = 0;
      if (out->volume > 100)
        out->volume = 100;
      break;
    case 'K':
      out->keyboard_volume = atoi(optarg);
      if (out->keyboard_volume < 0)
        out->keyboard_volume = 0;
      if (out->keyboard_volume > 100)
        out->keyboard_volume = 100;
      break;
    case 'O':
      out->mouse_volume = atoi(optarg);
      if (out->mouse_volume < 0)
        out->mouse_volume = 0;
      if (out->mouse_volume > 100)
        out->mouse_volume = 100;
      break;
    case 'm':
      if (optarg == NULL || strcmp(optarg, "both") == 0) {
        out->keyboard_mute = 1;
        out->mouse_mute = 1;
      } else if (strcmp(optarg, "keyboard") == 0) {
        out->keyboard_mute = 1;
      } else if (strcmp(optarg, "mouse") == 0) {
        out->mouse_mute = 1;
      } else {
        fprintf(stderr,
                "Invalid mute option: %s. Use 'keyboard', 'mouse', or 'both'\n",
                optarg);
        return 1;
      }
      break;
    case 'u':
      if (optarg == NULL || strcmp(optarg, "both") == 0) {
        out->keyboard_mute = 0;
        out->mouse_mute = 0;
      } else if (strcmp(optarg, "keyboard") == 0) {
        out->keyboard_mute = 0;
      } else if (strcmp(optarg, "mouse") == 0) {
        out->mouse_mute = 0;
      } else {
        fprintf(
            stderr,
            "Invalid unmute option: %s. Use 'keyboard', 'mouse', or 'both'\n",
            optarg);
        return 1;
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
