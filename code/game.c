#define LVL5_DEBUG

#include "platform.h"
#include "game.h"
#include "lvl5_math.h"
#include "lvl5_opengl.h"
#include "lvl5_context.h"
#include "lvl5_stretchy_buffer.h"

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
  Render_Item *item = group->items + group->item_count++;
  item->type = type;
  item->t = t;
  return item;
}

void push_rect(Render_Group *group, rect2 rect, v4 color, Transform t) {
  Render_Rect *item = push_render_item(group, Rect, t);
  item->rect = rect;
  item->color = color;
}

void push_sprite(Render_Group *group, Sprite sprite, rect2 rect, v4 color, Transform t) {
  Render_Sprite *item = push_render_item(group, Sprite, t);
  item->rect = rect;
  item->color = color;
  item->sprite = sprite;
}

void render_group_init(Render_Group *group, i32 item_capacity, v2 screen_size) {
  group->items = alloc_array(Render_Item, item_capacity, 4);
  group->item_count = 0;
  group->screen_size = screen_size;
}

void render_group_output(State *state, gl_Funcs gl, Render_Group *group) {
  assert(group->item_count <= 32);
  
  u32 vbo; 
  gl.GenBuffers(1, &vbo);
  u32 ebo;
  gl.GenBuffers(1, &ebo);
  u32 vao;
  gl.GenVertexArrays(1, &vao);
  
  gl.BindVertexArray(vao); {
    gl.BindBuffer(GL_ARRAY_BUFFER, vbo);
    gl.VertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *)offsetof(Vertex, p));
    gl.VertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *)offsetof(Vertex, tex_coord));
    gl.VertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *)offsetof(Vertex, color));
    gl.VertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                           (void *)offsetof(Vertex, transform_index));
    
    gl.EnableVertexAttribArray(0);
    gl.EnableVertexAttribArray(1);
    gl.EnableVertexAttribArray(2);
    gl.EnableVertexAttribArray(3);
    
    gl.BindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
  }
  
  Vertex *sprite_vertices = 0;
  sb_reserve(sprite_vertices, group->item_count*4, false);
  
  u32 *sprite_indices = 0;
  sb_reserve(sprite_indices, group->item_count*6, false);
  
  mat3x3 *model_matrices = 0;
  sb_reserve(model_matrices, group->item_count, false);
  
  u32 current_vertex_index = 0;
  f32 current_transform_index = 0;
  
  TextureAtlas *atlas = 0;
  
  for (i32 item_index = 0; item_index < group->item_count; item_index++) {
    Render_Item *item = group->items + item_index;
    switch (item->type) {
      case Render_Type_Sprite: {
        atlas = item->Sprite.sprite.atlas;
        
        rect2 tex = sprite_get_normalized_rect(item->Sprite.sprite);
        rect2 rect = item->Sprite.rect;
        
        Vertex v_0 = (Vertex){
          V2(rect.min.x, rect.min.y),
          V2(tex.min.x, tex.min.y),
          item->Sprite.color,
          current_transform_index,
        };
        Vertex v_1 = (Vertex){
          V2(rect.min.x, rect.max.y),
          V2(tex.min.x, tex.max.y),
          item->Sprite.color,
          current_transform_index,
        };
        
        Vertex v_2 = (Vertex){
          V2(rect.max.x, rect.max.y),
          V2(tex.max.x, tex.max.y),
          item->Sprite.color,
          current_transform_index,
        };
        
        Vertex v_3 = (Vertex){
          V2(rect.max.x, rect.min.y),
          V2(tex.max.x, tex.min.y),
          item->Sprite.color,
          current_transform_index,
        };
        
        sb_push(sprite_vertices, v_0);
        sb_push(sprite_vertices, v_1);
        sb_push(sprite_vertices, v_2);
        sb_push(sprite_vertices, v_3);
        
        sb_push(sprite_indices, current_vertex_index+0);
        sb_push(sprite_indices, current_vertex_index+1);
        sb_push(sprite_indices, current_vertex_index+2);
        sb_push(sprite_indices, current_vertex_index+0);
        sb_push(sprite_indices, current_vertex_index+2);
        sb_push(sprite_indices, current_vertex_index+3);
        
        mat3x3 model_m = transform_get_matrix(item->t);
        sb_push(model_matrices, model_m);
        
        current_transform_index += 1;
        current_vertex_index += 4;
      } break;
      
      case Render_Type_Rect: {
        
      } break;
      
      default: assert(false);
    }
  }
  
  
  // NOTE(lvl5): buffering data
  {
    gl.BindBuffer(GL_ARRAY_BUFFER, vbo);
    gl.BufferData(GL_ARRAY_BUFFER, sb_count(sprite_vertices)*sizeof(Vertex), sprite_vertices, GL_DYNAMIC_DRAW);
    
    gl.BindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    gl.BufferData(GL_ELEMENT_ARRAY_BUFFER, sb_count(sprite_indices)*sizeof(u32), sprite_indices, GL_DYNAMIC_DRAW);
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
  
  
#if 1
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
    
    
    gl.BindVertexArray(vao);
    gl.UseProgram(state->shader_basic);
    
    gl_set_uniform_mat3x3(gl, state->shader_basic, "u_model_arr", model_matrices, group->item_count);
    gl_set_uniform_mat3x3(gl, state->shader_basic, "u_view", &view_matrix, 1);
  }
#endif
  
  // NOTE(lvl5): drawing
  {
    gl.ClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    gl.Clear(GL_COLOR_BUFFER_BIT);
    
    u32 index_count = sb_count(sprite_indices);
    gl.DrawElements(GL_TRIANGLES, index_count, GL_UNSIGNED_INT, 0);
  }
  
  // NOTE(lvl5): cleanup
  {
    gl.DeleteBuffers(1, &vbo);
    gl.DeleteBuffers(1, &ebo);
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
    render_group_init(&group, 256, v2i_to_v2(screen_size));
  } pop_context();
  
  
  f32 leg_x = 0.25f;
  f32 leg_y = -0.25f;
  {
    Sprite spr;
    spr.atlas = &state->atlas;
    spr.index = 0;
    Transform sprite_transform;
    sprite_transform.p = V2(-leg_x, leg_y);
    sprite_transform.scale = V2(1, 1);
    sprite_transform.angle = 0;
    rect2 sprite_rect = rect2_center_size(V2(0, 0), V2(1, 1));
    v4 sprite_color = V4(1, 1, 1, 1);
    push_sprite(&group, spr, sprite_rect, sprite_color, sprite_transform);
  }
  
  {
    Sprite spr;
    spr.atlas = &state->atlas;
    spr.index = 0;
    Transform sprite_transform;
    sprite_transform.p = V2(leg_x, leg_y);
    sprite_transform.scale = V2(-1, 1);
    sprite_transform.angle = 0;
    rect2 sprite_rect = rect2_center_size(V2(0, 0), V2(1, 1));
    v4 sprite_color = V4(1, 1, 1, 1);
    push_sprite(&group, spr, sprite_rect, sprite_color, sprite_transform);
  }
  
  
  {
    static f32 body_x_t = 0;
    body_x_t += 0.2f;
    f32 body_y = sin_f32(body_x_t)*0.07f;
    
    Sprite spr;
    spr.atlas = &state->atlas;
    spr.index = 1;
    Transform sprite_transform;
    sprite_transform.p = V2(0, body_y);
    sprite_transform.scale = V2(1, 1);
    sprite_transform.angle = 0;
    rect2 sprite_rect = rect2_center_size(V2(0, 0), V2(1, 1));
    v4 sprite_color = V4(1, 1, 1, 1);
    push_sprite(&group, spr, sprite_rect, sprite_color, sprite_transform);
  }
  
  
  render_group_output(state, gl, &group);
  
  
  
  arena_set_mark(&state->temp, render_memory);
  
  temp_clear();
  pop_context();
}
