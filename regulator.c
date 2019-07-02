/**
 * regulator.c --- clock regulator tool
 *
 * Copyright (C) 2019 Darren Embry.  GPL2.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <getopt.h>

/* PulseAudio */
#include <pulse/simple.h>
#include <pulse/error.h>

/* sndfile */
#include <sndfile.h>

#include "regulator.h"

static char *progname;

int main(int argc, char * const argv[]) {
    char *p;
    progname = argv[0];
    if ((p = strrchr(progname, '/')) && *(p + 1)) {
        progname = p + 1;
    } else if ((p = strrchr(progname, '\\')) && *(p + 1)) {
        progname = p + 1;
    }
    regulator_options(&argc, &argv);
    regulator_run();
}

int debug = 0;
static char *audio_filename = NULL;
static size_t ticks_per_hour = 3600; /* how to guess? */
static size_t samples_per_tick = 0;
static size_t sample_buffer_frames;  /* e.g., 44100 */
static size_t sample_buffer_samples; /* e.g., 88200 if stereo */
static size_t sample_buffer_bytes;   /* e.g., 176400 if 16-bit */
static size_t bytes_per_frame;      /* e.g., 4 for 16-bit stereo */
static regulator_sample_t *sample_sort_buffer = NULL;

static int this_tick_is_good = 0;
static size_t this_tick_peak = SIZE_T_MAX;
static size_t this_tick_shift_by_half = 0;

static pa_simple *pa_s = NULL;
static pa_sample_spec pa_ss = { .format   = PA_SAMPLE_S16LE,
                                .rate     = 44100,
                                .channels = 1 };
static int pa_error = 0;
static char pa_ss_string[PA_SAMPLE_SPEC_BUFSIZE];

static int16_t pa_sample_max;
static int16_t pa_sample_min;
static int16_t pa_sample_avg;

static pa_usec_t pa_latency;
static pa_buffer_attr pa_ba = { .maxlength = 44100,
                                .minreq    = 0,
                                .prebuf    = 0,
                                .tlength   = 0,
                                .fragsize  = 44100 };

/* after options are set */
void regulator_run() {
    if (audio_filename == NULL) {
        regulator_pulseaudio_open();
    } else {
        regulator_sndfile_open();
    }

    if (debug >= 2) {
        printf("%d ticks per hour\n", (int)ticks_per_hour);
        printf("%d samples per tick\n", (int)samples_per_tick);
        printf("%d sample buffer frames\n", (int)sample_buffer_frames);
        printf("%d sample buffer samples\n", (int)sample_buffer_samples);
        printf("%d sample buffer bytes\n", (int)sample_buffer_bytes);
        printf("%d bytes per frame\n", (int)bytes_per_frame);
    }

    size_t samples;
    size_t tick_count = 0;
    size_t tick_index;
    size_t ticks_in_sample_data_block = (TICKS_PER_GROUP * 2 + 2);
    size_t samples_in_sample_data_block = ticks_in_sample_data_block * samples_per_tick;
    int16_t *sample_data_block = (int16_t *)malloc(sizeof(int16_t) * samples_in_sample_data_block);

    if (debug >= 2) {
        printf("%d ticks in sample data block\n", (int)ticks_in_sample_data_block);
        printf("%d samples in sample data block\n", (int)samples_in_sample_data_block);
    }

    int16_t *append_pointer = sample_data_block;
    size_t append_index = 0;
    int16_t *analyze_pointer = sample_data_block;
    size_t analyze_index = 0;

    int tried_shifting_by_half = 0;
    size_t tick_shift_by_half_count = 0;
    size_t good_tick_count = 0;
    tick_peak_t *tick_peak_data = (tick_peak_t *)malloc(sizeof(tick_peak_t) * ticks_per_hour);
    size_t tick_peak_index = 0;

    for (; tick_count < TICKS_PER_GROUP; tick_count += 1) {
        if ((samples = regulator_read(append_pointer, samples_per_tick)) < samples_per_tick) {
            fprintf(stderr, "%s: not enough data\n", progname);
            exit(1);
        }
        append_pointer += samples_per_tick;
        append_index   += samples_per_tick;
    }

 reanalyze:
    for (tick_index = 0; tick_index < tick_count; tick_index += 1) {
        regulator_analyze_tick(analyze_pointer);
        analyze_pointer += samples_per_tick;
        analyze_index   += samples_per_tick;
        if (this_tick_shift_by_half) {
            tick_shift_by_half_count += 1;
        }
        if (this_tick_is_good) {
            tick_peak_data[tick_peak_index].index = tick_index;
            tick_peak_data[tick_peak_index].peak  = this_tick_peak;
            if (debug >= 2) {
                printf("data point # %6d: %6d at tick # %6d\n",
                       (int)tick_peak_index,
                       (int)this_tick_peak,
                       (int)tick_index);
            }
            tick_peak_index += 1;
            good_tick_count += 1;
        } else {
            if (debug >= 2) {
                printf("data not good enough at tick # %6d\n",
                       (int)tick_index);
            }
        }
    }

    if (tick_shift_by_half_count >= (TICKS_PER_GROUP * 3 / 4)) {
        if (tried_shifting_by_half) {
            fprintf(stderr, "%s: UNEXPECTED ERROR 1\n", progname);
            exit(1);
        }
        if (debug >= 2) {
            printf("too many ticks at beginning or end of windows; shifting and trying again\n");
        }
        tried_shifting_by_half = 1;
        if ((samples = regulator_read(append_pointer, samples_per_tick / 2)) < (samples_per_tick / 2)) {
            fprintf(stderr, "%s: not enough data\n", progname);
            exit(1);
        }
        append_pointer += samples_per_tick / 2;
        append_index   += samples_per_tick / 2;
        analyze_pointer = sample_data_block + samples_per_tick / 2;
        analyze_index   =                     samples_per_tick / 2;
        tick_count = 0;
        tick_shift_by_half_count = 0;
        good_tick_count = 0;
        tick_peak_index = 0;
        goto reanalyze;
    }

    int samples_too_fast = 0;
    int samples_too_slow = 0;

    int too_fast = 0;
    int too_slow = 0;

    /* max. 1 hour of data, I guess */
    for (; tick_count < ticks_per_hour; tick_count += 1) {

        size_t samples_needed = samples_per_tick;
        size_t extra_samples = 0;

        if (this_tick_is_good) {
            if (this_tick_peak < samples_per_tick * SHIFT_POINT_PERCENT / 100) {
                samples_too_fast += 1;
                samples_too_slow = 0;
            } else if (this_tick_peak >= samples_per_tick * (100 - SHIFT_POINT_PERCENT) / 100) {
                samples_too_fast = 0;
                samples_too_slow += 1;
            } else {
                samples_too_fast = 0;
                samples_too_slow = 0;
            }
        }

        too_fast = (samples_too_fast >= 3);
        too_slow = (samples_too_slow >= 3);

        if (too_fast) {
            /* next tick is close at hand; next read will be tick */
            samples_needed += (extra_samples = samples_per_tick * SHIFT_POINT_PERCENT * 3 / 100);
            if (debug >= 2) {
                printf("too fast; will read %d extra samples\n", (int)extra_samples);
            }
        } else if (too_slow) {
            /* next tick is kinda far; next read will be blank */
            samples_needed += (extra_samples = samples_per_tick * (100 - SHIFT_POINT_PERCENT * 3) / 100);
            if (debug >= 2) {
                printf("too slow; will read %d extra samples\n", (int)extra_samples);
            }
        }

        if (append_index + samples_needed >= samples_in_sample_data_block) {
            if (debug >= 2) {
                printf("cleanup\n");
            }
            memmove(/* dest */ sample_data_block,
                    /* src  */ sample_data_block + (append_index - samples_per_tick * TICKS_PER_GROUP),
                    /* n    */ sizeof(int16_t) * samples_per_tick * TICKS_PER_GROUP);
            append_pointer = sample_data_block + samples_per_tick * TICKS_PER_GROUP;
            append_index   =                     samples_per_tick * TICKS_PER_GROUP;
            analyze_pointer = append_pointer;
            analyze_index   = append_index;
        }

        int must_analyze      = 1;
        int must_do_full_read = 1;

        if (too_fast) {
            /*                             V-- append */
            /*         V-- analyze */
            /* [  ^                ]  ^      */
            /*         [                   ] */
            if ((samples = regulator_read(append_pointer, extra_samples)) < extra_samples) {
                /* no more data */
                break;
            }
            append_pointer += extra_samples;
            append_index   += extra_samples;
            analyze_pointer = analyze_pointer + extra_samples - samples_per_tick;
            analyze_index   = analyze_index   + extra_samples - samples_per_tick;
            if (debug >= 2) {
                printf("shifting ticks 0 through %d by %d\n",
                       (int)(tick_count - 1),
                       (int)(samples_per_tick - extra_samples));
            }
            for (tick_index = 0; tick_index < tick_count; tick_index += 1) {
                tick_peak_data[tick_peak_index].peak += samples_per_tick - extra_samples;
            }
            samples_too_fast = 0;
            samples_too_slow = 0;

            /* to work on the next tick */
            must_analyze = 1;
            must_do_full_read = 0;
        } else if (too_slow) {
            /*                                 V-- append */
            /*                                 V-- analyze */
            /* [                ^  ]                ^ */
            /*             [                   ]      */
            if ((samples = regulator_read(append_pointer, extra_samples)) < extra_samples) {
                /* no more data */
                break;
            }
            append_pointer += extra_samples;
            append_index   += extra_samples;
            analyze_pointer += extra_samples;
            analyze_index   += extra_samples;
            if (debug >= 2) {
                printf("shifting ticks 0 through %d by %d\n",
                       (int)(tick_count - 1),
                       (int)(-extra_samples));
            }
            for (tick_index = 0; tick_index < tick_count; tick_index += 1) {
                tick_peak_data[tick_peak_index].peak -= extra_samples;
            }
            samples_too_fast = 0;
            samples_too_slow = 0;

            /* to work on the next tick */
            must_analyze = 1;
            must_do_full_read = 1;
        }

        if (must_do_full_read) {
            if ((samples = regulator_read(append_pointer, samples_per_tick)) < samples_per_tick) {
                /* no more data */
                break;
            }
            append_pointer += samples_per_tick;
            append_index   += samples_per_tick;
        }

        if (must_analyze) {
            regulator_analyze_tick(analyze_pointer);
            analyze_pointer += samples_per_tick;
            analyze_index   += samples_per_tick;

            if (this_tick_is_good) {
                tick_peak_data[tick_peak_index].index = tick_count;
                tick_peak_data[tick_peak_index].peak  = this_tick_peak;
                if (debug >= 2) {
                    printf("data point # %6d: %6d at tick # %6d\n",
                           (int)tick_peak_index,
                           (int)this_tick_peak,
                           (int)tick_count);
                }
                tick_peak_index += 1;
                good_tick_count += 1;
            } else {
                if (debug >= 2) {
                    printf("data not good enough at tick # %6d\n",
                           (int)tick_count);
                }
            }
        }
    }

    /* in samples per tick */
    float drift = compute_kendall_thiel_best_fit(tick_peak_data, tick_peak_index);

    /* in seconds per tick */
    drift = -drift / 44100;

    /* in seconds per hour */
    drift = drift * ticks_per_hour;

    /* in seconds per day */
    drift = drift * 24;

    if (drift < 0) {
        printf("your clock is slow by %f seconds per day\n", (double)(-drift));
    } else if (drift > 0) {
        printf("your clock is fast by %f seconds per day\n", (double)drift);
    } else {
        printf("perfect!\n");
    }

    if (debug >= 1) {
        printf("%d good data points out of %d wanted\n", (int)good_tick_count, (int)tick_count);
    }
}

/**
 * For sampling magnitude-index sample pairs in decreasing order of
 * magnitude.
 */
int sample_sort(const regulator_sample_t *a,
                const regulator_sample_t *b) {
    if (a->sample < b->sample) {
        return 1;
    }
    if (a->sample > b->sample) {
        return -1;
    }
    return 0;
}

SNDFILE* sf;
SF_INFO sfinfo = { .frames     = 0,
                   .samplerate = 0,
                   .channels   = 0,
                   .format     = 0, /* set to 0 before reading */
                   .sections   = 0,
                   .seekable   = 0 };
int *sf_sample_buffer;

sf_count_t sf_frames;
#define sf_num_frames sample_buffer_frames
#define sf_num_items  sample_buffer_samples

#define CHANNEL_NUMBER 0

void regulator_sndfile_open() {
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
    bytes_per_frame      = sfinfo.channels * sizeof(int);

    if (!(sf_sample_buffer = (int *)malloc(sample_buffer_bytes))) {
        perror(progname);
        exit(1);
    }
    if (!(sample_sort_buffer =
          (regulator_sample_t *)
          malloc(sample_buffer_frames * sizeof(regulator_sample_t)))) {
        perror(progname);
        exit(1);
    }
}

size_t regulator_read(int16_t *buffer, size_t samples) {
    size_t samples_read;
    if (audio_filename == NULL) {
        samples_read = regulator_pulseaudio_read(buffer, samples);
    } else {
        samples_read = regulator_sndfile_read(buffer, samples);
    }
    return samples_read;
}

/* mainly to find the peak */
void regulator_analyze_tick(int16_t *buffer) {
    size_t i;
    size_t peak_sample_indexes[PEAK_SAMPLES];
    size_t index;
    size_t low_indexes = 0;
    size_t high_indexes = 0;

    for (i = 0; i < samples_per_tick; i += 1) {
        sample_sort_buffer[i].sample = buffer[i];
        sample_sort_buffer[i].index = i;
    }

    qsort(sample_sort_buffer, samples_per_tick, sizeof(regulator_sample_t),
          (int (*)(const void *, const void *))sample_sort);

    for (i = 0; i < PEAK_SAMPLES; i += 1) {
        index = sample_sort_buffer[i].index;
        peak_sample_indexes[i] = index;
        if (index < (samples_per_tick * PEAK_WAY_OFF_THRESHOLD_1 / PEAK_SAMPLES)) {
            low_indexes += 1;
        }
        if ((samples_per_tick - index) <= (samples_per_tick * PEAK_WAY_OFF_THRESHOLD_1 / PEAK_SAMPLES)) {
            high_indexes += 1;
        }
    }

    /* heuristic */
    this_tick_shift_by_half = ((low_indexes >= (PEAK_WAY_OFF_THRESHOLD_2) &&
                                high_indexes >= (PEAK_WAY_OFF_THRESHOLD_2)) ||
                               low_indexes >= (PEAK_WAY_OFF_THRESHOLD_2 * 2) ||
                               high_indexes >= (PEAK_WAY_OFF_THRESHOLD_2 * 2));

    qsort(peak_sample_indexes, PEAK_SAMPLES, sizeof(size_t),
          (int (*)(const void *, const void *))size_t_sort);
    size_t lowest_index  = peak_sample_indexes[PEAK_WAY_OFF_THRESHOLD_2];
    size_t highest_index = peak_sample_indexes[PEAK_SAMPLES - 1 - PEAK_WAY_OFF_THRESHOLD_2];
    this_tick_is_good = ((highest_index - lowest_index) < samples_per_tick * PEAK_WAY_OFF_THRESHOLD_1 / PEAK_SAMPLES);
    if (this_tick_is_good) {
        this_tick_peak = (highest_index + lowest_index) / 2;
    } else {
        this_tick_peak = SIZE_T_MAX;
    }
}

int size_t_sort(const size_t *a, const size_t *b) {
    if (*a < *b) {
        return -1;
    }
    if (*a > *b) {
        return 1;
    }
    return 0;
}

size_t regulator_sndfile_read(int16_t *buffer, size_t samples) {
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

/* after options are set */
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
                progname, (int)ticks_per_hour, sfinfo.samplerate);
        exit(1);
    }
    samples_per_tick = 3600 * pa_ss.rate / ticks_per_hour;

    sample_buffer_frames  = samples_per_tick;
    sample_buffer_samples = sample_buffer_frames * pa_ss.channels;
    sample_buffer_bytes   = pa_bytes_per_second(&pa_ss) * 3600 / ticks_per_hour;
    bytes_per_frame      = pa_ss.channels * sizeof(int16_t);

    pa_ba.maxlength = sample_buffer_bytes;
    pa_ba.fragsize  = sample_buffer_bytes;

    pa_sample_spec_snprint(pa_ss_string, PA_SAMPLE_SPEC_BUFSIZE, &pa_ss);

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

    if ((pa_latency = pa_simple_get_latency(pa_s, &pa_error)) == (pa_usec_t)-1) {
        fprintf(stderr, "%s: pa_simple_get_latency() failed: %s\n", progname, pa_strerror(pa_error));
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

void regulator_show_vu() {
    int16_t min;
    int16_t max;
    int16_t avg;
    int i;
    min = pa_sample_min >> (sizeof(int16_t) * 8 - 7);
    max = pa_sample_max >> (sizeof(int16_t) * 8 - 7);
    avg = pa_sample_avg >> (sizeof(int16_t) * 8 - 7);
    putchar('[');
    for (i = 0; i < 64; i += 1) {
        if (i == avg) {
            putchar('*');
        } else if (i < min) {
            putchar(' ');
        } else if (i <= max) {
            putchar('-');
        } else {
            putchar(' ');
        }
    }
    putchar(']');
    putchar('\r');
    fflush(stdout);
}

void regulator_usage() {
    puts("usage: regulator [<option> ...] [<command>]");
    puts("commands:");
    puts("    test");
    puts("    guess");
    puts("    run");
    puts("options:");
    puts("    -h, --help                      display this message");
    puts("    -f, --file=<file>               read data from sound file");
    puts("        --ticks-per-hour=<ticks>    specify ticks per hour");
}

void regulator_options(int *argcp, char * const **argvp) {
    int c;

    char optstring[] = "hf:D";
    const char *longoptname;
    static struct option long_options[] =
        {
         /* name,            has_arg,           flag, val */
         { "help",           no_argument,       NULL, 'h' },
         { "44100",          no_argument,       NULL, 0   },
         { "48000",          no_argument,       NULL, 0   },
         { "96000",          no_argument,       NULL, 0   },
         { "192000",         no_argument,       NULL, 0   },
         { "alaw",           no_argument,       NULL, 0   },
         { "mulaw",          no_argument,       NULL, 0   },
         { "pcm",            no_argument,       NULL, 0   },
         { "file",           required_argument, NULL, 'f' },
         { "ticks-per-hour", required_argument, NULL, 0   },
         { "debug",          no_argument,       NULL, 'D' },
         { NULL,             0,                 NULL, 0   }
        };

    while (1) {
        /* int this_option_optind = optind ? optind : 1; */
        int option_index = 0;
        c = getopt_long(*argcp, *argvp, optstring, long_options, &option_index);
        if (c == -1) {
            break;
        }
        switch (c) {
        case -1:
            break;
        case 0:
            longoptname = long_options[option_index].name;
            if (!strcmp(longoptname, "44100")) {
                pa_ss.rate = 44100;
            } else if (!strcmp(longoptname, "48000")) {
                pa_ss.rate = 48000;
            } else if (!strcmp(longoptname, "96000")) {
                pa_ss.rate = 96000;
            } else if (!strcmp(longoptname, "192000")) {
                pa_ss.rate = 192000;
            } else if (!strcmp(longoptname, "alaw")) {
                pa_ss.format = PA_SAMPLE_ALAW;
            } else if (!strcmp(longoptname, "mulaw")) {
                pa_ss.format = PA_SAMPLE_ULAW;
            } else if (!strcmp(longoptname, "pcm")) {
                pa_ss.format = PA_SAMPLE_S16LE;
            } else if (!strcmp(longoptname, "ticks-per-hour")) {
                errno = 0;
                ticks_per_hour = (int)strtol(optarg, (char **)NULL, 10);
                if (ticks_per_hour < 1) {
                    fprintf(stderr, "%s: invalid --ticks-per-hour value: %s\n", progname, optarg);
                    exit(1);
                }
            } else if (!strcmp(longoptname, "debug")) {
                debug += 1;
            } else {
                fprintf(stderr, "%s: option not implemented: --%s\n", progname, longoptname);
                exit(1);
            }
            break;
        case 'D':
            debug += 1;
            break;
        case 'f':
            if (audio_filename != NULL) {
                free(audio_filename);
            }
            audio_filename = strdup(optarg);
            if (!audio_filename) {
                perror(progname);
                exit(1);
            }
            break;
        case 'h':
            regulator_usage();
            exit(0);
        case '?':
            fprintf(stderr, "%s: unknown or ambiguous option: %s\n", progname, "?"); /* FIXME */
            exit(1);
        case ':':
            fprintf(stderr, "%s: option missing argument: %s\n", progname, "?"); /* FIXME */
            exit(1);
        default:
            fprintf(stderr, "%s: ?? getopt returned character code 0x%02x ??\n", progname, c);
            exit(1);
        }
    }
    *argcp -= optind;
    *argvp += optind;
    optind = 0;
}

float compute_kendall_thiel_best_fit(tick_peak_t *data, size_t ticks) {
    if (ticks < 2) {
        return 0;
    }
    size_t nslopes = ((ticks * (ticks - 1)) / 2);
    float *slopes = (float *)malloc(sizeof(float) * nslopes);
    size_t i;
    size_t j;
    size_t si = 0;
    for (i = 0; i < ticks - 1; i += 1) {
        for (j = i + 1; j < ticks; j += 1) {
            slopes[si] = (0.0f + data[j].peak - data[i].peak) / (0.0f + data[j].index - data[i].index);
            si += 1;
        }
    }
    qsort(slopes, nslopes, sizeof(float),
          (int (*)(const void *, const void *))float_sort);
    float result;
    if (nslopes % 2 == 0) {
        result = (slopes[nslopes / 2] + slopes[nslopes / 2 - 1]) / 2;
    } else {
        result = slopes[nslopes / 2];
    }
    free(slopes);
    return result;
}

int float_sort(const float *a, const float *b) {
    return (*a < *b) ? -1 : (*a > *b) ? 1 : 0;
}
