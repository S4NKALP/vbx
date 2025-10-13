#ifndef KEYVIBE_MUTE_H
#define KEYVIBE_MUTE_H

int read_runtime_mute_file(void);
void write_runtime_mute_file(int mute);
void write_runtime_keyboard_mute_file(int mute);
void write_runtime_mouse_mute_file(int mute);

#endif // KEYVIBE_MUTE_H


