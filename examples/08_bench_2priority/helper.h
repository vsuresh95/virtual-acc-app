#ifndef __HELPER_H__
#define __HELPER_H__

#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <time.h>
#include <sched.h>
#include <unistd.h>
#include <nn_token.h>
#include <pthread.h>

#ifdef ENABLE_SM
const char *sm_print = "with SM";
#ifdef DISABLE_LB
const char *mozart_print = "with Mozart";
#else
const char *mozart_print = "without Mozart";
#endif
#else
const char *sm_print = "without SM";
const char *mozart_print = "";
#endif

#ifdef ENABLE_VAM
const char *vam_print = "with VAM";
#else
const char *vam_print = "without VAM";
#endif

#endif // __HELPER_H__
