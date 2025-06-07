#ifndef __AUDIO_OFFLINE_PROF_H__
#define __AUDIO_OFFLINE_PROF_H__

std::map<primitive_t, std::map<unsigned, unsigned>> audio_offline_prof = {
    {
        AUDIO_FFI, {
            {6,     6352},
            {7,     12864},
            {8,     26608},
            {9,     57376},
            {10,    116976},
            {11,    240336},
            {12,    460080},
        },
    },
    {
        AUDIO_FFT, {
            {6,     4128},
            {7,     8256},
            {8,     16464},
            {9,     34736},
            {10,    70912},
            {11,    143456},
            {12,    251840},
        },
    },
};

#endif // __AUDIO_OFFLINE_PROF_H__
