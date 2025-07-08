#include <common_helper.h>
#include <nn_helper.h>

mem_pool_t::mem_pool_t() {
    const unsigned mem_size = 2 * 1024 * 1024; // 2MB
    hw_buf = esp_alloc(mem_size);
    allocated = 0;
}

mem_pool_t::~mem_pool_t() {
    esp_free(hw_buf);
}

size_t mem_pool_t::alloc(size_t mem_size) {
    return allocated.fetch_add(mem_size, std::memory_order_seq_cst);
}

void initialize_data(const char *input_file, nn_token_t *mem, unsigned len) {
    if (input_file == NULL) return;

    FILE *file = fopen(input_file, "r");

    HIGH_DEBUG(printf("[NN FE] Initializing %d words from %s\n", len, input_file));
    for (unsigned i = 0; i < len; i++) {
        nn_token_t in;
        if (fscanf(file, "%d ", &in) != 1) {
            perror("Error reading input data file!");
            exit(1);
        }
        mem[i] = in;

        #if 0
            printf("%d ", in);
            if ((i+1) % 16 == 0) printf("\n");
        #endif
    }
}
