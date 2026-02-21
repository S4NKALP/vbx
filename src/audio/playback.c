#define _DEFAULT_SOURCE
#include "audio/playback.h"
#include "audio/types.h"
#include "common/utils.h"
#include "common/log.h"
#include <pthread.h>
#include <pulse/error.h>
#include <pulse/simple.h>
#include <sndfile.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>

static pa_simple *g_pa_handle = NULL;
static AudioVoice g_mixer_voices[MAX_MIXER_VOICES] = {0};
static pthread_mutex_t g_mixer_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_t g_mixer_thread;
static volatile int g_mixer_running = 0;

static inline float get_boosted_system_volume(float vol_multiplier) {
  if (vol_multiplier >= 1.0f) return vol_multiplier;
  if (vol_multiplier <= 0.0f) return 0.0f;
  float inv = 1.0f - vol_multiplier;
  return 1.0f - (inv * inv * inv);
}

void *mixer_thread_func(void *arg) {
  (void)arg;
  const int chunk_frames = 512;
  const int channels = g_sound_pack.sf_info.channels ? g_sound_pack.sf_info.channels : 2;
  short *mix_buffer = malloc(chunk_frames * channels * sizeof(short));
  
  while (g_mixer_running) {
    memset(mix_buffer, 0, chunk_frames * channels * sizeof(short));
    int any_active = 0;
    
    pthread_mutex_lock(&g_mixer_mutex);
    float sys_vol = get_boosted_system_volume(g_system_volume_multiplier);
    
    for (int i = 0; i < MAX_MIXER_VOICES; i++) {
      if (!g_mixer_voices[i].active) continue;
      any_active = 1;
      
      uint32_t samples_to_copy = chunk_frames * channels;
      if (g_mixer_voices[i].current_pos + samples_to_copy > g_mixer_voices[i].total_samples) {
        samples_to_copy = g_mixer_voices[i].total_samples - g_mixer_voices[i].current_pos;
      }
      
      float vol = g_mixer_voices[i].volume * sys_vol;
      for (uint32_t s = 0; s < samples_to_copy; s++) {
        int32_t mixed = mix_buffer[s] + (int32_t)(g_mixer_voices[i].data[g_mixer_voices[i].current_pos + s] * vol);
        // Clip
        if (mixed > 32767) mixed = 32767;
        else if (mixed < -32768) mixed = -32768;
        mix_buffer[s] = (short)mixed;
      }
      
      g_mixer_voices[i].current_pos += samples_to_copy;
      if (g_mixer_voices[i].current_pos >= g_mixer_voices[i].total_samples) {
        g_mixer_voices[i].active = 0;
      }
    }
    pthread_mutex_unlock(&g_mixer_mutex);
    
    if (any_active) {
      int pa_error;
      if (pa_simple_write(g_pa_handle, mix_buffer, chunk_frames * channels * sizeof(short), &pa_error) < 0) {
        LOG_ERROR("PulseAudio write error: %s", pa_strerror(pa_error));
      }
    } else {
      // Nothing playing, sleep a bit to avoid CPU spin
      struct timespec ts = {0, 10000000L};
      nanosleep(&ts, NULL); // 10ms
    }
  }
  
  free(mix_buffer);
  return NULL;
}

int init_audio() {
  // Use keyboard pack as reference for samplerate/channels
  pa_sample_spec ss = {
    .format = PA_SAMPLE_S16LE,
    .rate = g_sound_pack.sf_info.samplerate ? g_sound_pack.sf_info.samplerate : 44100,
    .channels = g_sound_pack.sf_info.channels ? g_sound_pack.sf_info.channels : 2
  };
  
  int pa_error;
  g_pa_handle = pa_simple_new(NULL, "vbx", PA_STREAM_PLAYBACK, NULL, "clicks", &ss, NULL, NULL, &pa_error);
  if (!g_pa_handle) {
    LOG_ERROR("Could not initialize PulseAudio stream: %s", pa_strerror(pa_error));
    return -1;
  }
  
  g_mixer_running = 1;
  if (pthread_create(&g_mixer_thread, NULL, mixer_thread_func, NULL) != 0) {
    LOG_ERROR("Failed to create mixer thread");
    return -1;
  }
  
  return 0;
}

void play_sound_segment(int key_code, int is_pressed) {
  if (g_mute) return;
  
  int is_mouse = (key_code == 272 || key_code == 273 || key_code == 274);
  int device_enabled = is_mouse ? g_mouse_enabled : g_keyboard_enabled;
  if (!device_enabled) return;

  SoundPack *pack = is_mouse ? &g_mouse_sound_pack : &g_sound_pack;
  float volume = is_mouse ? g_mouse_volume : g_volume;
  int device_mute = is_mouse ? g_mouse_mute : g_keyboard_mute;
  
  if (device_mute) return;
  
  short *pcm_data = NULL;
  uint32_t num_samples = 0;
  
  if (pack->is_multi) {
    if (is_pressed) {
      if (pack->multi_key_mappings[key_code].press_data) {
        pcm_data = pack->multi_key_mappings[key_code].press_data;
        num_samples = pack->multi_key_mappings[key_code].press_samples;
      } else if (pack->num_generic_press_files > 0) {
        int idx = rand() % pack->num_generic_press_files;
        pcm_data = pack->generic_press_data[idx];
        num_samples = pack->generic_press_samples[idx];
      }
    } else {
      if (pack->multi_key_mappings[key_code].release_data) {
        pcm_data = pack->multi_key_mappings[key_code].release_data;
        num_samples = pack->multi_key_mappings[key_code].release_samples;
      } else if (pack->release_data) {
        pcm_data = pack->release_data;
        num_samples = pack->release_samples;
      }
    }
  } else {
    if (is_pressed && key_code < 512) {
      pcm_data = pack->key_mappings[key_code].pcm_data;
      num_samples = pack->key_mappings[key_code].num_samples;
    }
  }
  
  if (!pcm_data || num_samples == 0) return;
  
  // Find a slot in the mixer
  pthread_mutex_lock(&g_mixer_mutex);
  int found = 0;
  for (int i = 0; i < MAX_MIXER_VOICES; i++) {
    if (!g_mixer_voices[i].active) {
      g_mixer_voices[i].data = pcm_data;
      g_mixer_voices[i].total_samples = num_samples;
      g_mixer_voices[i].current_pos = 0;
      g_mixer_voices[i].volume = volume;
      g_mixer_voices[i].active = 1;
      found = 1;
      break;
    }
  }
  pthread_mutex_unlock(&g_mixer_mutex);
  
  if (!found) {
    LOG_WARN("Mixer full, dropping sound");
  }
}
