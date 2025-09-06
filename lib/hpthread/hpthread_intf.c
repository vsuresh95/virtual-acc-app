#include <hpthread_intf.h>

// Global instance of hpthread interface
hpthread_intf_t intf;

// Helper function for swapping the state of the interface
bool hpthread_intf_swap(uint8_t expected_value, uint8_t new_value) {
    return __atomic_compare_exchange_n(&intf.state, &expected_value, new_value, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
}

// Helper function for testing the state of the interface
uint8_t hpthread_intf_test() {
    return __atomic_load_n(&intf.state, __ATOMIC_SEQ_CST);
}

// Helper function for setting the state of the interface
void hpthread_intf_set(uint8_t set_value) {
    __atomic_store_n(&intf.state, set_value, __ATOMIC_SEQ_CST);
}
