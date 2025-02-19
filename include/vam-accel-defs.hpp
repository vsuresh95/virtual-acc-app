#ifndef __VAM_ACCEL_DEFS_H__
#define __VAM_ACCEL_DEFS_H__

void VamWorker::configure_accel(void* generic_handle) {
    // Create a contiguous memory handle for the memory buf
    VirtualInst *accel_handle = (VirtualInst *) generic_handle;

    // Get the physical accelerator for this instance
    PhysicalAccel *accel = virt_to_phy_mapping[accel_handle];

    contig_handle_t *handle = lookup_handle(accel_handle->hw_buf, NULL);

    switch (accel->capab) {
        case AUDIO_FFT: {
            
            break;
        }
        case AUDIO_FIR: {

            break;
        }
        case AUDIO_FFI: {
            AudioInst* audio_handle = (AudioInst *) generic_handle;

            struct audio_ffi_stratus_access *audio_ffi_desc = new struct audio_ffi_stratus_access;

            audio_ffi_desc->esp.contig = contig_to_khandle(*handle);
            audio_ffi_desc->esp.run = true;
            audio_ffi_desc->esp.coherence = ACC_COH_RECALL;
            audio_ffi_desc->spandex_conf = 0;
            audio_ffi_desc->esp.start_stop = 1;
            audio_ffi_desc->esp.p2p_store = 0;
            audio_ffi_desc->esp.p2p_nsrcs = 0;

            audio_ffi_desc->logn_samples = audio_handle->logn_samples;
            audio_ffi_desc->do_inverse = audio_handle->do_inverse;
            audio_ffi_desc->prod_valid_offset = audio_handle->ConsRdyOffset;
            audio_ffi_desc->prod_ready_offset = audio_handle->ConsVldOffset;
            audio_ffi_desc->cons_valid_offset = audio_handle->ProdVldOffset;
            audio_ffi_desc->cons_ready_offset = audio_handle->ProdRdyOffset;
            audio_ffi_desc->flt_prod_valid_offset = audio_handle->FltVldOffset;
            audio_ffi_desc->flt_prod_ready_offset = audio_handle->FltRdyOffset;
            audio_ffi_desc->input_offset = audio_handle->InputOffset;
            audio_ffi_desc->output_offset = audio_handle->OutputOffset;
            audio_ffi_desc->flt_input_offset = audio_handle->FltInputOffset;
            audio_ffi_desc->twd_input_offset = audio_handle->TwdInputOffset;

            printf("[VAM] Configuring audio_ffi accelerator for thread %d.\n", audio_handle->thread_id);

            if (ioctl(accel->fd, accel->ioctl_req, audio_ffi_desc)) {
                perror("ioctl");
                exit(EXIT_FAILURE);
            }

            delete audio_ffi_desc;

            break;
        }
    }
}


#endif // __VAM_ACCEL_DEFS_H__
