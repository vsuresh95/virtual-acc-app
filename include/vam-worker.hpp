#ifndef __VIRT_ACC_MONITOR_H__
#define __VIRT_ACC_MONITOR_H__

#include <vam-req-intf.hpp>
#include <vam-helper.hpp>

#include "dirent.h"
#include "fnmatch.h"
#include "stdio.h"

class VamWorker {

    public:
			// VAM interface for providing accel allocations
			VAMReqIntf *req_intf;

            VamWorker(VAMReqIntf *req_intf_param);

            // Static version of run method
			static void* run(void* task_obj) {
				static_cast<VamWorker *>(task_obj)->run();
			}

    private:
            PhysicalAccel *accel_list;

            // Wait for virtual accelerator allocation requests and allocate
            void put_accel (VAMReqIntf *vam_intf);

            // Probe the ESP system for available physical accelerators
            void probe_accel (PhysicalAccel *accel_list);

            // Main run method -- which runs forever
            void run ();
};


#endif // __VIRT_ACC_MONITOR_H__