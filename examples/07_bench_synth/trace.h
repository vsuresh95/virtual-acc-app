#ifndef TRACE_H
#define TRACE_H

typedef struct {
    bool valid;
    unsigned delay;
    unsigned nprio;
} trace_entry_t;

const trace_entry_t light_trace[][MAX_THREADS] = {
    { {true, 15000, 1}, {false, 0, 0}, {false, 0, 0}, {false, 0, 0} },
    { {true, 15000, 1}, {false, 0, 0}, {false, 0, 0}, {false, 0, 0} },
    { {true, 15000, 1}, {true, 55000, 1}, {false, 0, 0}, {false, 0, 0} },
    { {true, 15000, 1}, {true, 55000, 1}, {false, 0, 0}, {false, 0, 0} },
    { {true, 15000, 1}, {true, 55000, 1}, {true, 15000, 1}, {false, 0, 0} },
    { {true, 15000, 1}, {true, 55000, 1}, {true, 15000, 1}, {false, 0, 0} },
    { {true, 15000, 1}, {true, 55000, 1}, {true, 15000, 1}, {true, 55000, 1} },
    { {true, 15000, 1}, {true, 55000, 1}, {true, 15000, 1}, {true, 55000, 1} },
    { {true, 15000, 1}, {true, 55000, 1}, {true, 15000, 1}, {true, 55000, 1} },
    { {true, 15000, 1}, {true, 55000, 1}, {true, 15000, 1}, {true, 55000, 1} },
    { {true, 15000, 1}, {true, 55000, 1}, {true, 15000, 1}, {true, 55000, 1} },
    { {true, 15000, 1}, {true, 55000, 1}, {true, 15000, 1}, {true, 55000, 1} },
};

const trace_entry_t heavy_trace[][MAX_THREADS] = {
};

const trace_entry_t mixed_trace[][MAX_THREADS] = {
};

#endif // TRACE_H
