#ifndef REGULATOR_H
#define REGULATOR_H

#include <unistd.h>

typedef struct regulator_sample_t {
    int16_t sample;
    size_t  index;
} regulator_sample_t;

void regulator_run();
void regulator_pulseaudio_open();
void regulator_sndfile_open();
size_t regulator_read();
size_t regulator_pulseaudio_read();
size_t regulator_sndfile_read();
void regulator_analyze_tick();
void regulator_usage();
void regulator_options(int *argcp, char * const **argvp);
void regulator_show_vu();

int sample_sort(const regulator_sample_t *a,
                const regulator_sample_t *b);
int size_t_sort(const size_t *a, const size_t *b);

#include <stdint.h>

/* You are not expected to understand this. */
int16_t _endian_test = 0x0001;
#define IS_LITTLE_ENDIAN (*((char *)&_endian_test))

#endif  /* REGULATOR_H */
