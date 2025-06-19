#ifndef __AUDIO_HELPER_H__
#define __AUDIO_HELPER_H__

typedef float token_t;
typedef uint64_t token64_t;

#include <audio_fft_stratus.h>
#include <audio_fir_stratus.h>
#include <audio_ffi_stratus.h>

struct ffi_params_t : public node_params_t {
	unsigned logn_samples;
	unsigned do_shift;

    void dump() {
        printf("\tlogn_samples = %d\n", logn_samples);
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

extern std::map<primitive_t, std::map<unsigned, unsigned>> audio_offline_prof;

struct time_helper {
    // Time variables
    uint64_t t_start;
    uint64_t t_end;
    uint64_t t_diff;
    uint64_t t_total;

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
        t_total += t_diff;
    }

    void lap_counter() {
        asm volatile (
            "li t0, 0;"
            "csrr t0, cycle;"
            "mv %0, t0"
            : "=r" (t_end)
            :
            : "t0"
        );

        t_total = t_end - t_start;
    }

    uint64_t get_diff() { return t_diff; }
    uint64_t get_total() { return t_total; }
    void reset_total() { t_total = 0; }

    time_helper() { t_total = 0; }
};

// Software implementations
void *audio_fft_sw_impl (void *args);
void *audio_fir_sw_impl (void *args);
void *audio_ffi_sw_impl (void *args);
void fft_comp(token_t *in_data, token_t *out_data, unsigned int logn, int sign, bool rev);
void fir_comp(unsigned len, token_t *in_data, token_t *out_data, token_t *filters, token_t *twiddles, token_t *tmpbuf);
unsigned int fft_rev(unsigned int v);
void fft_bit_reverse(float *w, unsigned int n, unsigned int bits);

// Data structures used for communicating between main thread and worker threads
typedef enum { EMPTY = 0, END = 1 } audio_cmd_t;

typedef struct {
    audio_cmd_t cmd;
    unsigned thread_id;
    std::atomic<bool> valid_cmd;
} audio_thread_intf_t;

// Helper function for main thread to send commands to worker threads
void send_thread_signal(audio_cmd_t c, unsigned t);

// Helper function for worker threads to receive commands from main thread
audio_cmd_t recv_thread_signal(unsigned t);

#endif /* __AUDIO_HELPER_H__ */
