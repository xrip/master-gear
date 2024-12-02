#include <alsa/asoundlib.h>

int main(int argc, char **argv) {
    const char *device = "default"; /* playback device */
    snd_pcm_t *handle;
    snd_pcm_sframes_t frames;
    int16_t *buffer;
    int rc;

    // Open PCM device for playback.
    rc = snd_pcm_open(&handle, device, SND_PCM_STREAM_PLAYBACK, 0);
    if (rc < 0) {
        fprintf(stderr, "unable to open pcm device: %s\n", snd_strerror(rc));
        exit(1);
    }

    // Set hardware parameters.
    snd_pcm_hw_params_t *params;
    snd_pcm_hw_params_alloca(&params);
    snd_pcm_hw_params_any(handle, params);
    snd_pcm_hw_params_set_access(handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);
    snd_pcm_hw_params_set_format(handle, params, SND_PCM_FORMAT_S16_LE);
    snd_pcm_hw_params_set_channels(handle, params, 2);
    snd_pcm_hw_params_set_rate_near(handle, params, 44100, NULL);

    rc = snd_pcm_hw_params(handle, params);
    if (rc < 0) {
        fprintf(stderr, "unable to set hw parameters: %s\n", snd_strerror(rc));
        exit(1);
    }

    // Prepare your PCM buffer (int16_t) and fill data here
    buffer = (int16_t *) malloc(44100 * 2 * sizeof(int16_t)); // Sample buffer

    while (1) {
        frames = snd_pcm_writei(handle, buffer, 44100);
        if (frames < 0) {
            frames = snd_pcm_recover(handle, frames, 0);
        }
        if (frames < 0) {
            fprintf(stderr, "snd_pcm_writei failed: %s\n", snd_strerror(frames));
            break;
        }
    }

    snd_pcm_drain(handle);
    snd_pcm_close(handle);

    free(buffer);
    return 0;
}