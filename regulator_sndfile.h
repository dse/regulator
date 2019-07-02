#ifndef REGULATOR_SNDFILE_H
#define REGULATOR_SNDFILE_H

#include <unistd.h>

#include "regulator_types.h"

void regulator_sndfile_open(struct regulator_t* rp);
size_t regulator_sndfile_read(struct regulator_t* rp, int16_t* ptr, size_t samples);

#endif  /* REGULATOR_SNDFILE_H */
