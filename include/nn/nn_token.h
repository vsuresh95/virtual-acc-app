#ifndef NN_TOKEN_H
#define NN_TOKEN_H

typedef struct {
    int32_t value; /* raw fixed-point: signed 16.16 */
} nn_token_t;

/* Constants (mirroring static constexpr in C++) */
enum { NN_FRACTIONAL_BITS = 16 };
enum { NN_SCALE = 1 << NN_FRACTIONAL_BITS };

/* “Constructors” */
static inline nn_token_t nn_token_zero(void) {
    nn_token_t t; t.value = 0; return t;
}
static inline nn_token_t nn_token_from_float(float f) {
    /* Round toward zero like C++ cast; adjust if needing round-to-nearest */
    nn_token_t t; t.value = (int32_t)(f * (float) NN_SCALE); return t;
}
static inline nn_token_t nn_token_from_int(int v) {
    nn_token_t t; t.value = ((int32_t)v) << NN_FRACTIONAL_BITS; return t;
}
/* Internal/raw ctor equivalent */
static inline nn_token_t nn_token_from_raw(int32_t raw) {
    nn_token_t t; t.value = raw; return t;
}

/* Conversion to float */
static inline float nn_token_to_float(nn_token_t t) {
    return (float)t.value / (float) NN_SCALE;
}

/* Arithmetic operators */
static inline nn_token_t nn_token_add(nn_token_t a, nn_token_t b) {
    return nn_token_from_raw(a.value + b.value);
}
static inline nn_token_t nn_token_sub(nn_token_t a, nn_token_t b) {
    return nn_token_from_raw(a.value - b.value);
}
static inline nn_token_t nn_token_mul(nn_token_t a, nn_token_t b) {
    /* widen to 64-bit to avoid overflow before rescaling */
    int64_t prod = (int64_t) a.value * (int64_t)b.value;
    /* arithmetic shift: signed; assumes two’s complement */
    return nn_token_from_raw((int32_t)(prod >> NN_FRACTIONAL_BITS));
}
static inline nn_token_t nn_token_div(nn_token_t a, nn_token_t b) {
    /* widen numerator before upscaling to preserve precision */
    int64_t num = ((int64_t) a.value << NN_FRACTIONAL_BITS);
    /* caller responsible for nonzero b.value */
    return nn_token_from_raw((int32_t)(num / (int64_t) b.value));
}

/* Compound assignment */
static inline void nn_token_iadd(nn_token_t* a, nn_token_t b) {
    a->value += b.value;
}
static inline void nn_token_isub(nn_token_t* a, nn_token_t b) {
    a->value -= b.value;
}
static inline void nn_token_imul(nn_token_t* a, nn_token_t b) {
    *a = nn_token_mul(*a, b);
}
static inline void nn_token_idiv(nn_token_t* a, nn_token_t b) {
    *a = nn_token_div(*a, b);
}

/* Comparisons */
static inline int nn_token_eq(nn_token_t a, nn_token_t b) { return a.value == b.value; }
static inline int nn_token_ne(nn_token_t a, nn_token_t b) { return a.value != b.value; }
static inline int nn_token_lt(nn_token_t a, nn_token_t b) { return a.value <  b.value; }
static inline int nn_token_le(nn_token_t a, nn_token_t b) { return a.value <= b.value; }
static inline int nn_token_gt(nn_token_t a, nn_token_t b) { return a.value >  b.value; }
static inline int nn_token_ge(nn_token_t a, nn_token_t b) { return a.value >= b.value; }

/* Print helper (like operator<<) */
static inline void nn_token_fprint(FILE* fp, nn_token_t t) {
    /* print as float to mimic C++ operator<< behavior */
    fprintf(fp, "%f", nn_token_to_float(t));
}
static inline void nn_token_print(nn_token_t t) { nn_token_fprint(stdout, t); }

/* Optional: safer constructors with explicit rounding behavior */
static inline nn_token_t nn_token_from_float_rn(float f) {
    /* round-to-nearest-even: add 0.5 ulp at fixed scale and floor */
    float scaled = f * (float) NN_SCALE;
    int32_t raw = (int32_t)(scaled >= 0 ? scaled + 0.5f : scaled - 0.5f);
    return nn_token_from_raw(raw);
}

#endif /* NN_TOKEN_H */
