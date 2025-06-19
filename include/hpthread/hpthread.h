#ifndef __HPTHREAD_H__
#define __HPTHREAD_H__

#include <hpthread_types.h>
#include <common_defines.h>

typedef struct {
    ////////////////////////////////////
    // Member variables defining the thread

    // Unsigned number identifying this thread
    unsigned id;

    // Attributes for this hpthread
    hpthread_attr_t *attr;

    // SW routine to start with
    void *(*start_routine) (void *);

    // Additional operational arguments for this routine
    void *arg;

    ////////////////////////////////////
    // Member functions serving as user APIs

    // API for user to create a hpthread
    int create(
        void *(*start_routine) (void *),
        void *arg);

    // API for user to join a hpthread
    int join();

    // API for initializing hpthread_attr
    void attr_init();

    // API for setting the primitive computation for this hpthread
    void attr_setname(const char *n);

    // API for setting the primitive computation for this hpthread
    void attr_setprimitive(hpthread_prim_t p);

    // API for setting the priority for this hpthread
    void attr_setpriority(unsigned p);

    ////////////////////////////////////
    // Helper functions for mostly internal use

    // get name of thread    
    char *get_name() { return attr->name; }
} hpthread_t;

#endif // __HPTHREAD_H__
