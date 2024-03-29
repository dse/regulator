/**
 * regulator.c --- clock regulator tool
 *
 * Copyright (C) 2019 Darren Embry.  GPL2.
 */

#define REGULATOR_C

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>

#include "regulator.h"
#include "regulator_pulseaudio.h"
#include "regulator_sndfile.h"

struct regulator_t* regulator_sighandler_ptr = NULL;

void regulator_read_first_batch_of_ticks(struct regulator_t* rp) {
    for (; rp->tick_count < TICKS_PER_GROUP; rp->tick_count += 1) {
        if (!regulator_read(rp, rp->samples_per_tick)) {
            fprintf(stderr, "%s: not enough data\n", rp->progname);
            exit(1);
        }
        regulator_process_tick(rp);
    }
}

void regulator_analyze_first_batch_of_ticks(struct regulator_t* rp) {
    for (; rp->buffer_analyze <= (rp->buffer_append - rp->samples_per_tick);
         rp->tick_count += 1) {
        regulator_analyze_tick(rp);
    }
}

/* after options are set */
void regulator_run(struct regulator_t* rp) {
    if (rp->filename == NULL) {
        regulator_pulseaudio_open(rp);
    } else {
        regulator_sndfile_open(rp);
    }

    if (rp->debug >= 2) {
        printf("%d ticks per hour\n", (int)rp->ticks_per_hour);
        printf("%d samples per tick\n", (int)rp->samples_per_tick);
        printf("%d sample buffer frames\n", (int)rp->sample_buffer_frames);
        printf("%d sample buffer samples\n", (int)rp->sample_buffer_samples);
        printf("%d sample buffer bytes\n", (int)rp->sample_buffer_bytes);
        printf("%d bytes per frame\n", (int)rp->bytes_per_frame);
    }

    rp->buffer_ticks = TICKS_PER_GROUP + 1;
    rp->buffer_samples = rp->buffer_ticks * rp->samples_per_tick;
    rp->buffer = (int16_t*)malloc(sizeof(int16_t) * rp->buffer_samples);
    if (!rp->buffer) {
        perror(rp->progname);
        exit(1);
    }
    rp->buffer_end = rp->buffer + rp->buffer_samples;
    rp->buffer_append = rp->buffer;
    rp->buffer_analyze = rp->buffer;

    if (rp->debug >= 2) {
        printf("%d ticks in sample data block\n", (int)rp->buffer_ticks);
        printf("%d samples in sample data block\n", (int)rp->buffer_samples);
    }

    rp->tick_peak_data =
        (tick_peak_t*)malloc(sizeof(tick_peak_t) * rp->ticks_per_hour);
    if (!rp->tick_peak_data) {
        perror(rp->progname);
        exit(1);
    }

    rp->tick_count = 0;
    rp->good_tick_count = 0;
    rp->tick_peak_count = 0;
    rp->boundary_peak_count = 0;

    regulator_sighandler_ptr = rp;
    signal(SIGINT, regulator_sighandler);

    regulator_read_first_batch_of_ticks(rp);
    regulator_analyze_first_batch_of_ticks(rp);

    if (rp->boundary_peak_count >= (TICKS_PER_GROUP * 3 / 4)) {
        if (rp->debug >= 2) {
            printf("too many ticks at beginning or end of windows; "
                   "shifting and trying again\n");
        }
        if (!regulator_read(rp, rp->samples_per_tick / 2)) {
            fprintf(stderr, "%s: not enough data\n", rp->progname);
            exit(1);
        }
        regulator_process_tick(rp);
        rp->buffer_analyze += rp->samples_per_tick / 2;

        /* be kind, rewind */
        regulator_buffer_rewind_max_ticks(rp);

        rp->tick_count = 0;
        rp->good_tick_count = 0;
        rp->tick_peak_count = 0;
        rp->boundary_peak_count = 0;

        regulator_analyze_first_batch_of_ticks(rp);
    }

    if (rp->boundary_peak_count >= (TICKS_PER_GROUP * 3 / 4)) {
        fprintf(stderr, "%s: UNEXPECTED ERROR 1\n", rp->progname);
        exit(1);
    }

    /* each a count of contiguous peaks */
    int early_peak_count = 0;
    int late_peak_count = 0;

    /* max 1 hour of data */
    for (; rp->tick_count < rp->ticks_per_hour; rp->tick_count += 1) {

        if (rp->this_tick_has_well_defined_peak) {
            if (rp->this_tick_peak <
                (rp->samples_per_tick * SHIFT_POINT_PERCENT / 100)) {
                early_peak_count += 1;
                late_peak_count = 0;
            } else if (rp->this_tick_peak >=
                       (rp->samples_per_tick *
                        (100 - SHIFT_POINT_PERCENT) / 100)) {
                early_peak_count = 0;
                late_peak_count += 1;
            } else {
                early_peak_count = 0;
                late_peak_count = 0;
            }
        }

        size_t samples_needed = rp->samples_per_tick;
        size_t extra_samples = 0;

        int peaks_are_early = (early_peak_count >= 3);
        int peaks_are_late = (late_peak_count >= 3);

        int must_do_full_read = 1;

        if (peaks_are_early) {
            /* next tick is close at hand; next read will be tick */
            samples_needed +=
                (extra_samples =
                 rp->samples_per_tick * SHIFT_POINT_PERCENT * 3 / 100);
            if (rp->debug >= 2) {
                printf("too fast; will read %d extra samples\n",
                       (int)extra_samples);
            }

            if (!regulator_read(rp, extra_samples)) {
                /* no more data */
                break;
            }
            regulator_process_tick(rp);
            rp->buffer_analyze =
                rp->buffer_analyze + extra_samples - rp->samples_per_tick;
            if (rp->debug >= 2) {
                printf("shifting ticks 0 through %d by %d\n",
                       (int)(rp->tick_count - 1),
                       (int)(rp->samples_per_tick - extra_samples));
            }
            for (size_t i = 0; i < rp->tick_count; i += 1) {
                rp->tick_peak_data[rp->tick_peak_count].peak +=
                    rp->samples_per_tick - extra_samples;
            }
            early_peak_count = 0;
            late_peak_count = 0;

            /* to work on the next tick */
            must_do_full_read = 0;
        } else if (peaks_are_late) {
            /* next tick is kinda far; next read will be blank */
            samples_needed +=
                (extra_samples =
                 rp->samples_per_tick * (100 - SHIFT_POINT_PERCENT * 3) / 100);
            if (rp->debug >= 2) {
                printf("too slow; will read %d extra samples\n",
                       (int)extra_samples);
            }

            if (!regulator_read(rp, extra_samples)) {
                /* no more data */
                break;
            }
            regulator_process_tick(rp);
            rp->buffer_analyze += extra_samples;
            if (rp->debug >= 2) {
                printf("shifting ticks 0 through %d by %d\n",
                       (int)(rp->tick_count - 1),
                       (int)(-extra_samples));
            }
            for (size_t i = 0; i < rp->tick_count; i += 1) {
                rp->tick_peak_data[rp->tick_peak_count].peak -= extra_samples;
            }
            early_peak_count = 0;
            late_peak_count = 0;

            /* to work on the next tick */
            must_do_full_read = 1;
        }

        if (must_do_full_read) {
            if (!regulator_read(rp, rp->samples_per_tick)) {
                /* no more data */
                break;
            }
            regulator_process_tick(rp);
        }

        regulator_analyze_tick(rp);
    }
    signal(SIGINT, SIG_DFL);
    regulator_show_result(rp, 0);
}

void regulator_sighandler(int signal) {
    putchar('\n');
    regulator_show_result(regulator_sighandler_ptr, 0);
    exit(0);
}

float regulator_result(struct regulator_t* rp, size_t ticks) {
    if (!ticks || ticks > rp->tick_peak_count) {
        ticks = rp->tick_peak_count;
    }

    /* in samples per tick, -/+ fast/slow */
    float drift = kt_best_fit(rp->tick_peak_data + rp->tick_peak_count - ticks,
                              ticks);

    /* in seconds per day, -/+ slow/fast */
    drift = -drift / rp->frames_per_second * rp->ticks_per_hour * 24;

    return drift;
}

void regulator_show_result(struct regulator_t* rp, size_t ticks) {
    float drift = regulator_result(rp, ticks);
    if (ticks) {
        printf("past %d data points: ", (int)ticks);
    } else {
        printf("all data: ");
    }
    printf("%f seconds %s\n",
           (double)(drift < 0 ? -drift : drift),
           (drift < 0 ? "slow" : "fast"));
    if (!ticks) {
        if (rp->debug >= 1) {
            printf("%d good data points out of %d wanted\n",
                   (int)rp->good_tick_count, (int)rp->tick_count);
        }
    }
}

void regulator_show_tick(struct regulator_t* rp) {
    int16_t* tick_start = rp->buffer_append - rp->samples_per_tick;
    if (tick_start < rp->buffer) {
        return;
    }
    int lines = 20;
    int16_t* temp =
        (int16_t*)malloc(sizeof(int16_t*) *
                         (rp->samples_per_tick / lines + 1));
    if (!temp) {
        perror(rp->progname);
        exit(1);
    }
    putchar('\n');
    for (size_t i = 0; i < (size_t)lines; i += 1) {
        int16_t* start = tick_start + (rp->samples_per_tick *    i   ) / lines;
        int16_t* end   = tick_start + (rp->samples_per_tick * (i + 1)) / lines;
        size_t size = end - start;
        for (size_t j = 0; j < size; j += 1) {
            temp[j] = start[j];
        }
        qsort(temp, size, sizeof(int16_t), (qsort_function)int16_t_sort);

        /* most significant six bits of 95th percentile maximum, so 0
           to 63 */
        int16_t ninety_fifth = temp[size * 95 / 100] /
            (1 << (sizeof(int16_t) * 8 - 7));

        for (int j = 0; j < ninety_fifth; j += 1) {
            putchar('#');
        }
        putchar('\n');
    }
    free(temp);
}

void regulator_cleanup(struct regulator_t* rp) {
    if (rp->tick_peak_data) {
        free(rp->tick_peak_data);
        rp->tick_peak_data = NULL;
    }
    if (rp->buffer) {
        free(rp->buffer);
        rp->buffer = NULL;
    }
    if (rp->type == REGULATOR_TYPE_PULSEAUDIO) {
        regulator_pulseaudio_close(rp);
    } else if (rp->type == REGULATOR_TYPE_SNDFILE) {
        regulator_sndfile_close(rp);
    }
    rp->type = REGULATOR_TYPE_NONE;
}

int regulator_buffer_can_shift_left_by(struct regulator_t* rp,
                                       size_t samples) {
    if (!samples) {
        return 1;
    }
    if (rp->buffer_append - samples < rp->buffer) {
        return 0;
    }
    if (rp->buffer_analyze - samples < rp->buffer) {
        return 0;
    }
    return 1;
}

void regulator_buffer_shift_left_by(struct regulator_t* rp,
                                    size_t samples) {
    if (!samples) {
        return;
    }
    if (rp->debug >= 3) {
        printf("%s: shifting left by %d\n", rp->progname, (int)samples);
        printf("    rp->buffer_append from %d to %d\n",
               (int)(rp->buffer_append - rp->buffer),
               (int)(rp->buffer_append - rp->buffer - samples));
        printf("    rp->buffer_analyze from %d to %d\n",
               (int)(rp->buffer_analyze - rp->buffer),
               (int)(rp->buffer_analyze - rp->buffer - samples));
    }
    if (rp->buffer_append - samples < rp->buffer) {
        fprintf(stderr, "%s: UNEXPECTED ERROR 2\n", rp->progname);
        exit(1);
    }
    if (rp->buffer_analyze - samples < rp->buffer) {
        fprintf(stderr, "%s: UNEXPECTED ERROR 2\n", rp->progname);
        exit(1);
    }
    memmove(/* dest */ rp->buffer,
            /* src  */ rp->buffer + samples,
            /* n    */ (sizeof(int16_t) *
                        (rp->buffer_append - (rp->buffer + samples))));
    rp->buffer_append -= samples;
    rp->buffer_analyze -= samples;
}

int regulator_buffer_can_shift_left_to_have(struct regulator_t* rp,
                                            size_t samples) {
    if (rp->buffer_append - samples < rp->buffer) {
        return 0;
    }
    size_t move_back = rp->buffer_append - rp->buffer - samples;
    if (!move_back) {
        return 1;
    }
    if (rp->buffer_analyze - move_back < rp->buffer) {
        return 0;
    }
    return 1;
}

void regulator_buffer_shift_left_to_have(struct regulator_t* rp,
                                         size_t samples) {
    if (rp->buffer_append - samples < rp->buffer) {
        fprintf(stderr, "%s: UNEXPECTED ERROR 3\n", rp->progname);
        exit(1);
    }
    size_t move_back = rp->buffer_append - rp->buffer - samples;
    if (!move_back) {
        return;
    }
    if (rp->debug >= 3) {
        printf("%s: shifting left by %d to have %d\n",
               rp->progname, (int)move_back, (int)samples);
        printf("    rp->buffer_append from %d to %d\n",
               (int)(rp->buffer_append - rp->buffer),
               (int)(rp->buffer_append - rp->buffer - move_back));
        printf("    rp->buffer_analyze from %d to %d\n",
               (int)(rp->buffer_analyze - rp->buffer),
               (int)(rp->buffer_analyze - rp->buffer - move_back));
    }
    if (rp->buffer_analyze - move_back < rp->buffer) {
        fprintf(stderr, "%s: UNEXPECTED ERROR 4\n", rp->progname);
        exit(1);
    }
    memmove(/* dest */ rp->buffer,
            /* src  */ rp->buffer_append - samples,
            /* n    */ sizeof(int16_t) * samples);
    rp->buffer_append -= move_back;
    rp->buffer_analyze -= move_back;
}

int regulator_buffer_can_rewind_by(struct regulator_t* rp, size_t samples) {
    if (!samples) {
        return 1;
    }
    if (rp->buffer_analyze - samples < rp->buffer) {
        return 0;
    }
    return 1;
}

void regulator_buffer_rewind_max_ticks(struct regulator_t* rp) {
    int ticks = (rp->buffer_analyze - rp->buffer) / rp->samples_per_tick;
    if (ticks < 1) {
        return;
    }
    if (rp->debug >= 3) {
        printf("    rp->buffer_analyze from %d to %d (%d ticks)\n",
               (int)(rp->buffer_analyze - rp->buffer),
               (int)((rp->buffer_analyze - rp->buffer) -
                     ticks * rp->samples_per_tick),
               (int)ticks);
    }
    rp->buffer_analyze -= ticks * rp->samples_per_tick;
}

void regulator_buffer_rewind_by(struct regulator_t* rp, size_t samples) {
    if (!samples) {
        return;
    }
    if (rp->debug >= 3) {
        printf("    rp->buffer_analyze from %d to %d\n",
               (int)(rp->buffer_analyze - rp->buffer),
               (int)((rp->buffer_analyze - rp->buffer) - samples));
    }
    if (rp->buffer_analyze - samples < rp->buffer) {
        fprintf(stderr, "%s: UNEXPECTED ERROR 5\n", rp->progname);
        exit(1);
    }
    rp->buffer_analyze -= samples;
}

size_t regulator_read(struct regulator_t* rp, size_t samples) {
    if (!samples) {
        return 1;
    }
    size_t samples_read;
    if (rp->buffer_end - samples < rp->buffer_append) {
        /* to have enough space to read the next batch of data... */
        if (rp->debug >= 2) {
            printf("not enough to read %d samples\n", (int)samples);
        }
        regulator_buffer_shift_left_by(
            rp, samples - (rp->buffer_end - rp->buffer_append)
        );
    }
    if (rp->filename == NULL) {
        samples_read =
            regulator_pulseaudio_read(rp, rp->buffer_append, samples);
    } else {
        samples_read =
            regulator_sndfile_read(rp, rp->buffer_append, samples);
    }
    rp->buffer_append += samples;
    return samples_read == samples;
}

/* mainly to find the peak */
void regulator_analyze_tick(struct regulator_t* rp) {
    size_t peak_sample_indexes[PEAK_SAMPLES];
    size_t index;
    size_t low_indexes = 0;
    size_t high_indexes = 0;

    if (!rp->sample_sort_buffer) {
        return;
    }

    for (size_t i = 0; i < rp->samples_per_tick; i += 1) {
        rp->sample_sort_buffer[i].sample = rp->buffer_analyze[i];
        rp->sample_sort_buffer[i].index = i;
    }

    rp->buffer_analyze += rp->samples_per_tick;

    qsort(rp->sample_sort_buffer,
          rp->samples_per_tick,
          sizeof(regulator_sample_t),
          (qsort_function)sample_sort);

    for (size_t i = 0; i < PEAK_SAMPLES; i += 1) {
        index = rp->sample_sort_buffer[i].index;
        peak_sample_indexes[i] = index;
        if (index <
            (rp->samples_per_tick * PEAK_WAY_OFF_THRESHOLD_1 / PEAK_SAMPLES)) {
            low_indexes += 1;
        } else if ((rp->samples_per_tick - index) <=
            (rp->samples_per_tick * PEAK_WAY_OFF_THRESHOLD_1 / PEAK_SAMPLES)) {
            high_indexes += 1;
        }
    }

    /* heuristic */
    rp->this_tick_peak_at_boundary = (
        (low_indexes >= (PEAK_WAY_OFF_THRESHOLD_2) &&
         high_indexes >= (PEAK_WAY_OFF_THRESHOLD_2)) ||
        low_indexes >= (PEAK_WAY_OFF_THRESHOLD_2 * 2) ||
        high_indexes >= (PEAK_WAY_OFF_THRESHOLD_2 * 2)
    );

    qsort(peak_sample_indexes, PEAK_SAMPLES, sizeof(size_t),
          (qsort_function)size_t_sort);
    size_t lowest_index  =
        peak_sample_indexes[PEAK_WAY_OFF_THRESHOLD_2];
    size_t highest_index =
        peak_sample_indexes[PEAK_SAMPLES - 1 - PEAK_WAY_OFF_THRESHOLD_2];
    rp->this_tick_has_well_defined_peak =
        ((highest_index - lowest_index) <
         rp->samples_per_tick * PEAK_WAY_OFF_THRESHOLD_1 / PEAK_SAMPLES);
    if (rp->this_tick_has_well_defined_peak) {
        rp->this_tick_peak = (highest_index + lowest_index) / 2;
        if (rp->this_tick_peak <
            rp->samples_per_tick * SHIFT_POINT_PERCENT / 100) {
            rp->this_tick_has_early_peak = 1;
            rp->this_tick_has_late_peak = 0;
        } else if (rp->this_tick_peak >=
                   rp->samples_per_tick * (100 - SHIFT_POINT_PERCENT) / 100) {
            rp->this_tick_has_early_peak = 0;
            rp->this_tick_has_late_peak = 1;
        } else {
            rp->this_tick_has_early_peak = 0;
            rp->this_tick_has_late_peak = 0;
        }
    } else {
        rp->this_tick_peak = SIZE_MAX;
    }

    if (rp->this_tick_peak_at_boundary) {
        rp->boundary_peak_count += 1;
    } else if (rp->this_tick_has_well_defined_peak) {
        rp->tick_peak_data[rp->tick_peak_count].index = rp->tick_count;
        rp->tick_peak_data[rp->tick_peak_count].peak = rp->this_tick_peak;
        if (rp->debug >= 2) {
            printf("data point # %6d: %6d at tick # %6d\n",
                   (int)rp->tick_peak_count,
                   (int)rp->this_tick_peak,
                   (int)rp->tick_count);
        }
        rp->tick_peak_count += 1;
        rp->good_tick_count += 1;
    } else {
        if (rp->debug >= 2) {
            printf("data not good enough at tick # %6d\n",
                   (int)rp->tick_count);
        }
    }
}

void regulator_process_tick(regulator_t* rp) {
    if (rp->show_ticks) {
        regulator_show_tick(rp);
    }
    if (rp->show_stats) {
        if (isatty(fileno(stdout))) {
            if (rp->tick_peak_count) {
                printf("%ld\r", (long)rp->this_tick_peak);
            } else {
                putchar('.');
            }
            fflush(stdout);
        }
        if (rp->tick_count >= 40 && rp->tick_count % 20 == 0) {
            putchar('\n');
            regulator_show_result(rp, 20);
            regulator_show_result(rp, 0);
        }
    }
}

/**
 * Kendall-Thiel best fit.  Mainly so outliers affect the results less.
 */
float kt_best_fit(tick_peak_t* data, size_t ticks) {
    if (ticks < 2) {
        return 0;
    }
    size_t nslopes = ((ticks * (ticks - 1)) / 2);
    float* slopes = (float*)malloc(sizeof(float) * nslopes);
    if (!slopes) {
        perror(progname);
        exit(1);
    }
    size_t si = 0;
    for (size_t i = 0; i < ticks - 1; i += 1) {
        for (size_t j = i + 1; j < ticks; j += 1) {
            slopes[si] = (0.0f + data[j].peak - data[i].peak) /
                (0.0f + data[j].index - data[i].index);
            si += 1;
        }
    }
    qsort(slopes, nslopes, sizeof(float), (qsort_function)float_sort);

    /* median */
    float result;
    if (nslopes % 2 == 0) {
        result = (slopes[nslopes / 2] + slopes[nslopes / 2 - 1]) / 2;
    } else {
        result = slopes[nslopes / 2];
    }

    free(slopes);
    return result;
}

/**
 * For sampling magnitude-index sample pairs in decreasing order of
 * magnitude, to find peaks.
 */
int sample_sort(const regulator_sample_t* a, const regulator_sample_t* b) {
    return (a->sample < b->sample) ? 1 : (a->sample > b->sample) ? -1 : 0;
}

/**
 * Helper function for peak finding.
 */
int size_t_sort(const size_t* a, const size_t* b) {
    return (*a < *b) ? -1 : (*a > *b) ? 1 : 0;
}

/**
 * Helper function for displaying ticks.
 */
int int16_t_sort(const int16_t* a, const int16_t* b) {
    return (*a < *b) ? -1 : (*a > *b) ? 1 : 0;
}

/**
 * Helper function for best fit.
 */
int float_sort(const float* a, const float* b) {
    return (*a < *b) ? -1 : (*a > *b) ? 1 : 0;
}
