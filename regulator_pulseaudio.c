/**
 * regulator_pulseaudio.c
 *
 * Copyright (C) 2019 Darren Embry.  GPL2.
 */

#define REGULATOR_PULSEAUDIO_C

#include <stdlib.h>
#include <stdio.h>
#include <math.h>

#include <pulse/simple.h>
#include <pulse/error.h>

#include "regulator.h"
#include "regulator_pulseaudio.h"

void regulator_pulseaudio_open(struct regulator_t* rp) {
    rp->type = REGULATOR_TYPE_PULSEAUDIO;
    regulator_pulseaudio_t pulseaudio = {
        .pa_ss = {
            .format   = PA_SAMPLE_S16LE,
            .rate     = 44100,
            .channels = 1
        },
        .pa_ba = {
            .maxlength = 44100,
            .minreq    = 0,
            .prebuf    = 0,
            .tlength   = 0,
            .fragsize  = 44100
        }
    };
    rp->implementation.pulseaudio = pulseaudio;
    regulator_pulseaudio_t *ip = &(rp->implementation.pulseaudio);

    /* sanity check */
    if (!pa_sample_spec_valid(&(ip->pa_ss))) {
        fprintf(stderr, "%s: invalid sample specification\n", rp->progname);
        exit(1);
    }
    if (!pa_sample_rate_valid(ip->pa_ss.rate)) {
        fprintf(stderr, "%s: invalid sample rate\n", rp->progname);
        exit(1);
    }

    if (!rp->ticks_per_hour) {
        rp->ticks_per_hour = 3600; /* default */
    }

    if ((3600 * ip->pa_ss.rate) % rp->ticks_per_hour) {
        fprintf(stderr,
                "%s: can't process --ticks-per-hour=%d, "
                "sample rate is %d/sec\n",
                rp->progname, (int)rp->ticks_per_hour, ip->pa_ss.rate);
        exit(1);
    }
    rp->samples_per_tick = 3600 * ip->pa_ss.rate / rp->ticks_per_hour;

    rp->sample_buffer_frames  = rp->samples_per_tick;
    rp->sample_buffer_samples = rp->sample_buffer_frames * ip->pa_ss.channels;
    rp->sample_buffer_bytes   =
        pa_bytes_per_second(&(ip->pa_ss)) * 3600 / rp->ticks_per_hour;
    rp->bytes_per_frame       = ip->pa_ss.channels * sizeof(int16_t);
    rp->frames_per_second     = ip->pa_ss.rate;

    ip->pa_ba.maxlength       = rp->sample_buffer_bytes;
    ip->pa_ba.fragsize        = rp->sample_buffer_bytes;

    ip->pa_s = pa_simple_new(NULL,             /* server name */
                             rp->progname,     /* name */
                             PA_STREAM_RECORD, /* direction */
                             NULL,             /* device name or default */
                             "record",         /* stream name */
                             &(ip->pa_ss),     /* sample type */
                             NULL,             /* channel map */
                             &(ip->pa_ba),     /* buffering attributes */
                             &(ip->pa_error)); /* error code */
    if (!ip->pa_s) {
        fprintf(stderr, "%s: pa_simple_new() failed: %s\n",
                rp->progname, pa_strerror(ip->pa_error));
        exit(1);
    }

    if (!rp->no_sample_sort_buffer) {
        if (!(rp->sample_sort_buffer =
              (regulator_sample_t*)
              malloc(rp->sample_buffer_frames * sizeof(regulator_sample_t)))) {
            perror(rp->progname);
            exit(1);
        }
    }
}

void regulator_pulseaudio_close(struct regulator_t* rp) {
    regulator_pulseaudio_t *ip = &(rp->implementation.pulseaudio);
    if (rp->sample_sort_buffer) {
        free(rp->sample_sort_buffer);
        rp->sample_sort_buffer = NULL;
    }
    if (ip->pa_s) {
        pa_simple_free(ip->pa_s);
        ip->pa_s = NULL;
    }
    regulator_pulseaudio_t pulseaudio = {};
    rp->implementation.pulseaudio = pulseaudio;
}

size_t regulator_pulseaudio_read(struct regulator_t* rp,
                                 int16_t* buffer, size_t samples) {
    regulator_pulseaudio_t *ip = &(rp->implementation.pulseaudio);
    size_t i;
    int16_t* samplep;

    if (pa_simple_read(ip->pa_s, buffer, samples * rp->bytes_per_frame,
                       &(ip->pa_error)) < 0) {
        fprintf(stderr, "%s: pa_simple_read failed: %s\n",
                rp->progname, pa_strerror(ip->pa_error));
        exit(1);
    }

    /* if on a big-endian system, convert from little endian by
       swapping bytes, lol */
    if (!IS_LITTLE_ENDIAN) {
        char* a;
        char* b;
        for (i = 0; i < samples * ip->pa_ss.channels; i += 1) {
            a = (char*)(buffer + i);
            b = a + 1;

            /* swap */
            *a = *a ^ *b;
            *b = *a ^ *b;
            *a = *a ^ *b;
        }
    }

    /* we want the amplitude, not the sign */
    for (i = 0; i < samples * ip->pa_ss.channels; i += 1) {
        samplep = buffer + i;
        if (*samplep == INT16_MIN) { /* -32768 => 32767 */
            *samplep = INT16_MAX;
        } else if (*samplep < 0) {
            *samplep = -*samplep;
        }
    }

    return samples;
}

void regulator_pulseaudio_test(struct regulator_t* rp) {
    float seconds = 0.05;

    int save__no_sample_sort_buffer = rp->no_sample_sort_buffer;
    rp->no_sample_sort_buffer = 1;
    rp->ticks_per_hour = (size_t)roundf(3600.0 / seconds);
    if (rp->debug >= 1) {
        printf("regulator_pulseaudio_test: %d blocks per hour\n", (int)rp->ticks_per_hour);
    }

    regulator_pulseaudio_open(rp);

    size_t frames = rp->sample_buffer_frames;
    if (rp->debug >= 1) {
        printf("regulator_pulseaudio_test: %d frames\n", (int)frames);
    }

    int16_t* buffer = (int16_t*)malloc(sizeof(int16_t) * frames);
    if (!buffer) {
        perror(rp->progname);
        exit(1);
    }
    while (1) {
        if (regulator_pulseaudio_read(rp, buffer, frames) < frames) {
            break;
        }
        regulator_pulseaudio_vu(rp, buffer, frames);
    }
    free(buffer);

    rp->no_sample_sort_buffer = save__no_sample_sort_buffer;
}

#pragma GCC diagnostic ignored "-Wunused-parameter"
void regulator_pulseaudio_vu(struct regulator_t* rp, int16_t* buffer, size_t frames) {
    int16_t max = 0;
    for (size_t i = 0; i < frames; i += 1) {
        if (max < buffer[i]) {
            max = buffer[i];
        }
    }

    /* quantize to 6-bit integer (0 to 63) */
    max = max / (1 << (sizeof(int16_t) * 8 - 7));

    putchar('[');
    int16_t i;
    for (i = 0; i <= max; i += 1) {
        putchar('#');
    }
    for (; i < 64; i += 1) {
        putchar(' ');
    }
    putchar(']');
    putchar('\r');
    fflush(stdout);
}
#pragma GCC diagnostic warning "-Wunused-parameter"
