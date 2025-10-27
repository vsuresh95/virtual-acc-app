#include <stdlib.h>
#include <hpthread.h>
#include <hpthread_intf.h>
#include <vam_backend.h>
#include <sched.h>
#include <string.h>

// External instance of hpthread interface
extern hpthread_intf_t intf;
// Helper function to wake up VAM, if not started already
extern void wakeup_vam();
// Running thread counter
static unsigned thread_count = 0;

void hpthread_init(hpthread_t *th, unsigned user_id) {
	th->is_active = false;
	th->user_id = user_id;
}

void hpthread_create(hpthread_t *th) {
	// Assign a thread ID
	th->id = ++thread_count;
	HIGH_DEBUG(printf("[HPTHREAD] Requested hpthread %s (ID:%d).\n", th->name, th->id);)

    // If VAM has not yet been started (i.e., interface is in vam_state_t::RESET, start one thread now)
    if (hpthread_intf_swap(VAM_RESET, VAM_WAKEUP)) {
		// Only one thread should enter here; remaining will wait for idle state
		wakeup_vam();
		hpthread_intf_set(VAM_IDLE);
    }

	// Check if the interface is IDLE. If yes, swap to BUSY. If not, block until it is
	while (!hpthread_intf_swap(VAM_IDLE, VAM_BUSY)) sched_yield();
	// Write the hpthread request to the interface
	intf.th = th;
	// Set the interface state to CREATE
    hpthread_intf_set(VAM_CREATE);
	// Block until the request is complete (interface state is DONE), then swap to IDLE
	while (!hpthread_intf_swap(VAM_DONE, VAM_IDLE)) sched_yield();
	HIGH_DEBUG(printf("[HPTHREAD] Received hpthread %s.\n", th->name);)
	th->is_active = true;
    th->th_last_move = get_counter();
}

int hpthread_join(hpthread_t *th) {
	HIGH_DEBUG(printf("[HPTHREAD] Joining hpthread %s.\n", th->name);)

	// If the interface is vam_state_t::RESET, return an error
    if (hpthread_intf_test() == VAM_RESET) return 1;

	// Check if the interface is IDLE. If yes, swap to BUSY. If not, block until it is
	while (!hpthread_intf_swap(VAM_IDLE, VAM_BUSY)) sched_yield();
	// Write the hpthread request to the interface
	intf.th = th;
	// Set the interface state to JOIN
    hpthread_intf_set(VAM_JOIN);
	// Block until the request is complete (interface state is DONE), then swap to IDLE
	while (!hpthread_intf_swap(VAM_DONE, VAM_IDLE)) sched_yield();
	HIGH_DEBUG(printf("[HPTHREAD] Join hpthread complete %s.\n", th->name);)
	th->is_active = false;
	return 0;
}

void hpthread_setargs(hpthread_t *th, hpthread_args_t *a) {
	th->args = a;
}

void hpthread_setname(hpthread_t *th, const char *n) {
	strcpy(th->name, n);
}

void hpthread_setprimitive(hpthread_t *th, hpthread_prim_t p) {
	th->prim = p;
}

void hpthread_setpriority(hpthread_t *th, unsigned p) {
	th->nprio = p;
	// If the thread is active, you need to inform VAM so the hardware can be configured
	if (th->is_active) {
		HIGH_DEBUG(printf("[HPTHREAD] Requested change of priority to %d for hpthread %s.\n", p, th->name);)
		// Check if the interface is IDLE. If yes, swap to BUSY. If not, block until it is
		while (!hpthread_intf_swap(VAM_IDLE, VAM_BUSY)) sched_yield();
		// Write the hpthread request to the interface
		intf.th = th;
		// Set the interface state to SETPRIO
		hpthread_intf_set(VAM_SETPRIO);
		// Block until the request is complete (interface state is DONE), then swap to IDLE
		while (!hpthread_intf_swap(VAM_DONE, VAM_IDLE)) sched_yield();
		HIGH_DEBUG(printf("[HPTHREAD] Change of priority to %d complete for hpthread %s.\n", p, th->name);)
	}
}

hpthread_cand_t *hpthread_query() {
	HIGH_DEBUG(printf("[HPTHREAD] Requested hpthread candidate list.\n");)

    // If VAM has not yet been started (i.e., interface is in vam_state_t::RESET, start one thread now)
    if (hpthread_intf_swap(VAM_RESET, VAM_WAKEUP)) {
		// Only one thread should enter here; remaining will wait for idle state
		wakeup_vam();
		hpthread_intf_set(VAM_IDLE);
    }

	// Check if the interface is IDLE. If yes, swap to BUSY. If not, block until it is
	while (!hpthread_intf_swap(VAM_IDLE, VAM_BUSY)) sched_yield();
	// Set the interface state to QUERY
    hpthread_intf_set(VAM_QUERY);
	// Block until the request is complete (interface state is DONE), then swap to IDLE
	while (!hpthread_intf_swap(VAM_DONE, VAM_IDLE)) sched_yield();
	HIGH_DEBUG(printf("[HPTHREAD] Received hpthread candidate list.\n");)
	// Return the empty hpthread candidate list to the caller
	return intf.list;
}

void hpthread_report() {
	HIGH_DEBUG(printf("[HPTHREAD] Requested report from VAM.\n");)
	// Check if the interface is IDLE. If yes, swap to BUSY. If not, block until it is
	while (!hpthread_intf_swap(VAM_IDLE, VAM_BUSY)) sched_yield();
	// Set the interface state to QUERY
    hpthread_intf_set(VAM_REPORT);
	// Block until the request is complete (interface state is DONE), then swap to IDLE
	while (!hpthread_intf_swap(VAM_DONE, VAM_IDLE)) sched_yield();
	HIGH_DEBUG(printf("[HPTHREAD] Report complete.\n");)	
}

// Helper function for printing hpthread primitive
const char *hpthread_get_prim_name(hpthread_prim_t p) {
    switch(p) {
        case PRIM_NONE : return (const char *) "NONE";
        case PRIM_AUDIO_FFT: return (const char *) "AUDIO_FFT";
        case PRIM_AUDIO_FIR: return (const char *) "AUDIO_FIR";
        case PRIM_AUDIO_FFI: return (const char *) "AUDIO_FFI";
        case PRIM_GEMM: return (const char *) "GEMM";
        default: return (const char *) "Unknown primitive";
    }
}
