#ifndef __VAM_ACCEL_DEFS_H__
#define __VAM_ACCEL_DEFS_H__

void VamWorker::configure_accel(void* generic_handle) {
    // Create a contiguous memory handle for the memory buf
    VirtualInst *accel_handle = (VirtualInst *) generic_handle;

    // Get the physical accelerator for this instance
    // We want to traverse the virt to phy mapping as a data flow graph
    // and perform the configurations that way. Traverse the graph and
    // ensure that flags are configured so that producers and consumers
    // communicate with each other. This must generalize to the 
    // non-composable capabilities as well, i.e., they have only one capability
    // in their comp_list. Maybe consider adding the capability again to its comp_list.
    contig_handle_t *handle = lookup_handle(accel_handle->hw_buf, NULL);

    DEBUG(printf("[VAM] Number of accelerators = %lu\n", virt_to_phy_mapping[accel_handle].size());)

    for (unsigned id = 0; id < virt_to_phy_mapping[accel_handle].size(); id++) {
        PhysicalAccel *accel = virt_to_phy_mapping[accel_handle][id];
            
        DEBUG(printf("[VAM] Beginning configuration for %s of thread %d\n", accel->devname, accel_handle->thread_id);)

        switch (accel->capab) {
            case AUDIO_FFT: {
                if (CapabilityRegistry[accel_handle->capab].composable == true && accel_handle->capab == AUDIO_FFI) {
                    AudioInst* audio_handle = (AudioInst *) generic_handle;

                    struct audio_fft_stratus_access *audio_fft_desc = new struct audio_fft_stratus_access;

                    audio_fft_desc->esp.contig = contig_to_khandle(*handle);
                    audio_fft_desc->esp.run = true;
                    audio_fft_desc->esp.coherence = ACC_COH_RECALL;
                    audio_fft_desc->spandex_conf = 0;
                    audio_fft_desc->esp.start_stop = 1;
                    audio_fft_desc->esp.p2p_store = 0;
                    audio_fft_desc->esp.p2p_nsrcs = 0;
                    audio_fft_desc->src_offset = 0;
                    audio_fft_desc->dst_offset = 0;

                    audio_fft_desc->logn_samples = audio_handle->logn_samples;
                    audio_fft_desc->do_shift = audio_handle->do_shift;
                    if (id != 0) {
                        audio_fft_desc->do_inverse = 1;
                        audio_fft_desc->prod_valid_offset = audio_handle->ConsVldOffset;
                        audio_fft_desc->prod_ready_offset = audio_handle->ConsRdyOffset;
                        audio_fft_desc->cons_valid_offset = audio_handle->acc_len + audio_handle->ConsVldOffset;
                        audio_fft_desc->cons_ready_offset = audio_handle->acc_len + audio_handle->ConsRdyOffset;
                        audio_fft_desc->input_offset = audio_handle->InputOffset;
                        audio_fft_desc->output_offset = audio_handle->acc_len + audio_handle->InputOffset;
                    } else {
                        audio_fft_desc->do_inverse = 0;
                        audio_fft_desc->prod_valid_offset = audio_handle->ProdVldOffset - audio_handle->acc_len;
                        audio_fft_desc->prod_ready_offset = audio_handle->ProdRdyOffset - audio_handle->acc_len;
                        audio_fft_desc->cons_valid_offset = audio_handle->ProdVldOffset;
                        audio_fft_desc->cons_ready_offset = audio_handle->ProdRdyOffset;
                        audio_fft_desc->input_offset = audio_handle->OutputOffset - audio_handle->acc_len;
                        audio_fft_desc->output_offset = audio_handle->OutputOffset;
                    }

                    printf("[VAM] Configuring %s for thread %d.\n", accel->devname, audio_handle->thread_id);

                    if (ioctl(accel->fd, accel->ioctl_req, audio_fft_desc)) {
                        perror("ioctl");
                        exit(EXIT_FAILURE);
                    }

                    delete audio_fft_desc;
                } else {

                }
                break;
            }
            case AUDIO_FIR: {
                if (CapabilityRegistry[accel_handle->capab].composable == true && accel_handle->capab == AUDIO_FFI) {
                    AudioInst* audio_handle = (AudioInst *) generic_handle;

                    struct audio_fir_stratus_access *audio_fir_desc = new struct audio_fir_stratus_access;

                    audio_fir_desc->esp.contig = contig_to_khandle(*handle);
                    audio_fir_desc->esp.run = true;
                    audio_fir_desc->esp.coherence = ACC_COH_RECALL;
                    audio_fir_desc->spandex_conf = 0;
                    audio_fir_desc->esp.start_stop = 1;
                    audio_fir_desc->esp.p2p_store = 0;
                    audio_fir_desc->esp.p2p_nsrcs = 0;
                    audio_fir_desc->src_offset = 0;
                    audio_fir_desc->dst_offset = 0;

                    audio_fir_desc->logn_samples = audio_handle->logn_samples;
                    audio_fir_desc->do_inverse = audio_handle->do_inverse;
                    audio_fir_desc->do_shift = audio_handle->do_shift;
                    audio_fir_desc->prod_valid_offset = audio_handle->acc_len + audio_handle->ConsVldOffset;
                    audio_fir_desc->prod_ready_offset = audio_handle->acc_len + audio_handle->ConsRdyOffset;
                    audio_fir_desc->cons_valid_offset = (2 * audio_handle->acc_len) + audio_handle->ConsVldOffset;
                    audio_fir_desc->cons_ready_offset = (2 * audio_handle->acc_len) + audio_handle->ConsRdyOffset;
                    audio_fir_desc->flt_prod_valid_offset = audio_handle->FltVldOffset;
                    audio_fir_desc->flt_prod_ready_offset = audio_handle->FltRdyOffset;
                    audio_fir_desc->input_offset = audio_handle->acc_len + audio_handle->InputOffset;
                    audio_fir_desc->output_offset = (2 * audio_handle->acc_len) + audio_handle->InputOffset;
                    audio_fir_desc->flt_input_offset = audio_handle->FltInputOffset;
                    audio_fir_desc->twd_input_offset = audio_handle->TwdInputOffset;

                    printf("[VAM] Configuring %s for thread %d.\n", accel->devname, audio_handle->thread_id);

                    if (ioctl(accel->fd, accel->ioctl_req, audio_fir_desc)) {
                        perror("ioctl");
                        exit(EXIT_FAILURE);
                    }

                    delete audio_fir_desc;
                } else {

                }
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
                audio_ffi_desc->src_offset = 0;
                audio_ffi_desc->dst_offset = 0;

                audio_ffi_desc->logn_samples = audio_handle->logn_samples;
                audio_ffi_desc->do_inverse = audio_handle->do_inverse;
                audio_ffi_desc->do_shift = audio_handle->do_shift;
                audio_ffi_desc->prod_valid_offset = audio_handle->ConsVldOffset;
                audio_ffi_desc->prod_ready_offset = audio_handle->ConsRdyOffset;
                audio_ffi_desc->cons_valid_offset = audio_handle->ProdVldOffset;
                audio_ffi_desc->cons_ready_offset = audio_handle->ProdRdyOffset;
                audio_ffi_desc->flt_prod_valid_offset = audio_handle->FltVldOffset;
                audio_ffi_desc->flt_prod_ready_offset = audio_handle->FltRdyOffset;
                audio_ffi_desc->input_offset = audio_handle->InputOffset;
                audio_ffi_desc->output_offset = audio_handle->OutputOffset;
                audio_ffi_desc->flt_input_offset = audio_handle->FltInputOffset;
                audio_ffi_desc->twd_input_offset = audio_handle->TwdInputOffset;

                printf("[VAM] Configuring %s for thread %d.\n", accel->devname, audio_handle->thread_id);

                if (ioctl(accel->fd, accel->ioctl_req, audio_ffi_desc)) {
                    perror("ioctl");
                    exit(EXIT_FAILURE);
                }

                delete audio_ffi_desc;

                break;
            }
        }
    }
}


#endif // __VAM_ACCEL_DEFS_H__
