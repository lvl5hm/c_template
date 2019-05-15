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
  e->box_collider.rect = rect2_center_size(V2(0, 0), V2(1, 1));
  e->box_collider.active = true;
  
  return e;
}


Entity *add_entity_enemy(State *state) {
  Entity *e = add_entity(state, Entity_Type_ENEMY);
  e->circle_collider.r = 0.5f;
  e->circle_collider.origin = v2_zero();
  e->circle_collider.active = true;
  
  e->t.scale = V3(0.3f, 0.3f, 1);
  
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


void add_force(Entity *e, v2 force) {
  e->dp = v3_add(v2_to_v3(force, 0), e->dp);
}

b32 collide_aabb_aabb(rect2 a, rect2 b) {
  b32 result = !(a.max.x < b.min.x ||
                 a.max.y < b.min.y ||
                 a.min.x > b.max.x ||
                 a.min.y > b.max.y);
  return result;
}

typedef struct {
  v2 v[4];
} Rect_Polygon;

Rect_Polygon aabb_transform(rect2 box, Transform t) {
  DEBUG_FUNCTION_BEGIN();
  Rect_Polygon result;
  result.v[0] = box.min;
  result.v[1] = V2(box.min.x, box.max.y);
  result.v[2] = box.max;
  result.v[3] = V2(box.max.x, box.min.y);
  
  mat4x4 matrix4 = transform_apply(mat4x4_identity(), t);
  
  for (i32 i = 0; i < 4; i++) {
    result.v[i] = mat4x4_mul_v4(matrix4, v2_to_v4(result.v[i], 1, 1)).xy;
  }
  
  DEBUG_FUNCTION_END();
  return result;
}

b32 collide_box_box(rect2 a_rect, Transform a_t, rect2 b_rect, Transform b_t) {
  DEBUG_FUNCTION_BEGIN();
  Rect_Polygon a = aabb_transform(a_rect, a_t);
  Rect_Polygon b = aabb_transform(b_rect, b_t);
  
  v2 normals[4];
  normals[0] = v2_sub(a.v[1], a.v[0]);
  normals[1] = v2_sub(a.v[2], a.v[1]);
  normals[2] = v2_sub(b.v[1], b.v[0]);
  normals[3] = v2_sub(b.v[2], b.v[1]);
  
  b32 result = true;
  
  for (i32 normal_index = 0; normal_index < 4; normal_index++) {
    v2 n = normals[normal_index];
    
    Range a_range = inverted_infinity_range();
    Range b_range = inverted_infinity_range();
    
    for (i32 vertex_index = 0; vertex_index < 4; vertex_index++) {
      {
        v2 vertex = a.v[vertex_index];
        f32 dot = v2_dot(vertex, n);
        if (dot < a_range.min) {
          a_range.min = dot;
        }
        if (dot > a_range.max) {
          a_range.max = dot;
        }
      }
      
      {
        v2 vertex = b.v[vertex_index];
        f32 dot = v2_dot(vertex, n);
        if (dot < b_range.min) {
          b_range.min = dot;
        }
        if (dot > b_range.max) {
          b_range.max = dot;
        }
      }
    }
    
    if (a_range.max < b_range.min || a_range.min > b_range.max) {
      result = false;
      break;
    }
  }
  
  DEBUG_FUNCTION_END();
  return result;
}

#define SCRATCH_SIZE kilobytes(32)

extern GAME_UPDATE(game_update) {
  dt *= 0.3f;
  
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
                                 const_string("sounds/durarara.wav"));
    state->snd_bop = load_wav(&state->temp, const_string("sounds/bop.wav"));
    
    Playing_Sound *snd = sound_play(&state->sound_state, 
                                    &state->test_sound, Sound_Type_MUSIC);
    
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
    
    
    state->spr_circle = make_sprite(&state->atlas, 0, V2(0.5f, 0.5f));
    state->spr_robot_eye = make_sprite(&state->atlas, 0, V2(0.5f, 0.5f));
    state->spr_robot_leg = make_sprite(&state->atlas, 1, V2(0, 1));
    state->spr_robot_torso = make_sprite(&state->atlas, 2, V2(0.5f, 0.5f));
    
    gl.Enable(GL_BLEND);
    gl.BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    quad_renderer_init(&state->renderer, state);
    add_entity(state, Entity_Type_NONE); // filler entity
    //add_entity_player(state);
    
    f32 radius = 0.15f;
    i32 row_length = 5;
    f32 start_x = -radius*row_length + radius;
    f32 y = radius*row_length;
    
    while (row_length > 0) {
      for (i32 i = 0; i < row_length; i++) {
        Entity *enemy = add_entity_enemy(state);
        enemy->t.p.x = start_x + i*radius*2 +
          random_range(&state->rand, -0.01f, 0.01f);
        enemy->t.p.y = y;
      }
      start_x += radius;
      y -= radius*2;
      row_length--;
    }
    
    
    
    {
      Entity *enemy = add_entity_enemy(state);
      enemy->t.p.x = 0.0f;
      enemy->t.p.y = -2.0f;
    }
    
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
  
  
#if 0
  if (state->frame_count % 60 == 0) {
    sound_play(&state->sound_state, &state->snd_bop, Sound_Type_EFFECTS);
  }
#endif
  
  u64 render_memory = arena_get_mark(&state->temp);
  Render_Group _group;
  Render_Group *group = &_group;
  render_group_init(&state->temp, state, group, 100000, screen_size);
  
  v2 half_screen_meters = v2_div_s(screen_size, PIXELS_PER_METER*2);
  v2 mouse_meters = get_mouse_p_meters(input, screen_size);
  
  DEBUG_SECTION_BEGIN(_entities);
  for (i32 entity_index = 1; entity_index < state->entity_count; entity_index++) {
    Entity *entity = get_entity(state, entity_index);
    
    switch (entity->type) {
      case Entity_Type_ENEMY: {
        
      } break;
      case Entity_Type_PLAYER: {
#if 0
        i32 h_speed = (input->move_right.is_down - 
                       input->move_left.is_down);
        i32 v_speed = (input->move_up.is_down - 
                       input->move_down.is_down);
        
        entity->dp = v2_to_v3(v2_mul_s(v2_i(h_speed, v_speed), 0.01f), 0);
        entity->t.p = v3_add(entity->t.p, entity->dp);
        
        render_save(group);
        render_transform(group, entity->t);
        
        //draw_robot(state, group);
        
        render_restore(group);
#endif
      } break;
    }
    
    
    entity->t.p = v3_add(entity->t.p, v3_mul_s(entity->dp, dt));
    f32 FRICTION = 0.5f*dt;
    entity->dp = v3_mul_s(entity->dp, 1 - FRICTION);
    f32 r = entity->circle_collider.r*entity->t.scale.x;
    
    b32 do_bop = false;
    
    if (entity->t.p.x + r > half_screen_meters.x) {
      entity->dp.x *= -1;
      do_bop = true;
    }
    if (entity->t.p.x - r < -half_screen_meters.x) {
      entity->dp.x *= -1;
      do_bop = true;
    }
    
    if (entity->t.p.y + r > half_screen_meters.y) {
      entity->dp.y *= -1;
      do_bop = true;
    }
    if (entity->t.p.y - r < -half_screen_meters.y) {
      entity->dp.y *= -1;
      do_bop = true;
    }
    
    if (input->mouse.left.went_down && point_in_circle(mouse_meters, entity->t.p.xy, r)) {
      state->selected_entity = entity_index;
    }
    
    Entity *bopped = 0;
    for (i32 other_index = entity_index+1; other_index < state->entity_count; other_index++) {
      DEBUG_SECTION_BEGIN(_resolve_collision);
      Entity *other = get_entity(state, other_index);
      
      f32 radius_sum = entity->circle_collider.r*entity->t.scale.x + other->circle_collider.r*other->t.scale.x;
      v2 diff = v2_sub(other->t.p.xy, entity->t.p.xy);
      f32 dist = v2_length(diff);
      
      if (dist < radius_sum) {
        if (entity->dp.x || entity->dp.y || other->dp.x || other->dp.y) {
          do_bop = true;
          bopped = other;
        }
        
        // NOTE(lvl5): collision happened
        v3 entity_new_dp = entity->dp;
        v3 other_new_dp = other->dp;
        v2 unit = v2_unit(diff);
        
        f32 overlap = dist - radius_sum;
        entity->t.p.xy = v2_add(entity->t.p.xy, v2_mul_s(unit, overlap));
        
        {
          v2 force = v2_project(entity->dp.xy, unit);
          other_new_dp.xy = v2_add(other_new_dp.xy, force);
          entity_new_dp.xy = v2_add(entity_new_dp.xy, 
                                    v2_mul_s(force, -1));
        }
        {
          v2 force = v2_project(other->dp.xy, unit);
          entity_new_dp.xy = v2_add(entity_new_dp.xy, force);
          other_new_dp.xy = v2_add(other_new_dp.xy, 
                                   v2_mul_s(force, -1));
        }
        
        other->dp = other_new_dp;
        entity->dp = entity_new_dp;
      }
      DEBUG_SECTION_END(_resolve_collision);
    }
    
    if (debug_get_var_i32(Debug_Var_Name_COLLIDERS)) {
      render_save(group);
      render_transform(group, entity->t);
      render_color(group, V4(1, 0, 0, 1));
      if (entity->box_collider.active) {
        push_rect_outline(group, entity->box_collider.rect, 0.02f);
      }
      if (entity->circle_collider.active) {
        push_sprite(group, state->spr_circle, transform_default());
      }
      render_restore(group);
    }
    
    if (do_bop) {
      Playing_Sound *snd = sound_play(&state->sound_state, &state->snd_bop, Sound_Type_EFFECTS);
#define MAX_BALL_SPEED 20
      f32 speed = v2_length(entity->dp.xy);
      if (bopped) {
        f32 other_speed = v2_length(bopped->dp.xy);
        if (other_speed > speed) speed = other_speed;
      }
      
      f32 volume = speed/MAX_BALL_SPEED;
      sound_set_volume(snd, V2(volume, volume), 0);
      //snd->speed = random_range(&state->rand, 0.95f, 1.08f);
    }
  }
  
  
  if (state->selected_entity) {
    Entity *e = get_entity(state, state->selected_entity);
    if (e->is_active) {
      v2 begin_p = e->t.p.xy;
      push_line(group, begin_p, mouse_meters, 0.03f);
      
      if (input->mouse.left.went_up) {
        e->dp = v3_mul_s(v3_sub(v2_to_v3(mouse_meters, 0), e->t.p), 5);
        state->selected_entity = 0;
      }
    }
  }
  
  DEBUG_SECTION_END(_entities);
  
  
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
