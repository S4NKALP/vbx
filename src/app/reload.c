#include "app/reload.h"
#include "app/process.h"
#include "soundpacks.h"
#include "user_config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int handle_reload(const char *user_cfg_path, char *current_sound_name,
                  char *current_mouse_sound_name, int *current_keyboard_volume,
                  int *current_mouse_volume, char *current_config_path,
                  char *current_sound_dir, char *current_mouse_config_path,
                  char *current_mouse_sound_dir, int current_mute,
                  int current_verbose, int current_mouse_volume_arg,
                  int current_keyboard_mute, int current_mouse_mute) {
  char *cfg_keyboard_sound = NULL;
  char *cfg_mouse_sound = NULL;
  int cfg_keyboard_volume = *current_keyboard_volume;
  int cfg_mouse_volume = *current_mouse_volume;
  if (read_user_config(user_cfg_path, &cfg_keyboard_sound, &cfg_mouse_sound,
                       &cfg_keyboard_volume, &cfg_mouse_volume)) {
    printf("Read config - keyboard:%s mouse:%s kvol:%d mvol:%d (current "
           "kvol:%d mvol:%d)\n",
           cfg_keyboard_sound ? cfg_keyboard_sound : "NULL",
           cfg_mouse_sound ? cfg_mouse_sound : "NULL", cfg_keyboard_volume,
           cfg_mouse_volume, *current_keyboard_volume, *current_mouse_volume);
    int keyboard_changed = 0, mouse_changed = 0, keyboard_volume_changed = 0,
        mouse_volume_changed = 0;
    if (cfg_keyboard_sound &&
        strcmp(current_sound_name, cfg_keyboard_sound) != 0) {
      keyboard_changed = 1;
      strncpy(current_sound_name, cfg_keyboard_sound, 1023);
      current_sound_name[1023] = '\0';
    }
    if (cfg_mouse_sound &&
        strcmp(current_mouse_sound_name, cfg_mouse_sound) != 0) {
      mouse_changed = 1;
      strncpy(current_mouse_sound_name, cfg_mouse_sound, 1023);
      current_mouse_sound_name[1023] = '\0';
    }
    free(cfg_keyboard_sound);
    free(cfg_mouse_sound);
    if (cfg_keyboard_volume != *current_keyboard_volume) {
      keyboard_volume_changed = 1;
      *current_keyboard_volume = cfg_keyboard_volume;
    }
    if (cfg_mouse_volume != *current_mouse_volume) {
      mouse_volume_changed = 1;
      *current_mouse_volume = cfg_mouse_volume;
    }
    printf("Config changed - keyboard:%d mouse:%d kvol:%d mvol:%d\n",
           keyboard_changed, mouse_changed, keyboard_volume_changed,
           mouse_volume_changed);
    if (keyboard_changed || mouse_changed || keyboard_volume_changed ||
        mouse_volume_changed) {
      int keyboard_valid = validate_keyboard_sound_pack(current_sound_name);
      int mouse_valid = validate_mouse_sound_pack(current_mouse_sound_name);
      if (keyboard_valid && mouse_valid) {
        if (build_paths_for_keyboard_sound(current_sound_name,
                                           current_config_path, 1024,
                                           current_sound_dir, 1024)) {
          if (build_paths_for_mouse_sound(current_mouse_sound_name,
                                          current_mouse_config_path, 1024,
                                          current_mouse_sound_dir, 1024)) {
            stop_children();
            start_children(current_sound_dir, current_config_path,
                           *current_keyboard_volume, current_verbose,
                           current_mute, current_mouse_sound_dir,
                           current_mouse_config_path, current_mouse_volume_arg,
                           current_keyboard_mute, current_mouse_mute);
            printf("Reloaded successfully: keyboard=%s, mouse=%s, kvol=%d, "
                   "mvol=%d\n",
                   current_sound_name, current_mouse_sound_name,
                   *current_keyboard_volume, *current_mouse_volume);
            return 1;
          } else {
            printf("Failed to build mouse sound paths\n");
          }
        } else {
          printf("Failed to build keyboard sound paths\n");
        }
      } else {
        if (!keyboard_valid)
          printf("Reload failed: invalid keyboard sound pack '%s'\n",
                 current_sound_name);
        if (!mouse_valid)
          printf("Reload failed: invalid mouse sound pack '%s'\n",
                 current_mouse_sound_name);
      }
    }
  } else {
    printf("Failed to read user config\n");
  }
  return 0;
}
