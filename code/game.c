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
#include "audio.c"

/*
TODO:

there probably is some memory corruption bug that sometimes sets 
audio_state.sound_count to 64

---- ENGINE ----

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
 -[ ] kerning (i added this, but for some reason sbt doesn't return good kerning)
 -[ ] line spacing
 -[ ] utf-8 support
 -[ ] rendering needs to be way more optimized
 -[ ] loading fonts from windows instead of stb??
 
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
 
 [ ] sounds
 -[x] basic mixer
 -[x] volume
 -[x] speed shifting
 -[x] SSE?
 
 ---- GAME ----
 [ ] basic player movement
 [ ] player active abilities
 [ ] ability rune examples
 [ ] basic enemy AI and abilities
 [ ] some basic GUI
 -[ ] hp/mana
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
 -[ ] GJK
 -[ ] removed 2D physics stuff for now. If I decide to go full physics,
 I would probably need to do GJK/EPA/find contact manifold using some method
 and I need to learn A LOT about rigidbody simulation if I want proper physics
 Implementing just GJK for general collision would be super nice though
 
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


Entity *get_entity(State *state, i32 index) {
  assert(index < state->entity_count);
  assert(index != 0);
  Entity *result = state->entities + index;
  if (!result->is_active) {
    result = null;
  }
  return result;
}

Entity *add_entity(State *state, Entity_Type type) {
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
  e->type = type;
  e->t = transform_default();
  e->index = index;
  
  return e;
}

Entity *add_entity_player(State *state) {
  Entity *e = add_entity(state, Entity_Type_PLAYER);
#if 0
  e->box_collider.rect = rect2_center_size(V2(0, 0), V2(1, 1));
  e->box_collider.active = true;
#else
  e->circle_collider.r = 0.5f;
  e->circle_collider.active = true;
#endif
  
  return e;
}

Entity *add_entity_box(State *state) {
  Entity *e = add_entity(state, Entity_Type_BOX);
  e->box_collider.rect = rect2_center_size(V2(0, 0), V2(1, 1));
  e->box_collider.active = true;
  e->t.scale = V3(0.5f, 0.5f, 1);
  
  return e;
}


Entity *add_entity_enemy(State *state) {
  Entity *e = add_entity(state, Entity_Type_ENEMY);
#if 0
  e->box_collider.rect = rect2_center_size(V2(0, 0), V2(1, 1));
  e->box_collider.active = true;
#else
  e->circle_collider.r = 0.5f;
  e->t.scale.x = 2.0f;
  e->circle_collider.active = true;
#endif
  
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
    
    result.t.p = v3_add(result.t.p, v3_mul_s(frame.t.p, coeff));
    result.t.scale = v3_add(result.t.scale, v3_mul_s(frame.t.scale, coeff));
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


// NOTE(lvl5): GJK

typedef struct {
  v2 v[16];
  i32 count;
} Polygon;


Polygon aabb_transform(rect2 box, Transform t) {
  Polygon result;
  result.v[0] = box.min;
  result.v[1] = V2(box.min.x, box.max.y);
  result.v[2] = box.max;
  result.v[3] = V2(box.max.x, box.min.y);
  result.count = 4;
  
  mat4x4 matrix4 = transform_apply(mat4x4_identity(), t);
  
  for (i32 i = 0; i < result.count; i++) {
    result.v[i] = mat4x4_mul_v4(matrix4, v2_to_v4(result.v[i], 1, 1)).xy;
  }
  
  return result;
}


v2 gjk_polygon_max_vertex_in_direction(v2 direction, Polygon a) {
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

#define GJK_SUPPORT(name) v2 name(v2 direction, void *data_ptr)
typedef GJK_SUPPORT(Gjk_Support_Fn);


v2 v2_transform(v2 v, Transform t) {
  mat4x4 matrix4 = transform_apply(mat4x4_identity(), t);
  v2 result = mat4x4_mul_v4(matrix4, v2_to_v4(v, 1, 1)).xy;
  return result;
}

typedef struct {
  v2 a_center;
  f32 a_radius;
  Transform a_t;
  
  v2 b_center;
  f32 b_radius;
  Transform b_t;
  Render_Group *group;
} Gjk_Support_Circles;

GJK_SUPPORT(gjk_support_circles) {
  DEBUG_FUNCTION_BEGIN();
  Gjk_Support_Circles *data = (Gjk_Support_Circles *)data_ptr;
  
#if 0
  f32 a_w = data->a_t.scale.x*data->a_radius;
  f32 a_h = data->a_t.scale.y*data->a_radius;
#endif
  
  v2 unit = v2_unit(direction);
  v2 anti_unit = v2_negate(unit);
  
  mat4x4 m = mat4x4_identity();
  m = mat4x4_rotate(m, data->a_t.angle);
  m = mat4x4_scale(m, data->a_t.scale);
  v2 a_max = v2_mul_s(v2_rotate(unit, data->a_t.angle), data->a_radius);
  a_max = mat4x4_mul_v4(m, v2_to_v4(a_max, 1, 1)).xy;
  a_max = v2_add(data->a_t.p.xy, a_max);
  
  
  m = mat4x4_identity();
  m = mat4x4_rotate(m, data->b_t.angle);
  m = mat4x4_scale(m, data->b_t.scale);
  v2 b_max = v2_mul_s(v2_rotate(anti_unit, data->b_t.angle), data->b_radius);
  b_max = mat4x4_mul_v4(m, v2_to_v4(b_max, 1, 1)).xy;
  b_max = v2_add(data->b_t.p.xy, b_max);
  
  
  
  push_rect(data->group, rect2_center_size(a_max, V2(0.1f, 0.1f)));
  push_rect(data->group, rect2_center_size(b_max, V2(0.1f, 0.1f)));
  
  DEBUG_FUNCTION_END();
  v2 result = v2_sub(a_max, b_max);
  return result;
}

typedef struct {
  Polygon a;
  Polygon b;
} Gjk_Support_Polygons;

GJK_SUPPORT(gjk_support_polygons) {
  DEBUG_FUNCTION_BEGIN();
  Gjk_Support_Polygons *data = (Gjk_Support_Polygons *)data_ptr;
  Polygon a = data->a;
  Polygon b = data->b;
  
  v2 a_max = gjk_polygon_max_vertex_in_direction(direction, a);
  v2 b_max = gjk_polygon_max_vertex_in_direction(v2_negate(direction), b);
  
  v2 result = v2_sub(a_max, b_max);
  DEBUG_FUNCTION_END();
  return result;
}

b32 collide_gjk(Render_Group *group, Gjk_Support_Fn *support, void *data) {
  DEBUG_FUNCTION_BEGIN();
  
  v2 simplex[3];
  i32 simplex_count = 1;
  v2 start_p = support(v2_right(), data);
  simplex[0] = start_p;
  v2 dir = v2_mul_s(start_p, -1);
  
  b32 result = true;
  while (true) {
    v2 p = support(dir, data);
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

b32 collide_gjk_circle_circle(Render_Group *group, Circle_Collider a_coll, Transform a_t, Circle_Collider b_coll, Transform b_t) {
  Gjk_Support_Circles data;
  data.a_center = a_coll.origin;
  data.a_radius = a_coll.r;
  data.a_t = a_t;
  
  data.b_center = b_coll.origin;
  data.b_radius = b_coll.r;
  data.b_t = b_t;
  
  data.group = group;
  
  b32 result = collide_gjk(group, gjk_support_circles, &data);
  return result;
}

b32 collide_gjk_box_box(Render_Group *group, rect2 a_rect, Transform a_t, rect2 b_rect, Transform b_t) {
  Gjk_Support_Polygons data;
  data.a = aabb_transform(a_rect, a_t);
  data.b = aabb_transform(b_rect, b_t);
  
  b32 result = collide_gjk(group, gjk_support_polygons, &data);
  return result;
}




#define SCRATCH_SIZE kilobytes(32)

extern GAME_UPDATE(game_update) {
  State *state = (State *)memory.perm;
  debug_state = (Debug_State *)memory.debug;
  Arena *arena = &state->arena;
  
  if (memory.is_reloaded) {
    // NOTE(lvl5): transient memory can be destroyed at this point
    platform = _platform;
    gl = _platform.gl;
    
    arena_init(&state->scratch, memory.temp, SCRATCH_SIZE);
    arena_init(&state->temp, memory.temp + SCRATCH_SIZE, 
               memory.temp_size - SCRATCH_SIZE);
  }
  
  if (!state->is_initialized) {
    arena_init(&state->arena, memory.perm + sizeof(State), 
               memory.perm_size - sizeof(State));
    debug_init(&state->temp, memory.debug + sizeof(Debug_State));
    
    // NOTE(lvl5): when live reloading, the sound file can be overwritten
    // with garbage since it's located in temp arena right now
    state->test_sound = load_wav(&state->temp, 
                                 const_string("sounds/koko_kara.wav"));
    state->snd_bop = load_wav(&state->temp, const_string("sounds/bop.wav"));
    
    //Playing_Sound *snd = sound_play(&state->sound_state, &state->test_sound, Sound_Type_MUSIC);
    
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
    
    gl.Enable(GL_BLEND);
    gl.BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    quad_renderer_init(&state->renderer, state);
    add_entity(state, Entity_Type_NONE); // filler entity
    add_entity_player(state);
    Entity *e = add_entity_enemy(state);
    e->t.p.x = 2;
    //e->t.scale.x = 2.0f;
    
    
#include "robot_animation.h"
    
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
  
  
  u64 render_memory = arena_get_mark(&state->temp);
  Render_Group _group;
  Render_Group *group = &_group;
  render_group_init(&state->temp, state, group, 100000, screen_size);
  
#if 0  
  v2 half_screen_meters = v2_div_s(screen_size, PIXELS_PER_METER*2);
  v2 mouse_meters = get_mouse_p_meters(input, screen_size);
#endif
  
  v4 collider_color = V4(1, 0, 0, 1);
  for (i32 entity_index = 1; entity_index < state->entity_count; entity_index++) {
    Entity *entity = get_entity(state, entity_index);
    
    switch (entity->type) {
      case Entity_Type_PLAYER: {
        v2 move_dir = v2_i(input->move_right.is_down - input->move_left.is_down,
                           input->move_up.is_down - input->move_down.is_down);
        v2 dp = v2_mul_s(move_dir, dt);
        entity->t.p = v3_add(entity->t.p, v2_to_v3(dp, 0));
        
        
        
        for (i32 other_index = entity_index+1;
             other_index < state->entity_count;
             other_index++) {
          Entity *other = get_entity(state, other_index);
          if (entity->box_collider.active) {
            if (collide_gjk_box_box(group, entity->box_collider.rect, entity->t,
                                    other->box_collider.rect, other->t)) {
              collider_color = V4(0, 1, 0, 1);
            }
          } else {
            if (collide_gjk_circle_circle(group, entity->circle_collider, entity->t,
                                          other->circle_collider, other->t)) {
              collider_color = V4(0, 1, 0, 1);
            }
          }
        }
      } break;
      
      case Entity_Type_ENEMY: {
        if (input->start.is_down) {
          entity->t.angle += 1.0f*dt;
        }
      } break;
    }
    
    if (debug_get_var_i32(Debug_Var_Name_COLLIDERS)) {
      render_save(group);
      render_transform(group, entity->t);
      render_color(group, collider_color);
      if (entity->box_collider.active) {
        push_rect_outline(group, entity->box_collider.rect, 0.04f);
      }
      if (entity->circle_collider.active) {
        Circle_Collider coll = entity->circle_collider;
        push_circle_outline(group, coll.origin, coll.r, 0.04f);
      }
      render_restore(group);
      
      if (entity_index == 1) {
        //entity->t.scale.x = 2.0f;
        //entity->t.scale.y = 1.0f;
        //Circle_Collider coll = entity->circle_collider;
        
        
#if 0
        {
          v2 scale = entity->t.scale.xy;
          f32 angle = entity->t.angle;
          f32 cos_a = cos_f32(angle);
          f32 sin_a = sin_f32(angle);
          f32 width = sqrt_f32(sqr_f32(cos_a*scale.x) +
                               sqr_f32(sin_a*scale.y));
          v2 start = entity->t.p.xy;
          v2 end = v2_add(start, V2(width, 0));
          push_line_color(group, start, end, 0.05f, V4(0, 1, 0, 1));
        }
        
        {
          f32 angle = entity->t.angle + PI/2;
          f32 cos_a = sqr_f32(cos_f32(angle));
          f32 sin_a = sqr_f32(sin_f32(angle));
          f32 height = cos_a*entity->t.scale.x + sin_a*entity->t.scale.y;
          v2 start = v2_sub(entity->t.p.xy, V2(0, height*0.5f));
          v2 end = v2_add(start, V2(0, height));
          push_line_color(group, start, end, 0.05f, V4(0, 1, 0, 1));
        }
#endif
      }
    }
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
