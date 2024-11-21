#pragma GCC push_options
#pragma GCC optimize ("unroll-loops")

#include <stdio.h>
#include <windows.h>
#include "win32/MiniFB.h"
#include "z80/z80.h"

#define SMS_WIDTH 256
#define SMS_HEIGHT 224

uint8_t SCREEN[SMS_WIDTH * SMS_HEIGHT] = {0};

uint8_t RAM[8192] = {0};
uint8_t ROM[1024 << 10] = {0};
uint8_t RAM_BANK[2][16384] = {0};

uint8_t *rom_slot1 = ROM;
uint8_t *rom_slot2 = ROM;
uint8_t *ram_rom_slot3 = ROM;

uint8_t slot3_is_ram = 0;


void WrZ80(register word address, const register byte value) {
    // printf("Write %04x to %04x\n", value, address);
    if (address >= 0x8000 && address < 0xC000 && slot3_is_ram) {
        ram_rom_slot3[address] = value;
        return;
    }

    if (address >= 0xC000) {
        RAM[(address - 0xC000) % 8192] = value;

        if (address >= 0xFFFC) {
            // Memory paging
            const uint8_t page = value & 0x1F;
            switch (address) {
                case 0xFFFC:
                    if (value & 0b1000) {
                        slot3_is_ram = 1 + value >> 2; // ram bank 2 or 1
                    } else {
                        slot3_is_ram = 0;
                    }
                    break;
                case 0xFFFD:
                    rom_slot1 = ROM + page * 0x4000;
                    // rom_slot1 -= 0x4000;
                    printf("slot 1 is ROM page %i\n", page);
                    break;
                case 0xFFFE:
                    rom_slot2 = ROM + page * 0x4000;
                    rom_slot2 -= 0x4000;
                    printf("slot 2 is ROM page %i\n", page);
                    break;
                case 0xFFFF:
                    if (slot3_is_ram) {
                        ram_rom_slot3 = RAM_BANK[slot3_is_ram - 1];
                        printf("slot 3 is RAM bank %i\n", slot3_is_ram - 1);
                    } else {
                        printf("slot 3 is ROM page %i\n", page);
                        ram_rom_slot3 = ROM + page * 0x4000;
                        ram_rom_slot3 -= 0x8000;
                    }
                    break;
            }
        }
    }
}

byte RdZ80(const register word address) {
    // printf("Mem read %x\n", address);
    if (address <= 0x400) {
        // non pageable
        return ROM[address];
    }

    if (address <= 0x3FFF) {
        return rom_slot1[address];
    }

    if (address <= 0x7FFF) {
        return rom_slot2[address];
    }

    if (address <= 0xBFFF) {
        return ram_rom_slot3[address];
    }

    return RAM[(address - 0xC000) % 8192];
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


uint8_t h_counter, v_counter;
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
        case 0x7E: break; // SN76489
        case 0x7F: break; // SN76489

        case 0xBE: // Data register
            vdp_latch = 0;
            vdp_read_buffer = value;

            switch (vdp_code_register) {
                case 0 ... 2: VRAM[vdp_address_register] = value;
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
        case 0x7E: return v_counter;
        case 0x7F: return h_counter;

        case 0xBE: // Data register
            vdp_latch = 0;
            const uint8_t result = vdp_read_buffer;
            vdp_read_buffer = VRAM[vdp_address_register];
            vdp_increment_address();
            return result; // returns buffer and fill it with next value at same time
        case 0xBF: // Status Register
            vdp_latch = 0;
            return vdp_status;
    }
    return 0xff;
}

void PatchZ80(register Z80 *R) {
}

// create a CPU core object
Z80 cpu;

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

void psg_update() {
}

#define SPRITE_COUNT 64
#define SPRITE_HEIGHT 8

// Sega Master System Frame update cycle
static inline void sms_update() {

    const uint8_t *nametable = VRAM + vdp_nametable;
    const uint8_t *sprite_table = VRAM + vdp_sprite_attribute_table;

    const uint8_t sprites_mode = vdp_register[1] >> 1 & 1;
    const uint8_t sprite_size = 8 << sprites_mode;
    const uint8_t sprites_offset = (vdp_register[6] >> 2 & 1) * 256;

    for (uint8_t scanline = 0; scanline < 192; scanline++) {
        const int hscroll = vdp_register[0] & 0x40 && scanline < 0x10  ? 0 : 0x100 - vdp_register[8];
        int nt_scroll = (hscroll >> 3);
        int shift = (hscroll & 7);
        // const uint8_t fine_scroll_x = horizontal_disabled ? 0 : vdp_register[8] & 7; // fine_x
        uint8_t *screen_pixel = SCREEN + scanline * SMS_WIDTH + (0 - shift);


        const uint8_t screen_row = scanline / 8;
        const uint8_t tile_row = scanline & 7;

        const uint16_t * tile_ptr = (uint16_t *)((nametable + screen_row * 64) );
        for (uint8_t column = 0; column < 32; ++column) {

            // const uint16_t horizontal_offset = ;
            const uint16_t tile_info = tile_ptr[(column + nt_scroll) & 0x1f];

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
            if (tile_info >> 9 & 1) { // horizontal flip
                *screen_pixel++ = palette_offset + (plane0 & 1) | (plane1 & 1) << 1 | (plane2 & 1) << 2 | (plane3 & 1) << 3;
                *screen_pixel++ = palette_offset + (plane0 >> 1 & 1) | (plane1 >> 1 & 1) << 1 | (plane2 >> 1 & 1) << 2 | (plane3 >> 1 & 1) << 3;
                *screen_pixel++ = palette_offset + (plane0 >> 2 & 1) | (plane1 >> 2 & 1) << 1 | (plane2 >> 2 & 1) << 2 | (plane3 >> 2 & 1) << 3;
                *screen_pixel++ = palette_offset + (plane0 >> 3 & 1) | (plane1 >> 3 & 1) << 1 | (plane2 >> 3 & 1) << 2 | (plane3 >> 3 & 1) << 3;
                *screen_pixel++ = palette_offset + (plane0 >> 4 & 1) | (plane1 >> 4 & 1) << 1 | (plane2 >> 4 & 1) << 2 | (plane3 >> 4 & 1) << 3;
                *screen_pixel++ = palette_offset + (plane0 >> 5 & 1) | (plane1 >> 5 & 1) << 1 | (plane2 >> 5 & 1) << 2 | (plane3 >> 5 & 1) << 3;
                *screen_pixel++ = palette_offset + (plane0 >> 6 & 1) | (plane1 >> 6 & 1) << 1 | (plane2 >> 6 & 1) << 2 | (plane3 >> 6 & 1) << 3;
                *screen_pixel++ = palette_offset + (plane0 >> 7 & 1) | (plane1 >> 7 & 1) << 1 | (plane2 >> 7 & 1) << 2 | (plane3 >> 7 & 1) << 3;
            } else {
                *screen_pixel++ = palette_offset + (plane0 >> 7 & 1) | (plane1 >> 7 & 1) << 1 | (plane2 >> 7 & 1) << 2 | (plane3 >> 7 & 1) << 3;
                *screen_pixel++ = palette_offset + (plane0 >> 6 & 1) | (plane1 >> 6 & 1) << 1 | (plane2 >> 6 & 1) << 2 | (plane3 >> 6 & 1) << 3;
                *screen_pixel++ = palette_offset + (plane0 >> 5 & 1) | (plane1 >> 5 & 1) << 1 | (plane2 >> 5 & 1) << 2 | (plane3 >> 5 & 1) << 3;
                *screen_pixel++ = palette_offset + (plane0 >> 4 & 1) | (plane1 >> 4 & 1) << 1 | (plane2 >> 4 & 1) << 2 | (plane3 >> 4 & 1) << 3;
                *screen_pixel++ = palette_offset + (plane0 >> 3 & 1) | (plane1 >> 3 & 1) << 1 | (plane2 >> 3 & 1) << 2 | (plane3 >> 3 & 1) << 3;
                *screen_pixel++ = palette_offset + (plane0 >> 2 & 1) | (plane1 >> 2 & 1) << 1 | (plane2 >> 2 & 1) << 2 | (plane3 >> 2 & 1) << 3;
                *screen_pixel++ = palette_offset + (plane0 >> 1 & 1) | (plane1 >> 1 & 1) << 1 | (plane2 >> 1 & 1) << 2 | (plane3 >> 1 & 1) << 3;
                *screen_pixel++ = palette_offset + (plane0 & 1) | (plane1 & 1) << 1 | (plane2 & 1) << 2 | (plane3 & 1) << 3;
            }
        }
        // Iterate through all 64 sprites
        for (int sprite_index = 0; sprite_index < SPRITE_COUNT; ++sprite_index) {
            const uint8_t sprite_y = sprite_table[sprite_index] + 1;
            if (scanline >= sprite_y && scanline < sprite_y + sprite_size) {
                const uint8_t sprite_x = sprite_table[128 + sprite_index * 2];
                const uint8_t tile_index = sprite_table[128 + sprite_index * 2 + 1];
                const uint16_t sprite_pattern_offset = sprites_offset + (tile_index * 32) + (scanline - sprite_y) * 4;

                // Extract Tile pattern
                const uint8_t *pattern_planes = &VRAM[sprite_pattern_offset];
                const uint8_t plane0 = pattern_planes[0];
                const uint8_t plane1 = pattern_planes[1];
                const uint8_t plane2 = pattern_planes[2];
                const uint8_t plane3 = pattern_planes[3];

                uint8_t *sprite_screen_pixels = SCREEN + scanline * SMS_WIDTH + sprite_x;

                #pragma GCC unroll(8)
                for (int8_t bit = 7; bit >= 0; --bit) {
                    const uint8_t color = (plane0 >> bit & 1) | (plane1 >> bit & 1) << 1 | (plane2 >> bit & 1) << 2 | (plane3 >> bit & 1) << 3;
                    if (color) {
                        sprite_screen_pixels[7-bit] = 16 + color;
                    }
                }
            }
        }
        v_counter = scanline;
        ExecZ80(&cpu, 272);
    }
}

int main(int argc, char **argv) {
    if (!argv[1]) {
        printf("Usage: gamate.exe <rom.bin> [scale_factor] [ghosting_level]\n");
        exit(-1);
    }

    if (!mfb_open("Sega Master System", SMS_WIDTH, SMS_HEIGHT, 4))
        return 1;


    readfile(argv[1], ROM);
    ResetZ80(&cpu);

    memset(SCREEN, 255, SMS_WIDTH * SMS_HEIGHT);


    for (int y = 0; y < SMS_HEIGHT; y++) {
        for (int x = 0; x < SMS_WIDTH; x++) {
            SCREEN[x + y * SMS_WIDTH] = (x / 16) + ((y / 16) & 1) * 16;
        }
    }

    while (1) {
        sms_update();
        psg_update();
        v_counter = 0;
        vdp_status |= 0x80;
        // vdp_status |= 0x40;
        IntZ80(&cpu, INT_IRQ);

        if (mfb_update(SCREEN, 60) == -1)
            exit(1);
    }
}
