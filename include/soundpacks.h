#ifndef VBX_SOUNDPACKS_H
#define VBX_SOUNDPACKS_H
#include <stddef.h>

int resolve_keyboard_sound_base_dir(const char *sound_name, char *out_basedir,
                                    size_t out_sz);
int resolve_mouse_sound_base_dir(const char *sound_name, char *out_basedir,
                                 size_t out_sz);
int build_paths_for_keyboard_sound(const char *sound_name,
                                   char *out_config_path, size_t out_config_sz,
                                   char *out_sound_dir, size_t out_sound_sz);
int build_paths_for_mouse_sound(const char *sound_name, char *out_config_path,
                                size_t out_config_sz, char *out_sound_dir,
                                size_t out_sound_sz);
int validate_keyboard_sound_pack(const char *sound_name);
int validate_mouse_sound_pack(const char *sound_name);
int list_sound_packs(void);

#endif // VBX_SOUNDPACKS_H
