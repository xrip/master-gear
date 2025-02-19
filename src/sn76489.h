#pragma once
#define SOUND_FREQUENCY 44100

int16_t sn76489_sample();
void sn76489_out(uint16_t value);
void sn76489_reset();
