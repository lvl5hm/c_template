#ifndef LVL5_INTRINSICS_H

#include <intrin.h>

typedef __m128 f32_4x;
typedef __m128i i32_4x;

inline f32_4x add_f32_4x(f32_4x a, f32_4x b) {
  f32_4x result = _mm_add_ps(a, b);
  return result;
}

inline f32_4x mul_f32_4x(f32_4x a, f32_4x b) {
  f32_4x result = _mm_mul_ps(a, b);
  return result;
}

#define LVL5_INTRINSICS_H
#endif