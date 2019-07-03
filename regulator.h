#ifndef REGULATOR_H
#define REGULATOR_H

#include <unistd.h>
#include <stdint.h>

#include "regulator_types.h"

#define TICKS_PER_GROUP          20
#define PEAK_SAMPLES             20
#define PEAK_WAY_OFF_THRESHOLD_1 (PEAK_SAMPLES * 5 / 100)
#define PEAK_WAY_OFF_THRESHOLD_2 (PEAK_SAMPLES * 5 / 100)
#define SHIFT_POINT_PERCENT      10

char* regulator_set_progname(struct regulator_t* rp, int argc, char* const argv[]);
void regulator_run(struct regulator_t* rp);
size_t regulator_read(struct regulator_t* rp, size_t samples);
void regulator_analyze_tick(struct regulator_t* rp);
void regulator_usage(struct regulator_t* rp);
void regulator_options(struct regulator_t* rp, int* argcp, char* const** argvp);
void regulator_buffer_shift_left_by(struct regulator_t* rp, size_t samples);
void regulator_buffer_shift_left_to_have(struct regulator_t* rp, size_t samples);
int regulator_buffer_can_shift_left_by(struct regulator_t* rp, size_t samples);
int regulator_buffer_can_shift_left_to_have(struct regulator_t* rp, size_t samples);
int regulator_buffer_can_rewind_by(struct regulator_t* rp, size_t samples);
void regulator_buffer_rewind_by(struct regulator_t* rp, size_t samples);
void regulator_buffer_rewind_max_ticks(struct regulator_t* rp);

float kt_best_fit(tick_peak_t* data, size_t ticks);

int sample_sort(const regulator_sample_t* a, const regulator_sample_t* b);
int size_t_sort(const size_t* a, const size_t* b);
int float_sort(const float* a, const float* b);

/* You are not expected to understand this. */
/* You are not guaranteed to be able to use this in an #if. */
#ifdef REGULATOR_C
int16_t _endian_test = 0x0001;
#else
extern int16_t _endian_test;
#endif
#define IS_LITTLE_ENDIAN (*((char*)&_endian_test))

extern char* progname;

typedef int(*qsort_function)(const void*, const void*);

#endif  /* REGULATOR_H */
