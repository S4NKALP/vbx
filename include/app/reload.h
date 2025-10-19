#ifndef VBX_RELOAD_H
#define VBX_RELOAD_H

int handle_reload(const char *user_cfg_path,
                  char *current_sound_name, char *current_mouse_sound_name,
                  int *current_keyboard_volume, int *current_mouse_volume,
                  char *current_config_path, char *current_sound_dir,
                  char *current_mouse_config_path, char *current_mouse_sound_dir,
                  int current_mute, int current_verbose,
                  int current_mouse_volume_arg, int current_keyboard_mute,
                  int current_mouse_mute, int *current_keyboard_enabled, int *current_mouse_enabled);

#endif // VBX_RELOAD_H


