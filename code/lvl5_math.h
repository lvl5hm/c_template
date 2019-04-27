#ifndef LVL5_MATH

#include "lvl5_types.h"
#include "math.h"

f32 sin_f32(f32 a) {
  f32 result = sinf(a);
  return result;
}

f32 cos_f32(f32 a) {
  f32 result = cosf(a);
  return result;
}

i16 abs_i16(i16 a) {
  i16 result = a > 0 ? a : -a;
  return result;
}


// v2

typedef struct {
  f32 x;
  f32 y;
} v2;

v2 V2(f32 x, f32 y) {
  v2 result;
  result.x = x;
  result.y = y;
  return result;
}

v2 v2_add(v2 a, v2 b) {
  v2 result;
  result.x = a.x + b.x;
  result.y = a.y + b.y;
  return result;
}

v2 v2_sub(v2 a, v2 b) {
  v2 result;
  result.x = a.x - b.x;
  result.y = a.y - b.y;
  return result;
}

v2 v2_invert(v2 a) {
  v2 result;
  result.x = 1.0f/a.x;
  result.y = 1.0f/a.y;
  return result;
}

f32 v2_dot(v2 a, v2 b) {
  f32 result = a.x*b.x + a.y*b.y;
  return result;
}

v2 v2_hadamard(v2 a, v2 b) {
  v2 result;
  result.x = a.x*b.x;
  result.y = a.y*b.y;
  return result;
}

v2 v2_mul_s(v2 a, f32 s) {
  v2 result;
  result.x = a.x*s;
  result.y = a.y*s;
  return result;
}

v2 v2_div_s(v2 a, f32 s) {
  v2 result;
  result.x = a.x/s;
  result.y = a.y/s;
  return result;
}

v2 v2_i(i32 x, i32 y) {
  v2 result;
  result.x = (f32)x;
  result.y = (f32)y;
  return result;
}



// v3

typedef union {
  struct {
    f32 x;
    f32 y;
    f32 z;
  };
  struct {
    f32 r;
    f32 g;
    f32 b;
  };
  struct {
    v2 xy;
  };
} v3;

v3 V3(f32 x, f32 y, f32 z) {
  v3 result;
  result.x = x;
  result.y = y;
  result.z = z;
  return result;
}

v3 v3_add(v3 a, v3 b) {
  v3 result;
  result.x = a.x + b.x;
  result.y = a.y + b.y;
  result.z = a.z + b.z;
  return result;
}

v3 v3_sub(v3 a, v3 b) {
  v3 result;
  result.x = a.x - b.x;
  result.y = a.y - b.y;
  result.z = a.z - b.z;
  return result;
}

v3 v3_invert(v3 a) {
  v3 result;
  result.x = 1.0f/a.x;
  result.y = 1.0f/a.y;
  result.z = 1.0f/a.z;
  return result;
}

f32 v3_dot(v3 a, v3 b) {
  f32 result = a.x*b.x + a.y*b.y + a.z*b.z;
  return result;
}

v3 v3_hadamard(v3 a, v3 b) {
  v3 result;
  result.x = a.x*b.x;
  result.y = a.y*b.y;
  result.z = a.z*b.z;
  return result;
}

v3 v3_mul_s(v3 a, f32 s) {
  v3 result;
  result.x = a.x*s;
  result.y = a.y*s;
  result.z = a.z*s;
  return result;
}

v3 v3_div_s(v3 a, f32 s) {
  v3 result;
  result.x = a.x/s;
  result.y = a.y/s;
  result.z = a.z/s;
  return result;
}


// V4
typedef union {
  struct {
    f32 x;
    f32 y;
    f32 z;
    f32 w;
  };
  struct {
    f32 r;
    f32 g;
    f32 b;
    f32 a;
  };
  struct {
    v2 xy;
    v2 zw;
  };
  struct {
    v3 xyz;
  };
  struct {
    v3 rgb;
  };
} v4;

v4 V4(f32 x, f32 y, f32 z, f32 w) {
  v4 result;
  result.x = x;
  result.y = y;
  result.z = z;
  result.w = w;
  return result;
}

v4 v4_add(v4 a, v4 b) {
  v4 result;
  result.x = a.x + b.x;
  result.y = a.y + b.y;
  result.z = a.z + b.z;
  result.w = a.w + b.w;
  return result;
}

v4 v4_sub(v4 a, v4 b) {
  v4 result;
  result.x = a.x - b.x;
  result.y = a.y - b.y;
  result.z = a.z - b.z;
  result.w = a.w - b.w;
  return result;
}

v4 v4_hadamard(v4 a, v4 b) {
  v4 result;
  result.x = a.x*b.x;
  result.y = a.y*b.y;
  result.z = a.z*b.z;
  result.w = a.w*b.w;
  return result;
}

v4 v4_mul_s(v4 a, f32 s) {
  v4 result;
  result.x = a.x*s;
  result.y = a.y*s;
  result.z = a.z*s;
  result.w = a.w*s;
  return result;
}

v4 v4_div_s(v4 a, f32 s) {
  v4 result;
  result.x = a.x/s;
  result.y = a.y/s;
  result.z = a.z/s;
  result.w = a.w/s;
  return result;
}



// v2i

typedef struct {
  i32 x;
  i32 y;
} v2i;

v2i V2i(i32 x, i32 y) {
  v2i result;
  result.x = x;
  result.y = y;
  return result;
}

v2i v2_to_v2i(v2 v) {
  v2i result;
  result.x = (i32)v.x;
  result.y = (i32)v.y;
  return result;
}

v2 v2i_to_v2(v2i v) {
  v2 result;
  result.x = (f32)v.x;
  result.y = (f32)v.y;
  return result;
}

v2i v2i_add(v2i a, v2i b) {
  v2i result;
  result.x = a.x + b.x;
  result.y = a.y + b.y;
  return result;
}

v2i v2i_sub(v2i a, v2i b) {
  v2i result;
  result.x = a.x - b.x;
  result.y = a.y - b.y;
  return result;
}

v2i v2i_mul_s(v2i a, i32 s) {
  v2i result;
  result.x = a.x*s;
  result.y = a.y*s;
  return result;
}

v2i v2i_div_s(v2i a, i32 s) {
  v2i result;
  result.x = a.x/s;
  result.y = a.y/s;
  return result;
}


typedef union {
  f32 e[4];
  struct {
    f32 e00; f32 e10;
    f32 e01; f32 e11;
  };
} mat2x2;

mat2x2 Mat2x2(f32 e00, f32 e10,
              f32 e01, f32 e11) {
  mat2x2 result;
  result.e00 = e00;
  result.e10 = e10;
  result.e01 = e01;
  result.e11 = e11;
  return result;
}

mat2x2 mat2x2_mul_mat2x2(mat2x2 a, mat2x2 b) {
  mat2x2 result;
  result.e00 = a.e00*b.e00 + a.e10*b.e01;
  result.e10 = a.e00*b.e10 + a.e10*b.e11;
  result.e01 = a.e01*b.e00 + a.e11*b.e01;
  result.e11 = a.e01*b.e10 + a.e11*b.e11;
  return result;
}

v2 mat2x2_mul_v2(mat2x2 m, v2 v) {
  v2 result;
  result.x = v.x*m.e00 + v.x*m.e10;
  result.y = v.y*m.e01 + v.y*m.e11;
  return result;
}

// mat3x3
typedef union {
  f32 e[9];
  struct {
    f32 e00; f32 e10; f32 e20;
    f32 e01; f32 e11; f32 e21;
    f32 e02; f32 e12; f32 e22;
  };
} mat3x3;

mat3x3 Mat3x3(f32 e00, f32 e10, f32 e20,
              f32 e01, f32 e11, f32 e21,
              f32 e02, f32 e12, f32 e22) {
  mat3x3 result;
  result.e00 = e00;
  result.e10 = e10;
  result.e20 = e20;
  result.e01 = e01;
  result.e11 = e11;
  result.e21 = e21;
  result.e02 = e02;
  result.e12 = e12;
  result.e22 = e22;
  return result;
}

mat3x3 mat3x3_mul_mat3x3(mat3x3 a, mat3x3 b) {
  mat3x3 result;
  result.e00 = a.e00*b.e00 + a.e10*b.e01 + a.e20*b.e02;
  result.e10 = a.e00*b.e10 + a.e10*b.e11 + a.e20*b.e12;
  result.e20 = a.e00*b.e20 + a.e10*b.e21 + a.e20*b.e22;
  
  result.e01 = a.e01*b.e00 + a.e11*b.e01 + a.e21*b.e02;
  result.e11 = a.e01*b.e10 + a.e11*b.e11 + a.e21*b.e12;
  result.e21 = a.e01*b.e20 + a.e11*b.e21 + a.e21*b.e22;
  
  result.e02 = a.e02*b.e00 + a.e12*b.e01 + a.e22*b.e02;
  result.e12 = a.e02*b.e10 + a.e12*b.e11 + a.e22*b.e12;
  result.e22 = a.e02*b.e20 + a.e12*b.e21 + a.e22*b.e22;
  return result;
}

// rect2

typedef struct {
  v2 min;
  v2 max;
} rect2;

rect2 rect2_min_max(v2 min, v2 max) {
  rect2 result;
  result.min = min;
  result.max = max;
  return result;
}

rect2 rect2_min_size(v2 min, v2 size) {
  rect2 result;
  result.min = min;
  result.max = v2_add(min, size);
  return result;
}

rect2 rect2_center_size(v2 center, v2 size) {
  rect2 result;
  v2 half_size = v2_mul_s(size, 0.5f);
  result.min = v2_sub(center, half_size);
  result.max = v2_add(center, half_size);
  return result;
}

v2 rect2_get_size(rect2 r) {
  v2 result = v2_sub(r.max, r.min);
  return result;
}


// rect2i

typedef struct {
  v2i min;
  v2i max;
} rect2i;

rect2i rect2i_min_max(v2i min, v2i max) {
  rect2i result;
  result.min = min;
  result.max = max;
  return result;
}

rect2i rect2i_min_size(v2i min, v2i size) {
  rect2i result;
  result.min = min;
  result.max = v2i_add(min, size);
  return result;
}

rect2i rect2i_center_size(v2i center, v2i size) {
  rect2i result;
  v2i half_size = v2i_div_s(size, 2);
  result.min = v2i_sub(center, half_size);
  result.max = v2i_add(center, half_size);
  return result;
}

v2i rect2i_get_size(rect2i r) {
  v2i result = v2i_sub(r.max, r.min);
  return result;
}

rect2 rect2i_to_rect2(rect2i r) {
  rect2 result;
  result.min = v2i_to_v2(r.min);
  result.max = v2i_to_v2(r.max);
  return result;
}

#define LVL5_MATH
#endif