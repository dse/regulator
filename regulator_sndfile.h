#ifndef REGULATOR_SNDFILE_H
#define REGULATOR_SNDFILE_H

#include <unistd.h>

void regulator_sndfile_open();
size_t regulator_sndfile_read(int16_t *ptr, size_t samples);

#endif  /* REGULATOR_SNDFILE_H */
