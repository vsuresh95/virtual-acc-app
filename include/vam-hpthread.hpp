#ifndef __VAM_HPTHREAD_H__
#define __VAM_HPTHREAD_H__

#include <common-defs.hpp>
#include <vam-mem-helper.hpp>
#include <vam-df-graph.hpp>

typedef df_int_node_t hpthread_routine_t;

struct hpthread_t {
    char hpthread_id[100];
    hpthread_routine_t *routine_dfg;

    hpthread_t(const char *name, primitive_t prim = NONE);

    void dump() { routine_dfg->dump(); }
};

typedef enum { IDLE = 0, ONGOING = 1, DONE = 2 } hpthread_intf_state_t;

// Globally visible interface to request for a thread
struct hpthread_intf_t {
    // Interface synchronization variable
    std::atomic<hpthread_intf_state_t> intf_state;

    // Result of previous task
    bool rsp_code;
    
    // hpthread routine
    std::atomic<hpthread_routine_t *> routine;

	// Atomically compare the interface state and swap
    bool CAS_intf_state(hpthread_intf_state_t expected_value, hpthread_intf_state_t new_value);

	// Atomically test the interface state
    hpthread_intf_state_t test_intf_state();

	// Atomically set the interface state
    void set_intf_state(hpthread_intf_state_t s);

	// Atomically set the hpthread routine
    void set_routine(hpthread_routine_t *r);

	// Atomically test the hpthread routine
    hpthread_routine_t *test_routine();

    hpthread_intf_t();
};

// API for creating hpthread
bool hpthread_create(hpthread_t *__newthread,
    void *(*__start_routine) (void *),
    void *__restrict __arg);

// Not yet implemented
void hpthread_join();

hpthread_routine_t *test_hpthread_req();

void ack_hpthread_req(bool success);

#endif // __VAM_HPTHREAD_H__
