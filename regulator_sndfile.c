/**
 * regulator_sndfile.c
 *
 * Copyright (C) 2019 Darren Embry.  GPL2.
 */

#define REGULATOR_SNDFILE_C

#include <stdlib.h>
#include <stdio.h>
#include <sndfile.h>

#include "regulator.h"
#include "regulator_sndfile.h"

static SNDFILE* sf;
static SF_INFO sfinfo = { .frames     = 0,
                          .samplerate = 0,
                          .channels   = 0,
                          .format     = 0, /* set to 0 before reading */
                          .sections   = 0,
                          .seekable   = 0 };
static int* sf_sample_buffer;

#define sf_num_frames sample_buffer_frames
#define sf_num_items  sample_buffer_samples

#define CHANNEL_NUMBER 0

void regulator_sndfile_open(struct regulator_t* rp) {
    sf = sf_open(audio_filename, SFM_READ, &sfinfo);
    if (!sf) {
        fprintf(stderr, "%s: unable to open %s: %s\n", progname, audio_filename, sf_strerror(NULL));
        exit(1);
    }

    if ((3600 * sfinfo.samplerate) % ticks_per_hour) {
        fprintf(stderr, "%s: can't process --ticks-per-hour=%d with this file, sample rate is %d/sec\n",
                progname, (int)ticks_per_hour, sfinfo.samplerate);
        exit(1);
    }
    samples_per_tick = 3600 * sfinfo.samplerate / ticks_per_hour;

    sample_buffer_frames  = samples_per_tick;
    sample_buffer_samples = sample_buffer_frames * sfinfo.channels;
    sample_buffer_bytes   = sample_buffer_samples * sizeof(int);
    bytes_per_frame       = sfinfo.channels * sizeof(int);
    frames_per_second     = sfinfo.samplerate;

    if (!(sf_sample_buffer = (int*)malloc(sample_buffer_bytes))) {
        perror(progname);
        exit(1);
    }
    if (!(sample_sort_buffer =
          (regulator_sample_t*)
          malloc(sample_buffer_frames * sizeof(regulator_sample_t)))) {
        perror(progname);
        exit(1);
    }
}

size_t regulator_sndfile_read(struct regulator_t* rp, int16_t* buffer, size_t samples) {
    sf_count_t sf_frames;
    sf_count_t i;
    int sample;                 /* int, as read from sf_readf_int */
    if ((sf_frames = sf_readf_int(sf, sf_sample_buffer, samples)) <= 0) {
        return 0;
    }
    for (i = 0; i < sf_frames; i += 1) {
        /* quantize an int to an int16_t */
        sample = sf_sample_buffer[i * sfinfo.channels + CHANNEL_NUMBER] / (1 << ((sizeof(int) - sizeof(int16_t)) * 8));
        if (sample == INT16_MIN) { /* -32768 => 32767 */
            sample = INT16_MAX;
        } else if (sample < 0) {
            sample = -sample;
        }
        buffer[i] = sample;
    }
    return sf_frames;
}
