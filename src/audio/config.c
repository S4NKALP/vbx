#include "audio/config.h"
#include "audio/types.h"
#include "common/log.h"
#include "common/utils.h"
#include <errno.h>
#include <json-c/json.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

SoundPack g_sound_pack = {0};
SoundPack g_mouse_sound_pack = {0};
float g_volume = 1.0f;
float g_mouse_volume = 1.0f;
int g_mute = 0;
int g_keyboard_mute = 0;
int g_mouse_mute = 0;

static void get_full_path(char *buffer, size_t buffer_size,
                          const char *base_dir, const char *filename) {
  if (!filename || !base_dir) {
    buffer[0] = '\0';
    return;
  }
  if (filename[0] == '/') {
    safe_strncpy(buffer, filename, buffer_size);
  } else {
    if (!safe_snprintf(buffer, buffer_size, "%s/%s", base_dir, filename)) {
      buffer[0] = '\0';
    }
  }
}

static short *load_pcm_data(const char *path, uint32_t *out_samples, SF_INFO *out_info) {
  if (!path || strlen(path) == 0) return NULL;
  SF_INFO sf_info = {0};
  SNDFILE *sf = sf_open(path, SFM_READ, &sf_info);
  if (!sf) return NULL;
  
  uint32_t total_samples = (uint32_t)(sf_info.frames * sf_info.channels);
  short *buffer = malloc(total_samples * sizeof(short));
  if (!buffer) {
    sf_close(sf);
    return NULL;
  }
  
  sf_count_t read = sf_readf_short(sf, buffer, sf_info.frames);
  if (read < sf_info.frames) {
    // Partial read or error
  }
  
  sf_close(sf);
  if (out_samples) *out_samples = total_samples;
  if (out_info) *out_info = sf_info;
  return buffer;
}

int load_sound_config(const char *config_path, SoundPack *pack) {
  memset(pack, 0, sizeof(SoundPack));
  FILE *file = fopen(config_path, "r");
  if (!file) {
    LOG_ERROR("Error: Cannot open sound pack config: %s", config_path);
    LOG_PERROR("fopen");
    return -1;
  }
  char config_path_copy[1024];
  safe_strncpy(config_path_copy, config_path, sizeof(config_path_copy));
  char *config_dir = dirname(config_path_copy);
  fseek(file, 0, SEEK_END);
  long size = ftell(file);
  if (size < 0) {
    fclose(file);
    LOG_ERROR("Error: ftell failed while reading %s", config_path);
    return -1;
  }
  errno = 0;
  rewind(file);
  if (errno != 0) {
    fclose(file);
    LOG_ERROR("Error: rewind failed while reading %s", config_path);
    return -1;
  }
  char *json_string = malloc((size_t)size + 1);
  if (!json_string) {
    fclose(file);
    LOG_ERROR("Error: Memory allocation failed");
    return -1;
  }
  fread(json_string, 1, size, file);
  json_string[size] = '\0';
  fclose(file);
  json_object *root = json_tokener_parse(json_string);
  free(json_string);
  if (!root) {
    LOG_ERROR("Error: Invalid JSON in sound pack config");
    return -1;
  }
  const char *key_type = "single";
  json_object *obj;
  if (json_object_object_get_ex(root, "key_define_type", &obj))
    key_type = json_object_get_string(obj);
  
  pack->is_multi = strcmp(key_type, "multi") == 0;
  LOG_DEBUG("Config loaded: Using %s mode", pack->is_multi ? "multi" : "single");
  
  if (pack->is_multi) {
    if (json_object_object_get_ex(root, "sound", &obj)) {
      const char *pattern = json_object_get_string(obj);
      if (strstr(pattern, "%d") || strstr(pattern, "{")) {
        for (int i = 0; i <= 4; i++) {
          char temp_filename[256];
          if (strstr(pattern, "{")) {
              // Simple pattern replacement for {0}, {1}, etc.
              safe_snprintf(temp_filename, sizeof(temp_filename), "%s", pattern);
              char *brace = strstr(temp_filename, "{");
              if (brace) {
                  *brace = '0' + i;
                  safe_memmove(brace + 1, brace + 3, strlen(brace + 3) + 1);
              }
          } else {
            safe_snprintf(temp_filename, sizeof(temp_filename), pattern, i);
          }
          get_full_path(pack->generic_press_files[i], sizeof(pack->generic_press_files[i]), config_dir, temp_filename);
          if (access(pack->generic_press_files[i], R_OK) == 0) {
            pack->num_generic_press_files = i + 1;
            pack->generic_press_data[i] = load_pcm_data(pack->generic_press_files[i], &pack->generic_press_samples[i], NULL);
          } else break;
        }
      } else {
        get_full_path(pack->generic_press_files[0], sizeof(pack->generic_press_files[0]), config_dir, pattern);
        if (access(pack->generic_press_files[0], R_OK) == 0) {
          pack->num_generic_press_files = 1;
          pack->generic_press_data[0] = load_pcm_data(pack->generic_press_files[0], &pack->generic_press_samples[0], NULL);
          LOG_DEBUG("Generic sound cached: %s", pack->generic_press_files[0]);
        }
      }
    }
    if (json_object_object_get_ex(root, "soundup", &obj)) {
      get_full_path(pack->release_file, sizeof(pack->release_file), config_dir, json_object_get_string(obj));
      pack->release_data = load_pcm_data(pack->release_file, &pack->release_samples, NULL);
      LOG_DEBUG("Release sound cached: %s", pack->release_file);
    }
    if (json_object_object_get_ex(root, "defines", &obj)) {
      json_object_object_foreach(obj, key, val) {
        int key_code = -1;
        int is_release = 0;
        if (strcmp(key, "MouseLeft") == 0) key_code = 272;
        else if (strcmp(key, "MouseRight") == 0) key_code = 273;
        else if (strcmp(key, "MouseMiddle") == 0) key_code = 274;
        else {
          char *up = strstr(key, "-up");
          if (up) { is_release = 1; key_code = atoi(key); }
          else key_code = atoi(key);
        }
        if (key_code >= 0 && key_code < 512) {
          char full_path[1024];
          get_full_path(full_path, sizeof(full_path), config_dir, json_object_get_string(val));
          if (is_release) {
            pack->multi_key_mappings[key_code].release = xstrdup(full_path);
            pack->multi_key_mappings[key_code].release_data = load_pcm_data(full_path, &pack->multi_key_mappings[key_code].release_samples, NULL);
            LOG_DEBUG("Key %d-up cached: %s", key_code, full_path);
          } else {
            pack->multi_key_mappings[key_code].press = xstrdup(full_path);
            pack->multi_key_mappings[key_code].press_data = load_pcm_data(full_path, &pack->multi_key_mappings[key_code].press_samples, NULL);
            LOG_DEBUG("Key %d cached: %s", key_code, full_path);
          }
        }
      }
    }
  } else {
    if (json_object_object_get_ex(root, "sound", &obj) || json_object_object_get_ex(root, "audio_file", &obj)) {
      get_full_path(pack->sound_file, sizeof(pack->sound_file), config_dir, json_object_get_string(obj));
      uint32_t s;
      short *d = load_pcm_data(pack->sound_file, &s, &pack->sf_info);
      if (d) free(d);
      LOG_DEBUG("Single mode sound file: %s (metadata loaded)", pack->sound_file);
    }
    json_object *defines_obj = NULL;
    if (json_object_object_get_ex(root, "defines", &defines_obj) || json_object_object_get_ex(root, "definitions", &defines_obj)) {
      json_object_object_foreach(defines_obj, key, val) {
        int key_code = -1;
        if (strcmp(key, "MouseLeft") == 0) key_code = 272;
        else if (strcmp(key, "MouseRight") == 0) key_code = 273;
        else if (strcmp(key, "MouseMiddle") == 0) key_code = 274;
        else key_code = atoi(key);
        
        if (key_code >= 0 && key_code < 512) {
          if (json_object_is_type(val, json_type_array)) {
            pack->key_mappings[key_code].start_ms = json_object_get_int(json_object_array_get_idx(val, 0));
            pack->key_mappings[key_code].duration_ms = json_object_get_int(json_object_array_get_idx(val, 1));
          } else if (json_object_is_type(val, json_type_object)) {
            json_object *timings;
            if (json_object_object_get_ex(val, "timing", &timings) && json_object_array_length(timings) > 0) {
              json_object *t = json_object_array_get_idx(timings, 0);
              pack->key_mappings[key_code].start_ms = json_object_get_int(json_object_array_get_idx(t, 0));
              pack->key_mappings[key_code].duration_ms = json_object_get_int(json_object_array_get_idx(t, 1));
            }
          }
          if (pack->key_mappings[key_code].duration_ms > 0) {
            uint32_t sr = pack->sf_info.samplerate;
            uint32_t ch = pack->sf_info.channels;
            uint32_t start_frame = (pack->key_mappings[key_code].start_ms * sr) / 1000;
            uint32_t duration_frames = (pack->key_mappings[key_code].duration_ms * sr) / 1000;
            SNDFILE *sf = sf_open(pack->sound_file, SFM_READ, &pack->sf_info);
            if (sf) {
              sf_seek(sf, start_frame, SEEK_SET);
              pack->key_mappings[key_code].num_samples = duration_frames * ch;
              pack->key_mappings[key_code].pcm_data = malloc(pack->key_mappings[key_code].num_samples * sizeof(short));
              if (pack->key_mappings[key_code].pcm_data) {
                sf_readf_short(sf, pack->key_mappings[key_code].pcm_data, duration_frames);
              }
              sf_close(sf);
            }
          }
        }
      }
    }
  }
  json_object_put(root);
  return 0;
}
