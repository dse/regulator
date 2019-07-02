#ifndef REGULATOR_SNDFILE_H
#define REGULATOR_SNDFILE_H

#include <unistd.h>

#include <sndfile.h>

typedef struct regulator_sndfile_t {
    SNDFILE* sf;
    SF_INFO sfinfo;
    int* sf_sample_buffer;
} regulator_sndfile_t;

void regulator_sndfile_open();
size_t regulator_sndfile_read(int16_t* ptr, size_t samples);

#endif  /* REGULATOR_SNDFILE_H */
