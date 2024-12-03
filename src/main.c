#pragma GCC push_options
#pragma GCC optimize ("unroll-loops")

#include <stdio.h>
#include <windows.h>
#include "win32/MiniFB.h"
#include "win32/audio.h"
#include "z80/z80.h"
#include "sn76489.h"

#include "sms.h"
#include "vdp.h"
// create a CPU core object
Z80 cpu;

uint8_t SCREEN[SMS_WIDTH * SMS_HEIGHT + 8] = {0}; // +8 possible sprite overflow


uint8_t RAM[8192] = {0};
uint8_t ROM[1024 << 10] = {0};
uint8_t RAM_BANK[2][16384] = {0};

uint8_t *rom_slot1 = ROM;
uint8_t *rom_slot2 = ROM;
uint8_t *ram_rom_slot3 = ROM;

uint8_t slot3_is_ram = 0;
static uint8_t *key_status;

uint8_t is_gamegear = 0;
uint8_t page_mask = 0x1f;

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
            const uint8_t page = value & page_mask; // todo check rom size
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
                        ram_rom_slot3 = &RAM_BANK[slot3_is_ram-2][0];
                        printf("slot 3 is RAM bank %i\n", slot3_is_ram-2);
                        // ram_rom_slot3 -= 0x8000;
                    } else {
                         // printf("slot 3 is ROM page %i\n", page);
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

void OutZ80(register word port, register byte value) {
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
    }
}

byte InZ80(register word port) {
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
        uint8_t *priority_table_ptr = priority_table + 8;

        const int hscroll = hscroll_lock && scanline < 16 ? 0 : vdp.registers[R8_BACKGROUND_X_SCROLL];
        const int nametable_scroll = 32 - (hscroll >> 3);

        uint8_t *screen_pixel = &SCREEN[scanline * SMS_WIDTH + (hscroll & 7)];

        const uint16_t scanline_offset = (vscroll + scanline) % 224;
        const uint8_t screen_row = scanline_offset / 8;
        const uint8_t tile_row = scanline_offset & 7;

        const uint16_t *tile_ptr = (uint16_t *)&vdp.nametable[screen_row * 64];

        // background rendering loop
        for (uint8_t column = 0; column < 32; ++column) {
            const uint16_t tile_info = tile_ptr[(nametable_scroll + column) % 32];
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
                // horizontal flip
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

            // if background priority
            screen_pixel -= 8;
            for (uint8_t x = 0; x < 8; x++) {
                *priority_table_ptr++ = priority && *screen_pixel != palette_offset;
                screen_pixel++;
            }
        }
        // Sprites rendering loop
        for (int sprite_index = 0; sprite_index < SPRITE_COUNT; ++sprite_index) {
            const uint8_t sprite_y = vdp.sprites[sprite_index] + 1;

            if (scanline >= sprite_y && scanline < sprite_y + sprite_height) {
                const uint8_t sprite_x = vdp.sprites[128 + sprite_index * 2];

                const uint16_t tile_index = sprites_offset + vdp.sprites[128 + sprite_index * 2 + 1];

                // Extract Tile pattern
                const uint8_t *pattern_planes = &VRAM[tile_index * 32 + (scanline - sprite_y) * 4];
                const uint8_t plane0 = pattern_planes[0];
                const uint8_t plane1 = pattern_planes[1];
                const uint8_t plane2 = pattern_planes[2];
                const uint8_t plane3 = pattern_planes[3];

                uint8_t *sprite_screen_pixels = SCREEN + scanline * SMS_WIDTH + (sprite_x + sprites_hshift);
                priority_table_ptr = priority_table + sprite_x + sprites_hshift;

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
        cpu_cycles = ExecZ80(&cpu, CYCLES_PER_LINE + cpu_cycles);

    }
    vdp.status |= VDP_VSYNC_PENDING;

    // vblank period
    while (scanline++ < 262) {
        if (vdp.status & VDP_VSYNC_PENDING && vdp.registers[R1_MODE_CONTROL_2] & ENABLE_FRAME_INTERRUPT) {
            IntZ80(&cpu, INT_IRQ);
        }
        cpu_cycles = ExecZ80(&cpu, CYCLES_PER_LINE + cpu_cycles);
    }
}


int main(const int argc, char **argv) {
    const int scale = argc > 2 ? atoi(argv[1]) : 1;

    if (!argv[1]) {
        printf("Usage: master-gear.exe <rom.bin> [scale_factor]\n");
        return EXIT_FAILURE;
    }


    const char *filename = argv[1];
    const size_t len = strlen(filename);
    if (readfile(filename, ROM) == 1048576) {
        page_mask = 0x7f;
    }
    if (len >= 2 && strcmp(&filename[len - 2], "gg") == 0) {
        is_gamegear = 1;
    }

    if (!mfb_open("Sega Master System", SMS_WIDTH, SMS_HEIGHT, scale))
        return EXIT_FAILURE;

    key_status = (uint8_t *) mfb_keystatus();

    CreateThread(NULL, 0, SoundThread, NULL, 0, NULL);
    CreateThread(NULL, 0, TicksThread, NULL, 0, NULL);
    sn76489_reset();

    memset(RAM, 0, sizeof(RAM));
    memset(VRAM, 0, sizeof(VRAM));
    ResetZ80(&cpu);


    memset(SCREEN, 255, SMS_WIDTH * SMS_HEIGHT);


    for (int y = 192; y < SMS_HEIGHT; y++) {
        for (int x = 0; x < SMS_WIDTH; x++) {
            SCREEN[x + y * SMS_WIDTH] = (x / 16) + ((y / 16) & 1) * 16;
        }
    }

    do {
        sms_frame(); // 192 scanlines
    } while (mfb_update(SCREEN, 60) != -1);

    return EXIT_FAILURE;
}
