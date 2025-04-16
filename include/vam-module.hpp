#ifndef __VAM_MODULE_H__
#define __VAM_MODULE_H__

#include <vam-hpthread.hpp>
#include <vam-accel-def.hpp>

#include <audio-helper.hpp>

class vam_worker {

    public:
			// VAM interface for providing accel allocations
			hpthread_intf_t *hpthread_intf;

            vam_worker(hpthread_intf_t *intf) { hpthread_intf = intf; }

            // Static version of run method
			static void* run(void* task_obj) {
				static_cast<vam_worker *>(task_obj)->run();
				return 0;
			}

            // List of physical devices in the system
            std::vector<physical_accel_t> accel_list;

            // Mapping from pyhsical to virtual instances (DFG graphs)
            std::unordered_map<physical_accel_t *, hpthread_routine_t *> phy_to_virt_mapping;
            std::unordered_map<hpthread_routine_t *, std::unordered_map<df_node_t *, physical_accel_t *>> virt_to_phy_mapping;

            // List of pthreads that we can launch a SW kernel from
            std::vector<pthread_t> cpu_thread_list;

            // Probe the ESP system for available physical accelerators
            void probe_accel();

            // Search for accelerator candidates for each node in a DFG
            bool search_accel(hpthread_routine_t *routine);

            // Once accelerator candidates are identified, configure each accelerator
            void configure_accel(df_node_t *node, physical_accel_t *accel);

            // Similar as accelerator counterpart; this function launches a pthread.
            void configure_cpu(df_node_t *node, physical_accel_t *accel);

            // Main run method -- which runs forever
            void run();

            // TODO: you need a query function in the VAM that applications would use to
            // get a list of devices available in the system. Like the /etc/cpuinfo app in Linux.
};


#endif // __VAM_MODULE_H__