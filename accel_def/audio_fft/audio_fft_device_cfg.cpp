#include <audio_fft_device_cfg.hpp>

// Device-dependent configuration functions
void audio_fft_access_cfg(df_node_t *node, esp_access *generic_esp_access, unsigned valid_contexts) {
    fft_params_t *params = (fft_params_t *) node->get_params();
    struct audio_fft_stratus_access *audio_fft_desc = (struct audio_fft_stratus_access *) generic_esp_access;

    audio_fft_desc->logn_samples = params->logn_samples;
    audio_fft_desc->do_shift = params->do_shift;
    audio_fft_desc->do_inverse = params->do_inverse;

    // Get the queue base from the in/out edges of the FFT node
    audio_fft_desc->input_queue_base = node->in_edges[0]->data->base / sizeof(token_t);
    audio_fft_desc->output_queue_base = node->out_edges[0]->data->base / sizeof(token_t);

    generic_esp_access->context_quota = audio_offline_prof[AUDIO_FFT][params->logn_samples];
}
