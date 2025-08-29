#ifndef __BITSET_H__
#define __BITSET_H__

//  Number of concurrent contexts possible in a single accelerator
#define MAX_CONTEXTS 4

// Custom implementation of bitset
typedef uint8_t bitset_t;            /* uses only the low 4 bits */

/* internal helpers */
#define BITSET4_MASK(i)   ((uint8_t)(1u << (i)))   /* 0 <= i < 4 */
#define BITSET4_CLAMP(x)  ((uint8_t)((x) & 0x0Fu))

/* element operations: set/reset/flip/test at position i */
#define bitset_set(v,i)     ((v) = BITSET4_CLAMP((v) |  BITSET4_MASK(i)))
#define bitset_reset(v,i)   ((v) = BITSET4_CLAMP((v) & ~BITSET4_MASK(i)))
#define bitset_flip(v,i)    ((v) = BITSET4_CLAMP((v) ^  BITSET4_MASK(i)))
#define bitset_test(v,i)    (((v) >> (i)) & 1u)

/* aggregate predicates: all/any/none */
#define bitset_all(v)       (BITSET4_CLAMP(v) == 0x0Fu)
#define bitset_any(v)       (BITSET4_CLAMP(v) != 0)
#define bitset_none(v)      (!bitset_any(v))

/* bulk operations like bitset.set() / bitset.reset() with no index */
#define bitset_set_all(v)   ((v) = 0x0Fu)
#define bitset_reset_all(v) ((v) = 0x00u)

#endif // __BITSET_H__
