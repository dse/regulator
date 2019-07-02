#ifndef REGULATOR_TYPES_H
#define REGULATOR_TYPES_H

#include <unistd.h>
#include <sndfile.h>
#include <pulse/simple.h>
#include <pulse/error.h>

typedef enum regulator_type_t { REGULATOR_TYPE_NONE,
                                REGULATOR_TYPE_PULSEAUDIO,
                                REGULATOR_TYPE_SNDFILE } regulator_type_t;

typedef struct regulator_sndfile_t {
    SNDFILE* sf;
    SF_INFO sfinfo;
    int* sf_sample_buffer;
} regulator_sndfile_t;

typedef struct regulator_pulseaudio_t {
    pa_simple* pa_s;
    int pa_error;
    pa_sample_spec pa_ss;
    pa_buffer_attr pa_ba;
} regulator_pulseaudio_t;

typedef struct regulator_sample_t {
    int16_t sample;
    size_t  index;
} regulator_sample_t;

typedef struct tick_peak_t {
    size_t index;
    size_t peak;
} tick_peak_t;

typedef union regulator_implementation_t {
    regulator_pulseaudio_t pulseaudio;
    regulator_sndfile_t    sndfile;
} regulator_implementation_t;

typedef struct regulator_t {
    char* progname;
    int debug;
    char* filename;
    size_t ticks_per_hour;        /* e.g., 3600 * 5  = 18000 for 5 ticks/second */
    size_t samples_per_tick;      /* e.g., 44100 / 5 = 8820 */
    size_t sample_buffer_frames;  /* e.g., 44100 / 5 = 8820 */
    size_t sample_buffer_samples; /* e.g., 8820 * 2  = 17640 for stereo */
    size_t sample_buffer_bytes;   /* e.g., 17640 * 2 = 35280 for 16-bit */
    size_t bytes_per_frame;       /* e.g., 2 * 2     = 4 for 16-bit stereo */
    size_t frames_per_second;     /* e.g.,             44100 */
    regulator_sample_t* sample_sort_buffer; /* for finding peaks */

    int this_tick_is_good;
    size_t this_tick_peak;
    int this_tick_shift_by_half;

    regulator_type_t type;
    regulator_implementation_t implementation;
} regulator_t;

#endif  /* REGULATOR_TYPES_H */
