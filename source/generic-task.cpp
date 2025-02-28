#include <iostream>
#include <generic-task.hpp>

generic_task::generic_task(unsigned thread_id_param, vam_req_intf_t *req_intf_param)
	: thread_id {thread_id_param}
	, req_intf {req_intf_param}
{
	t_start = 0;
	t_end = 0;
}

vam_code_t generic_task::get_accel(virtual_inst_t *virt_handle) {
	req_intf_state_t expected;

	// Atomically check the interface state is IDLE, and if yes, update to ONGOING.
	do {
		expected = IDLE;
	} while (!req_intf->intf_state.compare_exchange_strong(expected, ONGOING, std::memory_order_seq_cst));
	
	DEBUG(printf("[generic_task] Requested virtual instance for thread %d from VAM.\n", virt_handle->thread_id);)

	// Submit a new task (this was registered by the upstream task that got the lock)
	req_intf->accel_handle.store(virt_handle, std::memory_order_seq_cst);

	// Atomically check the interface state is DONE, and if yes, update to IDLE.
	do {
		expected = DONE;
	} while (!req_intf->intf_state.compare_exchange_strong(expected, IDLE, std::memory_order_seq_cst));
	
	DEBUG(printf("[generic_task] Received virtual instance for thread %d from VAM.\n", virt_handle->thread_id);)

	// For now, the get accel is not returning anything, but eventually you want to return more useful info.
	return req_intf->rsp_code;
}

void generic_task::start_counter() {
	asm volatile (
		"li t0, 0;"
		"csrr t0, cycle;"
		"mv %0, t0"
		: "=r" (t_start)
		:
		: "t0"
	);
}

uint64_t generic_task::end_counter() {
	asm volatile (
		"li t0, 0;"
		"csrr t0, cycle;"
		"mv %0, t0"
		: "=r" (t_end)
		:
		: "t0"
	);

	return (t_end - t_start);
}