#ifndef KEYVIBE_CLI_H
#define KEYVIBE_CLI_H

typedef struct {
  char *sound;
  char *mouse_sound;
  int volume;
  int keyboard_volume;
  int mouse_volume;
  int daemon_flag;
  int stop_flag;
  int list_flag;
  int verbose;
  int keyboard_mute; // -1 unchanged, 0 unmute, 1 mute
  int mouse_mute;    // -1 unchanged, 0 unmute, 1 mute
} CliOptions;

void print_usage(const char *program_name);
int parse_cli(int argc, char **argv, CliOptions *out);

#endif // KEYVIBE_CLI_H


