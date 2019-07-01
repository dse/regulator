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
#define PA_SAMPLE_SPEC_BUFSIZE 1024

/* sndfile */
#include <sndfile.h>

#include "regulator.h"

static char *progname;

int main(int argc, char * const argv[]) {
    progname = argv[0];
    regulator_options(&argc, &argv);
    regulator_run();
    /* if (!argc || !strcmp(argv[0], "run")) { */
    /*     printf("%s: running regulator_run()\n", progname); */
    /*     regulator_run(); */
    /* } else if (!strcmp(argv[0], "test")) { */
    /*     printf("%s: running regulator_test()\n", progname); */
    /*     regulator_test(); */
    /* } else if (!strcmp(argv[0], "guess")) { */
    /*     printf("%s: running regulator_guess()\n", progname); */
    /*     regulator_guess(); */
    /* } else { */
    /*     fprintf(stderr, "%s: unknown command: %s\n", progname, argv[0]); */
    /*     exit(1); */
    /* } */
}

static char *audio_filename   = NULL;
static int ticks_per_hour     = 3600; /* how to guess? */
static int samples_per_tick   = 44100;
static size_t sample_buffer_frames;  /* e.g., 44100 */
static size_t sample_buffer_samples; /* e.g., 88200 if stereo */
static size_t sample_buffer_bytes;   /* e.g., 176400 if 16-bit */
static int16_t *sample_buffer = NULL;

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
        regulator_pulseaudio_run();
    } else {
        regulator_sndfile_run();
    }
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

void regulator_sndfile_run() {
    sf = sf_open(audio_filename, SFM_READ, &sfinfo);
    if (!sf) {
        fprintf(stderr, "%s: unable to open %s: %s\n", progname, audio_filename, sf_strerror(NULL));
        exit(1);
    }

    if ((3600 * sfinfo.samplerate) % ticks_per_hour) {
        fprintf(stderr, "%s: can't process --ticks-per-hour=%d with this file, sample rate is %d/sec\n",
                progname, ticks_per_hour, sfinfo.samplerate);
        exit(1);
    }
    samples_per_tick = 3600 * sfinfo.samplerate / ticks_per_hour;

    sample_buffer_frames  = samples_per_tick;
    sample_buffer_samples = sample_buffer_frames * sfinfo.channels;
    sample_buffer_bytes   = sample_buffer_samples * sizeof(int);

    if (!(sf_sample_buffer = (int *)malloc(sample_buffer_bytes))) {
        perror(progname);
        exit(1);
    }
    if (!(sample_buffer = (int16_t *)malloc(sample_buffer_frames * sizeof(int16_t)))) {
        perror(progname);
        exit(1);
    }
    sf_count_t sf_frames;
    int i;
    int sample;
    while ((sf_frames = sf_readf_int(sf, sf_sample_buffer, sf_num_frames)) > 0) {
        for (i = 0; i < sf_frames; i += 1) {
            /* quantize an int to an int16_t */
            sample = sf_sample_buffer[i * sfinfo.channels + CHANNEL_NUMBER] / (1 << ((sizeof(int) - sizeof(int16_t)) * 8));
            if (sample == INT16_MIN) { /* -32768 => 32767 */
                sample = INT16_MAX;
            } else if (sample < 0) {
                sample = -sample;
            }
            sample_buffer[i] = sample;
        }
    }
}

/* after options are set */
void regulator_pulseaudio_run() {
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
                progname, ticks_per_hour, sfinfo.samplerate);
        exit(1);
    }
    samples_per_tick = 3600 * pa_ss.rate / ticks_per_hour;

    sample_buffer_frames  = samples_per_tick;
    sample_buffer_samples = sample_buffer_frames * pa_ss.channels;
    sample_buffer_bytes   = pa_bytes_per_second(&pa_ss) * 3600 / ticks_per_hour;

    pa_ba.maxlength = sample_buffer_bytes;
    pa_ba.fragsize  = sample_buffer_bytes;

    printf("%s: sample_buffer_bytes = %ld bytes\n", progname, (long)sample_buffer_bytes);

    pa_sample_spec_snprint(pa_ss_string, PA_SAMPLE_SPEC_BUFSIZE, &pa_ss);
    printf("%s: %s\n", progname, pa_ss_string);

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
    printf("%s: latency is %f seconds\n", progname, 0.000001 * pa_latency);

    sample_buffer = (int16_t *)malloc(sample_buffer_bytes);
    if (!sample_buffer) {
        perror(progname);
        exit(1);
    }

    while (1) {
        regulator_read_buffer();
        regulator_show_vu();
    }
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

    char optstring[] = "hf:";
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
            } else {
                fprintf(stderr, "%s: option not implemented: --%s\n",
                        progname, longoptname);
                exit(1);
            }
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
            printf("%s: unknown or ambiguous option: %s\n", progname, "?"); /* FIXME */
            exit(1);
        case ':':
            printf("%s: option missing argument: %s\n", progname, "?"); /* FIXME */
            exit(1);
        default:
            printf("%s: ?? getopt returned character code 0x%02x ??\n", progname, c);
            exit(1);
        }
    }
    *argcp -= optind;
    *argvp += optind;
    optind = 0;
}

void regulator_read_buffer() {
    size_t i;
    int16_t *samplep;
    if (pa_simple_read(pa_s, sample_buffer, sample_buffer_bytes, &pa_error) < 0) {
        fprintf(stderr, "%s: pa_simple_read failed: %s\n", progname, pa_strerror(pa_error));
        exit(1);
    }

    /* if on a big-endian system, convert from little endian by swapping bytes, lol */
    if (!IS_LITTLE_ENDIAN) {
        char *a;
        char *b;
        for (i = 0; i < sample_buffer_samples; i += 1) {
            a = (char *)(sample_buffer + i);
            b = a + 1;

            /* swap */
            *a = *a ^ *b;
            *b = *a ^ *b;
            *a = *a ^ *b;
        }
    }

    for (i = 0; i < sample_buffer_samples; i += 1) {
        samplep = sample_buffer + i;
        if (*samplep == INT16_MIN) { /* -32768 => 32767 */
            *samplep = INT16_MAX;
        } else if (*samplep < 0) {
            *samplep = -*samplep;
        }
    }
}
