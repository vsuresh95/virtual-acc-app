#ifndef __VIRT_ACC_MONITOR_H__
#define __VIRT_ACC_MONITOR_H__

#include <vam-req-intf.hpp>
#include <vam-helper.hpp>

#include <dirent.h>
#include <fnmatch.h>
#include <stdio.h>
#include <cstring>
#include <thread>

class VamWorker {

    public:
			// VAM interface for providing accel allocations
			VAMReqIntf *req_intf;

            VamWorker(VAMReqIntf *req_intf_param);

            // Static version of run method
			static void* run(void* task_obj) {
				static_cast<VamWorker *>(task_obj)->run();
				return 0;
			}

    private:
            // Registry of capabilities, including descriptions for composable ones
            // std::unordered_map<Capability, std::vector<CapabilityDef>> CapabilityRegistry;
            std::unordered_map<Capability, CapabilityDef> CapabilityRegistry = {
                {AUDIO_FFT, {false, {}}},
                {AUDIO_FIR, {false, {}}},
                {AUDIO_FFI, {true, {AUDIO_FFT, AUDIO_FIR, AUDIO_FFT}}},
            };

            // List of physical devices in the system
            std::vector<PhysicalAccel> accel_list;

            // Mappings between virtual instances and physical devices
            std::unordered_map<PhysicalAccel *, VirtualInst *> phy_to_virt_mapping;
            std::unordered_map<VirtualInst *, std::vector<PhysicalAccel *>> virt_to_phy_mapping;

            // // Probe the ESP system for available physical accelerators
            // void register_capabilities();

            // Probe the ESP system for available physical accelerators
            void probe_accel();

            // Probe the ESP system for available physical accelerators
            VAMcode search_accel(void* generic_handle);

            void configure_accel(void* generic_handle);

            // Main run method -- which runs forever
            void run();

            // TODO: you need a query function in the VAM that applications would use to
            // get a list of devices available in the system. Like the /etc/cpuinfo app in Linux.
};


#endif // __VIRT_ACC_MONITOR_H__