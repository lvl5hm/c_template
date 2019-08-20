#define LVL5_DEBUG

#include "platform.h"
#include "game.h"
#include "lvl5_math.h"
#include "lvl5_opengl.h"
#include "lvl5_stretchy_buffer.h"
#include "lvl5_random.h"

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

#include "debug.c"
#include "sound.c"

/*
TODO:

there probably is some memory corruption bug that sometimes sets 
audio_state.sound_count to 64

---- ENGINE ----
[ ] add separate render group for UI
[ ] linebreak size should not depend on PIXELS_PER_METER


[ ] multithreading
 -[x] thread queues in the windows layer
 -[ ] threaded things in the platform layer
 -[ ] all threads can do low priority queue, but
 they wake up from time to time to see if there's work in high priority
 
 [ ] debug
 -[x] performance counters
 -[x] logging
 -[x] performance graphs
 -[x] something like interactive flame charts
 -[ ] debug variables and widgets
 -[x] console?
 -[ ] will probably have to do some introspection and metaprogramming
 -[ ] needs to work with multithreading
 -[ ] loading from a save point to debug a slow frame
 
 [ ] animation
 -[ ] clean up
 -[ ] animation editor
 -[ ] b-splines (bezier curves)
 
 [ ] text rendering
 -[ ] kerning (i added this, but for some reason stb doesn't return good kerning)
 -[ ] line spacing
 -[ ] utf-8 support
 -[ ] rendering needs to be way more optimized
 -[ ] loading fonts from windows instead of stb
 
 [ ] windows layer
 -[x] proper opengl context creation for multisampling
 
 [ ] renderer
 -[ ] z-sorting
 -[ ] more shader stuff (materials, etc)
 -[ ] lighting
 --[ ] normal maps
 --[ ] phong shading
 
 [ ] assets
 -[ ] better texture map packing
 -[ ] live reload
 -[ ] asset file format
 --[ ] bitmaps
 --[ ] sounds
 --[ ] fonts
 --[ ] shaders
 -[ ] asset builder
 -[ ] streaming?
 -[ ] splitting bit sounds into chunks?
 
 [x] sounds
 -[x] basic mixer
 -[x] volume
 -[x] speed shifting
 -[x] SSE?
 
 ---- GAME ----
 [x] camera
 [x] basic player movement
 [x] player active abilities
 [ ] ability rune examples
 [ ] basic enemy AI and abilities
 [ ] some basic GUI
 -[x] hp/mana
 -[ ] gold/exp?
 -[ ] abilities and cooldowns
 -[ ] inventory and using runes
 [ ] world (probably tiles)
 [ ] buffs/debuffs
 [ ] damage over time
 [ ] auras
 [ ] ranged projectiles and melee strikes (how do runes affect each?)
 
 [ ] collision
 -[x] SAT
 -[x] GJK
 -[ ] removed 2D physics stuff for now. If I decide to go full physics,
 I would probably need to do GJK/EPA/find contact manifold using some method
 and I need to learn A LOT about rigidbody simulation if I want proper physics
 
 -[ ] tilemap collision
 -[ ] collision rules?
 -[ ] grid / quadtree
 
 [ ] basic procedural generation
 [ ] 5 enemy types
 [ ] first level boss
 [ ] sounds for abilities/enemies
 [ ] sound position volume shifting
 [ ] shops
 [ ] using exp for leveling up abilities or adding stats?
 [ ] damage/pick up popup text
 
 
*/

globalvar Render_Group *global_group = 0;

Entity *get_entity(State *state, i32 index) {
  assert(index < state->entity_count);
  assert(index != 0);
  Entity *result = state->entities + index;
  if (!result->is_active) {
    result = null;
  }
  return result;
}

Entity *add_entity(State *state) {
  assert(state->entity_count < array_count(state->entities));
  
  i32 index;
  if (state->entities_free_count) {
    index = state->entities_free_list[--state->entities_free_count];
  } else {
    index = state->entity_count++;
  }
  
  Entity *e = state->entities + index;
  zero_memory_slow(e, sizeof(Entity));
  
  e->is_active = true;
  e->t = transform_default();
  e->index = index;
  
  return e;
}

Entity *add_entity_player(State *state) {
  Entity *e = add_entity(state);
  
  e->hp.v = 10;
  e->mp.v = 10;
  e->hp.max = 10;
  e->mp.max = 10;
  e->hp.regen = 0;
  e->mp.regen = 1.0f;
  
  e->collider.box.rect = rect2_center_size(V2(0, 0), V2(1, 1));
  e->collider.type = Collider_Type_BOX;
  e->player_controller = true;
  e->friction = 0.92f;
  e->t.p.x = 3;
  
  e->skills[0] = (Skill){
    .type = Skill_Type_FIREBALL,
    .damage = 1,
    .mp_cost = 2.0f,
  };
  
  e->skills[1] = (Skill){
    .type = Skill_Type_BLINK,
    .mp_cost = 4.0f,
  };
  
  return e;
}

Entity *add_entity_box(State *state) {
  Entity *e = add_entity(state);
  return e;
}

Entity *add_entity_enemy(State *state) {
  Entity *e = add_entity(state);
  e->t.scale.xy = V2(3.0f, 0.5f);
  
  e->collider.circle.r = 0.5f;
  e->collider.circle.origin = V2(0, 0);
  e->collider.type = Collider_Type_CIRCLE;
  
  return e;
}

Entity *add_entity_fireball(State *state) {
  Entity *e = add_entity(state);
  
  
  e->collider.circle.r = 0.3f;
  e->collider.type = Collider_Type_CIRCLE;
  e->lifetime.active = true;
  e->lifetime.time = 3.0f;
  
  return e;
}



void remove_entity(State *state, i32 index) {
  Entity *removed = state->entities + index;
  removed->is_active = false;
  state->entities_free_list[state->entities_free_count++] = index;
}


Animation_Frame animation_get_frame(Animation *anim, f32 main_position) {
  DEBUG_FUNCTION_BEGIN();
  f32 position = main_position*anim->speed;
  position -= (i32)position;
  
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
  
  DEBUG_FUNCTION_END();
  return result;
}


Animation_Frame animation_pack_get_frame(Animation_Pack pack, Animation_Instance inst) {
  DEBUG_FUNCTION_BEGIN();
  Animation_Frame result = {0};
  
  f32 total_weight = 0;
  for (i32 anim_index = 0; anim_index < pack.animation_count; anim_index++) {
    f32 weight = inst.weights[anim_index];
    total_weight += weight;
  }
  
  for (i32 anim_index = 0; anim_index < pack.animation_count; anim_index++) {
    f32 weight = inst.weights[anim_index];
    f32 position = inst.positions[anim_index];
    Animation anim = pack.animations[anim_index];
    Animation_Frame frame = animation_get_frame(&anim, position);
    f32 coeff = weight/total_weight;
    
    result.t.p = v3_add(result.t.p, v3_mul(frame.t.p, coeff));
    result.t.scale = v3_add(result.t.scale, v3_mul(frame.t.scale, coeff));
    result.t.angle = result.t.angle + frame.t.angle*coeff;
  }
  
  DEBUG_FUNCTION_END();
  return result;
}


void draw_robot(State *state, Render_Group *group) {
  DEBUG_FUNCTION_BEGIN();
  
  f32 leg_x = 0.1f;
  
  render_save(group);
  {
    Animation_Frame frame = animation_pack_get_frame(state->robot_parts[Robot_Part_RIGHT_LEG], 
                                                     state->robot_anim);
    
    Transform t = {0};
    t.p = v3_add(V3(leg_x, 0, 0), frame.t.p);
    t.scale = v3_hadamard(V3(0.25f, 0.25f, 1), frame.t.scale);
    t.angle = frame.t.angle;
    
    push_sprite(group, state->spr_robot_leg, t);
  }
  
  
  render_restore(group);
  
  render_save(group);
  render_scale(group, V3(-1, 1, 1));
  
  {
    Animation_Instance inst = state->robot_anim;
    f32 *oldpos = inst.positions;
    inst.positions = arena_push_array(&state->scratch, f32, 2);
    copy_memory_slow(inst.positions, oldpos, sizeof(f32)*2);
    inst.positions[Robot_Animation_WALK] += 0.5f;
    
    Animation_Frame frame = animation_pack_get_frame(state->robot_parts[Robot_Part_RIGHT_LEG], 
                                                     inst);
    
    Transform t = {0};
    t.p = v3_add(V3(leg_x, 0, 0),
                 v3_hadamard(frame.t.p, V3(-1, 1, 1)));
    t.scale = v3_hadamard(V3(0.25f, 0.25f, 1), frame.t.scale);
    t.angle = frame.t.angle;
    
    push_sprite(group, state->spr_robot_leg, t);
  }
  
  render_restore(group);
  
  
  Animation_Frame frame = animation_pack_get_frame(state->robot_parts[Robot_Part_BODY],
                                                   state->robot_anim);
  render_save(group);
  render_translate(group, V3(0, 0.25f, 0));
  render_transform(group, frame.t);
  
  {
    Transform t = {0};
    t.scale = V3(0.5f, 0.75f, 1);
    
    push_sprite(group, state->spr_robot_torso, t);
  }
  
  {
    Animation_Frame frame = animation_pack_get_frame(state->robot_parts[Robot_Part_EYE],
                                                     state->robot_anim);
    Transform t = frame.t;
    t.scale = V3(0.2f, 0.2f, 1);
    push_sprite(group, state->spr_robot_eye, t);
  }
  
  render_restore(group);
  
  DEBUG_FUNCTION_END();
}

Sprite make_sprite(Texture_Atlas *atlas, i32 index, v2 origin) {
  Sprite result;
  result.atlas = atlas;
  result.index = index;
  result.origin = origin;
  return result;
}


b32 collide_aabb_aabb(rect2 a, rect2 b) {
  b32 result = !(a.max.x < b.min.x ||
                 a.max.y < b.min.y ||
                 a.min.x > b.max.x ||
                 a.min.y > b.max.y);
  return result;
}

v2 v2_transform(v2 v, Transform t) {
  mat4 matrix4 = transform_apply(mat4_identity(), t);
  v2 result = mat4_mul_v4(matrix4, v2_to_v4(v, 1, 1)).xy;
  return result;
}

v2 v2_transform_inverse(v2 v, Transform t) {
  mat4 matrix4 = transform_apply_inverse(mat4_identity(), t);
  v2 result = mat4_mul_v4(matrix4, v2_to_v4(v, 1, 1)).xy;
  return result;
}

Polygon aabb_transform(rect2 box, Transform t) {
  Polygon result;
  result.v[0] = box.min;
  result.v[1] = V2(box.min.x, box.max.y);
  result.v[2] = box.max;
  result.v[3] = V2(box.max.x, box.min.y);
  result.count = 4;
  
  mat4 matrix4 = transform_apply(mat4_identity(), t);
  
  for (i32 i = 0; i < result.count; i++) {
    result.v[i] = mat4_mul_v4(matrix4, v2_to_v4(result.v[i], 1, 1)).xy;
  }
  
  return result;
}

// NOTE(lvl5): GJK
v2 gjk_polygon_max_vertex_in_direction(Polygon a, v2 direction) {
  f32 max = -INFINITY;
  v2 result = {0};
  for (i32 i = 0; i < a.count; i++) {
    f32 dot = v2_dot(a.v[i], direction);
    if (dot > max) {
      max = dot;
      result = a.v[i];
    }
  }
  return result;
}

v2 gjk_circle_get_max_vertex_in_direction(Circle_Collider e, Transform t, v2 direction_unit) {
  v2 scale = v2_mul(t.scale.xy, e.r);
  
  v2 normal = v2_perp(v2_negate(direction_unit));
  normal = v2_rotate(normal, -t.angle);
  normal = v2_hadamard(normal, v2_invert(scale));
  
  v2 max = v2_unit(v2_perp(normal));
  max = v2_hadamard(max, scale);
  
  v2 origin =  v2_hadamard(e.origin, t.scale.xy);
  max = v2_add(max, origin);
  max = v2_rotate(max, t.angle);
  max = v2_add(max, t.p.xy);
  
  return max;
}

v2 gjk_collider_get_max(v2 direction, Collider coll, Transform t) {
  v2 result = {0};
  switch (coll.type) {
    case Collider_Type_CIRCLE: {
      v2 unit = v2_unit(direction);
      result = gjk_circle_get_max_vertex_in_direction(coll.circle, t,
                                                      unit);
    } break;
    
    case Collider_Type_BOX: {
      Polygon poly = aabb_transform(coll.box.rect, t);
      result = gjk_polygon_max_vertex_in_direction(poly, direction);
    } break;
    
    case Collider_Type_POINT: {
      result = coll.point;
    } break;
    
    default: assert(false);
  }
  return result;
}

v2 gjk_support(v2 direction, Collider a, Transform a_t, Collider b, Transform b_t) {
  DEBUG_FUNCTION_BEGIN();
  v2 a_max = gjk_collider_get_max(direction, a, a_t);
  v2 b_max = gjk_collider_get_max(v2_negate(direction), b, b_t);
  
  v2 result = v2_sub(a_max, b_max);
  DEBUG_FUNCTION_END();
  return result;
}

b32 gjk_collide(Collider a_coll, Transform a_t,
                Collider b_coll, Transform b_t) {
  DEBUG_FUNCTION_BEGIN();
  
  v2 simplex[3];
  i32 simplex_count = 1;
  v2 start_p = gjk_support(v2_right(), a_coll, a_t, b_coll, b_t);
  simplex[0] = start_p;
  v2 dir = v2_mul(start_p, -1);
  b32 result = true;
  while (true) {
    v2 p = gjk_support(dir, a_coll, a_t, b_coll, b_t);;
    if (v2_dot(p, dir) < 0) {
      result = false;
      break;
    } else {
      simplex[simplex_count++] = p;
      
      // NOTE(lvl5): do_simplex
      v2 o = v2_zero();
      if (simplex_count == 2) {
        v2 a = simplex[1];
        v2 b = simplex[0];
        v2 ab = v2_sub(b, a);
        v2 ao = v2_sub(o, a);
        dir = v2_perp_direction(ab, ao);
      } else {
        v2 a = simplex[2];
        v2 b = simplex[1];
        v2 c = simplex[0];
        v2 ao = v2_sub(o, a);
        v2 ab = v2_sub(b, a);
        v2 ac = v2_sub(c, a);
        v2 ab_perp = v2_perp_direction(ab, v2_negate(ac));
        v2 ac_perp = v2_perp_direction(ac, v2_negate(ab));
        
#define same_direction(v) v2_dot(v, ao) > 0
        
        f32 thick = 0.02f;
        
        if (same_direction(ab_perp)) {
          if (same_direction(ab)) {
            simplex[0] = a;
            simplex[1] = b;
            simplex_count = 2;
            dir = ab_perp;
          } else {
            if (same_direction(ac)) {
              simplex[0] = a;
              simplex[1] = c;
              simplex_count = 2;
              dir = ac_perp;
            } else {
              simplex[0] = a;
              simplex_count = 1;
              dir = ao;
            }
          }
        } else {
          if (same_direction(ac_perp)) {
            if (same_direction(ac)) {
              simplex[0] = a;
              simplex[1] = c;
              simplex_count = 2;
              dir = ac_perp;
            } else {
              simplex[0] = a;
              simplex_count = 1;
              dir = ao;
            }
          } else {
            result = true;
            break;
          }
        }
      }
    }
  }
  
  DEBUG_FUNCTION_END();
  return result;
}

String tsprintf(Arena *arena, char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  
  char *buffer = arena_push_array(arena, char, 256);
  vsprintf_s(buffer, 256, fmt, args);
  String result = make_string(buffer, c_string_length(buffer));
  
  va_end(args);
  
  return result;
}

b32 gjk_collide_point(v2 point, Collider a_coll, Transform a_t) {
  Collider b_coll;
  b_coll.type = Collider_Type_POINT;
  b_coll.point = point;
  b32 result = gjk_collide(a_coll, a_t, b_coll, transform_default());
  return result;
}

b32 skill_use(State *state, Entity *e, Skill *skill) {
  b32 result = false;
  
  f32 cost = skill->mp_cost;
  f32 damage = skill->damage;
  
  for (i32 rune_index = 0; rune_index < array_count(skill->runes); rune_index++) {
    Rune_Type rune = skill->runes[rune_index];
    switch (rune) {
      case Rune_Type_HALF_COST: {
        cost *= 0.5f;
      } break;
      case Rune_Type_DOUBLE_DAMAGE: {
        damage *= 2;
      } break;
    }
  }
  
  if (e->mp.v >= cost) {
    e->mp.v -= cost;
    result = true;
    switch (skill->type) {
      case Skill_Type_BLINK: {
        e->t.p.xy = e->target_p;
      } break;
      
      case Skill_Type_FIREBALL: {
        Entity *ball = add_entity_fireball(state);
        ball->t.p = e->t.p;
        
        v2 dir = v2_sub(e->target_p, e->t.p.xy);
        ball->t.angle = atan2_f32(dir.y, dir.x);
        ball->d_p = v2_to_v3(v2_mul(v2_unit(dir), 5.0f), 0);
        ball->contact_damage = damage;
      } break;
    }
  }
  
  return result;
}

rect2 get_bbox(Entity *e) {
  assert(e->collider.type == Collider_Type_BOX);
  Polygon poly = aabb_transform(e->collider.box.rect, e->t);
  
  rect2 result = rect2_inverted_infinity();
  for (i32 i = 0; i < poly.count; i++) {
    v2 corner = poly.v[i];
    if (corner.x < result.min.x) result.min.x = corner.x;
    if (corner.x > result.max.x) result.max.x = corner.x;
    if (corner.y < result.min.y) result.min.y = corner.y;
    if (corner.y > result.max.y) result.max.y = corner.y;
  }
  return result;
}

Tile_Position get_tile_position(v2 p) {
  p.x += 0.5f;
  p.y += 0.5f;
  Tile_Position result = {0};
  i32 round_x = floor_f32_i32(p.x/TILE_SIZE_IN_METERS);
  i32 round_y = floor_f32_i32(p.y/TILE_SIZE_IN_METERS);
  result.chunk_x = floor_f32_i32(p.x/CHUNK_SIZE*TILE_SIZE_IN_METERS);
  result.chunk_y = floor_f32_i32(p.y/CHUNK_SIZE*TILE_SIZE_IN_METERS);
  result.tile_x = round_x - result.chunk_x*CHUNK_SIZE;
  result.tile_y = round_y - result.chunk_y*CHUNK_SIZE;
  
  return result;
}

Tile *get_tile(Tile_Map *map, Tile_Position p) {
  // TODO(lvl5): hashtable
  Tile_Chunk *chunk = 0;
  for (u32 i = 0; i < sb_count(map->chunks); i++) {
    Tile_Chunk *test = map->chunks + i;
    if (test->x == p.chunk_x && test->y == p.chunk_y) {
      chunk = test;
      break;
    }
  }
  assert(chunk);
  Tile *result = chunk->tiles + p.tile_y*CHUNK_SIZE + p.tile_x;
  return result;
}

extern GAME_UPDATE(game_update) {
  State *state = (State *)memory.perm;
  debug_state = (Debug_State *)memory.debug;
  Arena *arena = &state->arena;
  
  if (memory.is_reloaded) {
    // NOTE(lvl5): transient memory can be destroyed at this point
    platform = _platform;
    gl = _platform.gl;
    scratch = &state->scratch;
    
#define SCRATCH_SIZE megabytes(10)
    arena_init(&state->scratch, memory.temp, SCRATCH_SIZE);
    arena_init(&state->temp, memory.temp + SCRATCH_SIZE, 
               memory.temp_size - SCRATCH_SIZE);
  }
  
  if (!state->is_initialized) {
    arena_init(&state->arena, memory.perm + sizeof(State), 
               memory.perm_size - sizeof(State));
    debug_init(&state->temp, memory.debug + sizeof(Debug_State));
    
    // NOTE(lvl5): when live reloading, the sound files can be overwritten
    // with garbage since it's located in temp arena right now
    //state->test_sound = load_wav(&state->temp, const_string("sounds/durarara.wav"));
    //sound_play(&state->sound_state, &state->test_sound, Sound_Type_MUSIC);
    state->snd_bop = load_wav(&state->temp, const_string("sounds/bop.wav"));
    
    debug_add_arena(&state->arena, const_string("main"));
    debug_add_arena(&state->temp, const_string("temp"));
    debug_add_arena(&debug_state->arena, const_string("debug_main"));
    debug_add_arena(&debug_state->gui.arena, const_string("debug_gui"));
  }
  
  if (!state->is_initialized) {
    sound_init(&state->sound_state);
    
    debug_state->gui.selected_frame_index = -1;
    
    state->frame_count = 0;
    state->rand = make_random_sequence(2312323342);
    
    // NOTE(lvl5): font stuff
    
    //state->font = load_ttf(state, platform, const_string("Gugi-Regular.ttf"));
    state->font = load_ttf(&state->temp, &state->arena, const_string("fonts/arial.ttf"));
    
    Buffer shader_src = platform.read_entire_file(const_string("shaders/textured_quad.glsl"));
    gl_Parse_Result sources = gl_parse_glsl(buffer_to_string(shader_src));
    
    state->shader_basic = gl_create_shader(&state->arena, gl, sources.vertex, sources.fragment);
    state->atlas = make_texture_atlas_from_folder(&state->temp, &state->arena,
                                                  const_string("sprites"));
    
    Bitmap *bmp = &state->debug_atlas.bmp;
    *bmp = make_empty_bitmap(&state->arena, 1, 1);
    ((u32 *)bmp->data)[0] = 0xFFFFFFFF;
    state->debug_atlas.rects = arena_push_array(&state->arena, rect2, 1);
    state->debug_atlas.rects[0] = rect2_min_max(V2(0, 0), V2(1, 1));
    state->debug_atlas.sprite_count = 1;
    
    
    state->spr_robot_eye = make_sprite(&state->atlas, 0, V2(0.5f, 0.5f));
    state->spr_robot_leg = make_sprite(&state->atlas, 1, V2(0, 1));
    state->spr_robot_torso = make_sprite(&state->atlas, 2, V2(0.5f, 0.5f));
    
    state->spr_grass = make_sprite(&state->atlas, 0, V2(0.5f, 0.5f));
    state->spr_wall = make_sprite(&state->atlas, 4, V2(0.5f, 0.5f));
    
    
    gl.Enable(GL_BLEND);
    gl.BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    quad_renderer_init(&state->renderer, state);
    add_entity(state); // filler entity
    add_entity_player(state);
    
    
    char *ascii_chunks[] = {
      "########"
        "#_____##"
        "#_______"
        "##______"
        "##______"
        "####____"
        "########" 
        "########",
      
      "______##"
        "______##"
        "_______#"
        "#______#"
        "_______#"
        "__#_____"
        "__##____"
        "________",
    };
    
    state->tile_map.chunks = sb_init(&state->arena, Tile_Chunk, 64, true);
    
    for (i32 chunk_y = -3; chunk_y < 3; chunk_y++) {
      for (i32 chunk_x = -3; chunk_x < 3; chunk_x++) {
        i32 random_index = random_range_i32(&state->rand, 0, array_count(ascii_chunks) - 1);
        char *ascii_chunk = ascii_chunks[random_index];
        
        Tile_Chunk chunk;
        chunk.x = chunk_x;
        chunk.y = chunk_y;
        
        for (i32 tile_y = 0; tile_y < CHUNK_SIZE; tile_y++) {
          for (i32 tile_x = 0; tile_x < CHUNK_SIZE; tile_x++) {
            char ch = ascii_chunk[tile_y*CHUNK_SIZE + tile_x];
            if (ch == '#') {
              chunk.tiles[tile_y*CHUNK_SIZE + tile_x].terrain = Terrain_Kind_WALL;
            } else if (ch == '_') {
              chunk.tiles[tile_y*CHUNK_SIZE + tile_x].terrain = Terrain_Kind_GRASS;
            }
          }
        }
        
        sb_push(state->tile_map.chunks, chunk);
      }
    }
    
#include "robot_animation.h"
    
    zero_memory_slow(&state->camera, sizeof(Camera));
    state->is_initialized = true;
  }
  
  if (memory.window_resized) {
    gl.Viewport(0, 0, (i32)screen_size.x,(i32) screen_size.y);
  }
  
  debug_begin_frame();
  DEBUG_FUNCTION_BEGIN();
  
  Input *debug_input = input;
  if (debug_state->gui.terminal.is_active) {
    input = &state->empty_input;
  }
  
#if 0  
  if (state->sound_state.sound_count == 0) {
    Playing_Sound *snd = sound_play(&state->sound_state, &state->snd_bop, Sound_Type_MUSIC);
  }
#endif
  
  state->camera.far = 10.0f;
  state->camera.near = 0.0f;
  state->camera.width = screen_size.x/PIXELS_PER_METER;
  state->camera.height = screen_size.y/PIXELS_PER_METER;
  
  u64 render_memory = arena_get_mark(&state->temp);
  Render_Group _group;
  Render_Group *group = &_group;
  
  
  render_group_init(&state->temp, state, group, 10000, &state->camera, screen_size);
  
  global_group = group;
  render_font(group, &state->font);
  
#if 1
  //v2 half_screen_meters = v2_div_s(screen_size, PIXELS_PER_METER*2);
  v2 mouse_meters = get_mouse_p_meters(input, screen_size);
  v2 mouse_world = v2_add(state->camera.p.xy, mouse_meters);
#endif
  
  
  for (u32 chunk_index = 0; chunk_index < sb_count(state->tile_map.chunks); chunk_index++) {
    Tile_Chunk *chunk = state->tile_map.chunks + chunk_index;
    //if (chunk->x != -1 || chunk->y != 0) continue;
    
    for (i32 tile_y = 0; tile_y < CHUNK_SIZE; tile_y++) {
      for (i32 tile_x = 0; tile_x < CHUNK_SIZE; tile_x++) {
        Sprite spr = {0};
        Tile *tile = chunk->tiles + tile_y*CHUNK_SIZE + tile_x;
        switch (tile->terrain) {
          case Terrain_Kind_GRASS: {
            spr = state->spr_grass;
          } break;
          
          case Terrain_Kind_WALL: {
            spr = state->spr_wall;
          } break;
          
          default: assert(false);
        }
        
        v2 world_p = V2((f32)(chunk->x*CHUNK_SIZE + tile_x)*TILE_SIZE_IN_METERS,
                        (f32)(chunk->y*CHUNK_SIZE + tile_y)*TILE_SIZE_IN_METERS);
        Transform t = transform_default();
        t.p = v2_to_v3(world_p, 0);
        push_sprite(group, spr, t);
      }
    }
  }
  
  for (i32 entity_index = 1; entity_index < state->entity_count; entity_index++) {
    Entity *e = get_entity(state, entity_index);
    if (!e) continue;
    
    e->t.p = v3_add(e->t.p, v3_mul(e->d_p, dt));
    
    if (e->player_controller) {
      v2 move_dir = v2_i(input->move_right.is_down - input->move_left.is_down,
                         input->move_up.is_down - input->move_down.is_down);
      f32 move_dir_len = v2_length(move_dir);
      if (move_dir_len) {
        move_dir = v2_div(move_dir, move_dir_len);
      }
      
      v2 d_d_p = v2_mul(move_dir, 1.0f);
      e->d_p = v3_add(e->d_p, v2_to_v3(d_d_p, 0));
      e->target_p = mouse_world;
      e->d_p = v3_mul(e->d_p, e->friction);
      
      for (i32 skill_index = 0; 
           skill_index < array_count(e->skills);
           skill_index++) {
        Skill *skill = e->skills + skill_index;
        Button btn = input->skills[skill_index];
        if (btn.went_up) {
          b32 use_success = skill_use(state, e, skill);
        }
      }
      
      state->camera.p = e->t.p;
      
      rect2 bbox = get_bbox(e);
      v2 corners[] = {
        bbox.min,
        V2(bbox.min.x, bbox.max.y),
        bbox.max,
        V2(bbox.max.x, bbox.min.y),
      };
#define EPS 0.001f
#if 0
      corners[0].x += EPS;
      corners[0].y += EPS;
      corners[1].x -= EPS;
      corners[1].y -= EPS;
      corners[2].x += EPS;
      corners[2].y -= EPS;
      corners[3].x -= EPS;
      corners[3].y += EPS;
#endif
      
      for (i32 corner_index = 0;
           corner_index < 4;
           corner_index++) {
        v2 corner = corners[corner_index];
        
        Tile_Position tile_p = get_tile_position(corner);
        Tile *tile = get_tile(&state->tile_map, tile_p);
        if (tile->terrain == Terrain_Kind_WALL) {
          v2 world_p = V2((f32)(tile_p.chunk_x*CHUNK_SIZE + tile_p.tile_x)*TILE_SIZE_IN_METERS,
                          (f32)(tile_p.chunk_y*CHUNK_SIZE + tile_p.tile_y)*TILE_SIZE_IN_METERS);
          
          Transform t = transform_default();
          t.p = v2_to_v3(world_p, 0);
          
          render_save(group);
          render_color(group, V4(1, 0, 0, 0.5f));
          
          rect2 wall_bbox = rect2_center_size(world_p, V2(1, 1));
          push_rect(group, wall_bbox);
          
          render_restore(group);
          
          b32 bias_x = corner_index >= 2;
          b32 bias_y = corner_index == 1 || corner_index == 2;
          
          f32 pen_x = bias_x 
            ? bbox.max.x - wall_bbox.min.x 
            : wall_bbox.max.x - bbox.min.x;
          
          f32 pen_y = bias_y
            ? bbox.max.y - wall_bbox.min.y
            : wall_bbox.max.y - bbox.min.y;
          
          if (pen_x > pen_y && pen_y > EPS) {
            e->t.p.y += bias_y ? -pen_y : pen_y;
            e->d_p.y = 0;
          } else if (pen_x < pen_y && pen_x > EPS) {
            e->t.p.x += bias_x ? -pen_x : pen_x;
            e->d_p.x = 0;
          }
          
          bbox = get_bbox(e);
          
#if 0
          render_color(group, V4(0, 0, 0, 1));
          char *buf = arena_push_array(scratch, char, 256);
          sprintf_s(buf, 256, "p: (%0.5f, %0.2f)", e->t.p.x, e->t.p.y);
          push_text(group, from_c_string(buf));
#endif
          // TODO(lvl5): do the collision properly
        }
        
      }
    }
    
    if (e->lifetime.active) {
      e->lifetime.time -= dt;
      if (e->lifetime.time <= 0) {
        remove_entity(state, entity_index);
      }
    }
    
    
    e->hp.v += e->hp.regen*dt;
    e->mp.v += e->mp.regen*dt;
    
    if (e->hp.v > e->hp.max) {
      e->hp.v = e->hp.max;
    }
    if (e->mp.v > e->mp.max) {
      e->mp.v = e->mp.max;
    }
    
    
    // NOTE(lvl5): draw hp and mp
    String hp_string = tsprintf(&state->scratch, "HP: %.02f", e->hp.v);
    f32 hp_string_width = text_get_size(group, hp_string);
    
    String mp_string = tsprintf(&state->scratch, "MP: %.02f", e->mp.v);
    f32 mp_string_width = text_get_size(group, mp_string);
    
    render_save(group);
    render_translate(group, v3_add(e->t.p, V3(-hp_string_width*0.5f, 0.8f, 0)));
    push_text(group, hp_string);
    
    render_translate(group, V3(0, -0.2f, 0));
    push_text(group, mp_string);
    
    render_restore(group);
    
    render_save(group);
    render_transform(group, e->t);
    
    if (debug_get_var_i32(Debug_Var_Name_COLLIDERS)) {
      switch (e->collider.type) {
        case Collider_Type_BOX: {
          push_rect_outline(group, e->collider.box.rect, 0.04f);
        } break;
        case Collider_Type_CIRCLE: {
          Circle_Collider coll = e->collider.circle;
          push_circle_outline(group, coll.origin, coll.r, 0.04f);
        } break;
        default: assert(false);
      }
    }
    render_restore(group);
  }
  
  DEBUG_SECTION_BEGIN(_glClear);
  gl.ClearColor(0.2f, 0.2f, 0.2f, 1.0f);
  gl.Clear(GL_COLOR_BUFFER_BIT);
  DEBUG_SECTION_END(_glClear);
  render_group_output(&state->temp, group, &state->renderer);
  
  Sound_Buffer *buffer = platform.request_sound_buffer();
  sound_mix_playing_sounds(buffer, &state->sound_state, &state->temp, dt);
  
  DEBUG_FUNCTION_END();
  debug_end_frame();
  
  arena_set_mark(&state->temp, render_memory);
  
  debug_draw_gui(state, screen_size, debug_input, dt);
  
  arena_set_mark(&state->scratch, 0);
  
  arena_check_no_marks(&state->temp);
  arena_check_no_marks(&state->arena);
  
  state->frame_count++;
}
