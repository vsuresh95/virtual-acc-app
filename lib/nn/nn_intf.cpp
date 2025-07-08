#include <nn_intf.h>

// Global instance of nn interface
nn_intf_t nn_intf;

// Helper function for swapping the state of the interface
bool nn_intf_t::swap(nn::fe_state_t expected_value, nn::fe_state_t new_value) {
    return nn_intf.state.compare_exchange_strong(expected_value, new_value, std::memory_order_seq_cst);
}

// Helper function for testing the state of the interface
nn::fe_state_t nn_intf_t::test() {
    return nn_intf.state.load(std::memory_order_seq_cst);
}

// Helper function for setting the state of the interface
void nn_intf_t::set(nn::fe_state_t s) {
    nn_intf.state.store(s, std::memory_order_seq_cst);
}
