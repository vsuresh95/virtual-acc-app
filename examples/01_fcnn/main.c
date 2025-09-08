#include <helper.h>
#include <nn_module.h>

// Example application for fully connected neural network (FCNN)
int main(int argc, char **argv) {
    printf("[APP] Starting app: FCNN!\n");
    uint64_t t_load = 0;
    uint64_t t_input_data = 0;
    uint64_t t_fcnn = 0;
    uint64_t t_release = 0;
    unsigned errors = 0;

    for (int i = 0; i < ITERATIONS; i++) {
        // Load a model from a text file (and) register with NN frontend
        nn_module *m = (nn_module *) malloc (sizeof(nn_module));
        m->id = 0;
        m->nprio = 1;
        uint64_t t_start = get_counter();
        nn_module_load_and_register(m, "model.txt");
        t_load += get_counter() - t_start;

        nn_token_t *mem = (nn_token_t *) m->mem;
        volatile uint64_t *input_flag = (volatile uint64_t *) &mem[m->input_flag_offset];
        volatile uint64_t *output_flag = (volatile uint64_t *) &mem[m->output_flag_offset];
        nn_token_t *input_data = &mem[m->input_flag_offset + (PAYLOAD_OFFSET/sizeof(nn_token_t))];
        nn_token_t *output_data = &mem[m->output_flag_offset + (PAYLOAD_OFFSET/sizeof(nn_token_t))];

        // Check if the input flag is set; else write inputs
        t_start = get_counter();
        initialize_data("input.txt", input_data, 4096);
        t_input_data += get_counter() - t_start;
        t_start = get_counter();
        __atomic_store_n(input_flag, 1, __ATOMIC_SEQ_CST); // set

        // Wait for the last node to finish.
        while(__atomic_load_n(output_flag, __ATOMIC_SEQ_CST) != 1); // test
        t_fcnn += get_counter() - t_start;

        // Load reference output
        nn_token_t *gold = (nn_token_t *) malloc (64 * sizeof(nn_token_t));
        initialize_data("output.txt", gold, 64);

        // Compare with actual output
        for (unsigned j = 0; j < 64; j++) {
            if ((fabs(nn_token_to_float(nn_token_sub(gold[j], output_data[j]))) / fabs(nn_token_to_float(gold[j]))) > ERR_TH) {
                if (errors < 2) { HIGH_DEBUG(printf("\tGOLD[%u] = %f vs %f = out[%u]\n", j, nn_token_to_float(gold[j]), nn_token_to_float(output_data[j]), j);) }
                errors++;
            }
        }

        __atomic_store_n(output_flag, 0, __ATOMIC_SEQ_CST); // set

        t_start = get_counter();
        nn_module_release(m);
        free(m);
        free(gold);
        t_release += get_counter() - t_start;
    }

    printf("\tRelative error > %.02f for %d values out of 64\n", ERR_TH, errors);
    printf("t_load = %lu\n", t_load/ITERATIONS);
    printf("t_input_data = %lu\n", t_input_data/ITERATIONS);
    printf("t_fcnn = %lu\n", t_fcnn/ITERATIONS);
    printf("t_release = %lu\n", t_release/ITERATIONS);
}
