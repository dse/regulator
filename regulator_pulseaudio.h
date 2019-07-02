#ifndef REGULATOR_PULSEAUDIO_H
#define REGULATOR_PULSEAUDIO_H

#include <unistd.h>

#include <pulse/simple.h>
#include <pulse/error.h>

typedef struct regulator_pulseaudio_t {
    pa_simple *pa_s;
    int pa_error;
    pa_sample_spec pa_ss;
    pa_buffer_attr pa_ba;
} regulator_pulseaudio_t;

void regulator_pulseaudio_open();
size_t regulator_pulseaudio_read(int16_t *ptr, size_t samples);

#endif  /* REGULATOR_PULSEAUDIO_H */
