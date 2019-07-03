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

void regulator_sndfile_open(struct regulator_t* rp) {
    rp->type = REGULATOR_TYPE_SNDFILE;
    regulator_sndfile_t sndfile = {
        .sfinfo = {
            .format = 0
        }
    };
    rp->implementation.sndfile = sndfile;

    regulator_sndfile_t *ip = &(rp->implementation.sndfile);

    ip->sf = sf_open(rp->filename, SFM_READ, &(ip->sfinfo));
    if (!ip->sf) {
        fprintf(stderr, "%s: unable to open %s: %s\n", rp->progname, rp->filename, sf_strerror(NULL));
        exit(1);
    }

    if ((3600 * ip->sfinfo.samplerate) % rp->ticks_per_hour) {
        fprintf(stderr, "%s: can't process --ticks-per-hour=%d with this file, sample rate is %d/sec\n",
                rp->progname, (int)rp->ticks_per_hour, ip->sfinfo.samplerate);
        exit(1);
    }
    rp->samples_per_tick = 3600 * ip->sfinfo.samplerate / rp->ticks_per_hour;

    rp->sample_buffer_frames  = rp->samples_per_tick;
    rp->sample_buffer_samples = rp->sample_buffer_frames * ip->sfinfo.channels;
    rp->sample_buffer_bytes   = rp->sample_buffer_samples * sizeof(int);
    rp->bytes_per_frame       = ip->sfinfo.channels * sizeof(int);
    rp->frames_per_second     = ip->sfinfo.samplerate;

    if (!(ip->sf_sample_buffer = (int*)malloc(rp->sample_buffer_bytes))) {
        perror(rp->progname);
        exit(1);
    }
    if (!(rp->sample_sort_buffer =
          (regulator_sample_t*)
          malloc(rp->sample_buffer_frames * sizeof(regulator_sample_t)))) {
        perror(rp->progname);
        exit(1);
    }
}

void regulator_sndfile_close(struct regulator_t* rp) {
    regulator_sndfile_t *ip = &(rp->implementation.sndfile);

    if (rp->sample_sort_buffer) {
        free(rp->sample_sort_buffer);
        rp->sample_sort_buffer = NULL;
    }
    if (ip->sf_sample_buffer) {
        free(ip->sf_sample_buffer);
        ip->sf_sample_buffer = NULL;
    }
    if (ip->sf) {
        sf_close(ip->sf);       /* don't care about return value */
    }
    regulator_sndfile_t sndfile = {};
    rp->implementation.sndfile = sndfile;
}

#define CHANNEL_NUMBER 0

size_t regulator_sndfile_read(struct regulator_t* rp, int16_t* buffer, size_t samples) {
    regulator_sndfile_t *ip = &(rp->implementation.sndfile);
    sf_count_t sf_frames;
    sf_count_t i;
    int sample;                 /* int, as read from sf_readf_int */
    if ((sf_frames = sf_readf_int(ip->sf, ip->sf_sample_buffer, samples)) <= 0) {
        return 0;
    }
    for (i = 0; i < sf_frames; i += 1) {
        /* quantize an int to an int16_t */
        sample = ip->sf_sample_buffer[i * ip->sfinfo.channels + CHANNEL_NUMBER] / (1 << ((sizeof(int) - sizeof(int16_t)) * 8));
        if (sample == INT16_MIN) { /* -32768 => 32767 */
            sample = INT16_MAX;
        } else if (sample < 0) {
            sample = -sample;
        }
        buffer[i] = sample;
    }
    return sf_frames;
}
