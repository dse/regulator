#ifndef REGULATOR_H
#define REGULATOR_H

#include <unistd.h>

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
size_t regulator_read(int16_t *ptr, size_t samples);
void regulator_analyze_tick(int16_t *ptr);
void regulator_usage();
void regulator_options(int *argcp, char * const **argvp);
float kt_best_fit(tick_peak_t *data, size_t ticks);

int sample_sort(const regulator_sample_t *a,
                const regulator_sample_t *b);
int size_t_sort(const size_t *a, const size_t *b);
int float_sort(const float *a, const float *b);

#include <stdint.h>

/* You are not expected to understand this. */
/* You are not guaranteed to be able to use this in an #if. */
#ifdef REGULATOR_C
int16_t _endian_test = 0x0001;
#else
extern int16_t _endian_test;
#endif
#define IS_LITTLE_ENDIAN (*((char *)&_endian_test))

extern char *progname;
extern int debug;
extern char *audio_filename;
extern size_t ticks_per_hour; /* how to guess? */
extern size_t samples_per_tick;
extern size_t sample_buffer_frames;  /* e.g., 44100 */
extern size_t sample_buffer_samples; /* e.g., 88200 if stereo */
extern size_t sample_buffer_bytes;   /* e.g., 176400 if 16-bit */
extern size_t bytes_per_frame;      /* e.g., 4 for 16-bit stereo */
extern size_t frames_per_second;
extern regulator_sample_t *sample_sort_buffer;

#endif  /* REGULATOR_H */
