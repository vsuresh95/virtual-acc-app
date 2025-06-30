#include <vam-hpthread.hpp>
#include <audio-helper.hpp>
#include <climits>
#include <cmath>

// Main run loops for SW implementations
void *audio_fft_sw_impl (void *args) {
    printf("[AUDIO FFT SW] Started new thread!\n");

    df_node_t *fft_node = (df_node_t *) args;

    // read node parameters and queue pointers
    mem_queue_t *input_queue = fft_node->in_edges[0]->data;
    mem_queue_t *output_queue = fft_node->out_edges[0]->data;

    fft_params_t *params = (fft_params_t *) fft_node->get_params();

    while(1) {
		DEBUG(printf("[AUDIO FFT SW] Starting new iteration for %s.\n", fft_node->get_root()->get_name());)

        // TODO: since we don't have ping pong buffers, we cannot
        // run allow a producer task to run in parallel.
        // Wait for a new task from producer queue and for consumer queue to be not full
        while(!input_queue->is_full());
        while(output_queue->is_full());

        // Perform FFT
        fft_comp((token_t *) input_queue->get_payload_base(),
                    (token_t *) output_queue->get_payload_base(),
                    params->logn_samples,
                    params->do_inverse,
                    params->do_shift
                );

        // Once the function is complete, dequeue and enqueue.
        input_queue->dequeue();
        output_queue->enqueue();
    }
}

void *audio_fir_sw_impl (void *args) {
    printf("[AUDIO FIR SW] Started new thread!\n");

    df_node_t *fir_node = (df_node_t *) args;

    // read node parameters and queue pointers
    mem_queue_t *input_queue = fir_node->in_edges[0]->data;
    mem_queue_t *output_queue = fir_node->out_edges[0]->data;
    mem_queue_t *filters_queue = fir_node->in_edges[1]->data;

    fir_params_t *params = (fir_params_t *) fir_node->get_params();

    token_t *twiddles = ((token_t *) filters_queue->get_payload_base()) + 2 * ((1 << params->logn_samples) + 1);
    token_t *tmpbuf = new token_t[2 * ((1 << params->logn_samples) + 1)];

    while(1) {
		DEBUG(printf("[AUDIO FIR SW] Starting new iteration for %s.\n", fir_node->get_root()->get_name());)

        // Wait for a new task from producer queue and for consumer queue to be not full
        while(!input_queue->is_full());
        while(!filters_queue->is_full());
        while(output_queue->is_full());

        // Perform FIR
        fir_comp(1 << params->logn_samples,
                    (token_t *) input_queue->get_payload_base(),
                    (token_t *) output_queue->get_payload_base(),
                    (token_t *) filters_queue->get_payload_base(),
                    twiddles,
                    tmpbuf
                );

        // Once the function is complete, dequeue and enqueue.
        input_queue->dequeue();
        filters_queue->dequeue();
        output_queue->enqueue();
    }

    delete tmpbuf;
}

void *audio_ffi_sw_impl (void *args) {
    printf("[AUDIO FFI SW] Started new thread!\n");

    df_node_t *ffi_node = (df_node_t *) args;

    // read node parameters and queue pointers
    mem_queue_t *input_queue = ffi_node->in_edges[0]->data;
    mem_queue_t *output_queue = ffi_node->out_edges[0]->data;
    mem_queue_t *filters_queue = ffi_node->in_edges[1]->data;

    ffi_params_t *params = (ffi_params_t *) ffi_node->get_params();

    token_t *twiddles = ((token_t *) filters_queue->get_payload_base()) + 2 * ((1 << params->logn_samples) + 1);
    token_t *tmpbuf = new token_t[2 * ((1 << params->logn_samples) + 1)];

    while(1) {
		DEBUG(printf("[AUDIO FFI SW] Starting new iteration for %s.\n", ffi_node->get_root()->get_name());)

        // Wait for a new task from producer queue and for consumer queue to be not full
        while(!input_queue->is_full());
        while(!filters_queue->is_full());
        while(output_queue->is_full());

        // Perform FFT
        fft_comp((token_t *) input_queue->get_payload_base(),
                    (token_t *) output_queue->get_payload_base(),
                    params->logn_samples,
                    0 /* do_inverse */,
                    params->do_shift
                );

        // Perform FIR
        fir_comp(1 << params->logn_samples,
                    (token_t *) input_queue->get_payload_base(),
                    (token_t *) output_queue->get_payload_base(),
                    (token_t *) filters_queue->get_payload_base(),
                    twiddles,
                    tmpbuf
                );

        // Perform IFFT
        fft_comp((token_t *) input_queue->get_payload_base(),
                    (token_t *) output_queue->get_payload_base(),
                    params->logn_samples,
                    1 /* do_inverse */,
                    params->do_shift
                );

        // Once the function is complete, dequeue and enqueue.
        input_queue->dequeue();
        filters_queue->dequeue();
        output_queue->enqueue();
    }

    delete tmpbuf;
}

// Software implementations of different kernels
void fft_comp(token_t *in_data, token_t *out_data, unsigned int logn, int sign, bool rev)
{
	unsigned int transform_length;
	unsigned int a, b, i, j, bit;
	token_t theta, t_real, t_imag, w_real, w_imag, s, t, s2, z_real, z_imag;

    unsigned int n = 2 * (1 << logn);

	if (rev)
		fft_bit_reverse(in_data, n, logn);

	transform_length = 1;

	/* calculation */
	for (bit = 0; bit < logn; bit++) {
		w_real = 1.0;
		w_imag = 0.0;

		theta = 1.0 * sign * M_PI / (token_t) transform_length;

		s = sin(theta);
		t = sin(0.5 * theta);
		s2 = 2.0 * t * t;

		for (a = 0; a < transform_length; a++) {
			for (b = 0; b < n; b += 2 * transform_length) {
				i = b + a;
				j = b + a + transform_length;

				z_real = in_data[2 * j];
				z_imag = in_data[2 * j + 1];

				t_real = w_real * z_real - w_imag * z_imag;
				t_imag = w_real * z_imag + w_imag * z_real;

				/* write the result */
				in_data[2*j]	= in_data[2*i] - t_real;
				in_data[2*j + 1]	= in_data[2*i + 1] - t_imag;
				in_data[2*i]	+= t_real;
				in_data[2*i + 1]	+= t_imag;
			}

			/* adjust w */
			t_real = w_real - (s * w_imag + s2 * w_real);
			t_imag = w_imag + (s * w_real - s2 * w_imag);
			w_real = t_real;
			w_imag = t_imag;

		}
		transform_length *= 2;
	}

    token64_t *in = (token64_t *) in_data;
    token64_t *out = (token64_t *) out_data;

    for (i = 0; i < n/2; i++) {
        out[i] = in[i];
    }
}

typedef struct {
    token_t r;
    token_t i;
} cpx_num;

#define S_MUL(a,b) ( (a)*(b) )
#define C_MUL(m,a,b) \
    do{ (m).r = (a).r*(b).r - (a).i*(b).i;\
        (m).i = (a).r*(b).i + (a).i*(b).r; }while(0)
#define C_FIXDIV(c,div) /* NOOP */
#define C_MULBYSCALAR( c, s ) \
    do{ (c).r *= (s);\
        (c).i *= (s); }while(0)

#define  C_ADD( res, a,b)\
    do { \
        (res).r=(a).r+(b).r;  (res).i=(a).i+(b).i; \
    }while(0)
#define  C_SUB( res, a,b)\
    do { \
        (res).r=(a).r-(b).r;  (res).i=(a).i-(b).i; \
    }while(0)

#define HALF_OF(x) ((x)*.5)


void fir_comp(unsigned len, token_t *in_data, token_t *out_data, token_t *filters, token_t *twiddles, token_t *_tmpbuf)
{
	unsigned j;

    cpx_num fpnk, fpk, f1k, f2k, tw, tdc;
    cpx_num fk, fnkc, fek, fok, tmp;
    cpx_num cptemp;
    cpx_num inv_twd;
    cpx_num *in = (cpx_num *) in_data;
    cpx_num *out = (cpx_num *) out_data;
    cpx_num *tmpbuf = (cpx_num *) _tmpbuf;
    cpx_num *super_twiddles = (cpx_num *) twiddles;
    cpx_num *filter = (cpx_num *) filters;

    // Post-processing
    tdc.r = in[0].r;
    tdc.i = in[0].i;
    C_FIXDIV(tdc,2);
    tmpbuf[0].r = tdc.r + tdc.i;
    tmpbuf[len].r = tdc.r - tdc.i;
    tmpbuf[len].i = tmpbuf[0].i = 0;

    for ( j=1;j <= len/2 ; ++j ) {
        fpk    = in[j];
        fpnk.r =   in[len-j].r;
        fpnk.i = - in[len-j].i;
        C_FIXDIV(fpk,2);
        C_FIXDIV(fpnk,2);

        C_ADD( f1k, fpk , fpnk );
        C_SUB( f2k, fpk , fpnk );
        C_MUL( tw , f2k , super_twiddles[j-1]);

        tmpbuf[j].r = HALF_OF(f1k.r + tw.r);
        tmpbuf[j].i = HALF_OF(f1k.i + tw.i);
        tmpbuf[len-j].r = HALF_OF(f1k.r - tw.r);
        tmpbuf[len-j].i = HALF_OF(tw.i - f1k.i);
    }

    // FIR
	for (j = 0; j < len + 1; j++) {
        C_MUL( cptemp, tmpbuf[j], filter[j]);
        tmpbuf[j] = cptemp;
    }

    // Pre-processing
    out[0].r = tmpbuf[0].r + tmpbuf[len].r;
    out[0].i = tmpbuf[0].r - tmpbuf[len].r;
    C_FIXDIV(out[0],2);

    for (j = 1; j <= len/2; ++j) {
        fk = tmpbuf[j];
        fnkc.r = tmpbuf[len-j].r;
        fnkc.i = -tmpbuf[len-j].i;
        C_FIXDIV( fk , 2 );
        C_FIXDIV( fnkc , 2 );
        inv_twd = super_twiddles[j-1];
        C_MULBYSCALAR(inv_twd,-1);

        C_ADD (fek, fk, fnkc);
        C_SUB (tmp, fk, fnkc);
        C_MUL (fok, tmp, inv_twd);
        C_ADD (out[j],     fek, fok);
        C_SUB (out[len-j], fek, fok);
        out[len-j].i *= -1;
    }
}

unsigned int fft_rev(unsigned int v)
{
	unsigned int r = v;
	int s = sizeof(v) * CHAR_BIT - 1;

	for (v >>= 1; v; v >>= 1) {
		r <<= 1;
		r |= v & 1;
		s--;
	}
	r <<= s;
	return r;
}

void fft_bit_reverse(float *w, unsigned int n, unsigned int bits)
{
	unsigned int i, s, shift;

	s = sizeof(i) * CHAR_BIT - 1;
	shift = s - bits + 1;

	for (i = 0; i < n; i++) {
		unsigned int r;
		float t_real, t_imag;

		r = fft_rev(i);
		r >>= shift;

		if (i < r) {
			t_real = w[2 * i];
			t_imag = w[2 * i + 1];
			w[2 * i] = w[2 * r];
			w[2 * i + 1] = w[2 * r + 1];
			w[2 * r] = t_real;
			w[2 * r + 1] = t_imag;
		}
	}
}
