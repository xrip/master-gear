#pragma once
#ifdef __cplusplus
extern "C" {
#endif

#include "inttypes.h"

#define PIO_I2S pio1
#define I2S_DATA_PIN 26
#define I2S_CLK_BASE_PIN 27


void fm_init();

void i2s_deinit();

void fm_out(int16_t l_out, int16_t r_out);

#pragma once
#ifdef __cplusplus
}
#endif