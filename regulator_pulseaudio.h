#ifndef REGULATOR_PULSEAUDIO_H
#define REGULATOR_PULSEAUDIO_H

#include <unistd.h>

#include "regulator_types.h"

void regulator_pulseaudio_open(struct regulator_t* rp);
void regulator_pulseaudio_close(struct regulator_t* rp);
size_t regulator_pulseaudio_read(struct regulator_t* rp, int16_t* ptr, size_t samples);

#endif  /* REGULATOR_PULSEAUDIO_H */
