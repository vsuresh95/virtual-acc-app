#ifndef TRACE_H
#define TRACE_H

typedef struct {
    bool run;
    unsigned period;
    unsigned nprio;
} trace_entry_t;

#define TINY_PERIOD 100000 
#define SMALL1_PERIOD 240000
#define SMALL2_PERIOD 300000
#define MEDIUM1_PERIOD 640000
#define MEDIUM2_PERIOD 960000
#define LARGE_PERIOD 1600000

const char light_models[MAX_THREADS][256] = {
    "models/model_16_4.txt",
    "models/model_32_6.txt",
    "models/model_32_6.txt",
    "models/model_48_4.txt",
};

const char heavy_models[MAX_THREADS][256] = {
    "models/model_32_4.txt",
    "models/model_48_6.txt",
    "models/model_48_6.txt",
    "models/model_64_4.txt",
};

const char mixed_models[MAX_THREADS][256] = {
    "models/model_16_4.txt",
    "models/model_32_6.txt",
    "models/model_48_6.txt",
    "models/model_64_4.txt",
};

unsigned light_deadlines[MAX_THREADS] = {
    TINY_PERIOD, SMALL2_PERIOD, SMALL2_PERIOD, MEDIUM1_PERIOD
};

unsigned heavy_deadlines[MAX_THREADS] = {
    SMALL1_PERIOD, MEDIUM2_PERIOD, MEDIUM2_PERIOD, LARGE_PERIOD
};

unsigned mixed_deadlines[MAX_THREADS] = {
    TINY_PERIOD, SMALL2_PERIOD, MEDIUM2_PERIOD, LARGE_PERIOD
};

unsigned model_threads[MAX_THREADS] = {
    1, 2, 2, 1
};

#endif // TRACE_H
