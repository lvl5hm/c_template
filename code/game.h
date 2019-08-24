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

typedef struct {
  Animation *animations;
  i32 animation_count;
} Animation_Pack;

typedef struct {
  f32 *weights;
  f32 *positions;
} Animation_Instance;



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

// TODO(lvl5): should every entity part be an entity?
// every entity part has a collider, when it animates, the collider moves with it


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
  [Ai_State_WALK_TO] = "walk_to",
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

typedef enum {
  Entity_Team_NONE,
  Entity_Team_PLAYER,
  Entity_Team_ENEMY,
} Entity_Team;

typedef struct {
  i32 id;
  i32 index;
} Entity_Handle;

#define NULL_ENTITY_HANDLE (Entity_Handle){.id = 0, .index = 0}
#define NULL_SPRITE (Sprite){0}

typedef struct Entity_Part Entity_Part;
struct Entity_Part {
  Sprite sprite;
  Collider collider;
  Transform t;
  
  Entity_Part *children;
  i32 child_count;
};

#define MISC_ENTITY_STORAGE_SIZE kilobytes(40)

typedef struct {
  byte data[MISC_ENTITY_STORAGE_SIZE];
  Arena arena;
} Misc_Entity_Storage;

typedef struct {
  b32 is_active;
  i32 id;
  i32 index;
  
  Transform t;
  v3 d_p;
  f32 d_angle;
  f32 speed;
  
  Animation_Instance instance;
  Entity_Part *parts;
  i32 part_count;
  
  Collider collider;
  Skill skills[4];
  
  Resource hp;
  Resource mp;
  
  f32 contact_damage;
  
  Controller_Type controller_type;
  Ai_State ai_state;
  f32 ai_progress;
  Entity_Handle aggro_handle;
  v3 target_p;
  v3 target_move_p;
  
  f32 friction;
  u64 flags;
  Entity_Team team;
  
  Lifetime lifetime;
  
  i32 misc_storage_index;
  union {
    struct {
      i32 *penetrated_entity_ids;
    } penetrating_projectile;
  };
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
  
  Misc_Entity_Storage misc_entity_storages[MAX_ENTITY_COUNT/10];
  i32 misc_entity_storage_count;
  
  i32 misc_entity_storage_free_list[MAX_ENTITY_COUNT/10];
  i32 misc_entity_storage_free_count;
  
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


typedef enum {
  Asset_NONE,
  Asset_ROBOT,
  Asset_GRASS,
} Asset_Id;

typedef struct {
  Arena arena;
  
  Sprite *sprites;
  Sound *sounds;
  Font *fonts;
} Assets;

typedef i32 Sprite_Id;
typedef i32 Sound_Id;
typedef i32 Font_Id;

Sprite asset_get_sprite(Assets *assets, Sprite_Id id) {
  Sprite result = {0};
  return result;
}


#define GAME_H
#endif