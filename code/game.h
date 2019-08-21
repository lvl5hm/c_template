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
  f32 v;
  f32 max;
  f32 regen;
} Resource;

typedef struct {
  Skill_Type type;
  f32 damage;
  f32 accuracy;
  
  Rune_Type runes[3];
  
  f32 mp_cost;
  Resource cooldown;
} Skill;

typedef struct {
  b32 active;
  f32 time;
} Lifetime;

typedef enum {
  Ai_State_NONE,
  Ai_State_IDLE,
  Ai_State_WALK_TO,
  Ai_State_ATTACK,
  Ai_State_TELE,
  Ai_State_ALERT,
} Ai_State;

char *Ai_State_to_string[] = {
  [Ai_State_ALERT] = "alert",
  [Ai_State_NONE] = "none",
  [Ai_State_IDLE] = "idle",
  [Ai_State_WALK_TO] = "walt_to",
  [Ai_State_ATTACK] = "attack",
  [Ai_State_TELE] = "tele",
};

typedef enum {
  Controller_Type_NONE,
  Controller_Type_PLAYER,
  Controller_Type_AI_SHOOTER,
  Controller_Type_AI_ZOMBIE,
} Controller_Type;

typedef enum {
  Entity_Flag_NONE = 0,
  Entity_Flag_PLAYER = 1 << 0,
  Entity_Flag_PROJECTILE = 1 << 1,
  Entity_Flag_ACTOR = 1 << 2,
} Entity_Flag;

typedef struct {
  b32 is_active;
  i32 id;
  
  Transform t;
  v3 d_p;
  f32 d_angle;
  f32 speed;
  
  Collider collider;
  Skill skills[4];
  
  Resource hp;
  Resource mp;
  
  f32 contact_damage;
  
  Controller_Type controller_type;
  Ai_State ai_state;
  f32 ai_progress;
  i32 aggro_entity_id;
  v3 target_p;
  v3 target_move_p;
  
  f32 friction;
  u64 flags;
  
  Lifetime lifetime;
} Entity;

typedef enum {
  Terrain_Kind_NONE,
  Terrain_Kind_GRASS,
  Terrain_Kind_WALL,
} Terrain_Kind;

typedef struct {
  Terrain_Kind terrain;
} Tile;

#define CHUNK_SIZE 8
#define TILE_SIZE_IN_METERS 1

typedef struct {
  Tile tiles[CHUNK_SIZE*CHUNK_SIZE];
  i32 x;
  i32 y;
} Tile_Chunk;

typedef struct {
  Tile_Chunk *chunks;
} Tile_Map;

typedef struct {
  i32 chunk_x;
  i32 chunk_y;
  i32 tile_x;
  i32 tile_y;
} Tile_Position;

#define MAX_ENTITY_COUNT 10000

typedef struct {
  i32 unique_entity_id;
  Tile_Map tile_map;
  
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
  Sprite spr_wall;
  Sprite spr_grass;
  
  Animation_Pack robot_parts[Robot_Part_COUNT];
  Animation_Instance robot_anim;
  
  Rand rand;
  u32 frame_count;
  
  Camera camera;
} State;




#define GAME_H
#endif