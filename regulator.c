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

#include <pulse/simple.h>
#include <pulse/error.h>

#define BUFSIZE 1024

void regulator_preinit();
void regulator_usage();
void regulator_run();
void regulator_test();
void regulator_guess();
void regulator_options(int *argcp, char * const **argvp);
void regulator_read_buffer();
void regulator_show_vu();

static char *progname;

int main(int argc, char * const argv[]) {
    regulator_preinit();
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

static pa_simple *s = NULL;
static pa_sample_spec ss = {
    .format   = PA_SAMPLE_U8,
    .rate     = 44100,
    .channels = 1
};
static int error = 0;
static char ss_string[BUFSIZE];
static size_t data_buffer_size;
static uint8_t *data_buffer = NULL;
static uint8_t sample_max;
static uint8_t sample_min;
static uint8_t sample_histogram[256];
static long sample_histogram_cum[256];
static uint8_t sample_avg;
static pa_usec_t latency;
static pa_buffer_attr ba = {
    .maxlength = 44100,
    .minreq    = 0,
    .prebuf    = 0,
    .tlength   = 0,
    .fragsize  = 44100
};

static int data_buffer_seconds         = 1;
static int data_buffer_fraction_second = 20;
static int data_buffer_reads           = 20;

/* mainly for defaults */
void regulator_preinit() {
    /* stub function */
}

/* after options are set */
void regulator_init() {
    /* sanity check */
    if (!pa_sample_spec_valid(&ss)) {
        fprintf(stderr, "%s: invalid sample specification\n", progname);
        exit(1);
    }
    if (!pa_sample_rate_valid(ss.rate)) {
        fprintf(stderr, "%s: invalid sample rate\n", progname);
        exit(1);
    }

    data_buffer_size = pa_bytes_per_second(&ss) / data_buffer_fraction_second;
    ba.maxlength = data_buffer_size;
    ba.fragsize = data_buffer_size;

    data_buffer_reads = data_buffer_seconds * data_buffer_fraction_second;

    printf("%s: data_buffer_size = %ld\n", progname, (long)data_buffer_size);

    pa_sample_spec_snprint(ss_string, BUFSIZE, &ss);
    printf("%s: %s\n", progname, ss_string);

    s = pa_simple_new(NULL,             /* server name */
                      progname,         /* name */
                      PA_STREAM_RECORD, /* direction */
                      NULL,             /* device name or default */
                      "record",         /* stream name */
                      &ss,              /* sample type */
                      NULL,             /* channel map */
                      &ba,              /* buffering attributes */
                      &error);          /* error code */
    if (!s) {
        fprintf(stderr, "%s: pa_simple_new() failed: %s\n", progname, pa_strerror(error));
        exit(1);
    }

    if ((latency = pa_simple_get_latency(s, &error)) == (pa_usec_t)-1) {
        fprintf(stderr, "%s: pa_simple_get_latency() failed: %s\n", progname, pa_strerror(error));
        exit(1);
    }
    printf("%s: latency is %f seconds\n", progname, 0.000001 * latency);

    data_buffer = (uint8_t *)malloc(data_buffer_size);
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
    min = sample_min / 4; /* 0..63 */
    max = sample_max / 4; /* 0..63 */
    avg = sample_avg / 4;
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
        memset(sample_histogram, 0, sizeof(sample_histogram));
        for (i = 0; i < (long)data_buffer_size; i += 1) {
            sample_histogram[data_buffer[i]] += 1;
        }
    }
    max = sizeof(sample_histogram);
    if (ss.format == PA_SAMPLE_U8) {
        long cum = 0;
        max = max / 2;
        for (i = max - 1; i >= 0; i -= 1) {
            cum += sample_histogram[i];
            sample_histogram_cum[i] = cum;
        }
        for (i = 0; i < max; i += 1) {
            printf("%3ld %3d %6f\n",
                   (long)i,
                   sample_histogram[i],
                   sample_histogram_cum[i] * 1.0 / data_buffer_size);
        }
    } else {
        for (i = 0; i < max; i += 1) {
            printf("%3ld %3d\n",
                   (long)i,
                   sample_histogram[i]);
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
}

void regulator_options(int *argcp, char * const **argvp) {
    int c;

    char optstring[] = "h";
    const char *longoptname;
    static struct option long_options[] = {
        /* name, has_arg, flag, val */
        { "help",   no_argument, 0, 'h' },
        { "44100",  no_argument, 0, 0 },
        { "48000",  no_argument, 0, 0 },
        { "96000",  no_argument, 0, 0 },
        { "192000", no_argument, 0, 0 },
        { "alaw",   no_argument, 0, 0 },
        { "mulaw",  no_argument, 0, 0 },
        { "pcm",    no_argument, 0, 0 },
        { 0,      0,           0, 0   }
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
                ss.rate = 44100;
            } else if (!strcmp(longoptname, "48000")) {
                ss.rate = 48000;
            } else if (!strcmp(longoptname, "96000")) {
                ss.rate = 96000;
            } else if (!strcmp(longoptname, "192000")) {
                ss.rate = 192000;
            } else if (!strcmp(longoptname, "alaw")) {
                ss.format = PA_SAMPLE_ALAW;
            } else if (!strcmp(longoptname, "mulaw")) {
                ss.format = PA_SAMPLE_ULAW;
            } else if (!strcmp(longoptname, "pcm")) {
                ss.format = PA_SAMPLE_U8;
            } else {
                fprintf(stderr, "%s: option not implemented: --%s\n",
                        progname, longoptname);
                exit(1);
            }
            break;
        case '?':
            break;
        case 'h':
            regulator_usage();
            exit(0);
        default:
            printf("%s: ?? getopt returned character code 0%o ??\n", progname, c);
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
    if (pa_simple_read(s, data_buffer, data_buffer_size, &error) < 0) {
        fprintf(stderr, "%s: pa_simple_read failed: %s\n", progname, pa_strerror(error));
        exit(1);
    }
    sample_min = 255;
    sample_max = 0;
    for (i = 0; i < data_buffer_size; i += 1) {
        samplep = data_buffer + i;
        if (ss.format == PA_SAMPLE_U8) {
            if (*samplep < 128) {
                *samplep = 128 - *samplep;
            } else {
                *samplep = *samplep - 128;
            }
        }
        sample = *samplep;
        sample_sum += sample;
        if (sample < sample_min) {
            sample_min = sample;
        }
        if (sample > sample_max) {
            sample_max = sample;
        }
    }
    sample_avg = (sample_sum + 128) / data_buffer_size;
}
