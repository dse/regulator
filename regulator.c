/**
 * regulator.c --- clock regulator tool
 *
 * Copyright (C) 2019 Darren Embry.  GPL2.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include <stdio.h>
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

void regulator_init();
void regulator_pulseaudio_init();
void regulator_sndfile_init();
void regulator_usage();
void regulator_run();
void regulator_test();
void regulator_guess();
void regulator_options(int *argcp, char * const **argvp);
void regulator_read_buffer();
void regulator_show_vu();

static char *progname;

int main(int argc, char * const argv[]) {
    progname = argv[0];
    regulator_options(&argc, &argv);
    if (!argc || !strcmp(argv[0], "run")) {
        printf("%s: running regulator_run()\n", progname);
        regulator_run();
    } else if (!strcmp(argv[0], "test")) {
        printf("%s: running regulator_test()\n", progname);
        regulator_test();
    } else if (!strcmp(argv[0], "guess")) {
        printf("%s: running regulator_guess()\n", progname);
        regulator_guess();
    } else {
        fprintf(stderr, "%s: unknown command: %s\n", progname, argv[0]);
        exit(1);
    }
}

static char *audio_filename = NULL;

static int data_buffer_seconds         = 1;
static int data_buffer_fraction_second = 20;
static int data_buffer_reads           = 20;
static size_t data_buffer_size;

static pa_simple *pa_s = NULL;
static pa_sample_spec pa_ss = { .format   = PA_SAMPLE_U8,
                                .rate     = 44100,
                                .channels = 1 };
static int pa_error = 0;
static char pa_ss_string[PA_SAMPLE_SPEC_BUFSIZE];
static uint8_t *pa_data_buffer = NULL;
static uint8_t pa_sample_max;
static uint8_t pa_sample_min;
static uint8_t pa_sample_histogram[256];
static long pa_sample_histogram_cum[256];
static uint8_t pa_sample_avg;
static pa_usec_t pa_latency;
static pa_buffer_attr pa_ba = { .maxlength = 44100,
                                .minreq    = 0,
                                .prebuf    = 0,
                                .tlength   = 0,
                                .fragsize  = 44100 };

/* after options are set */
void regulator_init() {
    if (audio_filename == NULL) {
        regulator_pulseaudio_init();
    } else {
        regulator_sndfile_init();
        exit(0);
    }
}

SNDFILE* sf;
SF_INFO sfinfo = { .frames     = 0,
                   .samplerate = 0,
                   .channels   = 0,
                   .format     = 0, /* set to 0 before reading */
                   .sections   = 0,
                   .seekable   = 0 };
int *sf_data_buffer;
sf_count_t sf_frames;
sf_count_t sf_num_frames;
sf_count_t sf_num_items;

void regulator_sndfile_init() {
    sf = sf_open(audio_filename, SFM_READ, &sfinfo);
    if (!sf) {
        fprintf(stderr, "%s: unable to open %s: %s\n", progname, audio_filename, sf_strerror(NULL));
        exit(1);
    }
    data_buffer_size = sizeof(int) * sfinfo.samplerate * sfinfo.channels / data_buffer_fraction_second;
    sf_num_frames = sfinfo.samplerate / data_buffer_fraction_second;
    sf_num_items  = sfinfo.samplerate / data_buffer_fraction_second * sfinfo.channels;
    sf_data_buffer = (int *)malloc(data_buffer_size);
    if (!sf_data_buffer) {
        perror(progname);
        exit(1);
    }
    while ((sf_frames = sf_readf_int(sf, sf_data_buffer, sf_num_frames)) > 0) {
        /* we have data! */
        int i;
        int j;
        for (i = 0; i < sf_frames; i += 1) {
            printf("%6d:", i);
            for (j = 0; j < sfinfo.channels; j += 1) {
                int sample = abs(sf_data_buffer[i * sfinfo.channels + j]);
            }
            putchar('\n');
        }
    }
}

/* after options are set */
void regulator_pulseaudio_init() {
    /* sanity check */
    if (!pa_sample_spec_valid(&pa_ss)) {
        fprintf(stderr, "%s: invalid sample specification\n", progname);
        exit(1);
    }
    if (!pa_sample_rate_valid(pa_ss.rate)) {
        fprintf(stderr, "%s: invalid sample rate\n", progname);
        exit(1);
    }

    data_buffer_size = pa_bytes_per_second(&pa_ss) / data_buffer_fraction_second;
    pa_ba.maxlength = data_buffer_size;
    pa_ba.fragsize = data_buffer_size;

    data_buffer_reads = data_buffer_seconds * data_buffer_fraction_second;

    printf("%s: data_buffer_size = %ld\n", progname, (long)data_buffer_size);

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

    pa_data_buffer = (uint8_t *)malloc(data_buffer_size);
    if (!pa_data_buffer) {
        perror(progname);
        exit(1);
    }
}

void regulator_run() {
    regulator_init();
    printf("this is regulator_run().  I'm a stub function.\n");
}

void regulator_test() {
    long counter;
    regulator_init();
    for (counter = 0; !data_buffer_reads || (counter < data_buffer_reads); counter += 1) {
        regulator_read_buffer();
        printf("%8ld: ", counter);
        regulator_show_vu();
    }
}

void regulator_show_vu() {
    int min;
    int max;
    int avg;
    int i;
    min = pa_sample_min / 4; /* 0..63 */
    max = pa_sample_max / 4; /* 0..63 */
    avg = pa_sample_avg / 4;
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

void regulator_guess() {
    long counter;
    long i;
    long max;
    data_buffer_seconds = 3;
    regulator_init();
    for (counter = 0; !data_buffer_reads || (counter < data_buffer_reads); counter += 1) {
        regulator_read_buffer();
        printf("%8ld: ", counter);
        regulator_show_vu();
        memset(pa_sample_histogram, 0, sizeof(pa_sample_histogram));
        for (i = 0; i < (long)data_buffer_size; i += 1) {
            pa_sample_histogram[pa_data_buffer[i]] += 1;
        }
    }
    max = sizeof(pa_sample_histogram);
    if (pa_ss.format == PA_SAMPLE_U8) {
        long cum = 0;
        max = max / 2;
        for (i = max - 1; i >= 0; i -= 1) {
            cum += pa_sample_histogram[i];
            pa_sample_histogram_cum[i] = cum;
        }
        for (i = 0; i < max; i += 1) {
            printf("%3ld %3d %6f\n",
                   (long)i,
                   pa_sample_histogram[i],
                   pa_sample_histogram_cum[i] * 1.0 / data_buffer_size);
        }
    } else {
        for (i = 0; i < max; i += 1) {
            printf("%3ld %3d\n",
                   (long)i,
                   pa_sample_histogram[i]);
        }
    }
}

void regulator_usage() {
    puts("usage: regulator [<option> ...] [<command>]");
    puts("commands:");
    puts("    test");
    puts("    guess");
    puts("    run");
    puts("options:");
    puts("    -h, --help                  display this message");
    puts("    -f, --file=<file>           read data from sound file");
}

void regulator_options(int *argcp, char * const **argvp) {
    int c;

    char optstring[] = "hf:";
    const char *longoptname;
    static struct option long_options[] =
        {
         /* name,    has_arg,           flag, val */
         { "help",   no_argument,       NULL, 'h' },
         { "44100",  no_argument,       NULL, 0   },
         { "48000",  no_argument,       NULL, 0   },
         { "96000",  no_argument,       NULL, 0   },
         { "192000", no_argument,       NULL, 0   },
         { "alaw",   no_argument,       NULL, 0   },
         { "mulaw",  no_argument,       NULL, 0   },
         { "pcm",    no_argument,       NULL, 0   },
         { "file",   required_argument, NULL, 'f' },
         { NULL,     0,                 NULL, 0   }
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
                pa_ss.format = PA_SAMPLE_U8;
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
    long sample_sum = 0;
    uint8_t *samplep;
    uint8_t sample;
    if (pa_simple_read(pa_s, pa_data_buffer, data_buffer_size, &pa_error) < 0) {
        fprintf(stderr, "%s: pa_simple_read failed: %s\n", progname, pa_strerror(pa_error));
        exit(1);
    }
    pa_sample_min = 255;
    pa_sample_max = 0;
    for (i = 0; i < data_buffer_size; i += 1) {
        samplep = pa_data_buffer + i;
        if (pa_ss.format == PA_SAMPLE_U8) {
            if (*samplep < 128) {
                *samplep = 128 - *samplep;
            } else {
                *samplep = *samplep - 128;
            }
        }
        sample = *samplep;
        sample_sum += sample;
        if (sample < pa_sample_min) {
            pa_sample_min = sample;
        }
        if (sample > pa_sample_max) {
            pa_sample_max = sample;
        }
    }
    pa_sample_avg = (sample_sum + 128) / data_buffer_size;
}
