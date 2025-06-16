#include <vam-hpthread.hpp>

// A global interface that is used for all communication
hpthread_intf_t hpthread_intf;

hpthread_t::hpthread_t(const char *name, primitive_t prim) {
    std::strcpy(hpthread_id, name);
    routine_dfg = new df_int_node_t(prim, NULL, true, name);
}

// Atomically compare the interface state and swap
bool hpthread_intf_t::CAS_intf_state(hpthread_intf_state_t expected_value, hpthread_intf_state_t new_value) {
    return intf_state.compare_exchange_strong(expected_value, new_value, std::memory_order_seq_cst);
}

// Atomically test the interface state
hpthread_intf_state_t hpthread_intf_t::test_intf_state() {
    return intf_state.load(std::memory_order_seq_cst);
}

// Atomically set the interface state
void hpthread_intf_t::set_intf_state(hpthread_intf_state_t s) {
    intf_state.store(s, std::memory_order_seq_cst);
}

// Atomically set the hpthread routine
void hpthread_intf_t::set_routine(hpthread_routine_t *r) {
    routine.store(r, std::memory_order_seq_cst);
}

// Atomically test the hpthread routine
hpthread_routine_t *hpthread_intf_t::test_routine() {
    return routine.load(std::memory_order_seq_cst);
}

hpthread_intf_t::hpthread_intf_t(){
    intf_state = IDLE;
    rsp_code = false;
}

// API for creating hpthread
bool hpthread_create(hpthread_t *__newthread,
    void *(*__start_routine) (void *),
    void *__restrict __arg) {

    // assign a name to this hpthread from the name in the DFG
    std::strcpy(__newthread->hpthread_id, __newthread->routine_dfg->get_name());

	// If the interface state is IDLE, update to ONGOING.
	while (!hpthread_intf.CAS_intf_state(IDLE, ONGOING)) sched_yield();;
	
	DEBUG(printf("[HPTHREAD] Requested hpthread for %s.\n", __newthread->hpthread_id);)

	// Submit an hpthread routine
	hpthread_intf.set_routine(__newthread->routine_dfg);
	hpthread_intf.req = hpthread_req_t::CREATE;

	// If the interface state is DONE, update to IDLE.
	while (!hpthread_intf.CAS_intf_state(DONE, IDLE)) sched_yield();;
	
	DEBUG(printf("[HPTHREAD] Received hpthread for %s.\n", __newthread->hpthread_id);)

	// For now, the get accel is not returning anything, but eventually you want to return more useful info.
	return hpthread_intf.rsp_code;
}

bool hpthread_join(hpthread_t *__newthread) {
	// If the interface state is IDLE, update to ONGOING.
	while (!hpthread_intf.CAS_intf_state(IDLE, ONGOING)) sched_yield();;
	
	DEBUG(printf("[HPTHREAD] Deleting hpthread for %s.\n", __newthread->hpthread_id);)

	// Submit an hpthread routine
	hpthread_intf.set_routine(__newthread->routine_dfg);
	hpthread_intf.req = hpthread_req_t::JOIN;

	// If the interface state is DONE, update to IDLE.
	while (!hpthread_intf.CAS_intf_state(DONE, IDLE)) sched_yield();;
	
	DEBUG(printf("[HPTHREAD] Deleted hpthread for %s.\n", __newthread->hpthread_id);)

	// For now, the get accel is not returning anything, but eventually you want to return more useful info.
	return hpthread_intf.rsp_code;    
}

hpthread_routine_t *test_hpthread_req(hpthread_req_t *r) {
    // wait until there is a request on the interface
    if (hpthread_intf.test_intf_state() == ONGOING) {
        *r = hpthread_intf.req;
        return hpthread_intf.test_routine();
    } else {
        return NULL;
    }
}

void ack_hpthread_req(bool success) {
    // set the interface response
    hpthread_intf.rsp_code = success;

    // Set the task as done by acquiring the lock
    hpthread_intf.set_intf_state(DONE);
}
