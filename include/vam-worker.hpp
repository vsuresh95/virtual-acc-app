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
            std::unordered_map<Capability, CapabilityDef> CapabilityRegistry = {
                {AUDIO_FFT, {false, {}}},
                {AUDIO_FIR, {false, {}}},
                {AUDIO_FFI, {true, {AUDIO_FFT, AUDIO_FIR, AUDIO_FFT}}},
            };

            std::vector<PhysicalAccel> accel_list;

            std::unordered_map<PhysicalAccel *, VirtualInst *> phy_to_virt_mapping;
            std::unordered_map<VirtualInst *, PhysicalAccel *> virt_to_phy_mapping;

            // Probe the ESP system for available physical accelerators
            void probe_accel();

            // Probe the ESP system for available physical accelerators
            VAMcode search_accel(void* generic_handle);

            void configure_accel(void* generic_handle);

            // Main run method -- which runs forever
            void run();
};


#endif // __VIRT_ACC_MONITOR_H__