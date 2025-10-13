#ifndef KEYVIBE_AUDIO_PLAYBACK_H
#define KEYVIBE_AUDIO_PLAYBACK_H

int init_audio(void);
int parse_keyboard_event(const char *json_line, int *key_code, int *is_pressed);
void play_sound_segment(int key_code, int is_pressed);

#endif // KEYVIBE_AUDIO_PLAYBACK_H


