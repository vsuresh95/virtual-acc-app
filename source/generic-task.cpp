#include <iostream>
#include <generic-task.hpp>

GenericTask::GenericTask(unsigned thread_id_param, VAMReqIntf *req_intf_param)
	: thread_id {thread_id_param}
	, req_intf {req_intf_param}
{
	t_start = 0;
	t_end = 0;
}

VAMcode GenericTask::get_accel(VirtualInst *virt_handle) {
	ReqIntfState expected;

	// Atomically check the interface state is IDLE, and if yes, update to ONGOING.
	do {
		expected = IDLE;
	} while (!req_intf->intf_state.compare_exchange_strong(expected, ONGOING));
	
	DEBUG(printf("[GenericTask] Requested virtual instance for thread %d from VAM.\n", virt_handle->thread_id);)

	// Submit a new task (this was registered by the upstream task that got the lock)
	req_intf->accel_handle = virt_handle;

	// Atomically check the interface state is DONE, and if yes, update to IDLE.
	do {
		expected = DONE;
	} while (!req_intf->intf_state.compare_exchange_strong(expected, IDLE));
	
	DEBUG(printf("[GenericTask] Received virtual instance for thread %d from VAM.\n", virt_handle->thread_id);)

	// For now, the get accel is not returning anything, but eventually you want to return more useful info.
	return req_intf->rsp_code;
}

void GenericTask::start_counter() {
	asm volatile (
		"li t0, 0;"
		"csrr t0, cycle;"
		"mv %0, t0"
		: "=r" (t_start)
		:
		: "t0"
	);
}

uint64_t GenericTask::end_counter() {
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