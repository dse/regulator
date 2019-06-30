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

static char *progname;

int main(int argc, char * const argv[]) {
    regulator_preinit();
    progname = argv[0];
    regulator_options(&argc, &argv);
    if (!argc || !strcmp(argv[0], "run")) {
        regulator_run();
    } else if (!strcmp(argv[0], "test")) {
        regulator_test();
    } else if (!strcmp(argv[0], "guess")) {
        regulator_guess();
    } else {
        fprintf(stderr, "%s: unknown command: %s\n", progname, argv[0]);
        exit(1);
    }
}

static pa_simple *s;
static pa_sample_spec ss;
static int error;
static char ss_string[BUFSIZE];
static size_t data_buffer_size;
static uint8_t *data_buffer;
static size_t i, j, k, l;
static uint8_t sample;
static uint8_t sample_max;
static uint8_t sample_min;
static pa_usec_t latency;
static long counter;
static pa_buffer_attr ba;

static int data_buffer_seconds         = 1;
static int data_buffer_fraction_second = 1;

/* mainly for defaults */
void regulator_preinit() {
    ss.format   = PA_SAMPLE_U8;
    ss.channels = 1;
    ss.rate     = 44100;
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

    data_buffer_size = pa_bytes_per_second(&ss) * data_buffer_seconds / data_buffer_fraction_second;
    ba.maxlength = data_buffer_size;
    ba.fragsize = data_buffer_size;

    pa_sample_spec_snprint(ss_string, BUFSIZE, &ss);
    fprintf(stderr, "%s: %s\n", progname, ss_string);

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

    for (;;) {
        fprintf(stderr, "%s: reading data\n", progname);
        if (pa_simple_read(s, data_buffer, data_buffer_size, &error) < 0) {
            fprintf(stderr, "%s: pa_simple_read failed: %s\n", progname, pa_strerror(error));
            exit(1);
        }
        fprintf(stderr, "%s: processing data\n", progname);
        for (i = 0; i < (data_buffer_size / 100); i += 1) {
            sample_min = 255;
            sample_max = 0;
            for (j = 0; j < 100; j += 1) {
                sample = data_buffer[i * 100 + j];
                if (sample < sample_min) {
                    sample_min = sample;
                }
                if (sample > sample_max) {
                    sample_max = sample;
                }
            }
            j = sample_min / 4;
            k = sample_max / 4;
            printf("%3d %3d ", sample_min, sample_max);
            for (l = 0; l < 255 / 4; l += 1) {
                putchar(l < j ? ' ' : l <= k ? '-' : ' ');
            }
            putchar('\n');
        }
        exit(0);
    }
}

void regulator_run() {
    regulator_init();
    for (;;) {
    }
}

void regulator_test() {
    regulator_init();
    for (;;) {
    }
}

void regulator_guess() {
    regulator_init();
    for (;;) {
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
