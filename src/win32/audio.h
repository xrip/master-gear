#include <windows.h>
#include <stdint.h>
#include "../sn76489.h"
HANDLE updateEvent;

#define AUDIO_BUFFER_LENGTH ((SOUND_FREQUENCY / 10))
static int16_t audio_buffer[AUDIO_BUFFER_LENGTH * 2] = {};
static int sample_index = 0;

DWORD WINAPI SoundThread(LPVOID lpParam) {
    WAVEHDR waveHeaders[4];

    WAVEFORMATEX format = {0};
    format.wFormatTag = WAVE_FORMAT_PCM;
    format.nChannels = 2;
    format.nSamplesPerSec = SOUND_FREQUENCY;
    format.wBitsPerSample = 16;
    format.nBlockAlign = format.nChannels * format.wBitsPerSample / 8;
    format.nAvgBytesPerSec = format.nSamplesPerSec * format.nBlockAlign;

    HANDLE waveEvent = CreateEvent(NULL, 1, 0, NULL);

    HWAVEOUT hWaveOut;
    waveOutOpen(&hWaveOut, WAVE_MAPPER, &format, (DWORD_PTR) waveEvent, 0, CALLBACK_EVENT);

    for (size_t i = 0; i < 4; i++) {
        int16_t audio_buffers[4][AUDIO_BUFFER_LENGTH * 2];
        waveHeaders[i] = (WAVEHDR){
            .lpData = (char *) audio_buffers[i],
            .dwBufferLength = AUDIO_BUFFER_LENGTH * 2,
        };
        waveOutPrepareHeader(hWaveOut, &waveHeaders[i], sizeof(WAVEHDR));
        waveHeaders[i].dwFlags |= WHDR_DONE;
    }
    WAVEHDR *currentHeader = waveHeaders;


    while (1) {
        if (WaitForSingleObject(waveEvent, INFINITE)) {
            //            fprintf(stderr, "Failed to wait for event.\n");
            return 1;
        }

        if (!ResetEvent(waveEvent)) {
            //            fprintf(stderr, "Failed to reset event.\n");
            return 1;
        }

        // Wait until audio finishes playing
        while (currentHeader->dwFlags & WHDR_DONE) {
            WaitForSingleObject(updateEvent, INFINITE);
            ResetEvent(updateEvent);
            //            PSG_calc_stereo(&psg, audiobuffer, AUDIO_BUFFER_LENGTH);
            memcpy(currentHeader->lpData, audio_buffer, AUDIO_BUFFER_LENGTH * 2);
            waveOutWrite(hWaveOut, currentHeader, sizeof(WAVEHDR));
            //waveOutPrepareHeader(hWaveOut, currentHeader, sizeof(WAVEHDR));
            currentHeader++;
            if (currentHeader == waveHeaders + 4) { currentHeader = waveHeaders; }
        }
    }
    return 0;
}

DWORD WINAPI TicksThread(LPVOID lpParam) {
    LARGE_INTEGER start, current, queryperf;


    QueryPerformanceFrequency(&queryperf);
    uint32_t hostfreq = (uint32_t) queryperf.QuadPart;

    QueryPerformanceCounter(&start); // Get the starting time
    uint32_t last_sound_tick = 0;


    updateEvent = CreateEvent(NULL, 1, 1, NULL);
    while (1) {
        QueryPerformanceCounter(&current); // Get the current time

        // Calculate elapsed time in ticks since the start
        uint32_t elapsedTime = (uint32_t) (current.QuadPart - start.QuadPart);

        if (elapsedTime - last_sound_tick >= hostfreq / SOUND_FREQUENCY) {
            const int16_t sample = sn76489_sample();
            audio_buffer[sample_index++] = sample;
            audio_buffer[sample_index++] = sample;

            if (sample_index >= AUDIO_BUFFER_LENGTH) {
                SetEvent(updateEvent);
                sample_index = 0;
            }

            last_sound_tick = elapsedTime;
        }
    }
}