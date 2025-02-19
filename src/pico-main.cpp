#include <cstring>
#include <hardware/flash.h>
#include <hardware/vreg.h>
#include <pico/multicore.h>
#include <pico/stdlib.h>

extern "C" {
#include "emu2413.h"
#include "z80/z80.h"
#include "sn76489.h"

#include "sms.h"
#include "vdp.h"
}

#include <graphics.h>
#include "audio.h"

#include "nespad.h"
#include "ff.h"
#include "ps2kbd_mrmltr.h"

#define HOME_DIR "\\SMS"
extern char __flash_binary_end;
#define FLASH_TARGET_OFFSET (((((uintptr_t)&__flash_binary_end - XIP_BASE) / FLASH_SECTOR_SIZE) + 4) * FLASH_SECTOR_SIZE)
static const uintptr_t rom = XIP_BASE + FLASH_TARGET_OFFSET;


uint8_t is_gamegear = 0, is_sg1000 = 0;

static FATFS fs;
bool reboot = false;

bool __uninitialized_ram(is_gg) = false;
char __uninitialized_ram(filename[256]);

static size_t __uninitialized_ram(rom_size) = 0;


typedef struct __attribute__((__packed__)) {
    bool is_directory;
    bool is_executable;
    size_t size;
    char filename[79];
} file_item_t;

uint8_t SCREEN[SMS_WIDTH * SMS_HEIGHT + 8] __attribute__ ((aligned(4))) = {0}; // +8 possible sprite overflow

constexpr int max_files = 500;
file_item_t *fileItems = reinterpret_cast<file_item_t *>(SCREEN + TEXTMODE_COLS * TEXTMODE_ROWS * 2);


struct input_bits_t {
    bool a: true;
    bool b: true;
    bool select: true;
    bool start: true;
    bool right: true;
    bool left: true;
    bool up: true;
    bool down: true;
};

static struct input_bits_t keyboard_bits = { false, false, false, false, false, false, false, false };
static struct input_bits_t gamepad1_bits = { false, false, false, false, false, false, false, false };
static struct input_bits_t gamepad2_bits = { false, false, false, false, false, false, false, false };

bool swap_ab = false;

void nespad_tick() {
    nespad_read();

    if (swap_ab) {
        gamepad1_bits.a = (nespad_state & DPAD_A) != 0;
        gamepad1_bits.b = (nespad_state & DPAD_B) != 0;
    } else {
        gamepad1_bits.b = (nespad_state & DPAD_A) != 0;
        gamepad1_bits.a = (nespad_state & DPAD_B) != 0;
    }
    gamepad1_bits.select = (nespad_state & DPAD_SELECT) != 0;
    gamepad1_bits.start = (nespad_state & DPAD_START) != 0;
    gamepad1_bits.up = (nespad_state & DPAD_UP) != 0;
    gamepad1_bits.down = (nespad_state & DPAD_DOWN) != 0;
    gamepad1_bits.left = (nespad_state & DPAD_LEFT) != 0;
    gamepad1_bits.right = (nespad_state & DPAD_RIGHT) != 0;

}

static bool isInReport(hid_keyboard_report_t const *report, const unsigned char keycode) {
    for (unsigned char i: report->keycode) {
        if (i == keycode) {
            return true;
        }
    }
    return false;
}

void
__not_in_flash_func(process_kbd_report)(hid_keyboard_report_t const *report, hid_keyboard_report_t const *prev_report) {
    /* printf("HID key report modifiers %2.2X report ", report->modifier);
    for (unsigned char i: report->keycode)
        printf("%2.2X", i);
    printf("\r\n");
     */
    keyboard_bits.start = isInReport(report, HID_KEY_ENTER);
    keyboard_bits.select = isInReport(report, HID_KEY_BACKSPACE);
    keyboard_bits.a = isInReport(report, HID_KEY_Z);
    keyboard_bits.b = isInReport(report, HID_KEY_X);
    keyboard_bits.up = isInReport(report, HID_KEY_ARROW_UP);
    keyboard_bits.down = isInReport(report, HID_KEY_ARROW_DOWN);
    keyboard_bits.left = isInReport(report, HID_KEY_ARROW_LEFT);
    keyboard_bits.right = isInReport(report, HID_KEY_ARROW_RIGHT);
    //-------------------------------------------------------------------------
}

Ps2Kbd_Mrmltr ps2kbd(
        pio1,
        0,
        process_kbd_report);


int compareFileItems(const void *a, const void *b) {
    const auto *itemA = (file_item_t *) a;
    const auto *itemB = (file_item_t *) b;
    // Directories come first
    if (itemA->is_directory && !itemB->is_directory)
        return -1;
    if (!itemA->is_directory && itemB->is_directory)
        return 1;
    // Sort files alphabetically
    return strcmp(itemA->filename, itemB->filename);
}

bool isExecutable(const char pathname[255], const char *extensions) {
    char *pathCopy = strdup(pathname);
    const char *token = strrchr(pathCopy, '.');

    if (token == nullptr) {
        return false;
    }

    token++;

    while (token != NULL) {
        if (strstr(extensions, token) != NULL) {
            free(pathCopy);
            return true;
        }
        token = strtok(NULL, ",");
    }
    free(pathCopy);
    return false;
}

bool filebrowser_loadfile(const char pathname[256]) {
    UINT bytes_read = 0;
    FIL file;

    constexpr int window_y = (TEXTMODE_ROWS - 5) / 2;
    constexpr int window_x = (TEXTMODE_COLS - 43) / 2;

    draw_window("Loading ROM", window_x, window_y, 43, 5);

    FILINFO fileinfo;
    f_stat(pathname, &fileinfo);
    rom_size = fileinfo.fsize;
    if (16384 - 64 << 10 < fileinfo.fsize) {
        draw_text("ERROR: ROM too large! Canceled!!", window_x + 1, window_y + 2, 13, 1);
        sleep_ms(5000);
        return false;
    }


    draw_text("Loading...", window_x + 1, window_y + 2, 10, 1);
    sleep_ms(500);


    multicore_lockout_start_blocking();
    auto flash_target_offset = FLASH_TARGET_OFFSET;
    const uint32_t ints = save_and_disable_interrupts();
    flash_range_erase(flash_target_offset, fileinfo.fsize);
    restore_interrupts(ints);

    if (FR_OK == f_open(&file, pathname, FA_READ)) {
        uint8_t buffer[FLASH_PAGE_SIZE];

        do {
            f_read(&file, &buffer, FLASH_PAGE_SIZE, &bytes_read);

            if (bytes_read) {
                const uint32_t ints = save_and_disable_interrupts();
                flash_range_program(flash_target_offset, buffer, FLASH_PAGE_SIZE);
                restore_interrupts(ints);

                gpio_put(PICO_DEFAULT_LED_PIN, flash_target_offset >> 13 & 1);

                flash_target_offset += FLASH_PAGE_SIZE;
            }
        } while (bytes_read != 0);

        gpio_put(PICO_DEFAULT_LED_PIN, true);
    }
    f_close(&file);
    multicore_lockout_end_blocking();
    // restore_interrupts(ints);

    strcpy(filename, fileinfo.fname);

    const size_t len = strlen(filename);
    if (len >= 2) {
        if (strcmp(&filename[len - 2], "gg") == 0) is_gamegear = 1;
        if (strcmp(&filename[len - 2], "sg") == 0) is_sg1000 = 1;
    }
    // is_gg = strstr(pathname, ".gg") != nullptr;
    return true;
}

void filebrowser(const char pathname[256], const char executables[11]) {
    bool debounce = true;
    char basepath[256];
    char tmp[TEXTMODE_COLS + 1];
    strcpy(basepath, pathname);
    constexpr int per_page = TEXTMODE_ROWS - 3;

    DIR dir;
    FILINFO fileInfo;

    if (FR_OK != f_mount(&fs, "SD", 1)) {
        draw_text("SD Card not inserted or SD Card error!", 0, 0, 12, 0);
        while (true);
    }
    while (true) {
        memset(fileItems, 0, sizeof(file_item_t) * max_files);
        int total_files = 0;

        snprintf(tmp, TEXTMODE_COLS, "SD:\\%s", basepath);
        draw_window(tmp, 0, 0, TEXTMODE_COLS, TEXTMODE_ROWS - 1);
        memset(tmp, ' ', TEXTMODE_COLS);


        draw_text(tmp, 0, 29, 0, 0);
        auto off = 0;
        draw_text("START", off, 29, 7, 0);
        off += 5;
        draw_text(" Run at cursor ", off, 29, 0, 3);
        off += 16;
        draw_text("SELECT", off, 29, 7, 0);
        off += 6;
        draw_text(" Run previous  ", off, 29, 0, 3);
#ifndef TFT
        off += 16;
        draw_text("ARROWS", off, 29, 7, 0);
        off += 6;
        draw_text(" Navigation    ", off, 29, 0, 3);
        off += 16;
        draw_text("A/F10", off, 29, 7, 0);
        off += 5;
        draw_text(" USB DRV ", off, 29, 0, 3);
#endif

        if (FR_OK != f_opendir(&dir, basepath)) {
            draw_text("Failed to open directory", 1, 1, 4, 0);
            while (true);
        }

        if (strlen(basepath) > 0) {
            strcpy(fileItems[total_files].filename, "..\0");
            fileItems[total_files].is_directory = true;
            fileItems[total_files].size = 0;
            total_files++;
        }

        while (f_readdir(&dir, &fileInfo) == FR_OK &&
               fileInfo.fname[0] != '\0' &&
               total_files < max_files
                ) {
            // Set the file item properties
            fileItems[total_files].is_directory = fileInfo.fattrib & AM_DIR;
            fileItems[total_files].size = fileInfo.fsize;
            fileItems[total_files].is_executable = isExecutable(fileInfo.fname, executables);
            strncpy(fileItems[total_files].filename, fileInfo.fname, 78);
            total_files++;
        }
        f_closedir(&dir);

        qsort(fileItems, total_files, sizeof(file_item_t), compareFileItems);

        if (total_files > max_files) {
            draw_text(" Too many files!! ", TEXTMODE_COLS - 17, 0, 12, 3);
        }

        int offset = 0;
        int current_item = 0;

        while (true) {
            sleep_ms(100);

            if (!debounce) {
                debounce = !(nespad_state & DPAD_START || keyboard_bits.start);
            }

            // ESCAPE
            if (nespad_state & DPAD_SELECT || keyboard_bits.select) {
                return;
            }

            if (nespad_state & DPAD_DOWN || keyboard_bits.down) {
                if (offset + (current_item + 1) < total_files) {
                    if (current_item + 1 < per_page) {
                        current_item++;
                    } else {
                        offset++;
                    }
                }
            }

            if (nespad_state & DPAD_UP || keyboard_bits.up) {
                if (current_item > 0) {
                    current_item--;
                } else if (offset > 0) {
                    offset--;
                }
            }

            if (nespad_state & DPAD_RIGHT || keyboard_bits.right) {
                offset += per_page;
                if (offset + (current_item + 1) > total_files) {
                    offset = total_files - (current_item + 1);
                }
            }

            if (nespad_state & DPAD_LEFT || keyboard_bits.left) {
                if (offset > per_page) {
                    offset -= per_page;
                } else {
                    offset = 0;
                    current_item = 0;
                }
            }

            if (debounce && (nespad_state & DPAD_START || keyboard_bits.start)) {
                auto file_at_cursor = fileItems[offset + current_item];

                if (file_at_cursor.is_directory) {
                    if (strcmp(file_at_cursor.filename, "..") == 0) {
                        const char *lastBackslash = strrchr(basepath, '\\');
                        if (lastBackslash != nullptr) {
                            const size_t length = lastBackslash - basepath;
                            basepath[length] = '\0';
                        }
                    } else {
                        sprintf(basepath, "%s\\%s", basepath, file_at_cursor.filename);
                    }
                    debounce = false;
                    break;
                }

                if (file_at_cursor.is_executable) {
                    sprintf(tmp, "%s\\%s", basepath, file_at_cursor.filename);

                    filebrowser_loadfile(tmp);
                    return;
                }
            }

            for (int i = 0; i < per_page; i++) {
                uint8_t color = 11;
                uint8_t bg_color = 1;

                if (offset + i < max_files) {
                    const auto item = fileItems[offset + i];


                    if (i == current_item) {
                        color = 0;
                        bg_color = 3;
                        memset(tmp, 0xCD, TEXTMODE_COLS - 2);
                        tmp[TEXTMODE_COLS - 2] = '\0';
                        draw_text(tmp, 1, per_page + 1, 11, 1);
                        snprintf(tmp, TEXTMODE_COLS - 2, " Size: %iKb, File %lu of %i ", item.size / 1024,
                                 offset + i + 1,
                                 total_files);
                        draw_text(tmp, 2, per_page + 1, 14, 3);
                    }

                    const auto len = strlen(item.filename);
                    color = item.is_directory ? 15 : color;
                    color = item.is_executable ? 10 : color;
                    //color = strstr((char *)rom_filename, item.filename) != nullptr ? 13 : color;

                    memset(tmp, ' ', TEXTMODE_COLS - 2);
                    tmp[TEXTMODE_COLS - 2] = '\0';
                    memcpy(&tmp, item.filename, len < TEXTMODE_COLS - 2 ? len : TEXTMODE_COLS - 2);
                } else {
                    memset(tmp, ' ', TEXTMODE_COLS - 2);
                }
                draw_text(tmp, 1, i + 1, color, bg_color);
            }
        }
    }
}


// SMS stuff goes here
// create a CPU core object
Z80 cpu;
// OPLL *ym2413;
// uint8_t ym2413_status;

uint8_t RAM[8192] __attribute__ ((aligned(4))) = {0};

uint8_t * ROM = (uint8_t *) rom;
// uint8_t RAM_BANK[2][16384] __attribute__ ((aligned(4))) = {0};

uint8_t *rom_slot1 = (uint8_t *) rom;
uint8_t *rom_slot2 = (uint8_t *) rom;
uint8_t *ram_rom_slot3 = (uint8_t *) rom;

uint8_t slot3_is_ram = 0;


uint8_t page_mask = 0x1f;

void (*frame_function)();

void WrZ80(register word address, const register byte value) {
    if (address >= 0x2000 && address < 0x4000) {
        rom_slot1[address] = value;
    }

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
                        // ram_rom_slot3 = &RAM_BANK[slot3_is_ram - 2][0];
                        // printf("slot 3 is RAM bank %i\n", slot3_is_ram - 2);
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

    if (is_sg1000 && address < 0x2000) {
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
                // printf("IO enabled\n");
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
            // OPLL_writeIO(ym2413, port, value);
            break;
        // case 0xF2: ym2413_status = value & 3;
            // break;
    }
}

byte InZ80(register word port) {
    // printf("Z80 in %02x\n", port & 0xff);

    switch (port & 0xff) {
        // gg input
        case 0x00: {
            uint8_t buttons = 0xff;
            // if (key_status[0x0d]) buttons ^= 0x80;
            // if (key_status[0x20]) buttons ^= 0x40;

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

            if (gamepad1_bits.up) buttons ^= 0b1;
            if (gamepad1_bits.down) buttons ^= 0b10;
            if (gamepad1_bits.left) buttons ^= 0b100;
            if (gamepad1_bits.right) buttons ^= 0b1000;

            if (gamepad1_bits.a) buttons ^= 0b10000;
            if (gamepad1_bits.b) buttons ^= 0b100000;

            if (gamepad1_bits.start) buttons ^= 0b1000000;
            if (gamepad1_bits.select) buttons ^= 0b10000000;

            return buttons;
        }
        // case 0xF2: return ym2413_status;
    }
    return 0xff;
}

void PatchZ80(register Z80 *R) {
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

#if 1
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
#endif
        gpio_put(PICO_DEFAULT_LED_PIN, true);
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


uint16_t frequencies[] = { 378, 396, 404, 408, 412, 416, 420, 424, 432 };
uint8_t frequency_index = 0;

static inline bool overclock() {
#if PICO_RP2350
    volatile uint32_t *qmi_m0_timing=(uint32_t *)0x400d000c;
    vreg_disable_voltage_limit();
    vreg_set_voltage(VREG_VOLTAGE_1_40);
    sleep_ms(10);
    *qmi_m0_timing = 0x60007204;
    set_sys_clock_khz(frequencies[frequency_index] * KHZ, false);
    *qmi_m0_timing = 0x60007303;
    return true;
#else
    hw_set_bits(&vreg_and_chip_reset_hw->vreg, VREG_AND_CHIP_RESET_VREG_VSEL_BITS);
    sleep_ms(33);
    return set_sys_clock_khz(frequencies[frequency_index] * KHZ, true);
#endif
}
#define NTSC_FRAME_TICK (16667)
i2s_config_t i2s_config;
/* Renderer loop on Pico's second core */
void __time_critical_func(render_core)() {
    multicore_lockout_victim_init();



    // 60 FPS loop

    uint64_t tick = time_us_64();
    uint64_t last_frame_tick = tick, last_sound_tick = tick;

    while (true) {

        if (tick >= last_frame_tick + NTSC_FRAME_TICK) {
#ifdef TFT
            refresh_lcd();
#endif
            ps2kbd.tick();
            nespad_tick();

            last_frame_tick = tick;
        }

        if (tick >= last_sound_tick + (1000000 / SOUND_FREQUENCY)) {
            int16_t samples[2];
            samples[0] = samples[1] = sn76489_sample(); // + OPLL_calc(ym2413);
            i2s_dma_write(&i2s_config, samples);
            last_sound_tick = tick;
        }

        tick = time_us_64();

        // tuh_task();
        // hid_app_task();
        tight_loop_contents();
    }

    __unreachable();
}

constexpr bool limit_fps = true;

int main() {
    uint frame_cnt = 0, frame_timer_start = 0;

    overclock();

    // ym2413 = OPLL_new(3579545, SOUND_FREQUENCY);
    ps2kbd.init_gpio();
    nespad_begin(clock_get_hz(clk_sys) / 1000, NES_GPIO_CLK, NES_GPIO_DATA, NES_GPIO_LAT);

    graphics_init();

    i2s_config = i2s_get_default_config();
    i2s_config.sample_freq = SOUND_FREQUENCY;
    i2s_config.dma_trans_count = 1;
    i2s_volume(&i2s_config, 0);
    i2s_init(&i2s_config);

    const auto buffer = static_cast<uint8_t *>(SCREEN);
    graphics_set_buffer(buffer, SMS_WIDTH, SMS_HEIGHT);
    graphics_set_textbuffer(buffer);
    graphics_set_bgcolor(0x000000);
    graphics_set_offset(32, 24);

    for (uint8_t c = 0; c < 16; ++c) {
        graphics_set_palette(c, sg1000_palette[c]);
    }

    graphics_set_flashmode(false, false);
    multicore_launch_core1(render_core);


    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);

    for (int i = 0; i < 6; i++) {
        sleep_ms(33);
        gpio_put(PICO_DEFAULT_LED_PIN, true);
        sleep_ms(33);
        gpio_put(PICO_DEFAULT_LED_PIN, false);
    }


    while (true) {
        graphics_set_mode(TEXTMODE_DEFAULT);
        filebrowser(HOME_DIR, "sms,gg,sg");

        graphics_set_mode(is_gamegear ? GG_160x144 : GRAPHICSMODE_DEFAULT);
        graphics_set_offset(is_gamegear ? 80 : 32, is_gamegear? 24 : 12);

        if (is_sg1000) {
            frame_function = sg1000_frame;
        } else {
            frame_function = sms_frame;
        }



        memset(RAM, 0, sizeof(RAM));
        memset(VRAM, 0, sizeof(VRAM));
        ResetZ80(&cpu);
        sn76489_reset();
        // OPLL_reset(ym2413);

        while (!reboot) {
            frame_function();

            if (limit_fps) {
                if (++frame_cnt == 3) {
                    while (time_us_64() - frame_timer_start < NTSC_FRAME_TICK * 3);  // 60 Hz
                    frame_timer_start = time_us_64();
                    frame_cnt = 0;
                }
            }

            tight_loop_contents();
        }
        reboot = false;
    }
    __unreachable();
}
