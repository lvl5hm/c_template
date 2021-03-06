#ifndef RENDERER_H

#include "platform.h"
#include "lvl5_math.h"
#include "font.h"

#define COLOR_WHITE (v4){1, 1, 1, 1}
#define COLOR_BLACK (v4){0, 0, 0, 1}
#define COLOR_RED (v4){1, 0, 0, 1}
#define COLOR_GREEN (v4){0, 1, 0, 1}
#define COLOR_BLUE (v4){0, 0, 1, 1}


typedef struct {
  v3 p;
} Quad_Vertex;

typedef struct {
  mat4 model;
  u16 tex_x;
  u16 tex_y;
  u16 tex_width;
  u16 tex_height;
  u32 color;
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


typedef struct {
  Transform t;
  v4 color;
  
  Transform d_t;
  v4 d_color;
  
  f32 lifetime;
} Particle;

typedef struct {
  Sprite sprite;
  Particle *particles;
  i32 particle_count;
  i32 particle_capacity;
} Particle_Emitter;



typedef enum {
  Render_Type_NONE,
  Render_Type_Rect,
  Render_Type_Sprite,
  Render_Type_Text,
  Render_Type_Particle_Emitter,
} Render_Type;

typedef struct {
  rect2 rect;
} Render_Rect;

typedef struct {
  Sprite sprite;
} Render_Sprite;

typedef struct {
  String text;
} Render_Text;

typedef struct {
  mat4 matrix;
  v4 color;
  Font *font;
} Render_State;

typedef struct {
  Particle_Emitter *emitter;
  f32 dt;
} Render_Particle_Emitter;

typedef struct {
  union {
    Render_Rect Rect;
    Render_Sprite Sprite;
    Render_Text Text;
    Render_Particle_Emitter Particle_Emitter;
  };
  Render_Type type;
  Render_State state;
} Render_Item;

typedef struct {
  f32 far;
  f32 near;
  v2 scale;
  
  f32 angle;
  v3 p;
} Camera;

typedef struct {
  Texture_Atlas *debug_atlas;
  Render_Item *items;
  i32 item_count;
  i32 item_capacity;
  i32 expected_quad_count;
  
  Camera *camera;
  v2 screen_size;
  
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