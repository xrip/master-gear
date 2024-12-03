#include "vdp.h"

uint16_t scanline = 0;
uint8_t VRAM[VRAM_SIZE]= { 0 };

VDP vdp = {
    .nametable = &VRAM[0x3800],
    .sprites = &VRAM[0x3C00],
    .registers = { 0 },
};

