#pragma GCC push_options
#pragma GCC optimize ("unroll-loops")

#include <stdio.h>
#include <windows.h>

#include "emu2413.h"
#include "win32/MiniFB.h"
#include "win32/audio.h"
#include "z80/z80.h"
#include "sn76489.h"

#include "cv_bios.h"

#include "sms.h"
#include "vdp.h"
// create a CPU core object
Z80 cpu;
OPLL *ym2413;
uint8_t ym2413_status;

uint8_t SCREEN[SMS_WIDTH * SMS_HEIGHT + 8] = {0}; // +8 possible sprite overflow


uint8_t RAM[8192] = {0};

uint8_t ROM[1024 << 10] = {0};
uint8_t RAM_BANK[2][16384] = {0};

uint8_t *rom_slot1 = ROM;
uint8_t *rom_slot2 = ROM;
uint8_t *ram_rom_slot3 = ROM;

uint8_t slot3_is_ram = 0;
static uint8_t *key_status;

uint8_t is_gamegear = 0, is_sg1000 = 0, is_cv = 0;
uint8_t page_mask = 0x1f;

void WrZ80(register word address, const register byte value) {
    if (address >= 0x6000 && address < 0x8000) {
        RAM[address & 1023] = value;
    }
}

byte RdZ80(const register word address) {
    if (address < 0x2000) { // BIOS
        return cv_bios[address];
    }
    if (address >= 0x6000 && address < 0x8000) {
        return RAM[address & 1023];
    }
    if (address >= 0x8000) {
        return ROM[address & 0x7FFF];
    }

    return 0;
}
void OutZ80(register word port, register byte value) {

    switch (port & 0xE0) {
        case 0xA0:
            // printf("CV port write %x %x %x\n", port & 0xE0, port & 1, value);
            vdp_write(port, value);
        break;
        case 0xE0:
             sn76489_out(value);
        break;
    }
}
byte InZ80(register word port) {
    // printf("CV port read %x\n", port & 0xE0);
    switch (port & 0xE0) {
        case 0xA0:
            return (port & 1) ? vdp_status() : vdp_read();
        case 0xE0: {
            if (port & 2) {

                /*if (key_status[VK_UP]) buttons |= 0x01;
                if (key_status[VK_DOWN]) buttons |= 0x04;
                if (key_status[VK_LEFT]) buttons |= 0x08;
                if (key_status[VK_RIGHT]) buttons |= 0x02;
                if (key_status['Z']) buttons |= 0x40;*/

                return ~0;
            } else {
                uint8_t buttons = 0x7F;
                if (key_status[VK_UP]) buttons &= 0xFE;
                if (key_status[VK_DOWN]) buttons &= 0xFB;
                if (key_status[VK_LEFT]) buttons &= 0xF7;
                if (key_status[VK_RIGHT]) buttons &= 0xFD;
                if (key_status['Z']) buttons &= 0xBF;

                return buttons;
            }
            }
    }
    return 0;
}
void OutZ801(register word port, register byte value) {
    // printf("Z80 out port %02x value %02x\n", port & 0xff, value);
    switch (port & 0xff) {
        case 0x3E:
            if (value & BIT_3) {
                rom_slot2 = ROM + 0x4000;
            }
            if (value & BIT_2) {
                printf("IO enabled\n");
            }
            break;
        case 0x7E: sn76489_out(value);
            break; // SN76489
        case 0x7F: sn76489_out(value);
            break; // SN76489

        case 0xBE: // Data register
        case 0xBF: // Control register
            vdp_write(port, value);
            break;

        case 0xF0:
        case 0xF1:
            OPLL_writeIO(ym2413, port, value);
            break;
        case 0xF2: ym2413_status = value & 3;
            break;
    }
}

byte InZ801(register word port) {
    // printf("Z80 in %02x\n", port & 0xff);

    switch (port & 0xff) {
        // gg input
        case 0x00: {
            uint8_t buttons = 0xff;
            if (key_status[0x0d]) buttons ^= 0x80;
            if (key_status[0x20]) buttons ^= 0x40;

            return buttons;
        }
        case 0x7E: return vdp_vcounter();
        case 0x7F: return vdp_hcounter(cpu.ICount % CYCLES_PER_LINE / 4 * 3 * 2);

        case 0xBE: // Data register
            return vdp_read();
        case 0xBF: // Status Register
            return vdp_status();

        case 0xC0:
        case 0xDC: {
            uint8_t buttons = 0xff;

            if (key_status[0x26]) buttons ^= 0b1;
            if (key_status[0x28]) buttons ^= 0b10;
            if (key_status[0x25]) buttons ^= 0b100;
            if (key_status[0x27]) buttons ^= 0b1000;
            if (key_status['Z']) buttons ^= 0b10000;
            if (key_status['X']) buttons ^= 0b100000;
            if (key_status[0x0d]) buttons ^= 0b1000000;
            if (key_status[0x20]) buttons ^= 0b10000000;

            return buttons;
        }
        case 0xF2: return ym2413_status;
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

    fseek(file, 0, SEEK_SET);

    fread(dst, sizeof(uint8_t), rom_size, file);
    fclose(file);
    return rom_size;
}

#define SPRITE_COUNT 64

// Sega Master System Frame update cycle
static inline void sms_frame() {
    int cpu_cycles = 0;
    uint8_t interrut_line = vdp.registers[R10_LINE_COUNTER];

    const uint8_t sprite_height = vdp.registers[R1_MODE_CONTROL_2] & EXTRA_HEIGHT_ENABLED ? 16 : 8;
    const uint8_t sprites_hshift = vdp.registers[R0_MODE_CONTROL_1] & SHIFT_SPRITES_LEFT_8PIXELS ? 8 : 0;

    const uint8_t hscroll_lock = vdp.registers[R0_MODE_CONTROL_1] & HORIZONTAL_SCROLL_LOCK;
    const uint8_t vscroll = vdp.registers[R9_BACKGROUND_Y_SCROLL];
    // const uint8_t start_column = vdp.registers[R0_MODE_CONTROL_1] & HIDE_LEFTMOST_8PIXELS ? 1 : 0;
    const uint16_t sprites_offset = vdp.registers[R6_SPRITE_PATTERN_GENERATOR_TABLE_BASE_ADDRESS] & BIT_2 ? 256 : 0;

    // const int hscroll_base = 256 - vdp.registers[R8_BACKGROUND_X_SCROLL];

    for (scanline = 0; scanline < 192; scanline++) {
        uint8_t priority_table[SMS_WIDTH + 8]; // allow 8 pixels overrun

        const int hscroll = hscroll_lock && scanline < 16 ? 0 : vdp.registers[R8_BACKGROUND_X_SCROLL];
        const uint8_t hscroll_fine = hscroll & 7;
        uint8_t *priority_table_ptr = priority_table + hscroll_fine;
        const int nametable_scroll = 32 - (hscroll >> 3);

        uint8_t *screen_pixel = &SCREEN[scanline * SMS_WIDTH + hscroll_fine];

        const uint16_t scanline_offset = (vscroll + scanline) % 224;
        const uint8_t screen_row = scanline_offset / 8;
        const uint8_t tile_row = scanline_offset & 7;

        const uint16_t *tile_ptr = (uint16_t *) &vdp.nametable[screen_row * 64];

        // background rendering loop
        for (uint8_t column = 0; column < 32; ++column) {
            const uint16_t tile_info = tile_ptr[(nametable_scroll + column) & 31];
            const uint8_t priority = (tile_info & TILE_PRIORITY) >> 12;

            const uint8_t palette_offset = (tile_info & TILE_PALETTE) >> 7; // palette select
            const uint16_t pattern_offset = tile_row * 4 ^ (tile_info & TILE_VERTICAL_FLIP ? 28 : 0); // vertical flip

            // Extract Tile pattern
            const uint8_t *pattern_planes = &VRAM[pattern_offset + (tile_info & 0x1FF) * 32];
            const uint8_t plane0 = pattern_planes[0];
            const uint8_t plane1 = pattern_planes[1];
            const uint8_t plane2 = pattern_planes[2];
            const uint8_t plane3 = pattern_planes[3];

            if (tile_info & TILE_HORIZONTAL_FLIP) {
#pragma GCC unroll(8)
                for (uint8_t bit = 0; bit < 8; ++bit) {
                    const uint8_t color = plane0 >> bit & 1 | (plane1 >> bit & 1) << 1 | (plane2 >> bit & 1) << 2 | (plane3 >> bit & 1) << 3;
                    *screen_pixel++ = palette_offset + color;
                    *priority_table_ptr++ = priority && color;
                }
            } else {
#pragma GCC unroll(8)
                for (int8_t bit = 7; bit >= 0; --bit) {
                    const uint8_t color = plane0 >> bit & 1 | (plane1 >> bit & 1) << 1 | (plane2 >> bit & 1) << 2 | (plane3 >> bit & 1) << 3;
                    *screen_pixel++ = palette_offset + color;
                    *priority_table_ptr++ = priority && color;
                }
            }
        }

        // Sprites rendering loop
        for (int sprite_index = 0; sprite_index < SPRITE_COUNT; ++sprite_index) {
            const uint8_t sprite_y = vdp.sprites[sprite_index] + 1;
            if (sprite_y == 208 + 1) break; // dont render anymore
            if (scanline >= sprite_y && scanline < sprite_y + sprite_height) {
                const uint8_t sprite_x = vdp.sprites[128 + sprite_index * 2];

                const uint16_t tile_index = sprites_offset + vdp.sprites[128 + sprite_index * 2 + 1];

                // Extract Tile pattern
                const uint8_t *pattern_planes = &VRAM[tile_index * 32 + (scanline - sprite_y) * 4];
                const uint8_t plane0 = pattern_planes[0];
                const uint8_t plane1 = pattern_planes[1];
                const uint8_t plane2 = pattern_planes[2];
                const uint8_t plane3 = pattern_planes[3];

                uint8_t *sprite_screen_pixels = SCREEN + scanline * SMS_WIDTH + (sprite_x - sprites_hshift);
                priority_table_ptr = priority_table + sprite_x - sprites_hshift;

#pragma GCC unroll(8)
                for (int8_t bit = 7; bit >= 0; --bit) {
                    if (*priority_table_ptr++) continue;

                    const uint8_t color = plane0 >> bit & 1 | (plane1 >> bit & 1) << 1 | (plane2 >> bit & 1) << 2 | (plane3 >> bit & 1) << 3;

                    if (color) {
                        sprite_screen_pixels[7 - bit] = 16 + color;
                    }
                }
            }
        }


        if (interrut_line == scanline && vdp.registers[R0_MODE_CONTROL_1] & ENABLE_LINE_INTERRUPT) {
            IntZ80(&cpu, INT_IRQ);
            interrut_line = scanline + vdp.registers[R10_LINE_COUNTER];
        }
        cpu_cycles = ExecZ80(&cpu, CYCLES_PER_LINE - cpu_cycles);
    }
    vdp.status |= VDP_VSYNC_PENDING;

    cpu_cycles = ExecZ80(&cpu, CYCLES_PER_LINE - cpu_cycles);
    scanline++;

    // vblank period
    while (scanline++ < 262) {
        if (vdp.status & VDP_VSYNC_PENDING && vdp.registers[R1_MODE_CONTROL_2] & ENABLE_FRAME_INTERRUPT) {
            IntZ80(&cpu, INT_IRQ);
        }
        cpu_cycles = ExecZ80(&cpu, CYCLES_PER_LINE - cpu_cycles);
    }
}

static inline void sg1000_frame() {
    int cpu_cycles = 0;

    const uint8_t *pattern_table = &VRAM[(vdp.registers[R4_PATTERN_GENERATOR_TABLE_BASE_ADDRESS] & 4) << 11];
    const uint8_t *sprites = &VRAM[(vdp.registers[R6_SPRITE_PATTERN_GENERATOR_TABLE_BASE_ADDRESS] & 7) << 11];
    const uint16_t region = (vdp.registers[R4_PATTERN_GENERATOR_TABLE_BASE_ADDRESS] & 3) << 8;
    const uint8_t *color_table = &VRAM[(vdp.registers[R3_COLOR_TABLE_BASE_ADDRESS] & 0x80) << 6];

    const uint8_t sprite_size = vdp.registers[R1_MODE_CONTROL_2] & EXTRA_HEIGHT_ENABLED ? 16 : 8;

    const uint8_t overscan_color = vdp.registers[R7_OVERSCAN_COLOR] & 0xf;

    for (scanline = 0; scanline < 192; scanline++) {
        uint8_t *screen_pixel = &SCREEN[scanline * SMS_WIDTH];

        const uint8_t screen_row = scanline / 8;
        const uint8_t tile_row = scanline & 7;

        const uint16_t nametable_offset = screen_row * 32;
        const uint8_t *tiles_row = &vdp.nametable[nametable_offset];

        // background rendering loop
        for (uint8_t column = 0; column < 32; ++column) {
            const uint16_t tile_index = (tiles_row[column] | region & 0x300 & nametable_offset + column) * 8 + tile_row;
            const uint8_t pattern = pattern_table[tile_index];
            const uint8_t color = color_table[tile_index];

            const uint8_t fg_color = color >> 4;
            const uint8_t bg_color = color & 0xf;

            for (uint8_t x = 0, bit = 7; x < 8; ++x, bit--) {
                const uint8_t pixel_color = pattern >> bit & 1 ? fg_color : bg_color;
                *screen_pixel++ = pixel_color ? pixel_color : overscan_color;
            }
        }
        // Sprites rendering loop
        for (uint8_t sprite_index = 0; sprite_index < 128; sprite_index += 4) {
            int sprite_y = vdp.sprites[sprite_index] + 1;

            if (sprite_y == 208 + 1) break; // dont render anymore

            if (sprite_y > 192) {
                sprite_y -= 256;
            }

            if (scanline >= sprite_y && scanline < sprite_y + sprite_size) {
                uint8_t sprite_color = vdp.sprites[sprite_index + 3];
                if (sprite_color == 0) continue;

                const uint8_t sprite_x = vdp.sprites[sprite_index + 1] - (sprite_color & BIT_7 ? 32 : 0);
                sprite_color &= 0xf;

                const uint8_t sprite_pattern = vdp.sprites[sprite_index + 2];
                const uint8_t line_offset = scanline - sprite_y;
                screen_pixel = &SCREEN[scanline * SMS_WIDTH + sprite_x];

                if (sprite_size > 8) {
                    const uint16_t sprite_address = (sprite_pattern & 252) * 8 + line_offset;

                    const uint8_t pattern_lift = sprites[sprite_address];
                    const uint8_t pattern_right = sprites[sprite_address + 16];

                    for (uint8_t x = 0, bit = 7; x < 8; ++x, bit--) {
                        if (pattern_lift >> bit & 1) {
                            screen_pixel[x] = sprite_color; // Set pixel color
                        }
                        if (pattern_right >> bit & 1) {
                            screen_pixel[8 + x] = sprite_color; // Set pixel color
                        }
                    }
                } else {
                    const uint8_t pattern = sprites[sprite_pattern * 8 + line_offset];

                    for (int col = 0, bit = 7; col < 8; ++col, bit--) {
                        if (pattern >> bit & 1) {
                            screen_pixel[col] = sprite_color & 0xf; // Set pixel color
                        }
                    }
                }
            }
        }
        cpu_cycles = ExecZ80(&cpu, CYCLES_PER_LINE - cpu_cycles);
    }
    vdp.status |= VDP_VSYNC_PENDING;

    // cpu_cycles = ExecZ80(&cpu, CYCLES_PER_LINE - cpu_cycles);
    // scanline++;

    // vblank period
    while (scanline++ < 262) {
        if (vdp.status & VDP_VSYNC_PENDING && vdp.registers[R1_MODE_CONTROL_2] & ENABLE_FRAME_INTERRUPT) {
            IntZ80(&cpu, INT_NMI);
        }
        cpu_cycles = ExecZ80(&cpu, CYCLES_PER_LINE - cpu_cycles);
    }
}


int main(const int argc, char **argv) {
    const int scale = argc > 2 ? atoi(argv[1]) : 4;

    if (!argv[1]) {
        printf("Usage: master-gear.exe <rom.bin> [scale_factor]\n");
        return EXIT_FAILURE;
    }


    const char *filename = argv[1];
    const size_t len = strlen(filename);
    if (readfile(filename, ROM) == 1048576) {
        page_mask = 0x7f;
    }

    if (len >= 2) {
        if (strcmp(&filename[len - 2], "gg") == 0) is_gamegear = 1;
        if (strcmp(&filename[len - 2], "sg") == 0) is_sg1000 = 1;
        if (strcmp(&filename[len - 3], "col") == 0) is_cv = 1;
    }

    char window_title[512] = "";
    strcat(window_title, is_sg1000 ? "SG-1000 - " : is_gamegear ? "Sega Gamegear - " : "Sega Master System - ");
    strcat(window_title, filename);
    if (!mfb_open(window_title, SMS_WIDTH, SMS_HEIGHT, scale))
        return EXIT_FAILURE;

    key_status = (uint8_t *) mfb_keystatus();

    ym2413 = OPLL_new(3579545, SOUND_FREQUENCY);
    OPLL_reset(ym2413);
    sn76489_reset();

    CreateThread(NULL, 0, SoundThread, NULL, 0, NULL);
    CreateThread(NULL, 0, TicksThread, NULL, 0, NULL);


    memset(RAM, 0, sizeof(RAM));
    memset(VRAM, 0, sizeof(VRAM));
    ResetZ80(&cpu);

    memset(SCREEN, 255, SMS_WIDTH * SMS_HEIGHT);

    mfb_set_pallete_array(sg1000_palette, 0, 16);

    if (is_sg1000) {
        // rom_slot1 = &RAM_BANK[0][0];
    }

    for (int y = 0; y < SMS_HEIGHT; y++) {
        for (int x = 0; x < SMS_WIDTH; x++) {
            SCREEN[x + y * SMS_WIDTH] = (x / 16) + ((y / 16) & 1) * 16;
        }
    }

    do {
        if (is_sg1000 || is_cv) {
            sg1000_frame();
        } else {
            sms_frame();
        }
    } while (mfb_update(SCREEN, 60) != -1);

    return EXIT_FAILURE;
}
