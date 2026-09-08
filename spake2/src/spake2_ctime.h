#ifndef SPAKE2_CTIME_H
#define SPAKE2_CTIME_H

#include <stdint.h>

static inline uint64_t spake2__ctime_msb_w(
        const uint64_t a) {
    return 0u - (a >> (sizeof(a) * 8 - 1));
}

// spake2__ctime_is_zero returns 0xff..f if a == 0 and 0 otherwise.
static inline uint64_t spake2__ctime_is_zero_w(
        const uint64_t a) {
    // Here is an SMT-LIB verification of this formula:
    //
    // (define-fun is_zero ((a (_ BitVec 32))) (_ BitVec 32)
    //   (bvand (bvnot a) (bvsub a #x00000001))
    // )
    //
    // (declare-fun a () (_ BitVec 32))
    //
    // (assert (not (= (= #x00000001 (bvlshr (is_zero a) #x0000001f)) (= a
    // #x00000000)))) (check-sat) (get-model)
    return spake2__ctime_msb_w(~a & (a - 1));
}

// spake2__ctime_eq_w returns 0xff..f if a == b and 0 otherwise.
static inline uint64_t spake2__ctime_eq_w(
        const uint64_t a, 
        const uint64_t b) {
    return spake2__ctime_is_zero_w(a ^ b);
}

// spake2__value_barrier_w returns `a`, but prevents GCC and Clang from reasoning about
// the returned value. This is used to mitigate compilers undoing constant-time
// code, until we can express our requirements directly in the language.
//
// Note the compiler is aware that `spake2__value_barrier_w` has no side effects and
// always has the same output for a given input. This allows it to eliminate
// dead code, move computations across loops, and vectorize.
static inline uint64_t spake2__value_barrier_w(
        uint64_t a) 
{
#if defined(__GNUC__) || defined(__clang__)
    __asm__("" : "+r"(a) : /* no inputs */);
#endif
    return a;
}

// spake2__ctime_select_w returns (mask & a) | (~mask & b). When `mask` is all
// 1s or all 0s (as returned by the methods above), the select methods return
// either `a` (if `mask` is nonzero) or `b` (if `mask` is zero).
static inline uint64_t spake2__ctime_select_w(
        const uint64_t mask, 
        const uint64_t a,
        const uint64_t b) 
{
    // Clang recognizes this pattern as a select. While it usually transforms it
    // to a cmov, it sometimes further transforms it into a branch, which we do
    // not want.
    //
    // Hiding the value of the mask from the compiler evades this transformation.
    uint64_t barrier = spake2__value_barrier_w(mask);
    return (barrier & a) | (~barrier & b);
}
#endif
