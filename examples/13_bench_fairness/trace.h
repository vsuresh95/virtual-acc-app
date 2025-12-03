#ifndef TRACE_H
#define TRACE_H

typedef struct {
    bool run;
    unsigned period;
    unsigned nprio;
} trace_entry_t;

#define MEDIUM_PERIOD 160000

#define MAX_THREADS 4

const char light_models[MAX_THREADS][256] = {
    "models/model_16_4.txt",
    "models/model_32_4.txt",
    "models/model_32_4.txt",
    "models/model_48_4.txt",
};

const char heavy_models[MAX_THREADS][256] = {
    "models/model_32_4.txt",
    "models/model_48_4.txt",
    "models/model_48_4.txt",
    "models/model_64_4.txt",
};

const char mixed_models[MAX_THREADS][256] = {
    "models/model_16_4.txt",
    "models/model_32_4.txt",
    "models/model_48_4.txt",
    "models/model_64_4.txt",
};

#endif // TRACE_H
