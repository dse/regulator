#ifndef REGULATOR_H
#define REGULATOR_H

void regulator_init();
void regulator_pulseaudio_init();
void regulator_sndfile_init();
void regulator_usage();
void regulator_run();
void regulator_test();
void regulator_guess();
void regulator_options(int *argcp, char * const **argvp);
void regulator_read_buffer();
void regulator_show_vu();

#include <stdint.h>

/* You are not expected to understand this. */
int16_t _endian_test = 0x0001;
#define IS_LITTLE_ENDIAN (*((char *)&_endian_test))

#endif  /* REGULATOR_H */
