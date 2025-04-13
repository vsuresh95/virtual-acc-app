#ifndef __AUDIO_HELPER_H__
#define __AUDIO_HELPER_H__

typedef unsigned token_t;
typedef uint64_t token64_t;

#include <audio_fft_stratus.h>
#include <audio_fir_stratus.h>
#include <audio_ffi_stratus.h>

struct ffi_params_t : public node_params_t {
	unsigned logn_samples;
	unsigned do_inverse;
	unsigned do_shift;

    void dump() {
        printf("\tlogn_samples = %d\n", logn_samples);
        printf("\tdo_inverse = %d\n", do_inverse);
        printf("\tdo_shift = %d\n", do_shift);

        node_params_t::dump();
    } 
};

struct fft_params_t : public node_params_t {
	unsigned logn_samples;
	unsigned do_inverse;
	unsigned do_shift;

    void dump() {
        printf("\tlogn_samples = %d\n", logn_samples);
        printf("\tdo_inverse = %d\n", do_inverse);
        printf("\tdo_shift = %d\n", do_shift);

        node_params_t::dump();
    } 
};

struct fir_params_t : public node_params_t {
	unsigned logn_samples;

    void dump() {
        printf("\tlogn_samples = %d\n", logn_samples);

        node_params_t::dump();
    } 
};

// Device-dependent configuration functions
static void audio_fft_access_cfg(df_node_t *node, esp_access *generic_esp_access) {
    fft_params_t *params = (fft_params_t *) node->get_params();
    struct audio_fft_stratus_access *audio_fft_desc = (struct audio_fft_stratus_access *) generic_esp_access;

    audio_fft_desc->logn_samples = params->logn_samples;
    audio_fft_desc->do_shift = params->do_shift;
    audio_fft_desc->do_inverse = params->do_inverse;

    // Get the queue base from the in/out edges of the FFT node
    audio_fft_desc->input_queue_base = node->in_edges[0]->data->base / sizeof(token_t);
    audio_fft_desc->output_queue_base = node->out_edges[0]->data->base / sizeof(token_t);
}

static void audio_fir_access_cfg(df_node_t *node, esp_access *generic_esp_access) {
    fir_params_t *params = (fir_params_t *) node->get_params();
    struct audio_fir_stratus_access *audio_fir_desc = (struct audio_fir_stratus_access *) generic_esp_access;

    audio_fir_desc->logn_samples = params->logn_samples;

    // Get the queue base from the in/out edges of the FIR node
    audio_fir_desc->input_queue_base = node->in_edges[0]->data->base / sizeof(token_t);
    audio_fir_desc->filter_queue_base = node->in_edges[1]->data->base / sizeof(token_t);
    audio_fir_desc->output_queue_base = node->out_edges[0]->data->base / sizeof(token_t);
}

static void audio_ffi_access_cfg(df_node_t *node, esp_access *generic_esp_access) {
    ffi_params_t *params = (ffi_params_t *) node->get_params();
    struct audio_ffi_stratus_access *audio_ffi_desc = (struct audio_ffi_stratus_access *) generic_esp_access;

    audio_ffi_desc->logn_samples = params->logn_samples;
    audio_ffi_desc->do_shift = params->do_shift;
    audio_ffi_desc->do_inverse = params->do_inverse;

    // Get the queue base from the in/out edges of the FFT node
    audio_ffi_desc->input_queue_base = node->in_edges[0]->data->base / sizeof(token_t);
    audio_ffi_desc->filter_queue_base = node->in_edges[1]->data->base / sizeof(token_t);
    audio_ffi_desc->output_queue_base = node->out_edges[0]->data->base / sizeof(token_t);
}

struct time_helper {
    // Time variables
    uint64_t t_start;
    uint64_t t_end;
    uint64_t t_diff;

    void start_counter() {
        asm volatile (
            "li t0, 0;"
            "csrr t0, cycle;"
            "mv %0, t0"
            : "=r" (t_start)
            :
            : "t0"
        );
    }

    void end_counter() {
        asm volatile (
            "li t0, 0;"
            "csrr t0, cycle;"
            "mv %0, t0"
            : "=r" (t_end)
            :
            : "t0"
        );

        t_diff = t_end - t_start;
    }

    uint64_t get_time() { return t_diff; }
};

#endif /* __AUDIO_HELPER_H__ */
