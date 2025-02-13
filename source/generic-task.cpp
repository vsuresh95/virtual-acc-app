#include <iostream>
#include <generic-task.hpp>

GenericTask::GenericTask(unsigned thread_id_param, VAMReqIntf *req_intf_param)
	: thread_id {thread_id_param}
	, req_intf {req_intf_param}
{
	t_start = 0;
	t_end = 0;
}

VAMcode GenericTask::get_accel() {
	// Try to acquire the mutex on the interface. If not, wait.
	pthread_mutex_lock(&(this->req_intf->intf_mutex));

	// If lock is acquired, update the param_list and clear req_empty
	// vam_intf->param_list = param_list;
	std::atomic_flag_clear(&(this->req_intf->req_empty));

	// Wait for VAM by checking rsp_empty, and set if high
	while(std::atomic_flag_test_and_set(&(this->req_intf->rsp_empty)) != false);

	// Release the mutex and exit
	pthread_mutex_unlock(&(this->req_intf->intf_mutex));

	// For now, the get accel is not returning anything, but eventually you want to return more useful info.
	return this->req_intf->rsp_code;
}

void GenericTask::start_counter() {
	asm volatile (
		"li t0, 0;"
		"csrr t0, cycle;"
		"mv %0, t0"
		: "=r" (this->t_start)
		:
		: "t0"
	);
}

uint64_t GenericTask::end_counter() {
	asm volatile (
		"li t0, 0;"
		"csrr t0, cycle;"
		"mv %0, t0"
		: "=r" (this->t_end)
		:
		: "t0"
	);

	return (this->t_end - this->t_start);
}