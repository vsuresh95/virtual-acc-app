#ifndef __VIRT_ACC_MONITOR_H__
#define __VIRT_ACC_MONITOR_H__

#include <vam-req-intf.hpp>
#include <vam-helper.hpp>

#include <dirent.h>
#include <fnmatch.h>
#include <stdio.h>
#include <cstring>
#include <thread>

class vam_worker {

    public:
			// VAM interface for providing accel allocations
			vam_req_intf_t *req_intf;

            vam_worker(vam_req_intf_t *req_intf_param);

            // Static version of run method
			static void* run(void* task_obj) {
				static_cast<vam_worker *>(task_obj)->run();
				return 0;
			}

    private:
            // Registry of capabilities, including descriptions for composable ones
            // std::unordered_map<capability_t, std::vector<capability_def_t>> capability_registry;
            std::unordered_map<capability_t, capability_def_t> capability_registry = {
                {AUDIO_FFT, {false, {}}},
                {AUDIO_FIR, {false, {}}},
                {AUDIO_FFI, {true, {AUDIO_FFT, AUDIO_FIR, AUDIO_FFT}}},
            };

            // List of physical devices in the system
            std::vector<physical_accel_t> accel_list;

            // Mappings between virtual instances and physical devices
            std::unordered_map<physical_accel_t *, virtual_inst_t *> phy_to_virt_mapping;
            std::unordered_map<virtual_inst_t *, std::vector<physical_accel_t *>> virt_to_phy_mapping;

            // // Probe the ESP system for available physical accelerators
            // void register_capabilities();

            // Probe the ESP system for available physical accelerators
            void probe_accel();

            // Probe the ESP system for available physical accelerators
            vam_code_t search_accel(void* generic_handle);

            void configure_accel(void* generic_handle);

            // Main run method -- which runs forever
            void run();

            // TODO: you need a query function in the VAM that applications would use to
            // get a list of devices available in the system. Like the /etc/cpuinfo app in Linux.
};


#endif // __VIRT_ACC_MONITOR_H__