#include <hpthread.h>
#include <hpthread_intf.h>
#include <vam_backend.h>

// External instance of hpthread interface
extern hpthread_intf_t intf;

// Helper function to wake up VAM, if not started already
extern void wakeup_vam();

// Running thread counter
static unsigned thread_count = 0;

// API for user to create a hpthread
int hpthread_t::create() {
	// Assign a thread ID
	this->id = thread_count++;

	HIGH_DEBUG(printf("[HPTHREAD] Requested hpthread %s (ID:%d).\n", attr->name, this->id);)

    // If VAM has not yet been started (i.e., interface is in vam_state_t::RESET, start one thread now)
	if (intf.swap(vam_state_t::RESET, vam_state_t::WAKEUP)) {
		// Only one thread should enter here; remaining will wait for idle state
		wakeup_vam();
		intf.set(vam_state_t::IDLE);
    }

	// Check if the interface is IDLE. If yes, swap to BUSY. If not, block until it is
	while (!intf.swap(vam_state_t::IDLE, vam_state_t::BUSY)) sched_yield();

	// Write the hpthread request to the interface
	intf.th = this;

	// Set the interface state to CREATE
	intf.set(vam_state_t::CREATE);

	// Block until the request is complete (interface state is DONE), then swap to IDLE
	while (!intf.swap(vam_state_t::DONE, vam_state_t::IDLE)) sched_yield();

	// Set hpthread as active
	active = true;

	HIGH_DEBUG(printf("[HPTHREAD] Received hpthread %s.\n", attr->name);)

	return 0;
}

// API for user to join a hpthread
int hpthread_t::join() {
	HIGH_DEBUG(printf("[HPTHREAD] Joining hpthread %s.\n", attr->name);)

	// If the interface is vam_state_t::RESET, return an error
    if (intf.test() == vam_state_t::RESET) return 1;

	// Check if the interface is IDLE. If yes, swap to BUSY. If not, block until it is
	while (!intf.swap(vam_state_t::IDLE, vam_state_t::BUSY)) sched_yield();

	// Write the hpthread request to the interface
	intf.th = this;

	// Set the interface state to JOIN
	intf.set(vam_state_t::JOIN);

	// Block until the request is complete (interface state is DONE), then swap to IDLE
	while (!intf.swap(vam_state_t::DONE, vam_state_t::IDLE)) sched_yield();

	// Set hpthread as inactive
	active = false;

	HIGH_DEBUG(printf("[HPTHREAD] Join hpthread complete %s.\n", attr->name);)

	return 0;
}

// API for setting the start routine
void hpthread_t::setroutine(void *(*s) (void *)) {
	start_routine = s;
}

// API for setting the arguments for the routine
void hpthread_t::setargs(hpthread_args *a) {
	args = a;
}

// API for initializing hpthread_attr
void hpthread_t::attr_init() {
	attr = new hpthread_attr_t;
	// Set attribute fields to default values
	std::strcpy(attr->name, "No name");
	attr->prim = hpthread_prim_t::NONE;
	attr->nprio = 1; // Lowest priority
	assigned_load = 0;
	active_load = 0;
	active = false;
}

// API for setting the primitive computation for this hpthread
void hpthread_t::attr_setname(const char *n) {
	std::strcpy(attr->name, n);
}

// API for setting the primitive computation for this hpthread
void hpthread_t::attr_setprimitive(hpthread_prim_t p) {
	attr->prim = p;
}

// API for setting the priority for this hpthread
void hpthread_t::attr_setpriority(unsigned p) {
	attr->nprio = p;
	// If the thread is active, you need to inform VAM so the hardware can be configured
	if (active) {	
		HIGH_DEBUG(printf("[HPTHREAD] Requested change of priority to %d for hpthread %s.\n", p, attr->name);)

		// Check if the interface is IDLE. If yes, swap to BUSY. If not, block until it is
		while (!intf.swap(vam_state_t::IDLE, vam_state_t::BUSY)) sched_yield();

		// Write the hpthread request to the interface
		intf.th = this;

		// Set the interface state to SETPRIO
		intf.set(vam_state_t::SETPRIO);

		// Block until the request is complete (interface state is DONE), then swap to IDLE
		while (!intf.swap(vam_state_t::DONE, vam_state_t::IDLE)) sched_yield();

		HIGH_DEBUG(printf("[HPTHREAD] Change of priority to %d complete for hpthread %s.\n", p, attr->name);)	
	}
}

// Helper function for printing hpthread primitive
// -- declared in hpthread_primitives.h
const char *get_prim_name(hpthread_prim_t p) {
    switch(p) {
        case hpthread_prim_t::NONE : return (const char *) "NONE";
        case hpthread_prim_t::AUDIO_FFT: return (const char *) "AUDIO_FFT";
        case hpthread_prim_t::AUDIO_FIR: return (const char *) "AUDIO_FIR";
        case hpthread_prim_t::AUDIO_FFI: return (const char *) "AUDIO_FFI";
        case hpthread_prim_t::GEMM: return (const char *) "GEMM";
        default: return (const char *) "Unknown primitive";
    }
}

// API for reporting the utilization of current reporting period
void hpthread_t::report() {
	HIGH_DEBUG(printf("[HPTHREAD] Requested report from VAM.\n");)

	// Check if the interface is IDLE. If yes, swap to CREATE. If not, block until it is
	while (!intf.swap(vam_state_t::IDLE, vam_state_t::BUSY)) sched_yield();

	// Set the interface state to CREATE
	intf.set(vam_state_t::REPORT);

	// Block until the request is complete (interface state is DONE), then swap to IDLE
	while (!intf.swap(vam_state_t::DONE, vam_state_t::IDLE)) sched_yield();

	HIGH_DEBUG(printf("[HPTHREAD] Report complete.\n");)	
}
