#include "common/utils.h"
#include "user_config.h"
#include <errno.h>
#include <json-c/json.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


char *get_user_config_path(char *buffer, size_t buflen) {
  const char *home = getenv("HOME");
  if (!home || strlen(home) == 0) {
    struct passwd *pw = getpwuid(getuid());
    if (pw && pw->pw_dir)
      home = pw->pw_dir;
  }
  if (!home)
    return NULL;
  if (!safe_snprintf(buffer, buflen, "%s/%s", home, ".keyvibe.json"))
    return NULL;
  return buffer;
}

int write_user_config(const char *path, const char *keyboard_sound,
                      const char *mouse_sound, int keyboard_volume,
                      int mouse_volume, int keyboard_enabled,
                      int mouse_enabled) {
  json_object *root = json_object_new_object();
  if (!root)
    return 0;

  json_object *keyboard = json_object_new_object();
  json_object_object_add(keyboard, "enabled",
                         json_object_new_boolean(keyboard_enabled));
  json_object_object_add(keyboard, "keyboard_sound",
                         json_object_new_string(keyboard_sound));
  json_object_object_add(keyboard, "volume",
                         json_object_new_int(keyboard_volume));
  json_object_object_add(root, "keyboard", keyboard);

  json_object *mouse = json_object_new_object();
  json_object_object_add(mouse, "enabled",
                         json_object_new_boolean(mouse_enabled));
  json_object_object_add(mouse, "mouse_sound",
                         json_object_new_string(mouse_sound));
  json_object_object_add(mouse, "volume", json_object_new_int(mouse_volume));
  json_object_object_add(root, "mouse", mouse);

  const char *json_str =
      json_object_to_json_string_ext(root, JSON_C_TO_STRING_PRETTY);
  FILE *f = fopen(path, "w");
  if (!f) {
    json_object_put(root);
    return 0;
  }
  fprintf(f, "%s\n", json_str);
  fclose(f);
  json_object_put(root);
  return 1;
}

int read_user_config(const char *path, char **out_keyboard_sound,
                     char **out_mouse_sound, int *out_keyboard_volume,
                     int *out_mouse_volume, int *out_keyboard_enabled,
                     int *out_mouse_enabled) {
  FILE *f = fopen(path, "r");
  if (!f)
    return 0;
  fseek(f, 0, SEEK_END);
  long sz = ftell(f);
  if (sz < 0) {
    fclose(f);
    return 0;
  }
  errno = 0;
  rewind(f);
  if (errno != 0) {
    fclose(f);
    return 0;
  }
  char *buf = malloc((size_t)sz + 1);
  if (!buf) {
    fclose(f);
    return 0;
  }
  fread(buf, 1, sz, f);
  buf[sz] = '\0';
  fclose(f);
  json_object *root = json_tokener_parse(buf);
  free(buf);
  if (!root)
    return 0;

  json_object *keyboard_obj, *mouse_obj;
  if (json_object_object_get_ex(root, "keyboard", &keyboard_obj) &&
      json_object_object_get_ex(root, "mouse", &mouse_obj)) {
    json_object *o;
    if (json_object_object_get_ex(keyboard_obj, "enabled", &o)) {
      *out_keyboard_enabled = json_object_get_boolean(o);
    } else {
      *out_keyboard_enabled = 1; // default enabled
    }
    if (json_object_object_get_ex(mouse_obj, "enabled", &o)) {
      *out_mouse_enabled = json_object_get_boolean(o);
    } else {
      *out_mouse_enabled = 1; // default enabled
    }
    if (json_object_object_get_ex(keyboard_obj, "keyboard_sound", &o)) {
      const char *s = json_object_get_string(o);
      if (s)
        *out_keyboard_sound = xstrdup(s);
    }
    if (json_object_object_get_ex(mouse_obj, "mouse_sound", &o)) {
      const char *s = json_object_get_string(o);
      if (s)
        *out_mouse_sound = xstrdup(s);
    }
    if (json_object_object_get_ex(keyboard_obj, "volume", &o)) {
      *out_keyboard_volume = json_object_get_int(o);
    }
    if (json_object_object_get_ex(mouse_obj, "volume", &o)) {
      *out_mouse_volume = json_object_get_int(o);
    }
  } else {
    // Legacy format - default to enabled
    *out_keyboard_enabled = 1;
    *out_mouse_enabled = 1;
    json_object *o;
    if (json_object_object_get_ex(root, "sound", &o)) {
      const char *s = json_object_get_string(o);
      if (s) {
        *out_keyboard_sound = xstrdup(s);
        *out_mouse_sound = xstrdup("ping");
      }
    }
    if (json_object_object_get_ex(root, "volume", &o)) {
      int legacy_volume = json_object_get_int(o);
      *out_keyboard_volume = legacy_volume;
      *out_mouse_volume = legacy_volume;
    }
  }
  json_object_put(root);
  return 1;
}
