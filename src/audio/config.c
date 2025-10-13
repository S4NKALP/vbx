#include "audio/types.h"
#include "common/utils.h"
#include <json-c/json.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

SoundPack g_sound_pack = {0};
SoundPack g_mouse_sound_pack = {0};
float g_volume = 1.0f;
float g_mouse_volume = 1.0f;
int g_verbose = 0;
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
    strncpy(buffer, filename, buffer_size - 1);
    buffer[buffer_size - 1] = '\0';
  } else {
    if (!safe_snprintf(buffer, buffer_size, "%s/%s", base_dir, filename)) {
      buffer[0] = '\0';
    }
  }
}

static char *xstrdup(const char *s) {
  if (!s) return NULL;
  size_t n = strlen(s) + 1;
  char *p = (char *)malloc(n);
  if (!p) return NULL;
  memcpy(p, s, n);
  return p;
}

int load_sound_config(const char *config_path) {
  FILE *file = fopen(config_path, "r");
  if (!file) {
    fprintf(stderr, "Error: Cannot open config file: %s\n", config_path);
    perror("fopen");
    return -1;
  }
  char config_path_copy[1024];
  strncpy(config_path_copy, config_path, sizeof(config_path_copy) - 1);
  config_path_copy[sizeof(config_path_copy) - 1] = '\0';
  char *config_dir = dirname(config_path_copy);
  fseek(file, 0, SEEK_END);
  long size = ftell(file);
  if (size < 0) {
    fclose(file);
    fprintf(stderr, "Error: ftell failed while reading %s\n", config_path);
    return -1;
  }
  errno = 0;
  rewind(file);
  if (errno != 0) {
    fclose(file);
    fprintf(stderr, "Error: rewind failed while reading %s\n", config_path);
    return -1;
  }
  char *json_string = malloc((size_t)size + 1);
  if (!json_string) {
    fclose(file);
    fprintf(stderr, "Error: Memory allocation failed\n");
    return -1;
  }
  fread(json_string, 1, size, file);
  json_string[size] = '\0';
  fclose(file);
  json_object *root = json_tokener_parse(json_string);
  free(json_string);
  if (!root) {
    fprintf(stderr, "Error: Invalid JSON in config file\n");
    return -1;
  }
  const char *key_type = "single";
  json_object *obj;
  if (json_object_object_get_ex(root, "key_define_type", &obj))
    key_type = json_object_get_string(obj);
  g_sound_pack.is_multi = strcmp(key_type, "multi") == 0;
  if (g_verbose) {
    printf("Config loaded: Using %s mode\n",
           g_sound_pack.is_multi ? "multi" : "single");
  }
  if (g_sound_pack.is_multi) {
    g_sound_pack.num_generic_press_files = 0;
    if (json_object_object_get_ex(root, "sound", &obj)) {
      const char *pattern = json_object_get_string(obj);
      if (g_verbose)
        printf("Sound pattern: %s\n", pattern);
      if (strstr(pattern, "%d") || strstr(pattern, "{")) {
        for (int i = 0; i <= 4; i++) {
          char temp_filename[256];
          if (strstr(pattern, "{")) {
            char temp_pattern[256];
            strncpy(temp_pattern, pattern, sizeof(temp_pattern) - 1);
            temp_pattern[sizeof(temp_pattern) - 1] = '\0';
            char *brace_start = strstr(temp_pattern, "{");
            char *brace_end = strstr(temp_pattern, "}");
            if (brace_start && brace_end) {
              *brace_start = '%';
              *(brace_start + 1) = 'd';
              memmove(brace_start + 2, brace_end + 1,
                      strlen(brace_end + 1) + 1);
            }
            snprintf(temp_filename, sizeof(temp_filename), temp_pattern, i);
          } else {
            snprintf(temp_filename, sizeof(temp_filename), pattern, i);
          }
          get_full_path(g_sound_pack.generic_press_files[i],
                        sizeof(g_sound_pack.generic_press_files[i]), config_dir,
                        temp_filename);
          if (access(g_sound_pack.generic_press_files[i], R_OK) == 0) {
            g_sound_pack.num_generic_press_files = i + 1;
          } else {
            if (g_verbose)
              printf("Generic sound file not found: %s\n",
                     g_sound_pack.generic_press_files[i]);
            break;
          }
        }
      } else {
        get_full_path(g_sound_pack.generic_press_files[0],
                      sizeof(g_sound_pack.generic_press_files[0]), config_dir,
                      pattern);
        if (access(g_sound_pack.generic_press_files[0], R_OK) == 0) {
          g_sound_pack.num_generic_press_files = 1;
          if (g_verbose)
            printf("Found single generic sound file: %s\n",
                   g_sound_pack.generic_press_files[0]);
        }
      }
      if (g_verbose)
        printf("Total generic press sound files: %d\n",
               g_sound_pack.num_generic_press_files);
    }
    if (json_object_object_get_ex(root, "soundup", &obj)) {
      char temp_release_file[256];
      strncpy(temp_release_file, json_object_get_string(obj),
              sizeof(temp_release_file) - 1);
      temp_release_file[sizeof(temp_release_file) - 1] = '\0';
      get_full_path(g_sound_pack.release_file,
                    sizeof(g_sound_pack.release_file), config_dir,
                    temp_release_file);
      if (g_verbose)
        printf("Release sound file: %s\n", g_sound_pack.release_file);
    }
    if (json_object_object_get_ex(root, "defines", &obj)) {
      json_object_object_foreach(obj, key, val) {
        int key_code;
        int is_release = 0;
        if (strcmp(key, "MouseLeft") == 0) {
          key_code = 272;
        } else if (strcmp(key, "MouseRight") == 0) {
          key_code = 273;
        } else if (strcmp(key, "MouseMiddle") == 0) {
          key_code = 274;
        } else {
          char *dash = strstr(key, "-up");
          if (dash) {
            char key_copy[16];
            strncpy(key_copy, key, dash - key);
            key_copy[dash - key] = '\0';
            key_code = atoi(key_copy);
            is_release = 1;
          } else {
            key_code = atoi(key);
          }
        }
        if (key_code >= 0 && key_code < 512) {
          const char *filename_relative = json_object_get_string(val);
          char full_filename[1024];
          get_full_path(full_filename, sizeof(full_filename), config_dir,
                        filename_relative);
          if (is_release) {
            if (g_sound_pack.multi_key_mappings[key_code].release)
              free(g_sound_pack.multi_key_mappings[key_code].release);
            g_sound_pack.multi_key_mappings[key_code].release =
                xstrdup(full_filename);
          } else {
            if (g_sound_pack.multi_key_mappings[key_code].press)
              free(g_sound_pack.multi_key_mappings[key_code].press);
            g_sound_pack.multi_key_mappings[key_code].press =
                xstrdup(full_filename);
          }
        }
      }
    }
  } else {
    if (json_object_object_get_ex(root, "sound", &obj) ||
        json_object_object_get_ex(root, "audio_file", &obj)) {
      char temp_sound_file[256];
      strncpy(temp_sound_file, json_object_get_string(obj),
              sizeof(temp_sound_file) - 1);
      temp_sound_file[sizeof(temp_sound_file) - 1] = '\0';
      get_full_path(g_sound_pack.sound_file, sizeof(g_sound_pack.sound_file),
                    config_dir, temp_sound_file);
      if (g_verbose)
        printf("Single mode sound file: %s\n", g_sound_pack.sound_file);
    }
    json_object *defines_obj = NULL;
    if (json_object_object_get_ex(root, "defines", &defines_obj) ||
        json_object_object_get_ex(root, "definitions", &defines_obj)) {
      json_object_object_foreach(defines_obj, key, val) {
        int key_code;
        if (strcmp(key, "MouseLeft") == 0) {
          key_code = 272;
        } else if (strcmp(key, "MouseRight") == 0) {
          key_code = 273;
        } else if (strcmp(key, "MouseMiddle") == 0) {
          key_code = 274;
        } else {
          key_code = atoi(key);
        }
        if (key_code >= 0 && key_code < 512) {
          if (json_object_is_type(val, json_type_array)) {
            if (json_object_array_length(val) >= 2) {
              g_sound_pack.key_mappings[key_code].start_ms =
                  json_object_get_int(json_object_array_get_idx(val, 0));
              g_sound_pack.key_mappings[key_code].duration_ms =
                  json_object_get_int(json_object_array_get_idx(val, 1));
            }
          } else if (json_object_is_type(val, json_type_object)) {
            json_object *timing_array;
            if (json_object_object_get_ex(val, "timing", &timing_array) &&
                json_object_is_type(timing_array, json_type_array) &&
                json_object_array_length(timing_array) > 0) {
              json_object *first_timing =
                  json_object_array_get_idx(timing_array, 0);
              if (json_object_is_type(first_timing, json_type_array) &&
                  json_object_array_length(first_timing) >= 2) {
                g_sound_pack.key_mappings[key_code].start_ms =
                    json_object_get_int(
                        json_object_array_get_idx(first_timing, 0));
                g_sound_pack.key_mappings[key_code].duration_ms =
                    json_object_get_int(
                        json_object_array_get_idx(first_timing, 1));
              }
            }
          }
        }
      }
    }
  }
  json_object_put(root);
  return 0;
}
