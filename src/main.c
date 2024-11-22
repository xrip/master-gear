#pragma GCC push_options
#pragma GCC optimize ("unroll-loops")

#include <stdio.h>
#include <windows.h>
#include "win32/MiniFB.h"
#include "win32/audio.h"
#include "z80/z80.h"
#include "sn76489.h"

// create a CPU core object
Z80 cpu;

/* Display timing (NTSC) */
#define MASTER_CLOCK        (3579545)
#define LINES_PER_FRAME     (262)
#define FRAMES_PER_SECOND   (60)
#define CYCLES_PER_LINE     ((MASTER_CLOCK / FRAMES_PER_SECOND) / LINES_PER_FRAME)

/* Return values from the V counter */
const uint8_t vcnt[0x200] =
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
const uint8_t hcnt[0x200] =
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


#define SMS_WIDTH 256
#define SMS_HEIGHT 224

uint8_t SCREEN[SMS_WIDTH * SMS_HEIGHT + 8] = {0}; // +8 possible sprite overflow

uint8_t RAM[8192] = {0};
uint8_t ROM[1024 << 10] = {0};
uint8_t RAM_BANK[2][16384] = {0};

uint8_t *rom_slot1 = ROM;
uint8_t *rom_slot2 = ROM;
uint8_t *ram_rom_slot3 = ROM;

uint8_t slot3_is_ram = 0;
static uint8_t *key_status;

void WrZ80(register word address, const register byte value) {
    // printf("Write %04x to %04x\n", value, address);
    if (address >= 0x8000 && address < 0xC000 && slot3_is_ram) {
        ram_rom_slot3[address - 0x8000] = value;
        return;
    }

    if (address >= 0xC000) {
        RAM[address & 8191] = value;

        if (address >= 0xFFFC) {
            // Memory paging
            const uint8_t page = value & 0x1f;
            switch (address) {
                case 0xFFFC:
                    if (value >> 3 & 1) {
                        slot3_is_ram = 1 + (value >> 2); // ram bank 2 or 1
                    } else {
                        slot3_is_ram = 0;
                    }
                    break;
                case 0xFFFD:
                    rom_slot1 = ROM + page * 0x4000;
                // printf("slot 1 is ROM page %i\n", page);
                    break;
                case 0xFFFE:
                    rom_slot2 = ROM + page * 0x4000;
                    rom_slot2 -= 0x4000;
                // printf("slot 2 is ROM page %i\n", page);
                    break;
                case 0xFFFF:
                    if (slot3_is_ram) {
                        ram_rom_slot3 = RAM_BANK[slot3_is_ram - 1];
                        printf("slot 3 is RAM bank %i\n", slot3_is_ram - 1);
                    } else {
                        // printf("slot 3 is ROM page %i\n", page);
                        ram_rom_slot3 = ROM + page * 0x4000;
                    }
                    ram_rom_slot3 -= 0x8000;
                    break;
            }
        }
    }
}

byte RdZ80(const register word address) {
    // printf("Mem read %x\n", address);
    if (address <= 1024) {
        // fixed 1kb
        // non pageable
        return ROM[address];
    }

    if (address < 0x4000) {
        return rom_slot1[address];
    }

    if (address < 0x8000) {
        return rom_slot2[address];
    }

    if (address < 0xC000) {
        return ram_rom_slot3[address];
    }

    return RAM[address & 8191];
}

/*
0x0000 - 0x1FFF = Sprite / tile patters (numbers 0 to 255)
0x2000 - 0x37FF = Sprite / tile patters (numbers 256 to 447)
0x3800 - 0x3EFF = Name Table
0x3F00 - 0x3FFF = Sprite Info Table
 */
// http://www.codeslinger.co.uk/pages/projects/mastersystem/vdp.html
#define VRAM_SIZE (16384)
#define VRAM_SIZE_WRAP (VRAM_SIZE - 1)
uint8_t VRAM[VRAM_SIZE];
uint8_t CRAM[64]; // 32?


uint16_t h_counter, v_counter;
/*
BIT 7 = VSync Interrupt Pending
BIT 6 = Sprite Overflow
BIT 5 = Sprite Collision
 */
uint8_t vdp_status = 0;
uint8_t vdp_register[11] = {0};
uint8_t vdp_read_buffer = 0;

uint8_t vdp_code_register = 0;
uint16_t vdp_address_register = 0;

uint8_t vdp_latch = 0;
uint16_t control_word;

uint16_t vdp_nametable = 0x3800;
uint16_t vdp_sprite_attribute_table = 0x3f00;

static inline void vdp_increment_address() {
    vdp_address_register = (vdp_address_register + 1) & VRAM_SIZE_WRAP;
}

uint8_t vdp_read_byte() {
    const uint8_t result = VRAM[vdp_address_register];
    vdp_increment_address();
    return result;
}

void OutZ80(register word port, register byte value) {
    // printf("Z80 out port %02x value %02x\n", port & 0xff, value);
    switch (port & 0xff) {
        case 0x7E: sn76489_out(value);
            break; // SN76489
        case 0x7F: sn76489_out(value);
            break; // SN76489

        case 0xBE: // Data register
            vdp_latch = 0;
            vdp_read_buffer = value;

            switch (vdp_code_register) {
                case 0:
                case 1:
                case 2: VRAM[vdp_address_register] = value;
                    break;
                case 3: CRAM[vdp_address_register & 31] = value;
                // printf("CRAM[%i] = %02x\n", vdp_address_register & 31, value);

                // TODO:
                    mfb_set_pallete(vdp_address_register & 31,
                                    MFB_RGB(
                                        (value & 3) << 6,
                                        (value >> 2 & 3) << 6,
                                        (value >> 4 & 3) << 6)
                    );

                    break;
            }

            vdp_increment_address();
            break;
        case 0xBF: // Control register
            if (vdp_latch) {
                control_word |= value << 8;
                vdp_code_register = (value >> 6) & 3; // control_word >> 14
                vdp_address_register = control_word & VRAM_SIZE_WRAP;

                // printf("control_word 0x%04x %x %x\n", control_word, vdp_code_register, vdp_address_register);
                if (vdp_code_register == 0) {
                    vdp_read_buffer = VRAM[vdp_address_register];
                    vdp_increment_address();
                }

                if (vdp_code_register == 2) {
                    // printf("VDP register %i write %02x\n", value & 0xf, control_word & 0xff);

                    vdp_register[value & 0xf] = (uint8_t) (control_word & 0xff);

                    vdp_nametable = (vdp_register[2] << 10) & 0x3800;
                    vdp_sprite_attribute_table = (vdp_register[5] << 7) & 0x3F00;
                }
            } else {
                // FIXME: should vdp_address_register also set here?
                control_word = value;
            }

            vdp_latch ^= 1;

            break;
    }
}

byte InZ80(register word port) {
    // printf("Z80 in %02x\n", port & 0xff);

    switch (port & 0xff) {
        case 0x7E: return vcnt[v_counter];
        case 0x7F: {
            printf("HCOUNTER read\n");
            int pixel = (((cpu.ICount % CYCLES_PER_LINE) / 4) * 3) * 2;
            return (hcnt[((pixel >> 1) & 0x1FF)]);
        }
        case 0xBE: // Data register
            vdp_latch = 0;
            const uint8_t result = vdp_read_buffer;
            vdp_read_buffer = VRAM[vdp_address_register];
            vdp_increment_address();
            return result; // returns buffer and fill it with next value at same time
        case 0xBF: // Status Register
            uint8_t temp_status = vdp_status;
            vdp_latch = 0;

        /* Clear pending interrupt and sprite collision flags */
            vdp_status &= ~(0x80 | 0x40 | 0x20);

            return temp_status;

        case 0xC0:
        case 0xDC:
            uint8_t buttons = 0xff;

            if (key_status[0x26]) buttons ^= 0b1;
            if (key_status[0x28]) buttons ^= 0b10;
            if (key_status[0x25]) buttons ^= 0b100;
            if (key_status[0x27]) buttons ^= 0b1000;
            if (key_status['Z']) buttons ^= 0b10000;
            if (key_status['X']) buttons ^= 0b100000;
        // if (key_status[0x0d]) buttons ^= 0b1000000;
        // if (key_status[0x20]) buttons ^= 0b10000000;

            return buttons;
    }
    return 0xff;
}

void PatchZ80(register Z80 *R) {
}


void HandleInput(WPARAM wParam, BOOL isKeyDown) {
}

static inline size_t readfile(const char *pathname, uint8_t *dst) {
    FILE *file = fopen(pathname, "rb");
    fseek(file, 0, SEEK_END);
    const size_t rom_size = ftell(file);
    if (rom_size == 0) {
    }
    fseek(file, 0, SEEK_SET);

    fread(dst, sizeof(uint8_t), rom_size, file);
    fclose(file);
    return rom_size;
}

#define SPRITE_COUNT 64
#define SPRITE_HEIGHT 8

// Sega Master System Frame update cycle
static inline void sms_frame() {
    const uint8_t *nametable = VRAM + vdp_nametable;
    const uint8_t *sprite_table = VRAM + vdp_sprite_attribute_table;

    const uint8_t sprites_mode = vdp_register[1] >> 1 & 1;
    const uint8_t sprite_size = 8 << sprites_mode;
    const uint16_t sprites_offset = vdp_register[6] >> 2 & 1 ? 256 : 0;


    const uint8_t vscroll = vdp_register[9];
    const uint8_t vshift = vdp_register[9] & 7;

    for (uint8_t scanline = 0; scanline < 192; scanline++) {
        uint8_t priority_table[SMS_WIDTH + 8];
        uint8_t *priority_table_ptr = priority_table + 8;

        const int hscroll = vdp_register[0] & 0x40 && scanline < 0x10 ? 0 : 0x100 - vdp_register[8];
        const int nt_scroll = (hscroll >> 3);
        const int hshift = (hscroll & 7);
        // const uint8_t fine_scroll_x = horizontal_disabled ? 0 : vdp_register[8] & 7; // fine_x
        uint8_t *screen_pixel = SCREEN + scanline * SMS_WIDTH + (0 - hshift);

        const uint8_t screen_row = (vscroll + scanline) % 224 / 8;
        const uint8_t tile_row = (vshift + scanline) & 7;

        const uint16_t *tile_ptr = (uint16_t *) (nametable + screen_row * 64);

        // background rendering loop
        for (uint8_t column = 0; column < 32; ++column) {
            const uint16_t tile_info = tile_ptr[(column + nt_scroll) & 0x1f];
            const uint8_t priority = tile_info >> 12 & 1;

            const uint8_t palette_offset = (tile_info >> 11 & 1) * 16; // palette select
            const uint16_t pattern_offset = tile_info >> 10 & 1 ? (7 - tile_row) * 4 : tile_row * 4; // vertical flip

            // Extract Tile pattern
            const uint8_t *pattern_planes = &VRAM[pattern_offset + (tile_info & 0x1FF) * 32];
            const uint8_t plane0 = pattern_planes[0];
            const uint8_t plane1 = pattern_planes[1];
            const uint8_t plane2 = pattern_planes[2];
            const uint8_t plane3 = pattern_planes[3];

            /* unrolled loop
                        for (int x = 0; x < 8; ++x) {
                            const uint8_t bit = 7 - x; // non flipped
                            const uint8_t color = palette_offset +
                            ((plane0 >> bit) & 1) |
                            ((plane1 >> bit) & 1) << 1 |
                            ((plane2 >> bit) & 1) << 2 |
                            ((plane3 >> bit) & 1) << 3;
                            *screen_pixel++ = color;
                        }
            */

            // render background
            if (tile_info >> 9 & 1) {
                // horizontal flip
                *screen_pixel++ = palette_offset + (plane0 & 1) | (plane1 & 1) << 1 | (plane2 & 1) << 2 | (plane3 & 1)
                                  << 3;
                *screen_pixel++ = palette_offset + (plane0 >> 1 & 1) | (plane1 >> 1 & 1) << 1 | (plane2 >> 1 & 1) << 2 |
                                  (plane3 >> 1 & 1) << 3;
                *screen_pixel++ = palette_offset + (plane0 >> 2 & 1) | (plane1 >> 2 & 1) << 1 | (plane2 >> 2 & 1) << 2 |
                                  (plane3 >> 2 & 1) << 3;
                *screen_pixel++ = palette_offset + (plane0 >> 3 & 1) | (plane1 >> 3 & 1) << 1 | (plane2 >> 3 & 1) << 2 |
                                  (plane3 >> 3 & 1) << 3;
                *screen_pixel++ = palette_offset + (plane0 >> 4 & 1) | (plane1 >> 4 & 1) << 1 | (plane2 >> 4 & 1) << 2 |
                                  (plane3 >> 4 & 1) << 3;
                *screen_pixel++ = palette_offset + (plane0 >> 5 & 1) | (plane1 >> 5 & 1) << 1 | (plane2 >> 5 & 1) << 2 |
                                  (plane3 >> 5 & 1) << 3;
                *screen_pixel++ = palette_offset + (plane0 >> 6 & 1) | (plane1 >> 6 & 1) << 1 | (plane2 >> 6 & 1) << 2 |
                                  (plane3 >> 6 & 1) << 3;
                *screen_pixel++ = palette_offset + (plane0 >> 7 & 1) | (plane1 >> 7 & 1) << 1 | (plane2 >> 7 & 1) << 2 |
                                  (plane3 >> 7 & 1) << 3;
            } else {
                *screen_pixel++ = palette_offset + (plane0 >> 7 & 1) | (plane1 >> 7 & 1) << 1 | (plane2 >> 7 & 1) << 2 |
                                  (plane3 >> 7 & 1) << 3;
                *screen_pixel++ = palette_offset + (plane0 >> 6 & 1) | (plane1 >> 6 & 1) << 1 | (plane2 >> 6 & 1) << 2 |
                                  (plane3 >> 6 & 1) << 3;
                *screen_pixel++ = palette_offset + (plane0 >> 5 & 1) | (plane1 >> 5 & 1) << 1 | (plane2 >> 5 & 1) << 2 |
                                  (plane3 >> 5 & 1) << 3;
                *screen_pixel++ = palette_offset + (plane0 >> 4 & 1) | (plane1 >> 4 & 1) << 1 | (plane2 >> 4 & 1) << 2 |
                                  (plane3 >> 4 & 1) << 3;
                *screen_pixel++ = palette_offset + (plane0 >> 3 & 1) | (plane1 >> 3 & 1) << 1 | (plane2 >> 3 & 1) << 2 |
                                  (plane3 >> 3 & 1) << 3;
                *screen_pixel++ = palette_offset + (plane0 >> 2 & 1) | (plane1 >> 2 & 1) << 1 | (plane2 >> 2 & 1) << 2 |
                                  (plane3 >> 2 & 1) << 3;
                *screen_pixel++ = palette_offset + (plane0 >> 1 & 1) | (plane1 >> 1 & 1) << 1 | (plane2 >> 1 & 1) << 2 |
                                  (plane3 >> 1 & 1) << 3;
                *screen_pixel++ = palette_offset + (plane0 & 1) | (plane1 & 1) << 1 | (plane2 & 1) << 2 | (plane3 & 1)
                                  << 3;
            }

            // if background priority
            screen_pixel -= 8;
            for (uint8_t x = 0; x < 8; x++) {
                *priority_table_ptr++ = priority && *screen_pixel != palette_offset;
                screen_pixel++;
            }
        }

        // Sprites rendering loop
        for (int sprite_index = 0; sprite_index < SPRITE_COUNT; ++sprite_index) {
            const uint8_t sprite_y = sprite_table[sprite_index] + 1;
            if (scanline >= sprite_y && scanline < sprite_y + sprite_size) {
                const uint8_t sprite_x = sprite_table[128 + sprite_index * 2];
                const int8_t sprite_x_offset = vdp_register[0] >> 3 & 1 ? -8 : 0;

                const uint16_t tile_index = sprites_offset + sprite_table[128 + sprite_index * 2 + 1];
                const uint16_t sprite_pattern_offset = (tile_index * 32) + (scanline - sprite_y) * 4;


                // Extract Tile pattern
                const uint8_t *pattern_planes = &VRAM[sprite_pattern_offset];
                const uint8_t plane0 = pattern_planes[0];
                const uint8_t plane1 = pattern_planes[1];
                const uint8_t plane2 = pattern_planes[2];
                const uint8_t plane3 = pattern_planes[3];

                uint8_t *sprite_screen_pixels = SCREEN + scanline * SMS_WIDTH + (sprite_x + sprite_x_offset);
                priority_table_ptr = priority_table + sprite_x + sprite_x_offset;
#pragma GCC unroll(8)
                for (int8_t bit = 7; bit >= 0; --bit) {
                    if (*priority_table_ptr++) {
                        continue;
                    };

                    const uint8_t color = (plane0 >> bit & 1) | (plane1 >> bit & 1) << 1 | (plane2 >> bit & 1) << 2 | (
                                              plane3 >> bit & 1) << 3;
                    if (color) {
                        sprite_screen_pixels[7 - bit] = 16 + color;
                    }
                }
            }
        }

        v_counter++;

        if (scanline == vdp_register[10] && vdp_register[0] & 0x10) {
            vdp_status |= 0x40;
            IntZ80(&cpu, INT_IRQ);
        }

        ExecZ80(&cpu, CYCLES_PER_LINE);
    }
    vdp_status |= 0x80;
}


int main(int argc, char **argv) {
    int scale = 4;
    if (!argv[1]) {
        printf("Usage: master-gear.exe <rom.bin> [scale_factor]\n");
        exit(-1);
    }
    if (argc > 2) {
        scale = atoi(argv[2]);
    }

    if (!mfb_open("Sega Master System", SMS_WIDTH, SMS_HEIGHT, scale))
        return 1;
    key_status = (uint8_t *) mfb_keystatus();

    CreateThread(NULL, 0, SoundThread, NULL, 0, NULL);
    CreateThread(NULL, 0, TicksThread, NULL, 0, NULL);
    sn76489_reset();
    readfile(argv[1], ROM);
    ResetZ80(&cpu);

    memset(SCREEN, 255, SMS_WIDTH * SMS_HEIGHT);


    for (int y = 0; y < SMS_HEIGHT; y++) {
        for (int x = 0; x < SMS_WIDTH; x++) {
            SCREEN[x + y * SMS_WIDTH] = (x / 16) + ((y / 16) & 1) * 16;
        }
    }

    while (1) {
        // vdp_status = 0;
        v_counter = 0;

        sms_frame(); // 192 scanlines

        // vblank period
        while (v_counter++ < 262) {
            if (v_counter < 224 && vdp_status & 0x80 && vdp_register[1] & 0x20) {
                IntZ80(&cpu, INT_IRQ);
            }
            ExecZ80(&cpu, CYCLES_PER_LINE);
        }


        if (mfb_update(SCREEN, 60) == -1)
            exit(1);
    }
}
