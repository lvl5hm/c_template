#define LVL5_DEBUG

#include "platform.h"
#include "game.h"
#include "lvl5_math.h"
#include "lvl5_opengl.h"
#include "lvl5_context.h"
#include "lvl5_stretchy_buffer.h"
#include "lvl5_random.h"

#define PIXELS_PER_METER 96

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


#define push_render_item(group, type) (Render_##type *)push_render_item_(group, Render_Type_##type)
Render_Item *push_render_item_(Render_Group *group, Render_Type type) {
  assert(group->item_count < group->item_capacity);
  Render_Item *item = group->items + group->item_count++;
  item->type = type;
  item->matrix = group->matrix;
  item->color = group->color;
  return item;
}

void push_rect(Render_Group *group, rect2 rect, v4 color) {
  Render_Rect *item = push_render_item(group, Rect);
  item->rect = rect;
}

void push_sprite(Render_Group *group, Sprite sprite, Transform t) {
  render_save(group);
  render_transform(group, t);
  render_translate(group, v2_to_v3(v2_mul_s(sprite.origin, -1), 0));
  
  Render_Sprite *item = push_render_item(group, Sprite);
  item->sprite = sprite;
  render_restore(group);
}



void quad_renderer_init(Quad_Renderer *renderer, State *state, gl_Funcs gl) {
  renderer->shader = state->shader_basic;
  
  gl.GenBuffers(1, &renderer->vertex_vbo);
  gl.GenBuffers(1, &renderer->instance_vbo);
  gl.GenVertexArrays(1, &renderer->vao);
  
  // NOTE(lvl5): data layout
  gl.BindVertexArray(renderer->vao);
  gl.BindBuffer(GL_ARRAY_BUFFER, renderer->vertex_vbo);
  gl.VertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Quad_Vertex), (void *)offsetof(Quad_Vertex, p));
  gl.EnableVertexAttribArray(0);
  
  u64 v4_size = sizeof(v4);
  u64 model_offset = offsetof(Quad_Instance, model);
  
  gl.BindBuffer(GL_ARRAY_BUFFER, renderer->instance_vbo);
  gl.VertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(Quad_Instance),
                         (void *)(model_offset+0*v4_size));
  gl.VertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(Quad_Instance),
                         (void *)(model_offset+1*v4_size));
  gl.VertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(Quad_Instance),
                         (void *)(model_offset+2*v4_size));
  gl.VertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, sizeof(Quad_Instance),
                         (void *)(model_offset+3*v4_size));
  
  gl.VertexAttribPointer(5, 2, GL_FLOAT, GL_FALSE, sizeof(Quad_Instance),
                         (void *)offsetof(Quad_Instance, tex_offset));
  gl.VertexAttribPointer(6, 2, GL_FLOAT, GL_FALSE, sizeof(Quad_Instance),
                         (void *)offsetof(Quad_Instance, tex_scale));
  gl.VertexAttribPointer(7, 4, GL_FLOAT, GL_FALSE, sizeof(Quad_Instance),
                         (void *)offsetof(Quad_Instance, color));
  
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
    (Quad_Vertex){V3(0, 1, 0)},
    (Quad_Vertex){V3(0, 0, 0)},
    (Quad_Vertex){V3(1, 1, 0)},
    (Quad_Vertex){V3(1, 0, 0)},
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

void quad_renderer_draw(gl_Funcs gl, Quad_Renderer *renderer, Bitmap *bmp, mat4x4 model_mat, Quad_Instance *instances, u32 instance_count) {
  gl.BindBuffer(GL_ARRAY_BUFFER, renderer->instance_vbo);
  gl.BufferData(GL_ARRAY_BUFFER, instance_count*sizeof(Quad_Instance),
                instances, GL_DYNAMIC_DRAW);
  
  gl.UseProgram(renderer->shader);
  gl_set_uniform_mat4x4(gl, renderer->shader, "u_view", &model_mat, 1);
  
  gl.TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, bmp->width, bmp->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, bmp->data);
  
  gl.BindVertexArray(renderer->vao);
  gl.DrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, instance_count);
}




void render_group_init(Render_Group *group, i32 item_capacity, v2 screen_size) {
  group->items = alloc_array(Render_Item, item_capacity, 4);
  group->item_count = 0;
  group->screen_size = screen_size;
  group->matrix = mat4x4_identity();//transform_get_matrix(transform_default());
  group->item_capacity = item_capacity;
  group->transform_stack_count = 0;
}

void render_group_output(State *state, gl_Funcs gl, Render_Group *group, Quad_Renderer *renderer) {
  assert(group->transform_stack_count == 0);
  
  if (group->item_count != 0) {
    Quad_Instance *instances = 0;
    sb_reserve(instances, group->item_count, false);
    
    Texture_Atlas *atlas = 0;
    
    for (i32 item_index = 0; item_index < group->item_count; item_index++) {
      Render_Item *item = group->items + item_index;
      switch (item->type) {
        case Render_Type_Sprite: {
          Sprite sprite = item->Sprite.sprite;
          
          atlas = sprite.atlas;
          mat4x4 model_m = item->matrix;
          rect2 tex_rect = sprite_get_rect(sprite);
          
          Quad_Instance inst = {0};
          inst.model = mat4x4_transpose(model_m);
          inst.tex_offset = tex_rect.min;
          inst.tex_scale = rect2_get_size(tex_rect);
          inst.color = item->color;
          
          sb_push(instances, inst);
        } break;
        
        case Render_Type_Rect: {
          
        } break;
        
        default: assert(false);
      }
    }
    
    v2 screen_size_v2 = group->screen_size;
    v2 screen_size_in_meters = v2_div_s(screen_size_v2, PIXELS_PER_METER);
    v2 gl_units_per_meter = v2_mul_s(v2_invert(screen_size_in_meters), 2);
    
    Transform view_transform;
    view_transform.p = V3(0, 0, 0);
    view_transform.angle = 0;
    view_transform.scale = v2_to_v3(gl_units_per_meter, 1.0f);
    
    mat4x4 view_matrix = transform_apply(mat4x4_identity(), view_transform);
    
    quad_renderer_draw(gl, renderer, &atlas->bmp, view_matrix, instances, sb_count(instances));
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

Texture_Atlas make_texture_atlas_from_folder(Platform platform, State *state,  String folder) {
  File_List dir = platform.get_files_in_folder(folder);
  Texture_Atlas result = {0};
  result.sprite_count = dir.count;
  result.rects = alloc_array(rect2, dir.count, 4);
  
  i32 max_width = 0;
  i32 total_height = 0;
  
  u64 loaded_bitmaps_memory = arena_get_mark(&state->temp);
  push_arena_context(&state->temp);
  Bitmap *bitmaps = alloc_array(Bitmap, dir.count, 4);
  
  for (i32 file_index = 0; file_index < dir.count; file_index++) {
    String file_name = dir.files[file_index];
    String full_name = scratch_concat(folder, scratch_concat(const_string("\\"), file_name));
    Bitmap sprite_bmp = load_bmp(platform, full_name);
    bitmaps[file_index] = sprite_bmp;
    
    if (sprite_bmp.width > max_width) {
      max_width = sprite_bmp.width;
    }
    total_height += sprite_bmp.height;
  }
  pop_context();
  
  result.bmp = make_empty_bitmap(max_width, total_height);
  i32 current_y = 0;
  
  for (i32 file_index = 0; file_index < dir.count; file_index++) {
    Bitmap *bmp = bitmaps + file_index;
    rect2 *rect = result.rects + file_index;
    
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
  
  arena_set_mark(&state->temp, loaded_bitmaps_memory);
  
  return result;
}

void draw_robot(State *state, Render_Group *group, f32 body_x_t) {
  f32 leg_x = 0.1f;
  
  render_save(group);
  {
    Animation_Instance *inst = &state->right_leg_anim;
    Animation_Frame frame = animation_get_frame(inst);
    
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
    Animation_Instance *inst = &state->left_leg_anim;
    Animation_Frame frame = animation_get_frame(inst);
    
    Transform t = {0};
    t.p = v3_add(V3(leg_x, 0, 0),
                 v3_hadamard(frame.t.p, V3(-1, 1, 1)));
    t.scale = v3_hadamard(V3(0.25f, 0.25f, 1), frame.t.scale);
    t.angle = frame.t.angle;
    
    push_sprite(group, state->spr_robot_leg, t);
  }
  
  render_restore(group);
  
  
  render_save(group);
  render_rotate(group, sin_f32(body_x_t*0.5f)*0.2f);
  render_translate(group, V3(0, 0.3f, 0));
  
  {
    Transform t = {0};
    t.scale = V3(0.5f, 0.75f, 1);
    push_sprite(group, state->spr_robot_eye, t);
  }
  
  {
    Transform t = {0};
    t.scale = V3(0.2f, 0.2f, 1);
    
    push_sprite(group, state->spr_robot_torso, t);
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
  context.allocator = arena_allocator;
  context.allocator_data = &state->arena;
  push_context(context);
  
  if (!state->is_initialized) {
    Buffer shader_src = platform.read_entire_file(const_string("basic.glsl"));
    gl_Parse_Result sources = gl_parse_glsl(buffer_to_string(shader_src));
    
    state->shader_basic = gl_create_shader(gl, sources.vertex, sources.fragment);
    
    state->atlas = make_texture_atlas_from_folder(platform, state, const_string("sprites"));
    
    state->spr_robot_eye = make_sprite(&state->atlas, 2, V2(0.5f, 0.5f));
    state->spr_robot_leg = make_sprite(&state->atlas, 1, V2(0, 1));
    state->spr_robot_torso = make_sprite(&state->atlas, 0, V2(0.5f, 0.5f));
    
    gl.Enable(GL_BLEND);
    gl.BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    quad_renderer_init(&state->renderer, state, gl);
    add_entity(state, Entity_Type_NONE); // filler entity
    add_entity(state, Entity_Type_PLAYER);
    
    
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
        V3(0.12f, 0, 0), // p
        V3(1, 1, 1), // scale
        PI*0.4, // angle
      },
      V4(1, 1, 1, 1),
    };
    
    Animation_Frame frame2 = (Animation_Frame){
      0.66f,
      (Transform){
        V3(0.15f, 0, 0), // p
        V3(1, 1, 1), // scale
        0, // angle
      },
      V4(1, 1, 1, 1),
    };
    
    Animation_Frame frame3 = (Animation_Frame){
      1,
      (Transform){
        V3(0, 0, 0), // p
        V3(1, 1, 1), // scale
        0, // angle
      },
      V4(1, 1, 1, 1),
    };
    
    
    sb_push(a.frames, frame0);
    sb_push(a.frames, frame1);
    sb_push(a.frames, frame2);
    sb_push(a.frames, frame3);
    
    a.frame_count = sb_count(a.frames);
    state->robot_leg_animation = a;
    
    
    state->left_leg_anim.anim = &state->robot_leg_animation;
    state->right_leg_anim.anim = &state->robot_leg_animation;
    state->left_leg_anim.position = 0.5f;
    
    
#if 0    
    Rand seed = make_random_sequence(214434334);
    for (i32 i = 0; i < 100; i++) {
      i32 player_index = add_entity_player(state);
      Entity *e = get_entity(state, player_index);
      e->t.p = V3(random_range(&seed, -10, 0), random_range(&seed, -3, 3), 0);
    }
#endif
    
    
    state->is_initialized = true;
  }
  
  f32 anim_speed = 0.02f;
  {
    Animation_Instance *inst = &state->right_leg_anim;
    inst->position += anim_speed;
    if (inst->position > 1) {
      inst->position = 0;
    }
  }
  {
    Animation_Instance *inst = &state->left_leg_anim;
    inst->position += anim_speed;
    if (inst->position > 1) {
      inst->position = 0;
    }
  }
  
  
  u64 render_memory = arena_get_mark(&state->temp);
  Render_Group group;
  push_arena_context(&state->temp); {
    render_group_init(&group, 100000, v2i_to_v2(screen_size));
  } pop_context();
  
  
  static f32 body_x_t = 0;
  body_x_t += 0.1f;
  
  for (i32 entity_index = 1; entity_index < state->entity_count; entity_index++) {
    Entity *entity = get_entity(state, entity_index);
    
    switch (entity->type) {
      case Entity_Type_PLAYER: {
#define PLAYER_SPEED 0.03f
        
#if 1
        f32 h_speed = (f32)(input.move_right.is_down - 
                            input.move_left.is_down);
        f32 v_speed = (f32)(input.move_up.is_down - 
                            input.move_down.is_down);
        v3 d_p = v3_mul_s(V3(h_speed, v_speed, 0), PLAYER_SPEED);
        entity->t.p = v3_add(entity->t.p, d_p);
        entity->t.scale = V3(sign_f32(h_speed), 1, 1);
        if (entity->t.scale.x == 0) {
          entity->t.scale.x = 1;
        }
#endif
        
        render_save(&group);
        render_color(&group, V4(0, 1, 0, 1));
        render_transform(&group, entity->t);
        draw_robot(state, &group, body_x_t);
        render_restore(&group);
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
