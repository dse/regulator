/**
 * regulator_pulseaudio.c
 *
 * Copyright (C) 2019 Darren Embry.  GPL2.
 */

#define REGULATOR_PULSEAUDIO_C

#include <stdlib.h>
#include <stdio.h>

#include <pulse/simple.h>
#include <pulse/error.h>

#include "regulator.h"
#include "regulator_pulseaudio.h"

static pa_simple *pa_s = NULL;
static int pa_error = 0;

/* sample specification */
static pa_sample_spec pa_ss = { .format   = PA_SAMPLE_S16LE,
                                .rate     = 44100,
                                .channels = 1 };

/* buffer attributes */
static pa_buffer_attr pa_ba = { .maxlength = 44100,
                                .minreq    = 0,
                                .prebuf    = 0,
                                .tlength   = 0,
                                .fragsize  = 44100 };

void regulator_pulseaudio_open() {
    /* sanity check */
    if (!pa_sample_spec_valid(&pa_ss)) {
        fprintf(stderr, "%s: invalid sample specification\n", progname);
        exit(1);
    }
    if (!pa_sample_rate_valid(pa_ss.rate)) {
        fprintf(stderr, "%s: invalid sample rate\n", progname);
        exit(1);
    }

    if ((3600 * pa_ss.rate) % ticks_per_hour) {
        fprintf(stderr, "%s: can't process --ticks-per-hour=%d, sample rate is %d/sec\n",
                progname, (int)ticks_per_hour, pa_ss.rate);
        exit(1);
    }
    samples_per_tick = 3600 * pa_ss.rate / ticks_per_hour;

    sample_buffer_frames  = samples_per_tick;
    sample_buffer_samples = sample_buffer_frames * pa_ss.channels;
    sample_buffer_bytes   = pa_bytes_per_second(&pa_ss) * 3600 / ticks_per_hour;
    bytes_per_frame      = pa_ss.channels * sizeof(int16_t);

    pa_ba.maxlength = sample_buffer_bytes;
    pa_ba.fragsize  = sample_buffer_bytes;

    pa_s = pa_simple_new(NULL,             /* server name */
                         progname,         /* name */
                         PA_STREAM_RECORD, /* direction */
                         NULL,             /* device name or default */
                         "record",         /* stream name */
                         &pa_ss,           /* sample type */
                         NULL,             /* channel map */
                         &pa_ba,           /* buffering attributes */
                         &pa_error);       /* error code */
    if (!pa_s) {
        fprintf(stderr, "%s: pa_simple_new() failed: %s\n", progname, pa_strerror(pa_error));
        exit(1);
    }

    if (!(sample_sort_buffer =
          (regulator_sample_t *)
          malloc(sample_buffer_frames * sizeof(regulator_sample_t)))) {
        perror(progname);
        exit(1);
    }
}

size_t regulator_pulseaudio_read(int16_t *buffer, size_t samples) {
    size_t i;
    int16_t *samplep;

    if (pa_simple_read(pa_s, buffer, samples * bytes_per_frame, &pa_error) < 0) {
        fprintf(stderr, "%s: pa_simple_read failed: %s\n", progname, pa_strerror(pa_error));
        exit(1);
    }

    /* if on a big-endian system, convert from little endian by swapping bytes, lol */
    if (!IS_LITTLE_ENDIAN) {
        char *a;
        char *b;
        for (i = 0; i < samples * pa_ss.channels; i += 1) {
            a = (char *)(buffer + i);
            b = a + 1;

            /* swap */
            *a = *a ^ *b;
            *b = *a ^ *b;
            *a = *a ^ *b;
        }
    }

    for (i = 0; i < samples * pa_ss.channels; i += 1) {
        samplep = buffer + i;
        if (*samplep == INT16_MIN) { /* -32768 => 32767 */
            *samplep = INT16_MAX;
        } else if (*samplep < 0) {
            *samplep = -*samplep;
        }
    }

    return samples;
}
