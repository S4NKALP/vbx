#include "audio/playback.h"
#include "audio/types.h"
#include "common/utils.h"
#include <json-c/json.h>
#include <pthread.h>
#include <pulse/error.h>
#include <pulse/simple.h>
#include <sndfile.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

pthread_t sound_threads[MAX_CONCURRENT_SOUNDS];
volatile int thread_active[MAX_CONCURRENT_SOUNDS] = {0};
pthread_mutex_t thread_mutex = PTHREAD_MUTEX_INITIALIZER;

int init_audio() {
  if (g_sound_pack.is_multi)
    return 0;
  if (strlen(g_sound_pack.sound_file) == 0) {
    fprintf(stderr, "Error: No sound file specified in config\n");
    return -1;
  }
  if (access(g_sound_pack.sound_file, R_OK) != 0) {
    fprintf(stderr, "Sound file not accessible: %s\n", g_sound_pack.sound_file);
    perror("access");
    return -1;
  }
  SNDFILE *test_sf =
      sf_open(g_sound_pack.sound_file, SFM_READ, &g_sound_pack.sf_info);
  if (!test_sf) {
    fprintf(stderr, "Could not open sound file: %s\n", g_sound_pack.sound_file);
    fprintf(stderr, "libsndfile error: %s\n", sf_strerror(NULL));
    return -1;
  }
  sf_close(test_sf);
  if (g_verbose) {
    printf("Sound file info: %ld frames, %d channels, %d Hz\n",
           g_sound_pack.sf_info.frames, g_sound_pack.sf_info.channels,
           g_sound_pack.sf_info.samplerate);
  }
  return 0;
}

void *play_sound_thread(void *arg) {
  PlaybackData *data = (PlaybackData *)arg;
  int key_code = data->key_code;
  int thread_id = data->thread_id;
  int is_pressed = data->is_pressed;
  const char *file_to_play = NULL;

  int is_mouse_event = (key_code == 272 || key_code == 273 || key_code == 274);
  SoundPack *sound_pack = is_mouse_event ? &g_mouse_sound_pack : &g_sound_pack;
  float volume = is_mouse_event ? g_mouse_volume : g_volume;
  int mute_state = is_mouse_event ? g_mouse_mute : g_keyboard_mute;

  if (g_verbose) {
    printf("Thread %d: Playing %s sound for key %d (%s)\n", thread_id,
           is_mouse_event ? "mouse" : "keyboard", key_code,
           is_pressed ? "press" : "release");
  }

  if (g_mute || mute_state) {
    if (g_verbose) {
      printf("Thread %d: %s sound is muted\n", thread_id,
             is_mouse_event ? "Mouse" : "Keyboard");
    }
    goto exit_cleanup;
  }
  if (sound_pack->is_multi) {
    if (is_pressed && sound_pack->multi_key_mappings[key_code].press)
      file_to_play = sound_pack->multi_key_mappings[key_code].press;
    else if (!is_pressed && sound_pack->multi_key_mappings[key_code].release)
      file_to_play = sound_pack->multi_key_mappings[key_code].release;
    else if (is_pressed && sound_pack->num_generic_press_files > 0) {
      int idx = rand() % sound_pack->num_generic_press_files;
      file_to_play = sound_pack->generic_press_files[idx];
    } else if (!is_pressed && strlen(sound_pack->release_file) > 0)
      file_to_play = sound_pack->release_file;
    if (!file_to_play) {
      if (g_verbose) {
        printf("Thread %d: No sound file found for key %d (%s)\n", thread_id,
               key_code, is_pressed ? "press" : "release");
      }
      goto exit_cleanup;
    }
    if (g_verbose) {
      printf("Thread %d: Using sound file: %s\n", thread_id, file_to_play);
    }
    SF_INFO sf_info = {0};
    SNDFILE *sf = sf_open(file_to_play, SFM_READ, &sf_info);
    if (!sf) {
      fprintf(stderr, "Could not open sound file: %s (Error: %s)\n",
              file_to_play, sf_strerror(NULL));
      goto exit_cleanup;
    }
    pa_sample_spec ss = {.format = PA_SAMPLE_S16LE,
                         .rate = sf_info.samplerate,
                         .channels = sf_info.channels};
    int pa_error;
    pa_simple *pa_handle =
        pa_simple_new(NULL, "KeyboardSounds", PA_STREAM_PLAYBACK, NULL,
                      "playback", &ss, NULL, NULL, &pa_error);
    if (!pa_handle) {
      fprintf(stderr, "Could not initialize PulseAudio: %s\n",
              pa_strerror(pa_error));
      sf_close(sf);
      goto exit_cleanup;
    }
    int frames = 2048;
    short *buffer = malloc(frames * sf_info.channels * sizeof(short));
    if (!buffer) {
      pa_simple_free(pa_handle);
      sf_close(sf);
      goto exit_cleanup;
    }
    sf_count_t read;
    while ((read = sf_readf_short(sf, buffer, frames)) > 0) {
      for (sf_count_t i = 0; i < read * sf_info.channels; i++) {
        buffer[i] = (short)(buffer[i] * volume);
      }
      int pa_write_error;
      if (pa_simple_write(pa_handle, buffer,
                          read * sf_info.channels * sizeof(short),
                          &pa_write_error) < 0) {
        fprintf(stderr, "PulseAudio write error: %s\n",
                pa_strerror(pa_write_error));
        break;
      }
    }
    int pa_drain_error;
    pa_simple_drain(pa_handle, &pa_drain_error);
    pa_simple_free(pa_handle);
    sf_close(sf);
    free(buffer);
  } else {
    if (key_code >= 512 ||
        sound_pack->key_mappings[key_code].duration_ms == 0) {
      if (g_verbose) {
        printf("Thread %d: No mapping for key %d\n", thread_id, key_code);
      }
      goto exit_cleanup;
    }
    SoundMapping *mapping = &sound_pack->key_mappings[key_code];
    SF_INFO sf_info = sound_pack->sf_info;
    SNDFILE *sf = sf_open(sound_pack->sound_file, SFM_READ, &sf_info);
    if (!sf) {
      fprintf(stderr, "Thread: Could not open sound file\n");
      goto exit_cleanup;
    }
    pa_sample_spec ss = {.format = PA_SAMPLE_S16LE,
                         .rate = sf_info.samplerate,
                         .channels = sf_info.channels};
    int pa_error;
    pa_simple *pa_handle =
        pa_simple_new(NULL, "KeyboardSounds", PA_STREAM_PLAYBACK, NULL,
                      "playback", &ss, NULL, NULL, &pa_error);
    if (!pa_handle) {
      sf_close(sf);
      fprintf(stderr, "Thread: Could not initialize PulseAudio: %s\n",
              pa_strerror(pa_error));
      goto exit_cleanup;
    }
    sf_count_t start_frame = (mapping->start_ms * sf_info.samplerate) / 1000;
    sf_count_t duration_frames =
        (mapping->duration_ms * sf_info.samplerate) / 1000;
    sf_seek(sf, start_frame, SEEK_SET);
    short *buffer = malloc(duration_frames * sf_info.channels * sizeof(short));
    if (!buffer) {
      pa_simple_free(pa_handle);
      sf_close(sf);
      goto exit_cleanup;
    }
    sf_count_t frames_read = sf_readf_short(sf, buffer, duration_frames);
    for (sf_count_t i = 0; i < frames_read * sf_info.channels; i++) {
      buffer[i] = (short)(buffer[i] * volume);
    }
    int pa_write_error, pa_drain_error;
    pa_simple_write(pa_handle, buffer,
                    frames_read * sf_info.channels * sizeof(short),
                    &pa_write_error);
    pa_simple_drain(pa_handle, &pa_drain_error);
    free(buffer);
    pa_simple_free(pa_handle);
    sf_close(sf);
  }
exit_cleanup:
  pthread_mutex_lock(&thread_mutex);
  thread_active[thread_id] = 0;
  pthread_mutex_unlock(&thread_mutex);
  free(data);
  return NULL;
}

int find_available_thread_slot() {
  pthread_mutex_lock(&thread_mutex);
  for (int i = 0; i < MAX_CONCURRENT_SOUNDS; i++) {
    if (!thread_active[i]) {
      pthread_mutex_unlock(&thread_mutex);
      return i;
    }
  }
  pthread_mutex_unlock(&thread_mutex);
  return -1;
}

void play_sound_segment(int key_code, int is_pressed) {
  if (g_mute) {
    if (g_verbose) {
      printf("Sound muted - ignoring key %d (%s)\n", key_code,
             is_pressed ? "press" : "release");
    }
    return;
  }

  // Check if this is a mouse event and if mouse is disabled
  int is_mouse_event = (key_code == 272 || key_code == 273 || key_code == 274);
  if (is_mouse_event) {
    // For mouse events, we need to check if mouse is enabled
    // This will be handled by the main loop passing enabled state
    // For now, we'll add a simple check here
    extern int g_mouse_enabled;
    if (!g_mouse_enabled) {
      if (g_verbose) {
        printf("Mouse sounds disabled - ignoring mouse event %d\n", key_code);
      }
      return;
    }
  } else {
    // For keyboard events, check if keyboard is enabled
    extern int g_keyboard_enabled;
    if (!g_keyboard_enabled) {
      if (g_verbose) {
        printf("Keyboard sounds disabled - ignoring key %d\n", key_code);
      }
      return;
    }
  }
  if (!g_sound_pack.is_multi && !is_pressed) {
    if (g_verbose) {
      printf("Single mode: Ignoring key release for key %d\n", key_code);
    }
    return;
  }
  int slot = find_available_thread_slot();
  if (slot == -1) {
    if (g_verbose) {
      printf("Warning: No available thread slots\n");
    }
    return;
  }
  PlaybackData *data = malloc(sizeof(PlaybackData));
  if (!data)
    return;
  data->key_code = key_code;
  data->thread_id = slot;
  data->is_pressed = is_pressed;
  pthread_mutex_lock(&thread_mutex);
  thread_active[slot] = 1;
  if (pthread_create(&sound_threads[slot], NULL, play_sound_thread, data) !=
      0) {
    thread_active[slot] = 0;
    free(data);
    fprintf(stderr, "Failed to create sound thread\n");
  } else {
    pthread_detach(sound_threads[slot]);
  }
  pthread_mutex_unlock(&thread_mutex);
}


int parse_keyboard_event(const char *json_line, int *key_code,
                         int *is_pressed) {
  char *line_copy = xstrdup(json_line);
  if (!line_copy)
    return -1;
  char *newline = strchr(line_copy, '\n');
  if (newline)
    *newline = '\0';
  if (g_verbose) {
    printf("Parsing JSON: %s\n", line_copy);
  }
  json_object *root = json_tokener_parse(line_copy);
  free(line_copy);
  if (!root) {
    fprintf(stderr, "Failed to parse JSON: %s\n", json_line);
    return -1;
  }
  json_object *key_code_obj, *state_code_obj;
  if (json_object_object_get_ex(root, "key_code", &key_code_obj) &&
      json_object_object_get_ex(root, "state_code", &state_code_obj)) {
    *key_code = json_object_get_int(key_code_obj);
    *is_pressed = json_object_get_int(state_code_obj);
    if (g_verbose) {
      printf("Parsed key event: key_code=%d, is_pressed=%d\n", *key_code,
             *is_pressed);
    }
    json_object_put(root);
    return 0;
  }
  json_object_put(root);
  return -1;
}
