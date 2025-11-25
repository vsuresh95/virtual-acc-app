#ifndef TRACE_H
#define TRACE_H

typedef struct {
    bool run;
    unsigned period;
    unsigned nprio;
} trace_entry_t;

#define TINY_LATENCY 55000 
#define SMALL_LATENCY 160000
#define MEDIUM_LATENCY 360000
#define LARGE_LATENCY 760000

const char light_models[MAX_THREADS][256] = {
    "models/model_16_2.txt",
    "models/model_32_2.txt",
    "models/model_32_2.txt",
};

const char heavy_models[MAX_THREADS][256] = {
    "models/model_48_2.txt",
    "models/model_48_2.txt",
    "models/model_64_2.txt",
};

const char mixed_models[MAX_THREADS][256] = {
    "models/model_16_2.txt",
    "models/model_32_2.txt",
    "models/model_48_2.txt",
    "models/model_64_2.txt",
};

unsigned light_deadlines[MAX_THREADS] = {
    TINY_LATENCY, SMALL_LATENCY, SMALL_LATENCY,
};

const trace_entry_t light_trace[][MAX_THREADS] = {
    { {true, TINY_LATENCY, 1}, {true, SMALL_LATENCY, 1}, {false, 0, 6} },
    { {true, TINY_LATENCY, 1}, {true, SMALL_LATENCY, 1}, {false, 0, 6} },
    { {true, TINY_LATENCY, 1}, {true, SMALL_LATENCY, 1}, {true, 3 * SMALL_LATENCY, 1} },
    { {true, TINY_LATENCY, 1}, {true, SMALL_LATENCY, 1}, {true, 3 * SMALL_LATENCY, 1} },
    { {true, TINY_LATENCY, 1}, {true, SMALL_LATENCY, 1}, {false, 0, 6} },
    { {true, TINY_LATENCY, 1}, {true, SMALL_LATENCY, 1}, {false, 0, 6} },
    { {true, TINY_LATENCY, 1}, {true, SMALL_LATENCY, 1}, {true, 3 * SMALL_LATENCY, 1} },
    { {true, TINY_LATENCY, 1}, {true, SMALL_LATENCY, 1}, {true, 2 * SMALL_LATENCY, 1} },
    { {true, TINY_LATENCY, 1}, {true, SMALL_LATENCY, 1}, {true, 1 * SMALL_LATENCY, 1} },
    { {true, TINY_LATENCY, 1}, {true, SMALL_LATENCY, 1}, {true, 1 * SMALL_LATENCY, 1} },
    { {true, TINY_LATENCY, 1}, {true, SMALL_LATENCY, 1}, {true, 2 * SMALL_LATENCY, 1} },
    { {true, TINY_LATENCY, 1}, {true, SMALL_LATENCY, 1}, {true, 3 * SMALL_LATENCY, 1} },
    { {true, TINY_LATENCY, 1}, {true, SMALL_LATENCY, 1}, {false, 0, 6} },
    { {true, TINY_LATENCY, 1}, {true, SMALL_LATENCY, 1}, {false, 0, 6} },
    { {true, TINY_LATENCY, 1}, {true, SMALL_LATENCY, 1}, {true, 1 * SMALL_LATENCY, 1} },
    { {true, TINY_LATENCY, 1}, {true, SMALL_LATENCY, 1}, {true, 1 * SMALL_LATENCY, 1} },
    { {true, TINY_LATENCY, 1}, {true, SMALL_LATENCY, 1}, {false, 0, 6} },
    { {true, TINY_LATENCY, 1}, {true, SMALL_LATENCY, 1}, {false, 0, 6} },
};

unsigned heavy_deadlines[MAX_THREADS] = {
    MEDIUM_LATENCY, MEDIUM_LATENCY, LARGE_LATENCY,
};

const trace_entry_t heavy_trace[][MAX_THREADS] = {
    { {true, MEDIUM_LATENCY, 1}, {false, 0, 6}, {true, LARGE_LATENCY, 1} },
    { {true, MEDIUM_LATENCY, 1}, {false, 0, 6}, {true, LARGE_LATENCY, 1} },
    { {true, MEDIUM_LATENCY, 1}, {true, 3 * MEDIUM_LATENCY, 1}, {true, LARGE_LATENCY, 1} },
    { {true, MEDIUM_LATENCY, 1}, {true, 3 * MEDIUM_LATENCY, 1}, {true, LARGE_LATENCY, 1} },
    { {true, MEDIUM_LATENCY, 1}, {false, 0, 6}, {true, LARGE_LATENCY, 1} },
    { {true, MEDIUM_LATENCY, 1}, {false, 0, 6}, {true, LARGE_LATENCY, 1} },
    { {true, MEDIUM_LATENCY, 1}, {true, 3 * MEDIUM_LATENCY, 1}, {true, LARGE_LATENCY, 1} },
    { {true, MEDIUM_LATENCY, 1}, {true, 2 * MEDIUM_LATENCY, 1}, {true, LARGE_LATENCY, 1} },
    { {true, MEDIUM_LATENCY, 1}, {true, 1 * MEDIUM_LATENCY, 1}, {true, LARGE_LATENCY, 1} },
    { {true, MEDIUM_LATENCY, 1}, {true, 1 * MEDIUM_LATENCY, 1}, {true, LARGE_LATENCY, 1} },
    { {true, MEDIUM_LATENCY, 1}, {true, 2 * MEDIUM_LATENCY, 1}, {true, LARGE_LATENCY, 1} },
    { {true, MEDIUM_LATENCY, 1}, {true, 3 * MEDIUM_LATENCY, 1}, {true, LARGE_LATENCY, 1} },
    { {true, MEDIUM_LATENCY, 1}, {false, 0, 6}, {true, LARGE_LATENCY, 1} },
    { {true, MEDIUM_LATENCY, 1}, {false, 0, 6}, {true, LARGE_LATENCY, 1} },
    { {true, MEDIUM_LATENCY, 1}, {true, 1 * MEDIUM_LATENCY, 1}, {true, LARGE_LATENCY, 1} },
    { {true, MEDIUM_LATENCY, 1}, {true, 1 * MEDIUM_LATENCY, 1}, {true, LARGE_LATENCY, 1} },
    { {true, MEDIUM_LATENCY, 1}, {false, 0, 6}, {true, LARGE_LATENCY, 1} },
    { {true, MEDIUM_LATENCY, 1}, {false, 0, 6}, {true, LARGE_LATENCY, 1} },
};

unsigned mixed_deadlines[MAX_THREADS] = {
    TINY_LATENCY, SMALL_LATENCY, MEDIUM_LATENCY, LARGE_LATENCY,
};

const trace_entry_t mixed_trace[][MAX_THREADS] = {
};

#endif // TRACE_H
