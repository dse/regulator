#ifndef REGULATOR_H
#define REGULATOR_H

#include <unistd.h>

#define PA_SAMPLE_SPEC_BUFSIZE   1024
#define TICKS_PER_GROUP          20
#define PEAK_SAMPLES             20
#define PEAK_WAY_OFF_THRESHOLD_1 (PEAK_SAMPLES * 5 / 100)
#define PEAK_WAY_OFF_THRESHOLD_2 (PEAK_SAMPLES * 5 / 100)
#define SHIFT_POINT_PERCENT      10

typedef struct regulator_sample_t {
    int16_t sample;
    size_t  index;
} regulator_sample_t;

typedef struct tick_peak_t {
    size_t index;
    size_t peak;
} tick_peak_t;

void regulator_run();
void regulator_pulseaudio_open();
void regulator_sndfile_open();
size_t regulator_read(int16_t *ptr, size_t samples);
size_t regulator_pulseaudio_read(int16_t *ptr, size_t samples);
size_t regulator_sndfile_read(int16_t *ptr, size_t samples);
void regulator_analyze_tick(int16_t *ptr);
void regulator_usage();
void regulator_options(int *argcp, char * const **argvp);
void regulator_show_vu();
float compute_kendall_thiel_best_fit(tick_peak_t *data, size_t ticks);

int sample_sort(const regulator_sample_t *a,
                const regulator_sample_t *b);
int size_t_sort(const size_t *a, const size_t *b);
int float_sort(const float *a, const float *b);

#include <stdint.h>

/* You are not expected to understand this. */
/* You are not guaranteed to be able to use this in an #if. */
int16_t _endian_test = 0x0001;
#define IS_LITTLE_ENDIAN (*((char *)&_endian_test))

#endif  /* REGULATOR_H */
