#ifndef __HPTHREAD_H__
#define __HPTHREAD_H__

#include <hpthread_types.h>
#include <common_helper.h>

////////////////////////////////////
// Definition for hpthread -- the layout is
// similar to pthread, however, it deviates
// in some aspects
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
    hpthread_args *args;

    // Used for load tracking within VAM 
    unsigned assigned_quota;

    ////////////////////////////////////
    // Member functions serving as user APIs

    // API for user to create a hpthread
    int create();

    // API for user to join a hpthread
    int join();

    // API for setting the start routine
    void setroutine(void *(*s) (void *));

    // API for setting the arguments for the routine
    void setargs(hpthread_args *a);

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

    // get primitive of thread    
    hpthread_prim_t get_prim() { return attr->prim; }
} hpthread_t;

#endif // __HPTHREAD_H__
