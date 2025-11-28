#ifndef TRACE_H
#define TRACE_H

typedef struct {
    bool run;
    unsigned delay;
    unsigned nprio;
} trace_entry_t;

const trace_entry_t trace[][MAX_THREADS] = {
    { {true, 1, 1}, {true, 1, 1}, },
    { {true, 1, 1}, {true, 1, 1}, },
    { {true, 1, 1}, {true, 1, 3}, },
    { {true, 1, 1}, {true, 1, 3}, },
};

#endif // TRACE_H
