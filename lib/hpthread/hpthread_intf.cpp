#include <hpthread_intf.h>

// Global instance of hpthread interface
hpthread_intf_t intf;

// Helper function for swapping the state of the interface
bool hpthread_intf_t::swap(vam_state_t expected_value, vam_state_t new_value) {
    return intf.state.compare_exchange_strong(expected_value, new_value, std::memory_order_seq_cst);
}

// Helper function for testing the state of the interface
vam_state_t hpthread_intf_t::test() {
    return intf.state.load(std::memory_order_seq_cst);
}

// Helper function for setting the state of the interface
void hpthread_intf_t::set(vam_state_t s) {
    intf.state.store(s, std::memory_order_seq_cst);
}
