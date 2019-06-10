#ifndef RENDERER_H

#include "platform.h"
#include "lvl5_math.h"
#include "font.h"



typedef struct {
  v3 p;
} Quad_Vertex;

typedef struct {
  mat4x4 model;
  v2 tex_offset;
  v2 tex_scale;
  v4 color;
} Quad_Instance;


typedef struct {
  v3 p;
  v3 scale;
  f32 angle;
} Transform;

Transform transform_default() {
  Transform t;
  t.p = V3(0, 0, 0);
  t.angle = 0;
  t.scale = V3(1, 1, 1);
  return t;
}

typedef struct {
  Texture_Atlas *atlas;
  i32 index;
  v2 origin;
} Sprite;



typedef enum {
  Render_Type_NONE,
  Render_Type_Rect,
  Render_Type_Sprite,
} Render_Type;

typedef struct {
  rect2 rect;
} Render_Rect;

typedef struct {
  Sprite sprite;
} Render_Sprite;

typedef struct {
  mat4x4 matrix;
  v4 color;
  Font *font;
} Render_State;

typedef struct {
  union {
    Render_Rect Rect;
    Render_Sprite Sprite;
  };
  Render_Type type;
  Render_State state;
} Render_Item;

typedef struct {
  Transform t;
} Camera;

typedef struct {
  Texture_Atlas *debug_atlas;
  Render_Item *items;
  i32 item_count;
  i32 item_capacity;
  
  v2 screen_size;
  Camera *camera;
  
  Render_State state;
  Render_State state_stack[16];
  i32 state_stack_count;
} Render_Group;


typedef struct {
  u32 vertex_vbo;
  u32 instance_vbo;
  u32 vao;
  u32 texture;
  u32 shader;
} Quad_Renderer;









#define RENDERER_H
#endif