#ifndef __SW_FUNC_H__
#define __SW_FUNC_H__

// Wrapper for fft2_comp to be mapped for the hpthread
void* sw_audio_fft(void *a) {
    printf("[SW FUNC] Started software thread for FFT!\n");

    audio_fft_hpthread_args *args = (audio_fft_hpthread_args *) a;

    // Temporary buffer for floating point data
    const unsigned num_samples = (1 << args->logn_samples);
    native_t *gold = new float[2 * num_samples];

    token_t *mem = (token_t *) args->mem;

    // Reading memory layout from args
    unsigned in_offset = args->input_queue_base;
    unsigned out_offset = args->output_queue_base;
    unsigned in_valid_offset = in_offset + VALID_OFFSET;
    unsigned out_valid_offset = out_offset + VALID_OFFSET;

    atomic_flag_t input_flag ((volatile uint64_t *) &mem[in_valid_offset]);
    atomic_flag_t output_flag ((volatile uint64_t *) &mem[out_valid_offset]);

    while (1) {
        // Wait for input to be ready
		while(input_flag.load() != 1);

        // Convert to floating point data
        for (unsigned j = 0; j < 2 * num_samples; j++) {
            gold[j] = fixed32_to_float(mem[in_offset + PAYLOAD_OFFSET + j], FX_IL);
        }

		// Reset for next iteration.
		input_flag.store(0);

        // Perform FFT
        fft2_comp(gold, 1 /* num_ffts */, num_samples, args->logn_samples, args->do_inverse, args->do_shift);

        // Convert to fixed point data
        for (unsigned j = 0; j < 2 * num_samples; j++) {
            mem[out_offset + PAYLOAD_OFFSET + j] = float_to_fixed32(gold[j], FX_IL);
        }

        // CPU is implicitly ready because computation is chained
        // Set output to be full
		output_flag.store(1);
    }

    delete gold;
}

typedef struct {
    native_t r;
    native_t i;
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

void fir_comp(float *data, unsigned len, float *filters, float *twiddles, float *_tmpbuf)
{
	unsigned j;

    cpx_num fpnk, fpk, f1k, f2k, tw, tdc;
    cpx_num fk, fnkc, fek, fok, tmp;
    cpx_num cptemp;
    cpx_num inv_twd;
    cpx_num *in = (cpx_num *) data;
    cpx_num *out = (cpx_num *) data;
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

    for (j=1; j <= len/2 ; ++j ) {
        fpk = in[j];
        fpnk.r = in[len-j].r;
        fpnk.i = - in[len-j].i;
        C_FIXDIV(fpk,2);
        C_FIXDIV(fpnk,2);

        C_ADD(f1k, fpk , fpnk);
        C_SUB(f2k, fpk , fpnk);
        C_MUL(tw , f2k , super_twiddles[j-1]);

        tmpbuf[j].r = HALF_OF(f1k.r + tw.r);
        tmpbuf[j].i = HALF_OF(f1k.i + tw.i);
        tmpbuf[len-j].r = HALF_OF(f1k.r - tw.r);
        tmpbuf[len-j].i = HALF_OF(tw.i - f1k.i);
    }

    // FIR
	for (j = 0; j < len + 1; j++) {
        C_MUL(cptemp, tmpbuf[j], filter[j]);
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
        C_FIXDIV(fk , 2 );
        C_FIXDIV(fnkc , 2 );
        inv_twd = super_twiddles[j-1];
        C_MULBYSCALAR(inv_twd,-1);

        C_ADD (fek, fk, fnkc);
        C_SUB (tmp, fk, fnkc);
        C_MUL (fok, tmp, inv_twd);
        C_ADD (out[j], fek, fok);
        C_SUB (out[len-j], fek, fok);
        out[len-j].i *= -1;
    }
}

#endif // __SW_FUNC_H__