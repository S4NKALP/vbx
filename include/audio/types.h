#ifndef VBX_AUDIO_TYPES_H
#define VBX_AUDIO_TYPES_H

#include <sndfile.h>

#define MAX_CONCURRENT_SOUNDS 10

typedef struct {
  int start_ms;
  int duration_ms;
  short *pcm_data;
  uint32_t num_samples;
} SoundMapping;

typedef struct {
  char press_file[256];
  char release_file[256];
  char generic_press_files[5][256];
  short *generic_press_data[5];
  uint32_t generic_press_samples[5];
  int num_generic_press_files;
  char sound_file[256];
  short *release_data;
  uint32_t release_samples;
  SoundMapping key_mappings[512];
  struct {
    char *press;
    char *release;
    short *press_data;
    uint32_t press_samples;
    short *release_data;
    uint32_t release_samples;
  } multi_key_mappings[512];
  int is_multi;
  SF_INFO sf_info;
} SoundPack;

typedef struct {
  short *data;
  uint32_t total_samples;
  uint32_t current_pos;
  float volume;
  int active;
} AudioVoice;

#define MAX_MIXER_VOICES 32

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
extern int g_keyboard_enabled;
extern int g_mouse_enabled;
extern int g_system_volume_following;
extern float g_system_volume_multiplier;

#endif // VBX_AUDIO_TYPES_H
