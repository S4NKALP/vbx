#ifndef VBX_CONFIG_IO_H
#define VBX_CONFIG_IO_H
#include <stddef.h>

// Resolve user config path: returns buffer on success, NULL on failure
char *get_user_config_path(char *buffer, size_t buflen);

// Write ~/.vbx.json in new format
int write_user_config(const char *path, const char *keyboard_sound,
                      const char *mouse_sound, int keyboard_volume,
                      int mouse_volume, int keyboard_enabled, int mouse_enabled);

// Read ~/.vbx.json supporting new and legacy formats
int read_user_config(const char *path, char **out_keyboard_sound,
                     char **out_mouse_sound, int *out_keyboard_volume,
                     int *out_mouse_volume, int *out_keyboard_enabled, int *out_mouse_enabled);

#endif // VBX_CONFIG_IO_H
