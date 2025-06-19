#include <hpthread.h>
#include <hpthread_intf.h>
#include <vam_backend.h>

// External instance of hpthread interface
extern hpthread_intf_t intf;

// Helper function to wake up VAM, if not started already
extern void wakeup_vam();

// API for user to create a hpthread
int hpthread_t::create(
    void *(*start_routine) (void *),
	void *arg) {

	DEBUG(printf("[HPTHREAD] Requested hpthread for %s.\n", attr->name);)

    // If VAM has not yet been started (i.e., interface is in VAM_RESET, start one thread now)
	if (intf.swap(VAM_RESET, VAM_WAKEUP)) {
		// Only one thread should enter here; remaining will wait for idle state
		wakeup_vam();
		intf.set(VAM_IDLE);
    }

	// Check if the interface is IDLE. If yes, swap to CREATE. If not, block until it is
	while (!intf.swap(VAM_IDLE, VAM_BUSY)) sched_yield();

	// Write the hpthread request to the interface
	intf.th = this;

	// Set the interface state to CREATE
	intf.set(VAM_CREATE);

	// Block until the request is complete (interface state is DONE), then swap to IDLE
	while (!intf.swap(VAM_DONE, VAM_IDLE)) sched_yield();

	DEBUG(printf("[HPTHREAD] Received hpthread for %s.\n", attr->name);)

	return 0;
}

// API for user to join a hpthread
int hpthread_t::join() {
	DEBUG(printf("[HPTHREAD] Joining hpthread for %s.\n", attr->name);)

	// If the interface is VAM_RESET, return an error
    if (intf.test() == VAM_RESET) return 1;

	// Check if the interface is IDLE. If yes, swap to CREATE. If not, block until it is
	while (!intf.swap(VAM_IDLE, VAM_BUSY)) sched_yield();

	// Write the hpthread request to the interface
	intf.th = this;

	// Set the interface state to JOIN
	intf.set(VAM_JOIN);

	// Block until the request is complete (interface state is DONE), then swap to IDLE
	while (!intf.swap(VAM_DONE, VAM_IDLE)) sched_yield();

	DEBUG(printf("[HPTHREAD] Join hpthread complete for %s.\n", attr->name);)

	return 0;
}

// API for initializing hpthread_attr
void hpthread_t::attr_init() {
	attr = new hpthread_attr_t;
	// Set attribute fields to default values
	std::strcpy(attr->name, "No name");
	attr->prim = NONE;
	attr->prio = 1; // Lowest priority
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
	attr->prio = p;
}
