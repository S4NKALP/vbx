#ifndef KEYVIBE_AUDIO_TYPES_H
#define KEYVIBE_AUDIO_TYPES_H

#include <sndfile.h>

#define MAX_CONCURRENT_SOUNDS 10

typedef struct {
  int start_ms;
  int duration_ms;
} SoundMapping;

typedef struct {
  char press_file[256];
  char release_file[256];
  char generic_press_files[5][256];
  int num_generic_press_files;
  char sound_file[256];
  SoundMapping key_mappings[512];
  struct {
    char *press;
    char *release;
  } multi_key_mappings[512];
  int is_multi;
  SF_INFO sf_info;
} SoundPack;

typedef struct {
  int key_code;
  int thread_id;
  int is_pressed;
} PlaybackData;

extern SoundPack g_sound_pack;
extern SoundPack g_mouse_sound_pack;
extern float g_volume;
extern float g_mouse_volume;
extern int g_verbose;
extern int g_mute;
extern int g_keyboard_mute;
extern int g_mouse_mute;

#endif // KEYVIBE_AUDIO_TYPES_H
