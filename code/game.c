#define LVL5_DEBUG

#include "platform.h"
#include "game.h"
#include "lvl5_math.h"
#include "lvl5_opengl.h"
#include "lvl5_context.h"
#include "lvl5_stretchy_buffer.h"
#include "lvl5_random.h"
#include "renderer.c"

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"



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


void push_arena_context(Arena *arena) {
  Context ctx = get_context();
  ctx.allocator = arena_allocator;
  ctx.allocator_data = arena;
  push_context(ctx);
}

Bitmap make_empty_bitmap(i32 width, i32 height) {
  Bitmap result;
  result.width = width;
  result.height = height;
  result.data = (byte *)alloc_array(u32, width*height, 4);
  zero_memory_slow(result.data, width*height*sizeof(u32));
  return result;
}



Texture_Atlas make_texture_atlas_from_bitmaps(Bitmap *bitmaps, i32 count) {
  Texture_Atlas result = {0};
  result.sprite_count = count;
  result.rects = alloc_array(rect2, count, 4);
  
  i32 max_width = 0;
  i32 total_height = 0;
  
  for (i32 bitmap_index = 0; bitmap_index < count; bitmap_index++) {
    Bitmap *bmp = bitmaps + bitmap_index;
    if (bmp->width > max_width) {
      max_width = bmp->width;
    }
    total_height += bmp->height;
  }
  
  result.bmp = make_empty_bitmap(max_width, total_height);
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
  
  push_arena_context(&state->temp);
  Bitmap *bitmaps = alloc_array(Bitmap, dir.count, 4);
  
  for (i32 file_index = 0; file_index < dir.count; file_index++) {
    String file_name = dir.files[file_index];
    String full_name = scratch_concat(folder, scratch_concat(const_string("\\"), file_name));
    Bitmap sprite_bmp = load_bmp(platform, full_name);
    bitmaps[file_index] = sprite_bmp;
  }
  pop_context();
  
  Texture_Atlas result = make_texture_atlas_from_bitmaps(bitmaps, dir.count); 
  arena_set_mark(&state->temp, loaded_bitmaps_memory);
  
  return result;
}

#define FONT_HEIGHT 32

Bitmap get_codepoint_bitmap(stbtt_fontinfo font, char c) {
  i32 width;
  i32 height;
  f32 scale = stbtt_ScaleForPixelHeight(&font, 32);
  byte *single_bitmap = stbtt_GetCodepointBitmap(&font, 0, scale, c, &width, &height, 0, 0);
  Bitmap bitmap = make_empty_bitmap(width, 32);
  
  i32 x0, y0, x1, y1;
  stbtt_GetCodepointBitmapBox(&font, c, scale, scale, &x0,&y0,&x1,&y1);
  
  if (c == '-') {
    x0 = 32;
  }
  
  u32 *row = (u32 *)bitmap.data + (FONT_HEIGHT-1 + y1)*width;
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
  
  return bitmap;
}

Font load_ttf(State *state, Platform platform, String file_name) {
  Font result;
  
  stbtt_fontinfo font;
  Buffer font_file = platform.read_entire_file(file_name);
  const unsigned char *font_buffer = (const unsigned char *)font_file.data;
  stbtt_InitFont(&font, font_buffer, stbtt_GetFontOffsetForIndex(font_buffer, 0));
  
  u64 loaded_bitmaps_memory = arena_get_mark(&state->temp);
  push_arena_context(&state->temp);
  result.first_codepoint_index = '!';
  
  Bitmap *bitmaps = 0;
  for (char ch = result.first_codepoint_index; ch <= '~'; ch++) {
    Bitmap bmp = get_codepoint_bitmap(font, ch);
    sb_push(bitmaps, bmp);
  }
  
  arena_set_mark(&state->temp, loaded_bitmaps_memory);
  pop_context();
  
  Texture_Atlas atlas = make_texture_atlas_from_bitmaps(bitmaps, sb_count(bitmaps));
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
    inst.positions = scratch_alloc_array(f32, 2, 4);
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
    
#if 0    
    static i32 index = 0;
    if ((index / 10) > 25) {
      index = 0;
    }
    
    Sprite letter;
    letter.atlas = &state->font.atlas;
    letter.index = index / 10;
    letter.origin = V2(0.5f, 0.5f);
#endif
    push_sprite(group, state->spr_robot_eye, t);
    
    //index++;
  }
  
  render_restore(group);
}

Sprite make_sprite(Texture_Atlas *atlas, i32 index, v2 origin) {
  Sprite result;
  result.atlas = atlas;
  result.index = index;
  result.origin = origin;
  return result;
}



extern GAME_UPDATE(game_update) {
  State *state = (State *)memory.data;
  Arena *arena = &state->arena;
  
  
  if (!state->is_initialized) {
    u64 permanent_size = megabytes(64);
    u64 scratch_size = kilobytes(32);
    u64 temp_size = megabytes(256);
    
    arena_init(&state->arena, memory.data + sizeof(State), megabytes(64));
    arena_init(&state->scratch, state->arena.data + state->arena.capacity, scratch_size);
    arena_init(&state->temp, state->scratch.data + state->scratch.capacity, temp_size);
  }
  
  gl_Funcs gl = platform.gl;
  
  Context context = {0};
  context.scratch_memory = &state->scratch;
  context.allocator_data = &state->arena;
  context.allocator = arena_allocator;
  push_context(context);
  
  if (!state->is_initialized) {
    state->rand = make_random_sequence(2312323342);
    
    // NOTE(lvl5): font stuff
    
    state->font = load_ttf(state, platform, const_string("Gugi-Regular.ttf"));
    
    Buffer shader_src = platform.read_entire_file(const_string("basic.glsl"));
    gl_Parse_Result sources = gl_parse_glsl(buffer_to_string(shader_src));
    
    state->shader_basic = gl_create_shader(gl, sources.vertex, sources.fragment);
    
    state->atlas = make_texture_atlas_from_folder(platform, state, const_string("sprites"));
    
    state->spr_robot_eye = make_sprite(&state->atlas, 0, V2(0.5f, 0.5f));
    state->spr_robot_leg = make_sprite(&state->atlas, 1, V2(0, 1));
    state->spr_robot_torso = make_sprite(&state->atlas, 2, V2(0.5f, 0.5f));
    
    gl.Enable(GL_BLEND);
    gl.BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    quad_renderer_init(&state->renderer, state, gl);
    add_entity(state, Entity_Type_NONE); // filler entity
    add_entity(state, Entity_Type_PLAYER);
    
    
    {
      Animation_Pack *pack = state->robot_parts + Robot_Part_RIGHT_LEG;
      pack->animation_count = 2;
      pack->animations = alloc_array(Animation, pack->animation_count, 4);
      {
        Animation a;
        a.frames = 0;
        sb_reserve(a.frames, 16, true);
        
        Animation_Frame frame0 = (Animation_Frame){
          0,
          (Transform){
            V3(0, 0, 0), // p
            V3(1, 1, 1), // scale
            0, // angle
          },
          V4(1, 1, 1, 1),
        };
        
        Animation_Frame frame1 = (Animation_Frame){
          0.33f,
          (Transform){
            V3(0.2f, 0, 0), // p
            V3(1, 1, 1), // scale
            PI*0.4, // angle
          },
          V4(1, 1, 1, 1),
        };
        
        Animation_Frame frame2 = (Animation_Frame){
          0.66f,
          (Transform){
            V3(0.26f, 0, 0), // p
            V3(1, 1, 1), // scale
            0, // angle
          },
          V4(1, 1, 1, 1),
        };
        
        
        sb_push(a.frames, frame0);
        sb_push(a.frames, frame1);
        sb_push(a.frames, frame2);
        
        frame0.position = 1.0f;
        sb_push(a.frames, frame0);
        
        a.frame_count = sb_count(a.frames);
        a.speed = 0.7f;
        pack->animations[Robot_Animation_WALK] = a;
      }
      
      {
        Animation a;
        a.frames = 0;
        sb_reserve(a.frames, 16, true);
        
        Animation_Frame frame0 = (Animation_Frame){
          0,
          (Transform){
            V3(0, 0, 0), // p
            V3(1, 1, 1), // scale
            0, // angle
          },
          V4(1, 1, 1, 1),
        };
        
        
        Animation_Frame frame1 = (Animation_Frame){
          0.5f,
          (Transform){
            V3(0, -0.08f, 0), // p
            V3(1, 1, 1), // scale
            0.2f, // angle
          },
          V4(1, 1, 1, 1),
        };
        
        sb_push(a.frames, frame0);
        sb_push(a.frames, frame1);
        
        frame0.position = 1.0f;
        sb_push(a.frames, frame0);
        a.frame_count = sb_count(a.frames);
        a.speed = 1;
        
        pack->animations[Robot_Animation_IDLE] = a;
      }
    }
    
    
    {
      Animation_Pack *pack = state->robot_parts + Robot_Part_BODY;
      pack->animation_count = 2;
      pack->animations = alloc_array(Animation, pack->animation_count, 4);
      {
        Animation a;
        a.frames = 0;
        sb_reserve(a.frames, 16, true);
        
        Animation_Frame frame0 = (Animation_Frame){
          0,
          (Transform){
            V3(0.1f, 0, 0), // p
            V3(1, 1, 1), // scale
            0.2f, // angle
          },
          V4(1, 1, 1, 1),
        };
        
        Animation_Frame frame1 = (Animation_Frame){
          0.5f,
          (Transform){
            V3(0.1f, 0, 0), // p
            V3(1, 1, 1), // scale
            0.1f, // angle
          },
          V4(1, 1, 1, 1),
        };
        
        sb_push(a.frames, frame0);
        sb_push(a.frames, frame1);
        
        frame0.position = 1.0f;
        sb_push(a.frames, frame0);
        
        a.frame_count = sb_count(a.frames);
        a.speed = 0.7f;
        pack->animations[Robot_Animation_WALK] = a;
      }
      
      {
        Animation a;
        a.frames = 0;
        sb_reserve(a.frames, 16, true);
        
        Animation_Frame frame0 = (Animation_Frame){
          0,
          (Transform){
            V3(0, 0.05f, 0), // p
            V3(1, 1, 1), // scale
            0, // angle
          },
          V4(1, 1, 1, 1),
        };
        
        Animation_Frame frame1 = (Animation_Frame){
          0.5f,
          (Transform){
            V3(0, -0.05f, 0), // p
            V3(1, 1, 1), // scale
            0, // angle
          },
          V4(1, 1, 1, 1),
        };
        
        
        sb_push(a.frames, frame0);
        sb_push(a.frames, frame1);
        
        frame0.position = 1.0f;
        sb_push(a.frames, frame0);
        a.frame_count = sb_count(a.frames);
        a.speed = 1;
        
        pack->animations[Robot_Animation_IDLE] = a;
      }
    }
    
    
    {
      Animation_Pack *pack = state->robot_parts + Robot_Part_EYE;
      pack->animation_count = 2;
      pack->animations = alloc_array(Animation, pack->animation_count, 4);
      {
        Animation a;
        a.frames = 0;
        sb_reserve(a.frames, 16, true);
        
        Animation_Frame frame0 = (Animation_Frame){
          0,
          (Transform){
            V3(0.07f, 0, 0), // p
            V3(1, 1, 1), // scale
            0, // angle
          },
          V4(1, 1, 1, 1),
        };
        
        Animation_Frame frame1 = (Animation_Frame){
          0.5f,
          (Transform){
            V3(0.09f, 0, 0), // p
            V3(1, 1, 1), // scale
            0, // angle
          },
          V4(1, 1, 1, 1),
        };
        
        sb_push(a.frames, frame0);
        sb_push(a.frames, frame1);
        
        frame0.position = 1.0f;
        sb_push(a.frames, frame0);
        
        a.frame_count = sb_count(a.frames);
        a.speed = 1.5f;
        pack->animations[Robot_Animation_WALK] = a;
      }
      
      {
        Animation a;
        a.frames = 0;
        sb_reserve(a.frames, 16, true);
        
        Animation_Frame frame0 = (Animation_Frame){
          0,
          (Transform){
            V3(0, 0, 0), // p
            V3(1, 1, 1), // scale
            0, // angle
          },
          V4(1, 1, 1, 1),
        };
        
        Animation_Frame frame1 = (Animation_Frame){
          0.5f,
          (Transform){
            V3(0, 0, 0), // p
            V3(1, 1, 1), // scale
            0, // angle
          },
          V4(1, 1, 1, 1),
        };
        
        
        Animation_Frame frame2 = (Animation_Frame){
          0.55f,
          (Transform){
            V3(-0.1f, 0.07f, 0), // p
            V3(1, 1, 1), // scale
            0, // angle
          },
          V4(1, 1, 1, 1),
        };
        
        
        Animation_Frame frame3 = (Animation_Frame){
          0.7f,
          (Transform){
            V3(-0.1f, 0.07f, 0), // p
            V3(1, 1, 1), // scale
            0, // angle
          },
          V4(1, 1, 1, 1),
        };
        
        Animation_Frame frame4 = (Animation_Frame){
          0.75f,
          (Transform){
            V3(-0.1f, 0.07f, 0), // p
            V3(1, 1, 1), // scale
            0, // angle
          },
          V4(1, 1, 1, 1),
        };
        
        
        Animation_Frame frame5 = (Animation_Frame){
          0.85f,
          (Transform){
            V3(0.07f, 0.03f, 0), // p
            V3(1, 1, 1), // scale
            0, // angle
          },
          V4(1, 1, 1, 1),
        };
        
        
        Animation_Frame frame6 = (Animation_Frame){
          0.95f,
          (Transform){
            V3(0.07f, 0.03f, 0), // p
            V3(1, 1, 1), // scale
            0, // angle
          },
          V4(1, 1, 1, 1),
        };
        
        sb_push(a.frames, frame0);
        sb_push(a.frames, frame1);
        sb_push(a.frames, frame2);
        sb_push(a.frames, frame3);
        sb_push(a.frames, frame4);
        sb_push(a.frames, frame5);
        sb_push(a.frames, frame6);
        
        
        frame0.position = 1.0f;
        sb_push(a.frames, frame0);
        a.frame_count = sb_count(a.frames);
        a.speed = 0.1f;
        
        pack->animations[Robot_Animation_IDLE] = a;
      }
    }
    
    
    Animation_Instance *inst = &state->robot_anim;
    inst->weights = alloc_array(f32, Robot_Animation_COUNT, 4);
    inst->positions = alloc_array(f32, Robot_Animation_COUNT, 4);
    
    state->is_initialized = true;
  }
  
  
  
  u64 render_memory = arena_get_mark(&state->temp);
  Render_Group group;
  push_arena_context(&state->temp); {
    render_group_init(&group, 100000, v2i_to_v2(screen_size));
  } pop_context();
  
#if 0  
  v2 screen_size_v2 = v2i_to_v2(screen_size);
  v2 screen_size_meters = v2_div_s(screen_size_v2, PIXELS_PER_METER);
  v2 half_screen_size_meters = v2_div_s(screen_size_meters, 2);
  v2 mouse_p_meters = v2_div_s(input.mouse.p, PIXELS_PER_METER);
#endif
  
  for (i32 entity_index = 1; entity_index < state->entity_count; entity_index++) {
    Entity *entity = get_entity(state, entity_index);
    
    switch (entity->type) {
      case Entity_Type_PLAYER: {
#define PLAYER_SPEED 0.03f
        
        entity->t.scale = V3(1, 1, 1);
        i32_to_string(-5343);
        //String text = scratch_sprintf(const_string("robot is at (%i, %i)"), (i32)entity->t.p.x, (i32)entity->t.p.y);
        String text = const_string("         -53");
        push_text(&group, state->font, text, entity->t);
        
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
        //render_color(&group, V4(0, 1, 0, 1));
        render_transform(&group, entity->t);
        draw_robot(state, &group);
        render_restore(&group);
#endif
      } break;
    }
  }
  
  
  
  gl.ClearColor(0.0f, 0.0f, 0.0f, 1.0f);
  gl.Clear(GL_COLOR_BUFFER_BIT);
  push_arena_context(&state->temp); {
    render_group_output(state, gl, &group, &state->renderer);
  } pop_context();
  
  
  
  arena_set_mark(&state->temp, render_memory);
  
  scratch_clear();
  pop_context();
}
