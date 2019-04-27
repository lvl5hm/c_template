#define LVL5_DEBUG

#include "platform.h"
#include "game.h"
#include "lvl5_math.h"
#include "lvl5_opengl.h"
#include "lvl5_context.h"
#include "lvl5_stretchy_buffer.h"
#include "lvl5_random.h"

#define SCREEN_WIDTH_IN_METERS 10.0f

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


i32 add_entity(State *state) {
  assert(state->entity_count < array_count(state->entities));
  
  i32 result = state->entity_count++;
  Entity *e = state->entities + result;
  e->is_active = true;
  return result;
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


#define push_render_item(group, type, transform) (Render_##type *)push_render_item_(group, Render_Type_##type, transform)
Render_Item *push_render_item_(Render_Group *group, Render_Type type, Transform t) {
  assert(group->item_count < group->item_capacity);
  Render_Item *item = group->items + group->item_count++;
  item->type = type;
  item->matrix = mat3x3_mul_mat3x3(transform_get_matrix(group->t),
                                   transform_get_matrix(t));
  return item;
}

void push_rect(Render_Group *group, rect2 rect, v4 color, Transform t) {
  Render_Rect *item = push_render_item(group, Rect, t);
  item->rect = rect;
  item->color = color;
}

void push_sprite(Render_Group *group, Sprite sprite, v4 color, Transform t) {
  Render_Sprite *item = push_render_item(group, Sprite, t);
  item->color = color;
  item->sprite = sprite;
}



void quad_renderer_init(State *state, gl_Funcs gl, Quad_Renderer *renderer) {
  renderer->shader = state->shader_basic;
  
  gl.GenBuffers(1, &renderer->vertex_vbo);
  gl.GenBuffers(1, &renderer->instance_vbo);
  gl.GenVertexArrays(1, &renderer->vao);
  
  gl.BindVertexArray(renderer->vao);
  gl.BindBuffer(GL_ARRAY_BUFFER, renderer->vertex_vbo);
  gl.VertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Quad_Vertex), (void *)offsetof(Quad_Vertex, p));
  gl.EnableVertexAttribArray(0);
  
  
  // NOTE(lvl5): data layout
  u64 v3_size = sizeof(v3);
  u64 model_offset = offsetof(Quad_Instance, model);
  
  gl.BindBuffer(GL_ARRAY_BUFFER, renderer->instance_vbo);
  gl.VertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Quad_Instance),
                         (void *)(model_offset+0*v3_size));
  gl.VertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Quad_Instance),
                         (void *)(model_offset+1*v3_size));
  gl.VertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(Quad_Instance),
                         (void *)(model_offset+2*v3_size));
  
  gl.VertexAttribPointer(4, 2, GL_FLOAT, GL_FALSE, sizeof(Quad_Instance),
                         (void *)offsetof(Quad_Instance, tex_offset));
  gl.VertexAttribPointer(5, 2, GL_FLOAT, GL_FALSE, sizeof(Quad_Instance),
                         (void *)offsetof(Quad_Instance, tex_scale));
  gl.VertexAttribPointer(6, 4, GL_FLOAT, GL_FALSE, sizeof(Quad_Instance),
                         (void *)offsetof(Quad_Instance, color));
  gl.VertexAttribPointer(7, 2, GL_FLOAT, GL_FALSE, sizeof(Quad_Instance),
                         (void *)offsetof(Quad_Instance, origin));
  gl.EnableVertexAttribArray(1);
  gl.EnableVertexAttribArray(2);
  gl.EnableVertexAttribArray(3);
  gl.EnableVertexAttribArray(4);
  gl.EnableVertexAttribArray(5);
  gl.EnableVertexAttribArray(6);
  gl.EnableVertexAttribArray(7);
  
  gl.VertexAttribDivisor(1, 1);
  gl.VertexAttribDivisor(2, 1);
  gl.VertexAttribDivisor(3, 1);
  gl.VertexAttribDivisor(4, 1);
  gl.VertexAttribDivisor(5, 1);
  gl.VertexAttribDivisor(6, 1);
  gl.VertexAttribDivisor(7, 1);
  gl.BindVertexArray(null);
  
  
  // NOTE(lvl5): buffer data
  Quad_Vertex vertices[4] = {
    (Quad_Vertex){V2(0, 1)},
    (Quad_Vertex){V2(0, 0)},
    (Quad_Vertex){V2(1, 1)},
    (Quad_Vertex){V2(1, 0)},
  };
  gl.BindBuffer(GL_ARRAY_BUFFER, renderer->vertex_vbo);
  gl.BufferData(GL_ARRAY_BUFFER, array_count(vertices)*sizeof(Quad_Vertex), vertices, GL_STATIC_DRAW);
  
  
  gl.GenTextures(1, &renderer->texture);
  gl.BindTexture(GL_TEXTURE_2D, renderer->texture);
  gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
}

void quad_renderer_destroy(gl_Funcs gl, Quad_Renderer *renderer) {
  gl.DeleteBuffers(1, &renderer->instance_vbo);
  gl.DeleteBuffers(1, &renderer->vertex_vbo);
  gl.DeleteVertexArrays(1, &renderer->vao);
  gl.DeleteTextures(1, &renderer->texture);
  zero_memory_slow(renderer, sizeof(Quad_Renderer));
}

void quad_renderer_draw(gl_Funcs gl, Quad_Renderer *renderer, Bitmap *bmp, mat3x3 model_mat, Quad_Instance *instances, u32 instance_count) {
  gl.BindBuffer(GL_ARRAY_BUFFER, renderer->instance_vbo);
  gl.BufferData(GL_ARRAY_BUFFER, instance_count*sizeof(Quad_Instance),
                instances, GL_DYNAMIC_DRAW);
  
  gl.UseProgram(renderer->shader);
  gl_set_uniform_mat3x3(gl, renderer->shader, "u_view", &model_mat, 1);
  
  gl.TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, bmp->width, bmp->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, bmp->data);
  
  gl.BindVertexArray(renderer->vao);
  gl.DrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, instance_count);
}




void render_group_init(Render_Group *group, i32 item_capacity, v2 screen_size) {
  group->items = alloc_array(Render_Item, item_capacity, 4);
  group->item_count = 0;
  group->screen_size = screen_size;
  group->t = transform_default();
  group->item_capacity = item_capacity;
}

void render_group_output(State *state, gl_Funcs gl, Render_Group *group) {
  Quad_Instance *instances = 0;
  sb_reserve(instances, group->item_count, false);
  
  TextureAtlas *atlas = 0;
  
  for (i32 item_index = 0; item_index < group->item_count; item_index++) {
    Render_Item *item = group->items + item_index;
    switch (item->type) {
      case Render_Type_Sprite: {
        atlas = item->Sprite.sprite.atlas;
        mat3x3 model_m = item->matrix;
        rect2 tex_rect = sprite_get_normalized_rect(item->Sprite.sprite);
        
        Quad_Instance inst = {0};
        inst.model = model_m;
        inst.tex_offset = tex_rect.min;
        inst.tex_scale = rect2_get_size(tex_rect);
        inst.color = item->Sprite.color;
        inst.origin = item->Sprite.sprite.origin;
        
        sb_push(instances, inst);
      } break;
      
      case Render_Type_Rect: {
        
      } break;
      
      default: assert(false);
    }
  }
  
  u32 vertex_vbo; 
  gl.GenBuffers(1, &vertex_vbo);
  u32 instance_vbo;
  gl.GenBuffers(1, &instance_vbo);
  
  u32 vao;
  gl.GenVertexArrays(1, &vao);
  
  // NOTE(lvl5): data layout
  gl.BindVertexArray(vao); {
    gl.BindBuffer(GL_ARRAY_BUFFER, vertex_vbo);
    gl.VertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Quad_Vertex), (void *)offsetof(Quad_Vertex, p));
    gl.EnableVertexAttribArray(0);
    
    
    u64 v3_size = sizeof(v3);
    u64 model_offset = offsetof(Quad_Instance, model);
    
    gl.BindBuffer(GL_ARRAY_BUFFER, instance_vbo);
    gl.VertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Quad_Instance),
                           (void *)(model_offset+0*v3_size));
    gl.VertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Quad_Instance),
                           (void *)(model_offset+1*v3_size));
    gl.VertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(Quad_Instance),
                           (void *)(model_offset+2*v3_size));
    
    gl.VertexAttribPointer(4, 2, GL_FLOAT, GL_FALSE, sizeof(Quad_Instance),
                           (void *)offsetof(Quad_Instance, tex_offset));
    gl.VertexAttribPointer(5, 2, GL_FLOAT, GL_FALSE, sizeof(Quad_Instance),
                           (void *)offsetof(Quad_Instance, tex_scale));
    gl.VertexAttribPointer(6, 4, GL_FLOAT, GL_FALSE, sizeof(Quad_Instance),
                           (void *)offsetof(Quad_Instance, color));
    gl.VertexAttribPointer(7, 2, GL_FLOAT, GL_FALSE, sizeof(Quad_Instance),
                           (void *)offsetof(Quad_Instance, origin));
    gl.EnableVertexAttribArray(1);
    gl.EnableVertexAttribArray(2);
    gl.EnableVertexAttribArray(3);
    gl.EnableVertexAttribArray(4);
    gl.EnableVertexAttribArray(5);
    gl.EnableVertexAttribArray(6);
    gl.EnableVertexAttribArray(7);
    
    gl.VertexAttribDivisor(1, 1);
    gl.VertexAttribDivisor(2, 1);
    gl.VertexAttribDivisor(3, 1);
    gl.VertexAttribDivisor(4, 1);
    gl.VertexAttribDivisor(5, 1);
    gl.VertexAttribDivisor(6, 1);
    gl.VertexAttribDivisor(7, 1);
  } gl.BindVertexArray(null);
  
  
  
  // NOTE(lvl5): buffering data
  {
    Quad_Vertex vertices[4] = {
      (Quad_Vertex){V2(0, 1)},
      (Quad_Vertex){V2(0, 0)},
      (Quad_Vertex){V2(1, 1)},
      (Quad_Vertex){V2(1, 0)},
    };
    
    gl.BindBuffer(GL_ARRAY_BUFFER, vertex_vbo);
    gl.BufferData(GL_ARRAY_BUFFER, array_count(vertices)*sizeof(Quad_Vertex), vertices, GL_STATIC_DRAW);
    
    gl.BindBuffer(GL_ARRAY_BUFFER, instance_vbo);
    gl.BufferData(GL_ARRAY_BUFFER, sb_count(instances)*sizeof(Quad_Instance),
                  instances, GL_STATIC_DRAW);
  }
  
  
  u32 texture;
  // NOTE(lvl5): texture
  {
    gl.GenTextures(1, &texture);
    gl.BindTexture(GL_TEXTURE_2D, texture);
    gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    
    Bitmap *bmp = &atlas->bmp;
    gl.TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, bmp->width, bmp->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, bmp->data);
  }
  
  
  
  // NOTE(lvl5): transform matrices
  {
    v2 screen_size_v2 = group->screen_size;
    f32 pixels_per_meter = screen_size_v2.x/SCREEN_WIDTH_IN_METERS;
    v2 screen_size_in_meters = v2_div_s(screen_size_v2, pixels_per_meter);
    v2 gl_units_per_meter = v2_mul_s(v2_invert(screen_size_in_meters), 2);
    
    Transform view_transform;
    view_transform.p = V2(0, 0);
    view_transform.angle = 0;
    view_transform.scale = gl_units_per_meter;
    
    mat3x3 view_matrix = transform_get_matrix(view_transform);
    
    gl.UseProgram(state->shader_basic);
    gl_set_uniform_mat3x3(gl, state->shader_basic, "u_view", &view_matrix, 1);
  }
  
  
  // NOTE(lvl5): drawing
  {
    gl.BindVertexArray(vao);
    
    gl.ClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    gl.Clear(GL_COLOR_BUFFER_BIT);
    
    gl.DrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, group->item_count);
  }
  
  // NOTE(lvl5): cleanup
  {
    gl.DeleteBuffers(1, &instance_vbo);
    gl.DeleteBuffers(1, &vertex_vbo);
    gl.DeleteVertexArrays(1, &vao);
    gl.DeleteTextures(1, &texture);
  }
}



Bitmap make_empty_bitmap(i32 width, i32 height) {
  Bitmap result;
  result.width = width;
  result.height = height;
  result.data = (byte *)alloc_array(u32, width*height, 4);
  zero_memory_slow(result.data, width*height*sizeof(u32));
  return result;
}

TextureAtlas make_texture_atlas_from_folder(Platform platform, State *state,  String folder) {
  File_List dir = platform.get_files_in_folder(folder);
  TextureAtlas result = {0};
  result.sprite_count = dir.count;
  result.sprites = alloc_array(rect2i, dir.count, 4);
  
  i32 max_width = 0;
  i32 total_height = 0;
  
  u64 loaded_bitmaps_memory = arena_get_mark(&state->temp);
  push_arena_context(&state->temp);
  Bitmap *bitmaps = alloc_array(Bitmap, dir.count, 4);
  
  for (i32 file_index = 0; file_index < dir.count; file_index++) {
    String file_name = dir.files[file_index];
    String full_name = temp_concat(folder, temp_concat(const_string("\\"), file_name));
    Bitmap sprite_bmp = load_bmp(platform, full_name);
    bitmaps[file_index] = sprite_bmp;
    
    rect2i *sprite = result.sprites + file_index;
    sprite->min.x = 0;
    sprite->min.y = total_height;
    sprite->max.x = sprite_bmp.width;
    sprite->max.y = total_height + sprite_bmp.height;
    
    if (sprite_bmp.width > max_width) {
      max_width = sprite_bmp.width;
    }
    total_height += sprite_bmp.height;
  }
  pop_context();
  
  result.bmp = make_empty_bitmap(max_width, total_height);
  
  for (i32 file_index = 0; file_index < dir.count; file_index++) {
    rect2i *sprite = result.sprites + file_index;
    Bitmap *bmp = bitmaps + file_index;
    
    u32 dst_offset = sprite->min.y*max_width*sizeof(u32);
    u32 size_in_bytes = bmp->height*bmp->width*sizeof(u32);
    
    copy_memory_slow(result.bmp.data + dst_offset, bmp->data, size_in_bytes);
  }
  
  arena_set_mark(&state->temp, loaded_bitmaps_memory);
  
  return result;
}

extern GAME_UPDATE(game_update) {
  State *state = (State *)memory.data;
  Arena *arena = &state->arena;
  
  
  if (!state->is_initialized) {
    u64 permanent_size = megabytes(64);
    u64 scratch_size = kilobytes(32);
    u64 temp_size = megabytes(32);
    
    arena_init(&state->arena, memory.data + sizeof(State), megabytes(64));
    arena_init(&state->scratch, state->arena.data + state->arena.capacity, scratch_size);
    arena_init(&state->temp, state->scratch.data + state->scratch.capacity, temp_size);
  }
  
  gl_Funcs gl = platform.gl;
  
  Context context = {0};
  context.temp_memory = &state->scratch;
  context.allocator = arena_allocator;
  context.allocator_data = &state->arena;
  push_context(context);
  
  if (!state->is_initialized) {
    Buffer shader_src = platform.read_entire_file(const_string("basic.glsl"));
    gl_Parse_Result sources = gl_parse_glsl(buffer_to_string(shader_src));
    
    state->shader_basic = gl_create_shader(gl, sources.vertex, sources.fragment);
    
    state->atlas = make_texture_atlas_from_folder(platform, state, const_string("sprites"));
    
    gl.Enable(GL_BLEND);
    gl.BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    state->is_initialized = true;
  }
  
  
  u64 render_memory = arena_get_mark(&state->temp);
  Render_Group group;
  push_arena_context(&state->temp); {
    render_group_init(&group, 10000, v2i_to_v2(screen_size));
  } pop_context();
  
  Rand seed = make_random_sequence(214434334);
  
  static f32 leg_t = 0;
  leg_t += 0.1f;
  
  static f32 body_x_t = 0;
  body_x_t += 0.1f;
  
  for (i32 i = 0; i < 3000; i++) {
    f32 scale = 3.0f;
    f32 leg_x = 0.15f;
    f32 leg_y = -0.25f;
    v2 leg_origin = V2(0.5f, 0.5f);
    
    Transform robot_t = {0};
    robot_t.p = V2(random_range(&seed, -5.0f, 5.0f), random_range(&seed, -4.0f, 4.0f));
    robot_t.angle = 0;
    robot_t.scale = V2(0.5f, 0.5f);
    
    
    group.t = robot_t;
    
    {
      Sprite spr;
      spr.atlas = &state->atlas;
      spr.index = 0;
      spr.origin = leg_origin;
      Transform sprite_transform;
      sprite_transform.p = V2(leg_x, leg_y);
      sprite_transform.scale = V2(1, 1);
      sprite_transform.angle = sin_f32(leg_t)*0.3f;
      v4 sprite_color = V4(1, 1, 1, 1);
      push_sprite(&group, spr, sprite_color, sprite_transform);
    }
    
    {
      Sprite spr;
      spr.atlas = &state->atlas;
      spr.index = 0;
      spr.origin = leg_origin;
      Transform sprite_transform;
      sprite_transform.p = V2(-leg_x, leg_y);
      sprite_transform.scale = V2(-1, 1);
      sprite_transform.angle = 0;
      v4 sprite_color = V4(1, 1, 1, 1);
      push_sprite(&group, spr, sprite_color, sprite_transform);
    }
    
    
    {
      f32 body_y = -0.2f;// + sin_f32(body_x_t)*0.07f;
      
      Sprite spr;
      spr.atlas = &state->atlas;
      spr.index = 1;
      spr.origin = V2(0.5f, 0.3f);
      Transform sprite_transform;
      sprite_transform.p = V2(0, body_y);
      
      //f32 scale = sin_f32(body_x_t)*0.07f;
      sprite_transform.scale = V2(1, 1);
      sprite_transform.angle = sin_f32(body_x_t*0.5f)*0.1f;
      v4 sprite_color = V4(1, 1, 1, 1);
      push_sprite(&group, spr, sprite_color, sprite_transform);
    }
  }
  
  push_arena_context(&state->temp); {
    render_group_output(state, gl, &group);
  } pop_context();
  
  
  
  arena_set_mark(&state->temp, render_memory);
  
  temp_clear();
  pop_context();
}
