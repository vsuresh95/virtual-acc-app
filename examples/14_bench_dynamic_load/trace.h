#ifndef TRACE_H
#define TRACE_H

typedef struct {
    bool run;
    unsigned period;
    unsigned nprio;
} trace_entry_t;

#define TINY_PERIOD 10000
#define SMALL_PERIOD 50000
#define MEDIUM_PERIOD 150000
#define LARGE_PERIOD 330000

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

unsigned light_period[MAX_THREADS] = {
    TINY_PERIOD, SMALL_PERIOD, SMALL_PERIOD, MEDIUM_PERIOD
};

unsigned heavy_period[MAX_THREADS] = {
    SMALL_PERIOD, MEDIUM_PERIOD, MEDIUM_PERIOD, LARGE_PERIOD
};

unsigned mixed_period[MAX_THREADS] = {
    TINY_PERIOD, SMALL_PERIOD, MEDIUM_PERIOD, LARGE_PERIOD
};

float scale_factor[80][4] = {
    {1.0f, 1.0f, 1.0f, 1.0f},
    {1.0f, 1.0f, 1.0f, 1.0f},
    {1.0f, 1.0f, 3.67f, 1.0f},
    {1.0f, 1.0f, 1.0f, 2.89f},
    {2.77f, 1.0f, 1.0f, 1.0f},
    {1.0f, 3.33f, 1.0f, 1.0f},
    {1.0f, 1.0f, 2.91f, 1.0f},
    {1.0f, 1.0f, 1.0f, 2.58f},
    {2.42f, 1.0f, 1.0f, 1.0f},
    {1.0f, 2.81f, 1.0f, 1.0f},
    {1.0f, 1.0f, 3.95f, 1.0f},
    {1.0f, 1.0f, 1.0f, 2.17f},
    {3.54f, 1.0f, 1.0f, 1.0f},
    {1.0f, 2.63f, 1.0f, 1.0f},
    {1.0f, 1.0f, 2.49f, 1.0f},
    {1.0f, 1.0f, 1.0f, 3.78f},
    {2.28f, 1.0f, 1.0f, 1.0f},
    {1.0f, 3.04f, 1.0f, 1.0f},
    {1.0f, 1.0f, 2.12f, 1.0f},
    {1.0f, 1.0f, 1.0f, 2.96f},
    {3.31f, 1.0f, 1.0f, 1.0f},
    {1.0f, 2.49f, 1.0f, 1.0f},
    {1.0f, 1.0f, 2.74f, 1.0f},
    {1.0f, 1.0f, 1.0f, 2.93f},
    {2.85f, 1.0f, 1.0f, 1.0f},
    {1.0f, 3.57f, 1.0f, 1.0f},
    {1.0f, 1.0f, 2.68f, 1.0f},
    {1.0f, 1.0f, 1.0f, 3.22f},
    {2.09f, 1.0f, 1.0f, 1.0f},
    {1.0f, 2.26f, 1.0f, 1.0f},
    {1.0f, 1.0f, 3.84f, 1.0f},
    {1.0f, 1.0f, 1.0f, 2.17f},
    {2.75f, 1.0f, 1.0f, 1.0f},
    {1.0f, 2.92f, 1.0f, 1.0f},
    {1.0f, 1.0f, 2.37f, 1.0f},
    {1.0f, 1.0f, 1.0f, 3.66f},
    {2.88f, 1.0f, 1.0f, 1.0f},
    {1.0f, 3.15f, 1.0f, 1.0f},
    {1.0f, 1.0f, 2.56f, 1.0f},
    {1.0f, 1.0f, 1.0f, 2.44f},
    {3.98f, 1.0f, 1.0f, 1.0f},
    {1.0f, 2.63f, 1.0f, 1.0f},
    {1.0f, 1.0f, 2.14f, 1.0f},
    {1.0f, 1.0f, 1.0f, 3.49f},
    {2.33f, 1.0f, 1.0f, 1.0f},
    {1.0f, 2.71f, 1.0f, 1.0f},
    {1.0f, 1.0f, 3.58f, 1.0f},
    {1.0f, 1.0f, 1.0f, 2.81f},
    {2.96f, 1.0f, 1.0f, 1.0f},
    {1.0f, 2.08f, 1.0f, 1.0f},
    {1.0f, 1.0f, 2.19f, 1.0f},
    {1.0f, 1.0f, 1.0f, 3.12f},
    {2.67f, 1.0f, 1.0f, 1.0f},
    {1.0f, 3.74f, 1.0f, 1.0f},
    {1.0f, 1.0f, 2.52f, 1.0f},
    {1.0f, 1.0f, 1.0f, 2.25f},
    {2.34f, 1.0f, 1.0f, 1.0f},
    {1.0f, 2.99f, 1.0f, 1.0f},
    {1.0f, 1.0f, 2.62f, 1.0f},
    {1.0f, 1.0f, 1.0f, 3.45f},
    {2.85f, 1.0f, 1.0f, 1.0f},
    {1.0f, 3.28f, 1.0f, 1.0f},
    {1.0f, 1.0f, 2.41f, 1.0f},
    {1.0f, 1.0f, 1.0f, 2.07f},
    {3.63f, 1.0f, 1.0f, 1.0f},
    {1.0f, 2.82f, 1.0f, 1.0f},
    {1.0f, 1.0f, 2.73f, 1.0f},
    {1.0f, 1.0f, 1.0f, 3.56f},
    {2.52f, 1.0f, 1.0f, 1.0f},
    {1.0f, 2.77f, 1.0f, 1.0f},
    {1.0f, 1.0f, 3.91f, 1.0f},
    {1.0f, 1.0f, 1.0f, 2.35f},
    {2.94f, 1.0f, 1.0f, 1.0f},
    {1.0f, 3.12f, 1.0f, 1.0f},
    {1.0f, 1.0f, 2.27f, 1.0f},
    {1.0f, 1.0f, 1.0f, 2.86f},
    {3.45f, 1.0f, 1.0f, 1.0f},
    {1.0f, 2.58f, 1.0f, 1.0f},
    {1.0f, 1.0f, 2.33f, 1.0f},
    {1.0f, 1.0f, 1.0f, 3.04f},
};

#endif // TRACE_H
