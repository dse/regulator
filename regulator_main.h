#ifndef REGULATOR_MAIN_H
#define REGULATOR_MAIN_H

#include "regulator.h"

char* regulator_set_progname(struct regulator_t* rp,
                             int argc, char* const argv[]);
void regulator_usage(struct regulator_t* rp);
void regulator_options(struct regulator_t* rp,
                       int* argcp, char* const** argvp);

#endif
