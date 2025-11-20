#ifndef TRACE_H
#define TRACE_H

typedef struct {
    bool run;
    unsigned delay;
    unsigned nprio;
} trace_entry_t;

#define TINY_LATENCY 111000 
#define SMALL_LATENCY 173000
#define MEDIUM_LATENCY 341000
#define LARGE_LATENCY 737000

const char light_models[MAX_THREADS][256] = {
    "models/model_16_2.txt",
    "models/model_32_2.txt",
    "models/model_16_2.txt",
    "models/model_32_2.txt",
};

const char heavy_models[MAX_THREADS][256] = {
    "models/model_48_2.txt",
    "models/model_64_2.txt",
    "models/model_48_2.txt",
    "models/model_64_2.txt",
};

const char mixed_models[MAX_THREADS][256] = {
    "models/model_16_2.txt",
    "models/model_32_2.txt",
    "models/model_48_2.txt",
    "models/model_64_2.txt",
};

const trace_entry_t light_trace[][MAX_THREADS] = {
    // Static FPS
    { {true, TINY_LATENCY, 1}, {false, 0, 6}, {false, 0, 6}, {false, 0, 6} },
    { {true, TINY_LATENCY, 1}, {true, SMALL_LATENCY, 1}, {false, 0, 6}, {false, 0, 6} },
    { {true, TINY_LATENCY, 1}, {true, SMALL_LATENCY, 1}, {true, 1.1 * TINY_LATENCY, 1}, {false, 0, 6} },
    { {true, TINY_LATENCY, 1}, {true, SMALL_LATENCY, 1}, {true, 1.1 * TINY_LATENCY, 1}, {true, 1.1 * SMALL_LATENCY, 1} },
    { {true, TINY_LATENCY, 1}, {true, SMALL_LATENCY, 1}, {true, 1.1 * TINY_LATENCY, 1}, {true, 1.1 * SMALL_LATENCY, 1} },
    // Dynamic FPS
    { {true, 0.7 * TINY_LATENCY, 1}, {true, SMALL_LATENCY, 1}, {true, 1.1 * TINY_LATENCY, 1}, {true, 1.1 * SMALL_LATENCY, 1} },
    { {true, 0.7 * TINY_LATENCY, 1}, {true, SMALL_LATENCY, 1}, {true, 1.1 * TINY_LATENCY, 1}, {true, 1.1 * SMALL_LATENCY, 1} },
    { {true, 0.7 * TINY_LATENCY, 1}, {true, SMALL_LATENCY, 1}, {true, 1.1 * TINY_LATENCY, 1}, {true, 1.1 * SMALL_LATENCY, 1} },
    { {true, 0.7 * TINY_LATENCY, 1}, {true, 10 * SMALL_LATENCY, 1}, {true, 1.1 * TINY_LATENCY, 1}, {true, 1.1 * SMALL_LATENCY, 1} },
    { {true, 0.7 * TINY_LATENCY, 1}, {true, 10 * SMALL_LATENCY, 1}, {true, 1.1 * TINY_LATENCY, 1}, {true, 1.1 * SMALL_LATENCY, 1} },
    { {true, 0.7 * TINY_LATENCY, 1}, {true, 1.3 * SMALL_LATENCY, 1}, {true, 1.1 * TINY_LATENCY, 1}, {true, 1.1 * SMALL_LATENCY, 1} },
    { {true, 0.7 * TINY_LATENCY, 1}, {true, 1.3 * SMALL_LATENCY, 1}, {true, 1.1 * TINY_LATENCY, 1}, {true, 1.1 * SMALL_LATENCY, 1} },
    { {true, 0.7 * TINY_LATENCY, 1}, {true, 10 * SMALL_LATENCY, 1}, {true, 1.1 * TINY_LATENCY, 1}, {true, 1.1 * SMALL_LATENCY, 1} },
    { {true, 0.7 * TINY_LATENCY, 1}, {true, 10 * SMALL_LATENCY, 1}, {true, 1.1 * TINY_LATENCY, 1}, {true, 1.1 * SMALL_LATENCY, 1} },
    { {true, 0.7 * TINY_LATENCY, 1}, {true, 1.3 * SMALL_LATENCY, 1}, {true, 1.1 * TINY_LATENCY, 1}, {true, 1.1 * SMALL_LATENCY, 1} },
    { {true, 0.7 * TINY_LATENCY, 1}, {true, 1.3 * SMALL_LATENCY, 1}, {true, 1.1 * TINY_LATENCY, 1}, {true, 1.1 * SMALL_LATENCY, 1} },
    { {true, 0.7 * TINY_LATENCY, 1}, {true, 10 * SMALL_LATENCY, 1}, {true, 1.1 * TINY_LATENCY, 1}, {true, 1.1 * SMALL_LATENCY, 1} },
    { {true, 0.7 * TINY_LATENCY, 1}, {true, 10 * SMALL_LATENCY, 1}, {true, 1.1 * TINY_LATENCY, 1}, {true, 1.1 * SMALL_LATENCY, 1} },
    { {true, 0.7 * TINY_LATENCY, 1}, {true, 1.3 * SMALL_LATENCY, 1}, {true, 1.1 * TINY_LATENCY, 1}, {true, 1.1 * SMALL_LATENCY, 1} },
    { {true, 0.7 * TINY_LATENCY, 1}, {true, 1.3 * SMALL_LATENCY, 1}, {true, 1.1 * TINY_LATENCY, 1}, {true, 1.1 * SMALL_LATENCY, 1} },
    { {true, TINY_LATENCY, 1}, {true, SMALL_LATENCY, 1}, {true, 1.1 * TINY_LATENCY, 1}, {true, 1.1 * SMALL_LATENCY, 1} },
    { {true, TINY_LATENCY, 1}, {true, SMALL_LATENCY, 1}, {true, 1.1 * TINY_LATENCY, 1}, {true, 1.1 * SMALL_LATENCY, 1} },
    // Priority changes    
    { {true, TINY_LATENCY, 1}, {true, SMALL_LATENCY, 1}, {true, 1.1 * TINY_LATENCY, 2}, {true, 1.1 * SMALL_LATENCY, 2} },
    { {true, TINY_LATENCY, 1}, {true, SMALL_LATENCY, 1}, {true, 1.1 * TINY_LATENCY, 2}, {true, 1.1 * SMALL_LATENCY, 2} },
    { {true, TINY_LATENCY, 1}, {true, SMALL_LATENCY, 1}, {true, 1.1 * TINY_LATENCY, 2}, {true, 1.1 * SMALL_LATENCY, 2} },
    { {true, TINY_LATENCY, 2}, {true, SMALL_LATENCY, 2}, {true, 1.1 * TINY_LATENCY, 1}, {true, 1.1 * SMALL_LATENCY, 1} },
    { {true, TINY_LATENCY, 2}, {true, SMALL_LATENCY, 2}, {true, 1.1 * TINY_LATENCY, 1}, {true, 1.1 * SMALL_LATENCY, 1} },
    { {true, TINY_LATENCY, 2}, {true, SMALL_LATENCY, 2}, {true, 1.1 * TINY_LATENCY, 1}, {true, 1.1 * SMALL_LATENCY, 1} },
    { {true, TINY_LATENCY, 2}, {true, SMALL_LATENCY, 2}, {true, 1.1 * TINY_LATENCY, 1}, {false, 0, 6} },
    { {true, TINY_LATENCY, 1}, {true, SMALL_LATENCY, 1}, {false, 0, 6}, {false, 0, 6} },
    { {true, TINY_LATENCY, 1}, {false, 0, 6}, {false, 0, 6}, {false, 0, 6} },
};

const trace_entry_t heavy_trace[][MAX_THREADS] = {
    // Static FPS
    { {true, MEDIUM_LATENCY, 1}, {false, 0, 6}, {false, 0, 6}, {false, 0, 6} },
    { {true, MEDIUM_LATENCY, 1}, {false, 0, 6}, {false, 0, 6}, {false, 0, 6} },
    { {true, MEDIUM_LATENCY, 1}, {true, LARGE_LATENCY, 1}, {false, 0, 6}, {false, 0, 6} },
    { {true, MEDIUM_LATENCY, 1}, {true, LARGE_LATENCY, 1}, {false, 0, 6}, {false, 0, 6} },
    { {true, MEDIUM_LATENCY, 1}, {true, LARGE_LATENCY, 1}, {true, 1.1 * MEDIUM_LATENCY, 1}, {false, 0, 6} },
    { {true, MEDIUM_LATENCY, 1}, {true, LARGE_LATENCY, 1}, {true, 1.1 * MEDIUM_LATENCY, 1}, {false, 0, 6} },
    { {true, MEDIUM_LATENCY, 1}, {true, LARGE_LATENCY, 1}, {true, 1.1 * MEDIUM_LATENCY, 1}, {true, 1.1 * LARGE_LATENCY, 1} },
    { {true, MEDIUM_LATENCY, 1}, {true, LARGE_LATENCY, 1}, {true, 1.1 * MEDIUM_LATENCY, 1}, {true, 1.1 * LARGE_LATENCY, 1} },
    // Dynamic FPS
    { {true, 0.7 * MEDIUM_LATENCY, 1}, {true, LARGE_LATENCY, 1}, {true, 1.1 * MEDIUM_LATENCY, 1}, {true, 1.1 * LARGE_LATENCY, 1} },
    { {true, 0.7 * MEDIUM_LATENCY, 1}, {true, LARGE_LATENCY, 1}, {true, 1.1 * MEDIUM_LATENCY, 1}, {true, 1.1 * LARGE_LATENCY, 1} },
    { {true, 0.7 * MEDIUM_LATENCY, 1}, {true, 10 * LARGE_LATENCY, 1}, {true, 1.1 * MEDIUM_LATENCY, 1}, {true, 1.1 * LARGE_LATENCY, 1} },
    { {true, 0.7 * MEDIUM_LATENCY, 1}, {true, 10 * LARGE_LATENCY, 1}, {true, 1.1 * MEDIUM_LATENCY, 1}, {true, 1.1 * LARGE_LATENCY, 1} },
    { {true, 0.7 * MEDIUM_LATENCY, 1}, {true, 1.5 * LARGE_LATENCY, 1}, {true, 1.1 * MEDIUM_LATENCY, 1}, {true, 1.1 * LARGE_LATENCY, 1} },
    { {true, 0.7 * MEDIUM_LATENCY, 1}, {true, 1.5 * LARGE_LATENCY, 1}, {true, 1.1 * MEDIUM_LATENCY, 1}, {true, 1.1 * LARGE_LATENCY, 1} },
    { {true, 0.7 * MEDIUM_LATENCY, 1}, {true, 10 * LARGE_LATENCY, 1}, {true, 1.1 * MEDIUM_LATENCY, 1}, {true, 1.1 * LARGE_LATENCY, 1} },
    { {true, 0.7 * MEDIUM_LATENCY, 1}, {true, 10 * LARGE_LATENCY, 1}, {true, 1.1 * MEDIUM_LATENCY, 1}, {true, 1.1 * LARGE_LATENCY, 1} },
    { {true, 0.7 * MEDIUM_LATENCY, 1}, {true, 1.5 * LARGE_LATENCY, 1}, {true, 1.1 * MEDIUM_LATENCY, 1}, {true, 1.1 * LARGE_LATENCY, 1} },
    { {true, 0.7 * MEDIUM_LATENCY, 1}, {true, 1.5 * LARGE_LATENCY, 1}, {true, 1.1 * MEDIUM_LATENCY, 1}, {true, 1.1 * LARGE_LATENCY, 1} },
    { {true, 0.7 * MEDIUM_LATENCY, 1}, {true, 10 * LARGE_LATENCY, 1}, {true, 1.1 * MEDIUM_LATENCY, 1}, {true, 1.1 * LARGE_LATENCY, 1} },
    { {true, 0.7 * MEDIUM_LATENCY, 1}, {true, 10 * LARGE_LATENCY, 1}, {true, 1.1 * MEDIUM_LATENCY, 1}, {true, 1.1 * LARGE_LATENCY, 1} },
    { {true, 0.7 * MEDIUM_LATENCY, 1}, {true, 1.5 * LARGE_LATENCY, 1}, {true, 1.1 * MEDIUM_LATENCY, 1}, {true, 1.1 * LARGE_LATENCY, 1} },
    { {true, 0.7 * MEDIUM_LATENCY, 1}, {true, 1.5 * LARGE_LATENCY, 1}, {true, 1.1 * MEDIUM_LATENCY, 1}, {true, 1.1 * LARGE_LATENCY, 1} },
    { {true, 0.7 * MEDIUM_LATENCY, 1}, {true, LARGE_LATENCY, 1}, {true, 1.1 * MEDIUM_LATENCY, 1}, {true, 1.1 * LARGE_LATENCY, 1} },
    { {true, 0.7 * MEDIUM_LATENCY, 1}, {true, LARGE_LATENCY, 1}, {true, 1.1 * MEDIUM_LATENCY, 1}, {true, 1.1 * LARGE_LATENCY, 1} },
    // Priority changes
    { {true, MEDIUM_LATENCY, 1}, {true, LARGE_LATENCY, 1}, {true, 1.1 * MEDIUM_LATENCY, 2}, {true, 1.1 * LARGE_LATENCY, 2} },
    { {true, MEDIUM_LATENCY, 1}, {true, LARGE_LATENCY, 1}, {true, 1.1 * MEDIUM_LATENCY, 2}, {true, 1.1 * LARGE_LATENCY, 2} },
    { {true, MEDIUM_LATENCY, 1}, {true, LARGE_LATENCY, 1}, {true, 1.1 * MEDIUM_LATENCY, 2}, {true, 1.1 * LARGE_LATENCY, 2} },
    { {true, MEDIUM_LATENCY, 2}, {true, LARGE_LATENCY, 2}, {true, 1.1 * MEDIUM_LATENCY, 1}, {true, 1.1 * LARGE_LATENCY, 1} },
    { {true, MEDIUM_LATENCY, 2}, {true, LARGE_LATENCY, 2}, {true, 1.1 * MEDIUM_LATENCY, 1}, {true, 1.1 * LARGE_LATENCY, 1} },
    { {true, MEDIUM_LATENCY, 2}, {true, LARGE_LATENCY, 2}, {true, 1.1 * MEDIUM_LATENCY, 1}, {true, 1.1 * LARGE_LATENCY, 1} },
    { {true, MEDIUM_LATENCY, 1}, {true, LARGE_LATENCY, 1}, {true, 1.1 * MEDIUM_LATENCY, 1}, {true, 1.1 * LARGE_LATENCY, 1} },
    { {true, MEDIUM_LATENCY, 1}, {true, LARGE_LATENCY, 1}, {true, 1.1 * MEDIUM_LATENCY, 1}, {true, 1.1 * LARGE_LATENCY, 1} },
    { {true, MEDIUM_LATENCY, 1}, {true, LARGE_LATENCY, 1}, {true, 1.1 * MEDIUM_LATENCY, 1}, {false, 0, 6} },
    { {true, MEDIUM_LATENCY, 1}, {true, LARGE_LATENCY, 1}, {true, 1.1 * MEDIUM_LATENCY, 1}, {false, 0, 6} },
    { {true, MEDIUM_LATENCY, 1}, {true, LARGE_LATENCY, 1}, {false, 0, 6}, {false, 0, 6} },
    { {true, MEDIUM_LATENCY, 1}, {true, LARGE_LATENCY, 1}, {false, 0, 6}, {false, 0, 6} },
    { {true, MEDIUM_LATENCY, 1}, {false, 0, 6}, {false, 0, 6}, {false, 0, 6} },
    { {true, MEDIUM_LATENCY, 1}, {false, 0, 6}, {false, 0, 6}, {false, 0, 6} },
};

const trace_entry_t mixed_trace[][MAX_THREADS] = {
};

#endif // TRACE_H
