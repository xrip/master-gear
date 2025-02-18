#include "vdp.h"

uint16_t scanline = 0;
uint8_t VRAM[VRAM_SIZE] __attribute__ ((aligned(4))) = { 0 };

VDP vdp = {
    .nametable = &VRAM[0x3800],
    .sprites = &VRAM[0x3C00],
    .registers = {
        0x04,
        0x20,
        0xF1,
        0xFF,
        0x03,
        0x81,
        0xFB,
        0x00,
        0x00,
        0x00,
        0xFF,
    },
};

