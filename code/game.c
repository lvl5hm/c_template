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

#define FONT_HEIGHT 32

Font load_ttf(State *state, Platform platform, String file_name) {
  Font result;
  
  stbtt_fontinfo font;
  Buffer font_file = platform.read_entire_file(file_name);
  const unsigned char *font_buffer = (const unsigned char *)font_file.data;
  stbtt_InitFont(&font, font_buffer, stbtt_GetFontOffsetForIndex(font_buffer, 0));
  
  u64 loaded_bitmaps_memory = arena_get_mark(&state->temp);
  result.first_codepoint_index = '!';
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
  
  return result;
}


Animation_Frame animation_pack_get_frame(Animation_Pack pack, Animation_Instance inst) {
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
  
  return result;
}


void draw_robot(State *state, Render_Group *group) {
  DEBUG_COUNTER_BEGIN();
  
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
  
  DEBUG_COUNTER_END();
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
  DEBUG_COUNTER_BEGIN();
  Rect_Polygon result;
  result.v[0] = box.min;
  result.v[1] = V2(box.min.x, box.max.y);
  result.v[2] = box.max;
  result.v[3] = V2(box.max.x, box.min.y);
  
  mat4x4 matrix4 = transform_apply(mat4x4_identity(), t);
  for (i32 i = 0; i < 4; i++) {
    result.v[i] = mat4x4_mul_v4(matrix4, v2_to_v4(result.v[i], 1, 1)).xy;
  }
  
  DEBUG_COUNTER_END();
  return result;
}

b32 collide_box_box(rect2 a_rect, Transform a_t, rect2 b_rect, Transform b_t) {
  DEBUG_COUNTER_BEGIN();
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
  
  DEBUG_COUNTER_END();
  return result;
}

#define SCRATCH_SIZE kilobytes(32)

void debug_render_node(State *state, Render_Group *group, Debug_Node *node) {
  String str = arena_sprintf(&state->scratch,
                             const_string("%s: %i"),
                             node->name,
                             node->cycle_count);
  push_text(group, &state->font, str, transform_default());
  render_translate(group, V3(0, -0.4f, 0));
  
  if (node->is_open) {
    for (u32 child_index = 0; child_index < sb_count(node->children); 
         child_index++) {
      debug_render_node(state, group, &node->children[child_index]);
    }
  }
}

extern GAME_UPDATE(game_update) {
  DEBUG_COUNTER_BEGIN();
  
  State *state = (State *)memory.perm;
  Arena *arena = &state->arena;
  
  gl = platform.gl;
  
  if (!state->is_initialized) {
    arena_init(&state->arena, memory.perm + sizeof(State), memory.perm_size - sizeof(State));
    arena_init(&state->scratch, memory.temp, SCRATCH_SIZE);
    arena_init(&state->temp, memory.temp + SCRATCH_SIZE, memory.temp_size - SCRATCH_SIZE);
  }
  
  if (!state->is_initialized) {
    state->frame_count = 0;
    state->rand = make_random_sequence(2312323342);
    
    // NOTE(lvl5): font stuff
    
    state->font = load_ttf(state, platform, const_string("Gugi-Regular.ttf"));
    //state->font = load_ttf(state, platform, const_string("arial.ttf"));
    
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
  
  
  v2 screen_size_v2 = v2i_to_v2(screen_size);
  v2 screen_size_meters = v2_div_s(screen_size_v2, PIXELS_PER_METER);
  v2 half_screen_size_meters = v2_div_s(screen_size_meters, 2);
#if 0  
  v2 mouse_p_meters = v2_div_s(input.mouse.p, PIXELS_PER_METER);
#endif
  
  
  u64 render_memory = arena_get_mark(&state->temp);
  Render_Group group;
  render_group_init(&state->temp, state, &group, 100000, v2i_to_v2(screen_size));
  
  for (i32 entity_index = 1; entity_index < state->entity_count; entity_index++) {
    Entity *entity = get_entity(state, entity_index);
    
    switch (entity->type) {
      case Entity_Type_ENEMY: {
        entity->t.angle += 0.01f;
        entity->t.p = V3(2.0f, 0, 0);
        render_save(&group);
        render_transform(&group, entity->t);
        
        push_rect(&group, rect2_center_size(V2(0, 0), V2(1, 1)), V4(1, 0, 0, 1));
        
        
        render_restore(&group);
      } break;
      case Entity_Type_PLAYER: {
#define PLAYER_SPEED 0.03f
        
        if (input.start.is_down) {
          entity->t.angle += 0.01f;
        }
        
        entity->t.scale = V3(1, 1, 1);
        //String text = scratch_sprintf(const_string("robot is at (%i, %i)"), (i32)entity->t.p.x, (i32)entity->t.p.y);
        
        
#if 1
        i32 h_speed = (input.move_right.is_down - 
                       input.move_left.is_down);
        i32 v_speed = (input.move_up.is_down - 
                       input.move_down.is_down);
        
        //entity->t.p = v2_to_v3(v2_sub(mouse_p_meters, half_screen_size_meters), 0);
        v3 d_p = v3_mul_s(V3((f32)h_speed, (f32)v_speed, 0), PLAYER_SPEED);entity->t.p = v3_add(entity->t.p, d_p);
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
        
        
        render_save(&group);
        render_transform(&group, entity->t);
        
        
        rect2 rect_1m = rect2_center_size(V2(0, 0), V2(1, 1));
        
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
        
#if 0        
        if (collision) {
          push_rect(&group, rect_1m, V4(0, 1, 1, 1));
        } else {
          push_rect(&group, rect_1m, V4(0, 1, 0, 1));
        }
#endif
        draw_robot(state, &group);
        
        render_restore(&group);
#endif
      } break;
    }
  }
  
  
  
  gl.ClearColor(0.0f, 0.0f, 0.0f, 1.0f);
  gl.Clear(GL_COLOR_BUFFER_BIT);
  render_group_output(state, &group, &state->renderer);
  
  
  arena_set_mark(&state->temp, render_memory);
  
  DEBUG_COUNTER_END();
  
  
  
  if (true) {
    u64 debug_render_memory = arena_get_mark(&state->temp);
    
    Debug_Node *node_stack[32];
    i32 stack_count = 1;
    
    Debug_Node root;
    root.is_open = true;
    root.cycle_count = 0;
    root.name = const_string("root");
    root.children = sb_init(&state->temp, Debug_Node, 8, true);
    node_stack[0] = &root;
    
    for (u32 event_index = 0; event_index < debug_event_count; event_index++) {
      Debug_Event *event = debug_events + event_index;
      Debug_Node *parent = node_stack[stack_count-1];
      
      if (event->type == Debug_Type_BEGIN_TIMER) {
        sb_push(parent->children, (Debug_Node){0});
        Debug_Node *node = parent->children + sb_count(parent->children)-1;
        node->name = alloc_string(&state->temp, event->name, c_string_length(event->name));
        node->is_open = true;
        node->children = sb_init(&state->temp, Debug_Node, 8, true);
        node->cycle_count = event->cycles;
        node_stack[stack_count++] = node;
      } else if (event->type == Debug_Type_END_TIMER) {
        parent->cycle_count = event->cycles - parent->cycle_count;
        stack_count--;
      }
    }
    
    Render_Group group;
    render_group_init(&state->temp, state, &group, 1000, v2i_to_v2(screen_size)); 
    
    render_save(&group);
    v3 text_p = V3(-half_screen_size_meters.x + 0.1f, 
                   half_screen_size_meters.y - 0.2f, 0);
    render_translate(&group, text_p);
    render_scale(&group, V3(0.5f, 0.5f, 0));
    
    Debug_Node *node = root.children + 0;
    debug_render_node(state, &group, node);
    
    render_restore(&group);
    render_group_output(state, &group, &state->renderer);
    
    debug_event_count = 0;
    arena_set_mark(&state->temp, debug_render_memory);
  }
  
  
  arena_set_mark(&state->scratch, 0);
  
  state->frame_count++;
}
