#ifndef GAME_H

#include "platform.h"
#include "lvl5_math.h"
#include "renderer.h"
#include "lvl5_random.h"
#include "font.h"
#include "sound.h"

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

// TODO(lvl5): should every entity part be an entity?
// every entity has a collider, when it animates, the collider moves with it


typedef struct {
  v2 v[16];
  i32 count;
} Polygon;

typedef struct {
  f32 r;
  v2 origin;
} Circle_Collider;

typedef struct {
  rect2 rect;
} Box_Collider;

typedef enum {
  Collider_Type_NONE,
  Collider_Type_CIRCLE,
  Collider_Type_BOX,
  Collider_Type_POINT,
} Collider_Type;

typedef struct {
  union {
    Box_Collider box;
    Circle_Collider circle;
    v2 point;
  };
  Collider_Type type;
} Collider;


typedef enum {
  Skill_Type_NONE,
  Skill_Type_FIREBALL,
  Skill_Type_BLINK,
  Skill_Type_BOMB,
  Skill_Type_LIGHTNING,
} Skill_Type;

typedef enum {
  Rune_Type_NONE,
  Rune_Type_DOUBLE_DAMAGE,
  Rune_Type_HALF_COST,
} Rune_Type;

typedef struct {
  Skill_Type type;
  f32 damage;
  
  Rune_Type runes[3];
  f32 mp_cost;
} Skill;

typedef struct {
  b32 active;
  f32 time;
} Lifetime;

typedef struct {
  f32 v;
  f32 max;
  f32 regen;
} Resource;

typedef struct {
  i32 index;
  b32 is_active;
  
  Transform t;
  v3 d_p;
  f32 d_angle;
  
  Collider collider;
  Skill skills[4];
  
  Resource hp;
  Resource mp;
  
  f32 contact_damage;
  
  v2 target_p;
  b32 player_controller;
  f32 friction;
  
  Lifetime lifetime;
} Entity;


#define MAX_ENTITY_COUNT 10000

typedef struct {
  Input empty_input;
  Sound test_sound;
  Sound snd_bop;
  
  Sound_State sound_state;
  b32 is_initialized;
  Arena arena;
  Arena scratch;
  Arena temp;
  
  Entity entities[MAX_ENTITY_COUNT];
  i32 entity_count;
  
  i32 entities_free_list[MAX_ENTITY_COUNT];
  i32 entities_free_count;
  
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
  
  Camera camera;
} State;




#define GAME_H
#endif