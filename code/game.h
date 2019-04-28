#ifndef GAME_H

#include "platform.h"
#include "lvl5_math.h"


typedef struct {
  v3 p;
} Quad_Vertex;

typedef struct {
  mat4x4 model;
  v2 tex_offset;
  v2 tex_scale;
  v4 color;
  v2 origin;
} Quad_Instance;

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
  v3 p;
  v3 scale;
  f32 angle;
} Transform;

typedef struct {
  i32 index;
  Entity_Type type;
  b32 is_active;
  Transform t;
} Entity;

typedef struct {
  Bitmap bmp;
  rect2 *rects;
  i32 sprite_count;
} TextureAtlas;

typedef struct {
  TextureAtlas *atlas;
  i32 index;
  v2 origin;
} Sprite;


rect2 sprite_get_rect(Sprite spr) {
  rect2 result = spr.atlas->rects[spr.index];
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
  v4 color;
  Sprite sprite;
} Render_Sprite;

typedef struct {
  union {
    Render_Rect Rect;
    Render_Sprite Sprite;
  };
  Render_Type type;
  mat4x4 matrix;
} Render_Item;

typedef struct {
  Render_Item *items;
  i32 item_count;
  i32 item_capacity;
  mat4x4 matrix;
  
  v2 screen_size;
} Render_Group;


typedef struct {
  u32 vertex_vbo;
  u32 instance_vbo;
  u32 vao;
  u32 texture;
  u32 shader;
} Quad_Renderer;





typedef struct {
  f32 position;  // 0 - 1
  Transform t;
  v4 color;
} Animation_Frame;

typedef struct {
  Animation_Frame *frames;
  i32 frame_count;
} Animation;

typedef struct {
  f32 position;
  Animation *anim;
} Animation_Instance;

Animation_Frame animation_get_frame(Animation_Instance *inst) {
  Animation *anim = inst->anim;
  f32 position = inst->position;
  
  i32 prev_index = -1;
  while ((prev_index < anim->frame_count) && (position > anim->frames[prev_index+1].position)) {
    prev_index++;
  }
  
  Animation_Frame result;
  if (prev_index == -1) {
    result = anim->frames[0];
  } else if (prev_index == anim->frame_count) {
    result = anim->frames[anim->frame_count-1];
  } else {
    Animation_Frame prev = anim->frames[prev_index];
    Animation_Frame next = anim->frames[prev_index+1];
    f32 c = (position - prev.position)/(next.position - prev.position);
    
    result.t.p = lerp_v3(prev.t.p, next.t.p, V3(c, c, c));
    result.t.scale = lerp_v3(prev.t.scale, next.t.scale, V3(c, c, c));
    result.t.angle = lerp_f32(prev.t.angle, next.t.angle, c);
    result.color = lerp_v4(prev.color, next.color, V4(c, c, c, c));
  }
  
  return result;
}


typedef struct {
  b32 is_initialized;
  Arena arena;
  Arena scratch;
  Arena temp;
  
  
  Entity entities[40000];
  i32 entity_count;
  
  GLuint shader_basic;
  
  TextureAtlas atlas;
  Quad_Renderer renderer;
  
  Animation robot_leg_animation;
  Animation_Instance left_leg_anim;
  Animation_Instance right_leg_anim;
} State;



Transform transform_default() {
  Transform t;
  t.p = V3(0, 0, 0);
  t.angle = 0;
  t.scale = V3(1, 1, 1);
  return t;
}

mat4x4 transform_apply(mat4x4 matrix, Transform t) {
  mat4x4 result = matrix;
  result = mat4x4_translate(result, t.p);
  result = mat4x4_scale(result, t.scale);
  result = mat4x4_rotate(result, t.angle);
  return result;
}


void render_translate(Render_Group *group, v3 p) {
  group->matrix = mat4x4_translate(group->matrix, p);
}

void render_scale(Render_Group *group, v3 scale) {
  group->matrix = mat4x4_scale(group->matrix, scale);
}

void render_rotate(Render_Group *group, f32 angle) {
  group->matrix = mat4x4_rotate(group->matrix, angle);
}

void render_transform(Render_Group *group, Transform t) {
  group->matrix = transform_apply(group->matrix, t);
}

#define GAME_H
#endif