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
	// Try to acquire the mutex on the interface. If not, wait.
	pthread_mutex_lock(&(req_intf->intf_mutex));
	
	// Wait until the interface is free
	while (req_intf->task_available) {
		pthread_cond_wait(&(req_intf->req_ready), &(req_intf->intf_mutex));
	}

	DEBUG(printf("[GenericTask] Requested virtual instance for %d from VAM.\n", virt_handle->thread_id);)
        
	// Submit a new task (this was registered by the upstream task that got the lock)
	req_intf->accel_handle = virt_handle;

	req_intf->task_available = true;
	req_intf->task_completed = false;
	
	// Notify server and wait for completion
	pthread_cond_signal(&(req_intf->req_ready));
	while (!(req_intf->task_completed)) {
		pthread_cond_wait(&(req_intf->req_done), &(req_intf->intf_mutex));
	}
	
	pthread_mutex_unlock(&(req_intf->intf_mutex));

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