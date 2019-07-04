/**
 * regulator_main.c --- clock regulator tool
 *
 * Copyright (C) 2019 Darren Embry.  GPL2.
 */

#define REGULATOR_MAIN_C

#include <stdio.h>
#include <getopt.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include "regulator.h"
#include "regulator_main.h"
#include "regulator_pulseaudio.h"

int main(int argc, char* const argv[]) {
    regulator_t r = {};
    regulator_set_progname(&r, argc, argv);
    regulator_options(&r, &argc, &argv);
    if (argc >= 1 && (!strcmp(argv[0], "test-vu") ||
                      !strcmp(argv[0], "vu") ||
                      !strcmp(argv[0], "test-mic") ||
                      !strcmp(argv[0], "mic"))) {
        regulator_pulseaudio_test(&r);
        exit(0);
    }
    if (!r.ticks_per_hour) {
        fprintf(stderr, "%s: --ticks-per-hour is required\n", r.progname);
        exit(1);
    }
    regulator_run(&r);
    regulator_cleanup(&r);
}

char* regulator_set_progname(struct regulator_t* rp,
                             int argc, char* const argv[]) {
    char* p;
    if (argc < 1) {
        rp->progname = "regulator";
    } else {
        rp->progname = argv[0];
        if ((p = strrchr(rp->progname, '/')) && *(p + 1)) {
            rp->progname = p + 1;
        } else if ((p = strrchr(rp->progname, '\\')) && *(p + 1)) {
            rp->progname = p + 1;
        }
    }
    progname = rp->progname;
    return rp->progname;
}

#pragma GCC diagnostic ignored "-Wunused-parameter"
void regulator_usage(struct regulator_t* rp) {
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
#pragma GCC diagnostic warning "-Wunused-parameter"

void regulator_options(struct regulator_t* rp,
                       int* argcp, char* const** argvp) {
    int c;

    char optstring[] = "hf:D";
    const char* longoptname;
    static struct option long_options[] = {
        /* name,            has_arg,           flag, val */
        { "help",           no_argument,       NULL, 'h' },
        { "file",           required_argument, NULL, 'f' },
        { "ticks-per-hour", required_argument, NULL, 0   },
        { "debug",          no_argument,       NULL, 'D' },
        { "stats",          no_argument,       NULL, 0   },
        { "ticks",          no_argument,       NULL, 0   },
        { NULL,             0,                 NULL, 0   }
    };

    while (1) {
        /* int this_option_optind = optind ? optind : 1; */
        int option_index = 0;
        c = getopt_long(*argcp, *argvp, optstring, long_options,
                        &option_index);
        if (c == -1) {
            break;
        }
        switch (c) {
        case -1:
            break;
        case 0:
            longoptname = long_options[option_index].name;
            if (!strcmp(longoptname, "ticks-per-hour")) {
                errno = 0;
                rp->ticks_per_hour = (int)strtol(optarg, (char**)NULL, 10);
                if (rp->ticks_per_hour < 1) {
                    fprintf(stderr,
                            "%s: invalid --ticks-per-hour value: %s\n",
                            rp->progname, optarg);
                    exit(1);
                }
            } else if (!strcmp(longoptname, "debug")) {
                rp->debug += 1;
            } else if (!strcmp(longoptname, "stats")) {
                rp->show_stats += 1;
            } else if (!strcmp(longoptname, "ticks")) {
                rp->show_ticks += 1;
            } else {
                fprintf(stderr,
                        "%s: option not implemented: --%s\n",
                        rp->progname, longoptname);
                exit(1);
            }
            break;
        case 'D':
            rp->debug += 1;
            break;
        case 'f':
            if (rp->filename != NULL) {
                free(rp->filename);
            }
            rp->filename = strdup(optarg);
            if (!rp->filename) {
                perror(rp->progname);
                exit(1);
            }
            break;
        case 'h':
            regulator_usage(rp);
            exit(0);
        case '?':
            fprintf(stderr,
                    "%s: unknown or ambiguous option: %s\n",
                    rp->progname, "?"); /* FIXME */
            exit(1);
        case ':':
            fprintf(stderr,
                    "%s: option missing argument: %s\n",
                    rp->progname, "?"); /* FIXME */
            exit(1);
        default:
            fprintf(stderr,
                    "%s: ?? getopt returned character code 0x%02x ??\n",
                    rp->progname, c);
            exit(1);
        }
    }
    *argcp -= optind;
    *argvp += optind;
    optind = 0;
}
