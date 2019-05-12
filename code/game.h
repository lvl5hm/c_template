#ifndef GAME_H

#include "platform.h"
#include "lvl5_math.h"
#include "renderer.h"
#include "lvl5_random.h"
#include "font.h"
#include "audio.h"

typedef enum {
  Entity_Type_NONE,
  Entity_Type_PLAYER,
  Entity_Type_ENEMY,
} Entity_Type;


typedef struct {
  i32 index;
  Entity_Type type;
  b32 is_active;
  Transform t;
} Entity;


typedef struct {
  f32 position;  // 0 - 1
  Transform t;
  v4 color;
} Animation_Frame;

typedef struct {
  f32 speed;
  Animation_Frame *frames;
  i32 frame_count;
} Animation;


typedef enum {
  Robot_Animation_IDLE,
  Robot_Animation_WALK,
  Robot_Animation_ATTACK,
  
  Robot_Animation_COUNT,
} Robot_Animation;

typedef enum {
  Robot_Part_RIGHT_LEG,
  Robot_Part_LEFT_LEG,
  Robot_Part_BODY,
  Robot_Part_EYE,
  
  Robot_Part_COUNT,
} Robot_Part;

typedef struct {
  Animation *animations;
  i32 animation_count;
} Animation_Pack;

typedef struct {
  f32 *weights;
  f32 *positions;
} Animation_Instance;

typedef struct {
  Sound test_sound;
  Sound_State sound_state;
  b32 is_initialized;
  Arena arena;
  Arena scratch;
  Arena temp;
  
  Entity entities[40000];
  i32 entity_count;
  
  GLuint shader_basic;
  
  Texture_Atlas atlas;
  Quad_Renderer renderer;
  
  
  Font font;
  Texture_Atlas debug_atlas;
  
  Sprite spr_robot_torso;
  Sprite spr_robot_leg;
  Sprite spr_robot_eye;
  
  Animation_Pack robot_parts[Robot_Part_COUNT];
  Animation_Instance robot_anim;
  
  Rand rand;
  
  u32 frame_count;
} State;




#define GAME_H
#endif