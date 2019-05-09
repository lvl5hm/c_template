#define LVL5_DEBUG

#include "platform.h"
#include "game.h"
#include "lvl5_math.h"
#include "lvl5_opengl.h"
#include "lvl5_stretchy_buffer.h"
#include "lvl5_random.h"
#include "renderer.c"

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

#include "debug.h"
#include <stdio.h>


/*
TODO:

---- ENGINE ----

[ ] multithreading
 -[x] thread queues in the windows layer
 -[ ] threaded things in the platform layer
 -[ ] all threads can do low priority queue, but
 they wake up from time to time to see if there's work in high priority
 
 [ ] debug
 -[ ] performance counters
 -[ ] logging
 -[ ] performance graphs
 -[ ] something like interactive flame charts
 -[ ] debug variables and widgets
 -[ ] console?
 -[ ] will probably have to do some introspection and metaprogramming
 -[ ] needs to work with multithreading
 -[ ] loading from a save point to debug a slow frame
 
 [ ] animation
 -[ ] clean up
 -[ ] animation editor
 -[ ] b-splines (bezier curves)
 
 [ ] text rendering
 -[ ] kerning
 -[ ] line spacing
 -[ ] utf-8 support
 -[ ] loading fonts from windows instead of stb??
 
 [ ] windows layer
 -[ ] proper opengl context creation for multisampling
 
 [ ] renderer
 -[ ] z-sorting
 -[ ] more shader stuff (materials, etc)
 -[ ] lighting
 --[ ] normal maps
 --[ ] phong shading
 
 [ ] assets
 -[ ] live reload
 -[ ] asset file format
 --[ ] bitmaps
 --[ ] sounds
 --[ ] fonts
 --[ ] shaders
 -[ ] asset builder
 -[ ] streaming?
 
 [ ] sounds
 -[ ] basic mixer
 -[ ] volume
 -[ ] speed shifting
 -[ ] SSE?
 
 
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


typedef union {
  struct {
    byte r;
    byte g;
    byte b;
    byte a;
  };
  u32 full;
} Pixel;

#pragma pack(push, 1)
typedef struct {
  u32 info_header_size;
  u32 width;
  u32 height;
  u16 planes;
  u16 bits_per_pixel;
  u32 compression;
  u32 image_size;
  u32 x_pixels_per_meter;
  u32 y_pixels_per_meter;
  u32 colors_used;
  u32 important_colors;
} Bmp_Info_Header;

typedef struct {
  u16 signature;
  u32 file_size;
  u32 reserved;
  u32 data_offset;
  Bmp_Info_Header info;
} Bmp_File_Header;
#pragma pack(pop)

Bitmap load_bmp(Platform platform, String file_name) {
  Buffer file = platform.read_entire_file(file_name);
  assert(file.data);
  Bmp_File_Header *header = (Bmp_File_Header *)file.data;
  assert(header->signature == (('B' << 0) | ('M' << 8)));
  assert(header->file_size == file.size);
  assert(header->info.planes == 1);
  assert(header->info.bits_per_pixel == 32);
  
  byte *data = file.data + header->data_offset;
  
#define R_MASK 0x00FF0000
#define G_MASK 0x0000FF00
#define B_MASK 0x000000FF
#define A_MASK 0xFF000000
  
  Bitmap result = {0};
  result.data = data;
  result.width = header->info.width;
  result.height = header->info.height;
  
  for (i32 pixel_index = 0; 
       pixel_index < result.width*result.height;
       pixel_index++) {
    u32 *pixel = (u32 *)result.data + pixel_index;
    u32 swizzled = *pixel;
    
    u32 r = (R_MASK & swizzled) >> 16;
    u32 g = (G_MASK & swizzled) >> 8;
    u32 b = (B_MASK & swizzled) >> 0;
    u32 a = (A_MASK & swizzled) >> 24;
    u32 unswizzled = (r << 0)|(g << 8)|(b << 16)|(a << 24);
    *pixel = unswizzled;
  }
  
  return result;
}

Entity *get_entity(State *state, i32 index) {
  assert(index < state->entity_count);
  assert(index != 0);
  Entity *result = state->entities + index;
  return result;
}

i32 add_entity(State *state, Entity_Type type) {
  assert(state->entity_count < array_count(state->entities));
  
  i32 result = state->entity_count++;
  Entity *e = state->entities + result;
  e->is_active = true;
  e->type = type;
  e->t = transform_default();
  return result;
}

i32 add_entity_player(State *state) {
  i32 index = add_entity(state, Entity_Type_PLAYER);
  return index;
}

void remove_entity(State *state, i32 index) {
  Entity *last = state->entities + state->entity_count - 1;
  Entity *removed = state->entities + index;
  *removed = *last;
  state->entity_count--;
}


Bitmap make_empty_bitmap(Arena *arena, i32 width, i32 height) {
  Bitmap result;
  result.width = width;
  result.height = height;
  result.data = (byte *)arena_push_array(arena, u32, width*height);
  zero_memory_slow(result.data, width*height*sizeof(u32));
  return result;
}



Texture_Atlas make_texture_atlas_from_bitmaps(Arena *arena, Bitmap *bitmaps, i32 count) {
  Texture_Atlas result = {0};
  result.sprite_count = count;
  result.rects = arena_push_array(arena, rect2, count);
  
  i32 max_width = 0;
  i32 total_height = 0;
  
  for (i32 bitmap_index = 0; bitmap_index < count; bitmap_index++) {
    Bitmap *bmp = bitmaps + bitmap_index;
    if (bmp->width > max_width) {
      max_width = bmp->width;
    }
    total_height += bmp->height;
  }
  
  result.bmp = make_empty_bitmap(arena, max_width, total_height);
  i32 current_y = 0;
  
  for (i32 bitmap_index = 0; bitmap_index < count; bitmap_index++) {
    Bitmap *bmp = bitmaps + bitmap_index;
    rect2 *rect = result.rects + bitmap_index;
    
    rect->min = V2(0, (f32)current_y/(f32)total_height);
    rect->max = V2(bmp->width/(f32)max_width, rect->min.y + (f32)bmp->height/(f32)total_height);
    
    u32 *row = (u32 *)result.bmp.data + current_y*max_width;
    for (i32 y = 0; y < bmp->height; y++) {
      u32 *dst = row;
      for (i32 x = 0; x < bmp->width; x++) {
        *dst++ = ((u32 *)bmp->data)[y*bmp->width + x];
      }
      row += max_width;
    }
    
    current_y += bmp->height;
  }
  
  return result;
}

Texture_Atlas make_texture_atlas_from_folder(Platform platform, State *state,  String folder) {
  File_List dir = platform.get_files_in_folder(folder);
  u64 loaded_bitmaps_memory = arena_get_mark(&state->temp);
  
  Bitmap *bitmaps = arena_push_array(&state->temp, Bitmap, dir.count);
  
  for (i32 file_index = 0; file_index < dir.count; file_index++) {
    String file_name = dir.files[file_index];
    String full_name = concat(&state->temp, folder, concat(&state->temp, const_string("\\"), file_name));
    Bitmap sprite_bmp = load_bmp(platform, full_name);
    bitmaps[file_index] = sprite_bmp;
  }
  
  Texture_Atlas result = make_texture_atlas_from_bitmaps(&state->arena, bitmaps, dir.count); 
  arena_set_mark(&state->temp, loaded_bitmaps_memory);
  
  return result;
}

#define FONT_HEIGHT 16

Font load_ttf(State *state, Platform platform, String file_name) {
  Font result;
  
  stbtt_fontinfo font;
  Buffer font_file = platform.read_entire_file(file_name);
  const unsigned char *font_buffer = (const unsigned char *)font_file.data;
  stbtt_InitFont(&font, font_buffer, stbtt_GetFontOffsetForIndex(font_buffer, 0));
  
  u64 loaded_bitmaps_memory = arena_get_mark(&state->temp);
  result.first_codepoint_index = ' ';
  i32 last_codepoint_index = '~';
  result.codepoint_count = last_codepoint_index - result.first_codepoint_index;
  result.metrics = sb_init(&state->arena, Codepoint_Metrics,
                           result.codepoint_count, false);
  
  
  Bitmap *bitmaps = sb_init(&state->temp, Bitmap, result.codepoint_count, false);
  for (char ch = result.first_codepoint_index; ch < last_codepoint_index; ch++) {
    i32 width;
    i32 height;
    f32 scale = stbtt_ScaleForPixelHeight(&font, FONT_HEIGHT);
    byte *single_bitmap = stbtt_GetCodepointBitmap(&font, 0, scale, ch, &width, &height, 0, 0);
    Bitmap bitmap = make_empty_bitmap(&state->temp, width, height);
    
    i32 x0, y0, x1, y1;
    stbtt_GetCodepointBitmapBox(&font, ch, scale, scale, &x0,&y0,&x1,&y1);
    
    u32 *row = (u32 *)bitmap.data + (height-1)*width;
    for (i32 y = 0; y < height; y++) {
      u32 *dst = row;
      for (i32 x = 0; x < width; x++) {
        byte a = single_bitmap[y*width + x];
        Pixel pixel;
        pixel.r = 255;
        pixel.g = 255;
        pixel.b = 255;
        pixel.a = a;
        *dst++ = pixel.full;
      }
      row -= width;
    }
    
    Codepoint_Metrics metrics;
    metrics.origin = V2(0, (f32)y1/height);
    
    i32 advance, _lsb;
    stbtt_GetCodepointHMetrics(&font, ch, &advance, &_lsb);
    metrics.advance = advance*scale;
    
    metrics.kerning = arena_push_array(&state->arena, f32, result.codepoint_count);
    for (char codepoint_index = 0;
         codepoint_index< result.codepoint_count;
         codepoint_index++) {
      char other_ch = result.first_codepoint_index + codepoint_index;
      f32 kern = scale*stbtt_GetCodepointKernAdvance(&font, ch, 
                                                     other_ch);
      metrics.kerning[codepoint_index] = kern;
    }
    
    sb_push(bitmaps, bitmap);
    sb_push(result.metrics, metrics);
  }
  
  arena_set_mark(&state->temp, loaded_bitmaps_memory);
  
  Texture_Atlas atlas = make_texture_atlas_from_bitmaps(&state->arena, bitmaps, sb_count(bitmaps));
  result.atlas = atlas;
  
  return result;
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
    f32 *old_pos = inst.positions;
    inst.positions = arena_push_array(&state->scratch, f32, 2);
    copy_memory_slow(inst.positions, old_pos, sizeof(f32)*2);
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

b32 point_in_rect(v2 point, rect2 rect) {
  b32 result = point.x > rect.min.x &&
    point.x < rect.max.x &&
    point.y > rect.min.y &&
    point.y < rect.max.y;
  return result;
}

v2 get_mouse_p_meters(game_Input *input, v2 screen_size) {
  v2 screen_size_meters = v2_div_s(screen_size, PIXELS_PER_METER);
  v2 half_screen_size_meters = v2_div_s(screen_size_meters, 2);
  v2 mouse_p_meters = v2_sub(v2_div_s(input->mouse.p, PIXELS_PER_METER),
                             half_screen_size_meters);
  return mouse_p_meters;
}

#if 0
void debug_render_node(game_Input *input, State *state, Render_Group *group, Debug_Node *node) {
  f32 percent = node->parent
    ? (f32)node->cycle_count/(f32)node->parent->cycle_count*100
    : 100;
  
  char buffer[256];
  sprintf_s(buffer, array_count(buffer), "%s: %.2f%%  (%lld)",
            to_c_string(&state->debug_arena, node->name), percent, node->cycle_count);
  
  String str = from_c_string(buffer);
  rect2 rect = rect2_apply_matrix(rect2_min_size(V2(0, 0), V2(3, 0.2f)), 
                                  group->state.matrix);
  
  v2 mouse_p_meters = get_mouse_p_meters(input, group->screen_size);
  
  if (sb_count(node->children) && point_in_rect(mouse_p_meters, rect)) {
    render_color(group, V4(0, 1, 0, 1));
    if (input->mouse.left.went_down) {
      node->is_open = !node->is_open;
    }
  }
  
  push_text(group, &state->font, str, transform_default());
  render_color(group, V4(1, 1, 1, 1));
  
  render_translate(group, V3(0, -0.2f, 0));
  
  if (node->is_open && sb_count(node->children)) {
    v3 child_trans = V3(0.1f, 0, 0);
    render_translate(group, child_trans);
    for (u32 child_index = 0; child_index < sb_count(node->children); 
         child_index++) {
      debug_render_node(input, state, group, &node->children[child_index]);
    }
    render_translate(group, v3_mul_s(child_trans, -1));
  }
}

#endif

extern GAME_UPDATE(game_update) {
  debug_begin_frame();
  
  DEBUG_FUNCTION_BEGIN();
  
  State *state = (State *)memory.perm;
  Arena *arena = &state->arena;
  
  gl = platform.gl;
  
  if (!state->is_initialized) {
    arena_init(&state->arena, memory.perm + sizeof(State), memory.perm_size - sizeof(State));
    arena_init(&state->scratch, memory.temp, SCRATCH_SIZE);
    arena_init(&state->temp, memory.temp + SCRATCH_SIZE, memory.temp_size - SCRATCH_SIZE);
    arena_init_subarena(&state->temp, &state->debug_arena, megabytes(32));
    arena_init_subarena(&state->debug_arena, &debug_state.gui.arena, kilobytes(32));
  }
  
  if (!state->is_initialized) {
    debug_state.gui.selected_frame_index = -1;
    
    state->frame_count = 0;
    state->rand = make_random_sequence(2312323342);
    
    // NOTE(lvl5): font stuff
    
    //state->font = load_ttf(state, platform, const_string("Gugi-Regular.ttf"));
    state->font = load_ttf(state, platform, const_string("arial.ttf"));
    
    Buffer shader_src = platform.read_entire_file(const_string("basic.glsl"));
    gl_Parse_Result sources = gl_parse_glsl(buffer_to_string(shader_src));
    
    state->shader_basic = gl_create_shader(&state->arena, gl, sources.vertex, sources.fragment);
    
    state->atlas = make_texture_atlas_from_folder(platform, state, const_string("sprites"));
    
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
    add_entity(state, Entity_Type_PLAYER);
    add_entity(state, Entity_Type_ENEMY);
    
#include "robot_animation.h"
    state->is_initialized = true;
  }
  
  if (memory.is_reloaded) {
    // NOTE(lvl5): transient memory can be destroyed at this point
  }
  
  if (memory.window_resized) {
    gl.Viewport(0, 0, (i32)screen_size.x,(i32) screen_size.y);
  }
  
  
  u64 render_memory = arena_get_mark(&state->temp);
  Render_Group _group;
  Render_Group *group = &_group;
  render_group_init(&state->temp, state, group, 100000, screen_size);
  
  rect2 rect_1m = rect2_center_size(V2(0, 0), V2(2, 1));
  
  for (i32 entity_index = 1; entity_index < state->entity_count; entity_index++) {
    Entity *entity = get_entity(state, entity_index);
    
    switch (entity->type) {
      case Entity_Type_ENEMY: {
        entity->t.angle += 0.01f;
        entity->t.p = V3(2.0f, 0, 0);
        render_save(group);
        render_transform(group, entity->t);
        
        push_rect(group, rect_1m, V4(1, 0, 0, 1));
        
        
        render_restore(group);
      } break;
      case Entity_Type_PLAYER: {
        
        if (input.start.is_down) {
          entity->t.angle += 0.01f;
        }
        
        entity->t.scale = V3(1, 1, 1);
        //String text = scratch_sprintf(const_string("robot is at (%i, %i)"), (i32)entity->t.p.x, (i32)entity->t.p.y);
        
        
        i32 h_speed = (input.move_right.is_down - 
                       input.move_left.is_down);
        i32 v_speed = (input.move_up.is_down - 
                       input.move_down.is_down);
        
        //entity->t.p = v2_to_v3(v2_sub(mouse_p_meters, half_screen_size_meters), 0);
#define PLAYER_SPEED 0.03f
        
        v3 d_p = v3_mul_s(V3((f32)h_speed, (f32)v_speed, 0), PLAYER_SPEED);
        entity->t.p = v3_add(entity->t.p, d_p);
        
        entity->t.scale = V3(1, 1, 1);
        if (entity->t.scale.x == 0) {
          entity->t.scale.x = 1;
        }
        
        if (input.mouse.left.is_down) {
          h_speed = 1;
        }
        
        Animation_Instance *inst = &state->robot_anim;
        
        if (h_speed || v_speed) {
          inst->positions[Robot_Animation_WALK] += 0.03f;
          inst->weights[Robot_Animation_WALK] += 0.05f;
          inst->weights[Robot_Animation_IDLE] -= 0.05f;
        } else {
          inst->positions[Robot_Animation_IDLE] += 0.03f;
          inst->weights[Robot_Animation_WALK] -= 0.05f;
          inst->weights[Robot_Animation_IDLE] += 0.05f;
        }
        
        if (inst->weights[Robot_Animation_IDLE] > 1) inst->weights[Robot_Animation_IDLE] = 1;
        
        if (inst->weights[Robot_Animation_IDLE] < 0) {
          inst->weights[Robot_Animation_IDLE] = 0;
          inst->positions[Robot_Animation_IDLE] = 0;
        }
        if (inst->weights[Robot_Animation_WALK] > 1) inst->weights[Robot_Animation_WALK] = 1;
        
        if (inst->weights[Robot_Animation_WALK] < 0) {
          inst->weights[Robot_Animation_WALK] = 0;
          inst->positions[Robot_Animation_WALK] = 0;
        }
        
        
        render_save(group);
        render_transform(group, entity->t);
        
        
        b32 collision = false;
        for (i32 other_index = 1;
             other_index < state->entity_count;
             other_index++) {
          if (other_index != entity_index) {
            Entity *other = get_entity(state, other_index);
            if (collide_box_box(rect_1m, entity->t, rect_1m, other->t)) {
              collision = true;
              break;
            }
          }
        }
        
        if (collision) {
          push_rect(group, rect_1m, V4(0, 1, 1, 1));
        } else {
          push_rect(group, rect_1m, V4(0, 1, 0, 1));
        }
        //draw_robot(state, &group);
        
        render_restore(group);
      } break;
    }
  }
  
  
  
  gl.ClearColor(0.0f, 0.0f, 0.0f, 1.0f);
  gl.Clear(GL_COLOR_BUFFER_BIT);
  render_group_output(&state->temp, group, &state->renderer);
  
  
  arena_set_mark(&state->temp, render_memory);
  
  DEBUG_FUNCTION_END();
  debug_end_frame();
  
  Debug_GUI *gui = &debug_state.gui;
  
  u64 debug_render_memory = arena_get_mark(&state->debug_arena);
  Render_Group _debug_render_group;
  group = &_debug_render_group;
  
  render_group_init(&state->debug_arena, state, group, 10000, screen_size); 
  f32 total_width = 3.0f;
  f32 rect_width = total_width/array_count(debug_state.frames);
  
  for (i32 i = 0; i < array_count(debug_state.frames); i++) {
    i32 frame_index = (debug_state.frame_index + i) % array_count(debug_state.frames);
    
    Debug_Frame *frame = debug_state.frames + frame_index;
    if (frame->event_count && frame_index != debug_state.frame_index) {
      u64 begin_cycles = frame->events[0].cycles;
      u64 end_cycles = frame->events[frame->event_count-1].cycles;
      u64 duration = end_cycles - begin_cycles;
      
      f32 rect_height = (f32)duration/40891803*10;
      v2 screen_meters = v2_div_s(screen_size, PIXELS_PER_METER);
      
      rect2 rect = rect2_min_size(V2(-screen_meters.x*0.5f, 
                                     screen_meters.y*0.5f-1), 
                                  V2(rect_width, rect_height));
      v4 color = V4(0, 1, 0, 1);
      rect2 mouse_rect = rect;
      mouse_rect.max.y = mouse_rect.min.y+1;
      mouse_rect = rect2_apply_matrix(mouse_rect, group->state.matrix);
      v2 mouse_p_meters = get_mouse_p_meters(&input, screen_size);
      if (point_in_rect(mouse_p_meters, mouse_rect)) {
        color = V4(1, 0, 0, 1);
        debug_state.pause = true;
        
        if (input.mouse.left.went_down) {
          if (gui->selected_frame_index != -1) {
            arena_set_mark(&gui->arena, gui->node_memory);
          }
          gui->selected_frame_index = frame_index;
          gui->node_memory = arena_get_mark(&gui->arena);
          
          Debug_Frame *frame = debug_state.frames + gui->selected_frame_index;
          gui->nodes = arena_push_array(&gui->arena, 
                                        Debug_View_Node, frame->timer_count);
          gui->node_count = 0;
          i32 node_index = -1;
          i32 depth = 0;
          
          for (u32 event_index = 0;
               event_index < frame->event_count; 
               event_index++) {
            Debug_Event *event = frame->events + event_index;
            
            if (event->type == Debug_Type_BEGIN_TIMER) {
              i32 new_index =  gui->node_count++;
              Debug_View_Node *node = gui->nodes + new_index;
              node->type = Debug_View_Type_TREE;
              node->event_index = event_index;
              node->first_child_index = new_index + 1;
              node->parent_index = node_index;
              node->depth = depth++;
              node->self_duration = 0;
              
              node_index = new_index;
            } else if (event->type == Debug_Type_END_TIMER) {
              Debug_View_Node *node = gui->nodes + node_index;
              node->one_past_last_child_index = gui->node_count;
              Debug_Event *begin = frame->events + node->event_index;
              node->duration = event->cycles - begin->cycles;
              
              if (node->parent_index != -1) {
                Debug_View_Node *parent = gui->nodes + node->parent_index;
                parent->self_duration -= node->duration;
              }
              
              node->self_duration += node->duration;
              
              
              node_index = node->parent_index;
              depth--;
            }
          }
        }
      }
      
      push_rect(group, rect, color);
      render_translate(group, V3(rect_width, 0, 0));
    }
  }
  
  render_group_output(&state->debug_arena, group, &state->renderer);
  arena_set_mark(&state->debug_arena, debug_render_memory);
  
  
  if (debug_state.gui.selected_frame_index >= 0) {
    Debug_Frame *frame = debug_state.frames +
      debug_state.gui.selected_frame_index;
    
    u64 debug_render_memory = arena_get_mark(&state->debug_arena);
    v2 half_screen_size_meters = v2_div_s(screen_size, PIXELS_PER_METER*2);
    
    Render_Group _group;
    Render_Group *group = &_group;
    render_group_init(&state->debug_arena, state, group, 10000, screen_size); 
    
    render_save(group);
    v3 text_p = V3(-half_screen_size_meters.x + 0.1f, 
                   half_screen_size_meters.y - 1.2f, 0);
    render_translate(group, text_p);
    
    
    i32 node_index = 0;
    
    while (node_index < gui->node_count) {
      Debug_View_Node *node = gui->nodes + node_index;
      Debug_View_Node *parent = node->parent_index == -1 
        ? null
        : gui->nodes + node->parent_index;
      u64 parent_duration = 40000000;
      if (parent) {
        parent_duration = parent->duration;
      }
      
      f32 percent = (f32)node->duration/(f32)parent_duration*100;
      f32 self_percent = (f32)node->self_duration/(f32)parent_duration*100;
      
      Debug_Event *event = frame->events + node->event_index;
      
      char buffer[256];
      sprintf_s(buffer, array_count(buffer), "%s: %.2f%% (self %.2f%%)  (%lld)", 
                event->name, percent, self_percent, node->duration);
      
      String str = from_c_string(buffer);
      rect2 rect = rect2_apply_matrix(rect2_min_size(V2(0, 0), V2(3, 0.2f)), 
                                      group->state.matrix);
      
      v2 mouse_p_meters = get_mouse_p_meters(&input, group->screen_size);
      i32 child_indices_count = node->one_past_last_child_index - node->first_child_index;
      
      render_save(group);
      
      if (child_indices_count > 0 && 
          point_in_rect(mouse_p_meters, rect)) {
        render_color(group, V4(0, 1, 0, 1));
        if (input.mouse.left.went_down) {
          if (node->type == Debug_View_Type_TREE) 
            node->type = Debug_View_Type_NONE;
          else
            node->type = Debug_View_Type_TREE;
#if 0
          node->type++;
          if (node->type > Debug_View_Type_FLAT) {
            node->type = Debug_View_Type_NONE;
          }
#endif
        }
      }
      
      v3 indent_trans = V3(0.1f*node->depth, 0, 0);
      render_translate(group, indent_trans);
      
      push_text(group, &state->font, str, transform_default());
      render_restore(group);
      
      render_translate(group, V3(0, -0.2f, 0));
      if (node->type == Debug_View_Type_TREE)
        node_index++;
      else
        node_index = node->one_past_last_child_index;
    }
    
    render_restore(group);
    render_group_output(&state->debug_arena, group, &state->renderer);
    
    arena_set_mark(&state->debug_arena, debug_render_memory);
  }
  
  
  arena_set_mark(&state->scratch, 0);
  
  arena_check_no_marks(&state->temp);
  arena_check_no_marks(&state->arena);
  state->frame_count++;
}