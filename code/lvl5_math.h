#ifndef LVL5_MATH

#include "lvl5_types.h"
#include "lvl5_intrinsics.h"
#include "math.h"

#define PI 3.14159265359f

#define U8_MIN 0x0
#define U16_MIN 0x0
#define U32_MIN 0x0
#define U64_MIN 0x0
#define U8_MAX 0xFF
#define U16_MAX 0xFFFF
#define U32_MAX 0xFFFFFFFF
#define U64_MAX 0xFFFFFFFFFFFFFFFF

#define I8_MAX 0x7F
#define I16_MAX 0x7FFF
#define I32_MAX 0x7FFFFFFF
#define I64_MAX 0x7FFFFFFFFFFFFFFF
#define I8_MIN -I8_MAX-1
#define I16_MIN -I16_MAX-1
#define I32_MIN -I32_MAX-1
#define I64_MIN -I64_MAX-1


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

f32 sign_f32(f32 a) {
  f32 result = 0;
  if (a > 0) result = 1;
  if (a < 0) result = -1;
  return result;
}

i32 sign_i32(i32 a) {
  i32 result = 0;
  if (a > 0) result = 1;
  if (a < 0) result = -1;
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

v2 v2_perp(v2 a) {
  v2 result = V2(-a.y, a.x);
  return result;
}

v2 v2_zero() {return V2(0, 0);}
v2 v2_right() {return V2(1, 0);}
v2 v2_left() {return V2(-1, 0);}
v2 v2_up() {return V2(0, 1);}
v2 v2_down() {return V2(0, -1);}


typedef struct {
  f32 min;
  f32 max;
} Range;

Range inverted_infinity_range() {
  Range result;
  result.min = INFINITY;
  result.max = -INFINITY;
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

v3 v2_to_v3(v2 a, f32 z) {
  v3 result;
  result.x = a.x;
  result.y = a.y;
  result.z = z;
  return result;
}


v3 v3_zero() {return V3(0, 0, 0);}
v3 v3_right() {return V3(1, 0, 0);}
v3 v3_left() {return V3(-1, 0, 0);}
v3 v3_up() {return V3(0, 1, 0);}
v3 v3_down() {return V3(0, -1, 0);}
v3 v3_forward() {return V3(0, 0, 1);}
v3 v3_backward() {return V3(0, 0, -1);}


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

v4 v2_to_v4(v2 v, f32 z, f32 w) {
  v4 result;
  result.xy = v;
  result.z = z;
  result.w = w;
  return result;
}

// rotors
typedef struct {
  f32 dot;
  f32 xy;
} rotor2;

typedef struct {
  f32 dot;
  f32 xy;
  f32 xz;
  f32 yz;
} rotor3;

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


// mat2x2

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


// mat4x4
typedef union {
  f32 e[16];
  struct {
    f32 e00; f32 e10; f32 e20; f32 e30;
    f32 e01; f32 e11; f32 e21; f32 e31;
    f32 e02; f32 e12; f32 e22; f32 e32;
    f32 e03; f32 e13; f32 e23; f32 e33;
  };
} mat4x4;

mat4x4 Mat4x4(f32 e00, f32 e10, f32 e20, f32 e30,
              f32 e01, f32 e11, f32 e21, f32 e31,
              f32 e02, f32 e12, f32 e22, f32 e32,
              f32 e03, f32 e13, f32 e23, f32 e33) {
  mat4x4 result;
  result.e00 = e00;
  result.e10 = e10;
  result.e20 = e20;
  result.e30 = e30,
  
  result.e01 = e01;
  result.e11 = e11;
  result.e21 = e21;
  result.e31 = e31;
  
  result.e02 = e02;
  result.e12 = e12;
  result.e22 = e22;
  result.e32 = e32;
  
  result.e03 = e03;
  result.e13 = e13;
  result.e23 = e23;
  result.e33 = e33;
  
  return result;
}

mat2x2 mat4x4_to_mat2x2(mat4x4 m) {
  mat2x2 result;
  result.e00 = m.e00;
  result.e01 = m.e01;
  result.e10 = m.e10;
  result.e11 = m.e11;
  return result;
}

v4 mat4x4_mul_v4(mat4x4 m, v4 v) {
  v4 result;
  result.x = v.x*m.e00 + v.y*m.e10 + v.z*m.e20 + v.w*m.e30;
  result.y = v.x*m.e01 + v.y*m.e11 + v.z*m.e21 + v.w*m.e31;
  result.z = v.x*m.e02 + v.y*m.e12 + v.z*m.e22 + v.w*m.e32;
  result.w = v.x*m.e03 + v.y*m.e13 + v.z*m.e23 + v.w*m.e33;
  return result;
}


// TODO(lvl5): simd this shit?
mat4x4 mat4x4_mul_mat4x4(mat4x4 a, mat4x4 b) {
  mat4x4 result;
  
  result.e00 = a.e00*b.e00 + a.e10*b.e01 + a.e20*b.e02 + a.e30*b.e03;
  result.e10 = a.e00*b.e10 + a.e10*b.e11 + a.e20*b.e12 + a.e30*b.e13;
  result.e20 = a.e00*b.e20 + a.e10*b.e21 + a.e20*b.e22 + a.e30*b.e23;
  result.e30 = a.e00*b.e30 + a.e10*b.e31 + a.e20*b.e32 + a.e30*b.e33;
  
  result.e01 = a.e01*b.e00 + a.e11*b.e01 + a.e21*b.e02 + a.e31*b.e03;
  result.e11 = a.e01*b.e10 + a.e11*b.e11 + a.e21*b.e12 + a.e31*b.e13;
  result.e21 = a.e01*b.e20 + a.e11*b.e21 + a.e21*b.e22 + a.e31*b.e23;
  result.e31 = a.e01*b.e30 + a.e11*b.e31 + a.e21*b.e32 + a.e31*b.e33;
  
  result.e02 = a.e02*b.e00 + a.e12*b.e01 + a.e22*b.e02 + a.e32*b.e03;
  result.e12 = a.e02*b.e10 + a.e12*b.e11 + a.e22*b.e12 + a.e32*b.e13;
  result.e22 = a.e02*b.e20 + a.e12*b.e21 + a.e22*b.e22 + a.e32*b.e23;
  result.e32 = a.e02*b.e30 + a.e12*b.e31 + a.e22*b.e32 + a.e32*b.e33;
  
  result.e03 = a.e03*b.e00 + a.e13*b.e01 + a.e23*b.e02 + a.e33*b.e03;
  result.e13 = a.e03*b.e10 + a.e13*b.e11 + a.e23*b.e12 + a.e33*b.e13;
  result.e23 = a.e03*b.e20 + a.e13*b.e21 + a.e23*b.e22 + a.e33*b.e23;
  result.e33 = a.e03*b.e30 + a.e13*b.e31 + a.e23*b.e32 + a.e33*b.e33;
  return result;
}

mat4x4 mat4x4_identity() {
  mat4x4 result = Mat4x4(1, 0, 0, 0,
                         0, 1, 0, 0,
                         0, 0, 1, 0,
                         0, 0, 0, 1);
  return result;
}

mat4x4 mat4x4_scale(mat4x4 m, v3 scale) {
  mat4x4 scale_m = Mat4x4(scale.x, 0,         0,         0,
                          0,         scale.y, 0,         0,
                          0,         0,         scale.z, 0,
                          0,         0,         0,         1.0f);
  
  mat4x4 result = mat4x4_mul_mat4x4(m, scale_m);
  return result;
}

mat4x4 mat4x4_rotate(mat4x4 m, f32 angle) {
  f32 cos = cos_f32(-angle);
  f32 sin = sin_f32(-angle);
  mat4x4 rotate_m = Mat4x4(cos,  sin,  0,    0,
                           -sin, cos,  0,    0,
                           0,    0,    1.0f, 0,
                           0,    0,    0,    1.0f);
  mat4x4 result = mat4x4_mul_mat4x4(m, rotate_m);
  return result;
}

mat4x4 mat4x4_translate(mat4x4 m, v3 p) {
  mat4x4 translate_m = Mat4x4(1.0f, 0,    0,    p.x,
                              0,    1.0f, 0,    p.y,
                              0,    0,    1.0f, p.z,
                              0,    0,    0,    1.0f);
  mat4x4 result = mat4x4_mul_mat4x4(m, translate_m);
  return result;
}

mat4x4 mat4x4_transpose(mat4x4 m) {
  mat4x4 result;
  for (i32 y = 0; y < 4; y++) {
    for (i32 x = 0; x < 4; x++) {
      result.e[y*4 + x] = m.e[x*4 + y];
    }
  }
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

v2 rect2_get_center(rect2 r) {
  v2 result = v2_add(r.min, v2_mul_s(rect2_get_size(r), 0.5f));
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




// NOTE(lvl5): misc
f32 lerp_f32(f32 a, f32 b, f32 c) {
  f32 result = a*(1 - c) + b*c;
  return result;
}

v2 lerp_v2(v2 a, v2 b, v2 c) {
  v2 result;
  result.x = lerp_f32(a.x, b.x, c.x);
  result.y = lerp_f32(a.y, b.y, c.y);
  return result;
}


v3 lerp_v3(v3 a, v3 b, v3 c) {
  v3 result;
  result.x = lerp_f32(a.x, b.x, c.x);
  result.y = lerp_f32(a.y, b.y, c.y);
  result.z = lerp_f32(a.z, b.z, c.z);
  return result;
}


v4 lerp_v4(v4 a, v4 b, v4 c) {
  v4 result;
  result.x = lerp_f32(a.x, b.x, c.x);
  result.y = lerp_f32(a.y, b.y, c.y);
  result.z = lerp_f32(a.z, b.z, c.z);
  result.w = lerp_f32(a.w, b.w, c.w);
  return result;
}



#define LVL5_MATH
#endif