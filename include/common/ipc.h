#ifndef VBX_IPC_H
#define VBX_IPC_H

#include <stdint.h>

// Lightweight binary structure for IPC between vbx-input and vbx-audio
// Replaces the heavy JSON parsing
typedef struct {
    uint32_t key_code;
    uint8_t  is_pressed;
} VbxEvent;

#endif // VBX_IPC_H
