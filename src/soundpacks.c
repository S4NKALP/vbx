#include "soundpacks.h"
#include "common/utils.h"
#include "config.h"
#include <dirent.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define AUDIO_BASE_DIR VBX_DATA_DIR "/soundpacks"
#define KEYBOARD_AUDIO_DIR AUDIO_BASE_DIR "/keyboard"
#define MOUSE_AUDIO_DIR AUDIO_BASE_DIR "/mouse"
#define USER_KEYBOARD_AUDIO_SUBPATH "/.local/share/vbx/soundpacks/keyboard"
#define USER_MOUSE_AUDIO_SUBPATH "/.local/share/vbx/soundpacks/mouse"

static void get_user_keyboard_audio_dir(char *buffer, size_t buflen) {
  buffer[0] = '\0';
  const char *home = get_home_dir();
  if (home)
    safe_snprintf_wrapper(buffer, buflen, "%s%s", home, USER_KEYBOARD_AUDIO_SUBPATH);
}

static void get_user_mouse_audio_dir(char *buffer, size_t buflen) {
  buffer[0] = '\0';
  const char *home = get_home_dir();
  if (home)
    safe_snprintf_wrapper(buffer, buflen, "%s%s", home, USER_MOUSE_AUDIO_SUBPATH);
}

int resolve_keyboard_sound_base_dir(const char *sound_name, char *out_basedir,
                                    size_t out_sz) {
  char user_audio_dir[1024];
  get_user_keyboard_audio_dir(user_audio_dir, sizeof(user_audio_dir));
  if (user_audio_dir[0]) {
    char p[1024];
    if ((size_t)snprintf(p, sizeof(p), "%s/%s/config.json", user_audio_dir,
                         sound_name) < sizeof(p) &&
        access(p, R_OK) == 0) {
    safe_strncpy(out_basedir, user_audio_dir, out_sz);
      return 1;
    }
  }
  char p2[1024];
  if ((size_t)snprintf(p2, sizeof(p2), "%s/%s/config.json", KEYBOARD_AUDIO_DIR,
                       sound_name) >= sizeof(p2)) {
    return 0;
  }
  if (access(p2, R_OK) == 0) {
    safe_strncpy(out_basedir, KEYBOARD_AUDIO_DIR, out_sz);
    return 1;
  }
  return 0;
}

int resolve_mouse_sound_base_dir(const char *sound_name, char *out_basedir,
                                 size_t out_sz) {
  char user_audio_dir[1024];
  get_user_mouse_audio_dir(user_audio_dir, sizeof(user_audio_dir));
  if (user_audio_dir[0]) {
    char p[1024];
    if ((size_t)snprintf(p, sizeof(p), "%s/%s/config.json", user_audio_dir,
                         sound_name) < sizeof(p) &&
        access(p, R_OK) == 0) {
    safe_strncpy(out_basedir, user_audio_dir, out_sz);
      return 1;
    }
  }
  char p2[1024];
  if ((size_t)snprintf(p2, sizeof(p2), "%s/%s/config.json", MOUSE_AUDIO_DIR,
                       sound_name) >= sizeof(p2)) {
    return 0;
  }
  if (access(p2, R_OK) == 0) {
    safe_strncpy(out_basedir, MOUSE_AUDIO_DIR, out_sz);
    return 1;
  }
  return 0;
}

int build_paths_for_keyboard_sound(const char *sound_name,
                                   char *out_config_path, size_t out_config_sz,
                                   char *out_sound_dir, size_t out_sound_sz) {
  char basedir[1024];
  if (!resolve_keyboard_sound_base_dir(sound_name, basedir, sizeof(basedir))) {
    return 0;
  }
  size_t base_len = strlen(basedir);
  size_t name_len = strlen(sound_name);
  const char *cfg_suffix = "/config.json";
  size_t cfg_suffix_len = strlen(cfg_suffix);
  if (base_len + 1 + name_len + cfg_suffix_len + 1 > out_config_sz)
    return 0;
  if (base_len + 1 + name_len + 1 > out_sound_sz)
    return 0;
  if (!safe_snprintf(out_config_path, out_config_sz, "%s/%s%s", basedir,
                     sound_name, cfg_suffix))
    return 0;
  if (!safe_snprintf(out_sound_dir, out_sound_sz, "%s/%s", basedir, sound_name))
    return 0;
  return 1;
}

int build_paths_for_mouse_sound(const char *sound_name, char *out_config_path,
                                size_t out_config_sz, char *out_sound_dir,
                                size_t out_sound_sz) {
  char basedir[1024];
  if (!resolve_mouse_sound_base_dir(sound_name, basedir, sizeof(basedir))) {
    return 0;
  }
  size_t base_len = strlen(basedir);
  size_t name_len = strlen(sound_name);
  const char *cfg_suffix = "/config.json";
  size_t cfg_suffix_len = strlen(cfg_suffix);
  if (base_len + 1 + name_len + cfg_suffix_len + 1 > out_config_sz)
    return 0;
  if (base_len + 1 + name_len + 1 > out_sound_sz)
    return 0;
  if (!safe_snprintf(out_config_path, out_config_sz, "%s/%s%s", basedir,
                     sound_name, cfg_suffix))
    return 0;
  if (!safe_snprintf(out_sound_dir, out_sound_sz, "%s/%s", basedir, sound_name))
    return 0;
  return 1;
}

int validate_keyboard_sound_pack(const char *sound_name) {
  char basedir[1024];
  if (!resolve_keyboard_sound_base_dir(sound_name, basedir, sizeof(basedir))) {
    fprintf(
        stderr,
        "Error: Keyboard sound pack '%s' not found in user or system dirs.\n",
        sound_name);
    fprintf(stderr, "Use --list to see available sound packs.\n");
    return 0;
  }
  char config_path[1024];
  if ((size_t)snprintf(config_path, sizeof(config_path), "%s/%s/config.json",
                       basedir, sound_name) >= sizeof(config_path)) {
    fprintf(stderr, "Error: Sound pack path too long: %s/%s\n", basedir,
            sound_name);
    fprintf(stderr, "Try using a shorter sound pack name.\n");
    return 0;
  }
  if (access(config_path, R_OK) != 0) {
    fprintf(stderr, "Error: Sound pack config not found: %s\n", config_path);
    fprintf(stderr, "Make sure the sound pack directory contains a valid config.json file.\n");
    return 0;
  }
  return 1;
}

int validate_mouse_sound_pack(const char *sound_name) {
  char basedir[1024];
  if (!resolve_mouse_sound_base_dir(sound_name, basedir, sizeof(basedir))) {
    fprintf(stderr,
            "Error: Mouse sound pack '%s' not found in user or system dirs.\n",
            sound_name);
    fprintf(stderr, "Use --list to see available sound packs.\n");
    return 0;
  }
  char config_path[1024];
  if ((size_t)snprintf(config_path, sizeof(config_path), "%s/%s/config.json",
                       basedir, sound_name) >= sizeof(config_path)) {
    fprintf(stderr, "Error: Sound pack path too long: %s/%s\n", basedir,
            sound_name);
    fprintf(stderr, "Try using a shorter sound pack name.\n");
    return 0;
  }
  if (access(config_path, R_OK) != 0) {
    fprintf(stderr, "Error: Sound pack config not found: %s\n", config_path);
    fprintf(stderr, "Make sure the sound pack directory contains a valid config.json file.\n");
    return 0;
  }
  return 1;
}

int list_sound_packs(void) {
  DIR *dir;
  struct dirent *entry;
  char path[1024];
  struct stat st;

  char user_keyboard_dir[1024] = {0};
  char user_mouse_dir[1024] = {0};
  const char *home = getenv("HOME");
  if (!home || strlen(home) == 0) {
    struct passwd *pw = getpwuid(getuid());
    if (pw && pw->pw_dir)
      home = pw->pw_dir;
  }
  if (home) {
    snprintf(user_keyboard_dir, sizeof(user_keyboard_dir), "%s%s", home,
             USER_KEYBOARD_AUDIO_SUBPATH);
    snprintf(user_mouse_dir, sizeof(user_mouse_dir), "%s%s", home,
             USER_MOUSE_AUDIO_SUBPATH);
  }

  printf("Available Sound Packs\n");
  printf("=====================\n\n");
  printf("\nKEYBOARD SOUND PACKS:\n");
  printf("======================\n");
  printf("%-30s %-15s\n", "Pack Name", "Source");
  printf("%-30s %-15s\n", "---------", "------");
  if (user_keyboard_dir[0] && (dir = opendir(user_keyboard_dir)) != NULL) {
    while ((entry = readdir(dir)) != NULL) {
      if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
        continue;
      if ((size_t)snprintf(path, sizeof(path), "%s/%s", user_keyboard_dir,
                           entry->d_name) >= sizeof(path)) {
        fprintf(stderr, "Warning: Path too long, skipping: %s/%s\n",
                user_keyboard_dir, entry->d_name);
        continue;
      }
      if (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) {
        printf("%-30s %-15s\n", entry->d_name, "user");
      }
    }
    closedir(dir);
  }
  dir = opendir(KEYBOARD_AUDIO_DIR);
  if (dir != NULL) {
    while ((entry = readdir(dir)) != NULL) {
      if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
        continue;
      if ((size_t)snprintf(path, sizeof(path), "%s/%s", KEYBOARD_AUDIO_DIR,
                           entry->d_name) >= sizeof(path)) {
        fprintf(stderr, "Warning: Path too long, skipping: %s/%s\n",
                KEYBOARD_AUDIO_DIR, entry->d_name);
        continue;
      }
      if (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) {
        printf("%-30s %-15s\n", entry->d_name, "system");
      }
    }
    closedir(dir);
  }
  printf("\nMOUSE SOUND PACKS:\n");
  printf("==================\n");
  printf("%-30s %-15s\n", "Pack Name", "Source");
  printf("%-30s %-15s\n", "---------", "------");
  if (user_mouse_dir[0] && (dir = opendir(user_mouse_dir)) != NULL) {
    while ((entry = readdir(dir)) != NULL) {
      if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
        continue;
      if ((size_t)snprintf(path, sizeof(path), "%s/%s", user_mouse_dir,
                           entry->d_name) >= sizeof(path)) {
        fprintf(stderr, "Warning: Path too long, skipping: %s/%s\n",
                user_mouse_dir, entry->d_name);
        continue;
      }
      if (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) {
        printf("%-30s %-15s\n", entry->d_name, "user");
      }
    }
    closedir(dir);
  }
  dir = opendir(MOUSE_AUDIO_DIR);
  if (dir != NULL) {
    while ((entry = readdir(dir)) != NULL) {
      if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
        continue;
      if ((size_t)snprintf(path, sizeof(path), "%s/%s", MOUSE_AUDIO_DIR,
                           entry->d_name) >= sizeof(path)) {
        fprintf(stderr, "Warning: Path too long, skipping: %s/%s\n",
                MOUSE_AUDIO_DIR, entry->d_name);
        continue;
      }
      if (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) {
        printf("%-30s %-15s\n", entry->d_name, "system");
      }
    }
    closedir(dir);
  }
  printf("\nUsage: vbx -S <pack-name> for keyboard or vbx -M <pack-name> for mouse\n");
  return 0;
}
