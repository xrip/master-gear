#pragma once
#include <stdint.h>
#include <stdio.h>

#include "shared.h"

#include "win32/MiniFB.h"

#define VRAM_SIZE 16384
#define VRAM_SIZE_WRAP (VRAM_SIZE - 1)

enum STATUS {
    VDP_VSYNC_PENDING = BIT_7,
    VDP_SPRITE_OVERFLOW = BIT_6,
    VDP_SPRITE_COLLISION = BIT_5,
};

enum REGISTERS {
    R0_MODE_CONTROL_1,
    R1_MODE_CONTROL_2,

    R2_NAMETABLE_BASE_ADDRESS,
    R3_COLOR_TABLE_BASE_ADDRESS,

    R4_PATTERN_GENERATOR_TABLE_BASE_ADDRESS,

    R5_SPRITE_ATTRIBUTE_TABLE_BASE_ADDRESS,
    R6_SPRITE_PATTERN_GENERATOR_TABLE_BASE_ADDRESS,

    R7_OVERSCAN_COLOR,

    R8_BACKGROUND_X_SCROLL,
    R9_BACKGROUND_Y_SCROLL,

    R10_LINE_COUNTER,
};

enum MODE_CONTROL_1 {
    VERTICAL_SCROLL_LOCK = BIT_7,
    HORIZONTAL_SCROLL_LOCK = BIT_6,
    HIDE_LEFTMOST_8PIXELS = BIT_5,
    ENABLE_LINE_INTERRUPT = BIT_4,
    SHIFT_SPRITES_LEFT_8PIXELS = BIT_3,
    MODE4 = BIT_2,

    EXTRA_HEIGHT_ENABLED = BIT_1, // SMS
    MODE2 = BIT_1, // TMS9918A
};


enum MODE_CONTROL_2 {
    VRAM_SIZE_16KB = BIT_7, // TMS9918A
    ENABLE_DISPLAY = BIT_6,
    ENABLE_FRAME_INTERRUPT = BIT_5,

    LINES_224_MODE = BIT_4, // SMS
    TEXTMODE = BIT_4, // TMS9918A

    LINES_240_MODE = BIT_3, // SMS
    MULTICOLOR_MODE = BIT_3, // TMS9918A

    TILED_SPRITES = BIT_2, // SMS
    LARGE_SPRITES = BIT_2, // TMS9918A

    DOUBLED_SPRITES = BIT_0
};

enum TILE_ATTRIBUTES {
    TILE_HORIZONTAL_FLIP = BIT_9,
    TILE_VERTICAL_FLIP = BIT_10,
    TILE_PALETTE = BIT_11,
    TILE_PRIORITY = BIT_12,
};

typedef struct {
    uint8_t status;
    uint8_t latch;
    uint8_t read_buffer;

    uint16_t address;
    uint16_t code;

    uint8_t *nametable;
    uint8_t *sprites;

    /* 32 for SMS, 64 for GG */
    uint8_t CRAM[64];
    uint8_t registers[11];
} VDP;

static const uint32_t sg1000_palette[16] = {
    0x000000,
    0x000000,
    0x21c942,
    0x5edc78,
    0x5455ed,
    0x7d75fc,
    0xd3524d,
    0x43ebf6,
    0xfd5554,
    0xff7978,
    0xd3c153,
    0xe5ce80,
    0x21b03c,
    0xc95bba,
    0xcccccc,
    0xffffff,
};
/* Return values from the V counter */
static const uint8_t vcnt[262] =
{
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F,
    0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F,
    0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F,
    0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F,
    0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F,
    0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E, 0x6F,
    0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7A, 0x7B, 0x7C, 0x7D, 0x7E, 0x7F,
    0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8A, 0x8B, 0x8C, 0x8D, 0x8E, 0x8F,
    0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9A, 0x9B, 0x9C, 0x9D, 0x9E, 0x9F,
    0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7, 0xA8, 0xA9, 0xAA, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF,
    0xB0, 0xB1, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7, 0xB8, 0xB9, 0xBA, 0xBB, 0xBC, 0xBD, 0xBE, 0xBF,
    0xC0, 0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7, 0xC8, 0xC9, 0xCA, 0xCB, 0xCC, 0xCD, 0xCE, 0xCF,
    0xD0, 0xD1, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7, 0xD8, 0xD9, 0xDA,
    0xD5, 0xD6, 0xD7, 0xD8, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE, 0xDF,
    0xE0, 0xE1, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7, 0xE8, 0xE9, 0xEA, 0xEB, 0xEC, 0xED, 0xEE, 0xEF,
    0xF0, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD, 0xFE, 0xFF,
};

/* Return values from the H counter */
static const uint8_t hcnt[343] =
{
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F,
    0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F,
    0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F,
    0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F,
    0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F,
    0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E, 0x6F,
    0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7A, 0x7B, 0x7C, 0x7D, 0x7E, 0x7F,
    0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8A, 0x8B, 0x8C, 0x8D, 0x8E, 0x8F,
    0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9A, 0x9B, 0x9C, 0x9D, 0x9E, 0x9F,
    0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7, 0xA8, 0xA9, 0xAA, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF,
    0xB0, 0xB1, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7, 0xB8, 0xB9, 0xBA, 0xBB, 0xBC, 0xBD, 0xBE, 0xBF,
    0xC0, 0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7, 0xC8, 0xC9, 0xCA, 0xCB, 0xCC, 0xCD, 0xCE, 0xCF,
    0xD0, 0xD1, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7, 0xD8, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE, 0xDF,
    0xE0, 0xE1, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7, 0xE8, 0xE9,
    0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9A, 0x9B, 0x9C, 0x9D, 0x9E, 0x9F,
    0xA0, 0xA1, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7, 0xA8, 0xA9, 0xAA, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF,
    0xB0, 0xB1, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7, 0xB8, 0xB9, 0xBA, 0xBB, 0xBC, 0xBD, 0xBE, 0xBF,
    0xC0, 0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7, 0xC8, 0xC9, 0xCA, 0xCB, 0xCC, 0xCD, 0xCE, 0xCF,
    0xD0, 0xD1, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7, 0xD8, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE, 0xDF,
    0xE0, 0xE1, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7, 0xE8, 0xE9, 0xEA, 0xEB, 0xEC, 0xED, 0xEE, 0xEF,
    0xF0, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD, 0xFE, 0xFF,
};


extern VDP vdp;
extern uint8_t VRAM[VRAM_SIZE];
extern uint16_t scanline;

extern uint8_t is_gamegear;

static inline uint8_t vdp_hcounter(const uint16_t pixel) {
    return hcnt[pixel >> 1 & 0x1FF];
}

static inline uint8_t vdp_vcounter() {
    return vcnt[scanline];
}

inline void vdp_increment_address() {
    vdp.address++;
    vdp.address &= VRAM_SIZE_WRAP;
}

inline uint8_t vdp_read_byte() {
    const uint8_t result = VRAM[vdp.address];
    vdp_increment_address();
    return result;
}

static inline uint8_t vdp_read() {
    vdp.latch = 0;
    const uint8_t result = vdp.read_buffer;
    vdp.read_buffer = vdp_read_byte();
    return result;
}

static inline void vdp_write(const uint8_t reg, const uint8_t value) {
    static uint16_t control_word;

    switch (reg & 1) {
        case 0: // Data Register
            vdp.latch = 0;

            switch (vdp.code) {
                case 0:
                case 1:
                case 2: VRAM[vdp.address] = value;
                    break;
                case 3:
                    vdp.CRAM[vdp.address & 63] = value;

                    if (is_gamegear) {
                        static uint16_t color_latch;
                        if (vdp.address & 1) {
                            color_latch |= value << 8;

                            mfb_set_pallete(vdp.address >> 1,
                                            MFB_RGB(
                                                (color_latch & 0b1111) << 4,
                                                (color_latch >> 4 & 0b1111) << 4,
                                                (color_latch >> 8 & 0b1111) << 4)
                            );
                        } else {
                            color_latch = value;
                        }
                    } else {
                        mfb_set_pallete(vdp.address & 31,
                                        MFB_RGB(
                                            (value & 3) << 6,
                                            (value >> 2 & 3) << 6,
                                            (value >> 4 & 3) << 6)
                        );
                    }

                    break;
            }

            vdp_increment_address();
            break;
        case 1: // Control Register
            if (vdp.latch ^= 1) {
                control_word = value;
            } else {
                control_word |= value << 8;

                vdp.code = control_word >> 14;
                vdp.address = control_word & VRAM_SIZE_WRAP;

                if (vdp.code == 0) {
                    vdp.read_buffer = vdp_read_byte();
                }

                if (vdp.code == 2) {
                    // printf("Register write %x %x\n", value & 0xf, control_word & 0xff);
                    vdp.registers[value & 0xf] = control_word & 0xff;

                    vdp.nametable = &VRAM[(vdp.registers[R2_NAMETABLE_BASE_ADDRESS] << 10) & 0x3800];
                    vdp.sprites = &VRAM[(vdp.registers[R5_SPRITE_ATTRIBUTE_TABLE_BASE_ADDRESS] << 7) & 0x3F00];
                    // vdp.sprites += vdp.registers[R6_SPRITE_PATTERN_GENERATOR_TABLE_BASE_ADDRESS] & BIT_2 ? 256 : 0; // 256 or 0
                }
            }
            break;
    }
}

static inline uint8_t vdp_status() {
    const uint8_t temp_status = vdp.status;
    vdp.latch = 0;

    /* Clear pending interrupt and sprite collision flags */
    vdp.status &= ~(VDP_VSYNC_PENDING | VDP_SPRITE_OVERFLOW | VDP_SPRITE_COLLISION);

    return temp_status;
}
