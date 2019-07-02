#ifndef REGULATOR_PULSEAUDIO_H
#define REGULATOR_PULSEAUDIO_H

#include <unistd.h>

void regulator_pulseaudio_open();
size_t regulator_pulseaudio_read(int16_t *ptr, size_t samples);

#endif  /* REGULATOR_PULSEAUDIO_H */
