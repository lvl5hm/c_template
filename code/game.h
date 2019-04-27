#ifndef GAME_H

#include "platform.h"
#include "lvl5_math.h"


typedef struct {
  v2 p;
  v2 tex_coord;
  v4 color;
  f32 transform_index;
} Vertex;
#if 0
typedef struct {
  v2 p;
  v2 tex;
} Quad_Vertex;

typedef struct {
  mat3x3 model;
  v2 tex_offset;
  v2 tex_scale;
  v4 color;
} Quad_Instance;
#endif

typedef enum {
  Entity_Type_NONE,
  Entity_Type_PLAYER,
  Entity_Type_ENEMY,
} Entity_Type;


typedef struct {
  i32 width;
  i32 height;
  byte *data;
} Bitmap;

typedef struct {
  v2 scale;
  f32 angle;
  v2 p;
} Transform;

typedef struct {
  i32 index;
  Entity_Type type;
  b32 is_active;
  Transform t;
} Entity;

typedef struct {
  Bitmap bmp;
  rect2i *sprites;
  i32 sprite_count;
} TextureAtlas;

typedef struct {
  TextureAtlas *atlas;
  i32 index;
} Sprite;


rect2 sprite_get_normalized_rect(Sprite spr) {
  rect2 rect = rect2i_to_rect2(spr.atlas->sprites[spr.index]);
  v2 inv_size = v2_invert(V2((f32)spr.atlas->bmp.width, (f32)spr.atlas->bmp.height));
  
  rect2 result;
  result.min = v2_hadamard(rect.min, inv_size);
  result.max = v2_hadamard(rect.max, inv_size);
  
  return result;
}



typedef enum {
  Render_Type_NONE,
  Render_Type_Rect,
  Render_Type_Sprite,
} Render_Type;

typedef struct {
  rect2 rect;
  v4 color;
} Render_Rect;

typedef struct {
  rect2 rect;
  v4 color;
  Sprite sprite;
} Render_Sprite;

typedef struct {
  union {
    Render_Rect Rect;
    Render_Sprite Sprite;
  };
  Render_Type type;
  Transform t;
} Render_Item;

typedef struct {
  Render_Item *items;
  i32 item_count;
  Transform t;
  v2 screen_size;
} Render_Group;


typedef struct {
  b32 is_initialized;
  Arena arena;
  Arena scratch;
  Arena temp;
  
  
  Entity entities[128];
  i32 entity_count;
  
  GLuint shader_basic;
  
  TextureAtlas atlas;
} State;



mat3x3 transform_get_matrix(Transform t) {
  f32 cos = cos_f32(t.angle);
  f32 sin = sin_f32(t.angle);
  mat3x3 rotate_m = Mat3x3(cos, sin, 0,
                           -sin, cos, 0,
                           0, 0, 1.0f);
  
  mat3x3 scale_m = Mat3x3(t.scale.x, 0, 0,
                          0, t.scale.y, 0,
                          0, 0, 1.0f);
  
  
  mat3x3 translate_m = Mat3x3(1.0f, 0, t.p.x,
                              0, 1.0f, t.p.y,
                              0, 0, 1.0f);
  mat3x3 transform_m = mat3x3_mul_mat3x3(translate_m,
                                         mat3x3_mul_mat3x3(rotate_m, scale_m));
  
  return transform_m;
}


#define GAME_H
#endif