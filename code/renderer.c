#include "renderer.h"
#include "debug.h"
#define PIXELS_PER_METER 96
#include "font.c"

mat4x4 transform_apply(mat4x4 matrix, Transform t) {
  DEBUG_FUNCTION_BEGIN();
  
  mat4x4 result = matrix;
  result = mat4x4_translate(result, t.p);
  result = mat4x4_rotate(result, t.angle);
  result = mat4x4_scale(result, t.scale);
  
  DEBUG_FUNCTION_END();
  return result;
}

Transform make_transform(v3 p, v3 scale, f32 angle) {
  Transform t;
  t.p = p;
  t.scale = scale;
  t.angle = angle;
  return t;
}

void render_save(Render_Group *group) {
  assert(group->state_stack_count < array_count(group->state_stack));
  group->state_stack[group->state_stack_count++] = group->state;
}

void render_restore(Render_Group *group) {
  assert(group->state_stack_count > 0);
  group->state = group->state_stack[--group->state_stack_count];
}

void render_translate(Render_Group *group, v3 p) {
  group->state.matrix = mat4x4_translate(group->state.matrix, p);
}

void render_scale(Render_Group *group, v3 scale) {
  group->state.matrix = mat4x4_scale(group->state.matrix, scale);
}

void render_rotate(Render_Group *group, f32 angle) {
  group->state.matrix = mat4x4_rotate(group->state.matrix, angle);
}

void render_transform(Render_Group *group, Transform t) {
  group->state.matrix = transform_apply(group->state.matrix, t);
}

void render_color(Render_Group *group, v4 color) {
  group->state.color = color;
}

void render_font(Render_Group *group, Font *font) {
  group->state.font = font;
}


rect2 sprite_get_rect(Sprite spr) {
  rect2 result = spr.atlas->rects[spr.index];
  return result;
}



#define push_render_item(group, type) (Render_##type *)push_render_item_(group, Render_Type_##type)
Render_Item *push_render_item_(Render_Group *group, Render_Type type) {
  assert(group->item_count < group->item_capacity);
  Render_Item *item = group->items + group->item_count++;
  item->type = type;
  item->state = group->state;
  return item;
}

void push_sprite(Render_Group *group, Sprite sprite, Transform t) {
  DEBUG_FUNCTION_BEGIN();
  if (group->item_count == 20) {
    assert(sprite.atlas->rects);
  }
  
  render_save(group);
  render_transform(group, t);
  render_translate(group, v2_to_v3(v2_mul_s(sprite.origin, -1), 0));
  
  Render_Sprite *item = push_render_item(group, Sprite);
  item->sprite = sprite;
  render_restore(group);
  DEBUG_FUNCTION_END();
}

void push_rect(Render_Group *group, rect2 rect) {
  Transform t;
  t.angle = 0;
  t.p = v2_to_v3(rect2_get_center(rect), 0);
  t.scale = v2_to_v3(rect2_get_size(rect), 0);
  
  Sprite spr;
  spr.atlas = group->debug_atlas;
  spr.index = 0;
  spr.origin = V2(0.5f, 0.5f);
  
  render_save(group);
  push_sprite(group, spr, t);
  render_restore(group);
}


void push_line(Render_Group *group, v2 start, v2 end, f32 thick) {
  v2 diff = v2_sub(end, start);
  f32 angle = atan2_f32(diff.y, diff.x);
  f32 width = v2_length(diff);
  render_save(group);
  render_translate(group, v2_to_v3(start, 0));
  render_rotate(group, angle);
  push_rect(group, rect2_min_size(V2(0, -thick*0.5f), V2(width, thick)));
  render_restore(group);
}

void push_line_color(Render_Group *group, v2 start, v2 end, f32 thick, v4 color) {
  render_save(group);
  render_color(group, color);
  push_line(group, start, end, thick);
  render_restore(group);
}

void push_rect_outline(Render_Group *group, rect2 rect, f32 thick) {
  push_rect(group, rect2_min_max(rect.min,
                                 V2(rect.min.x+thick, rect.max.y)));
  push_rect(group, rect2_min_max(V2(rect.min.x, rect.max.y-thick),
                                 rect.max));
  push_rect(group, rect2_min_max(rect.min,
                                 V2(rect.max.x, rect.min.y+thick)));
  push_rect(group, rect2_min_max(V2(rect.max.x-thick, rect.min.y),
                                 rect.max));
}

void push_text(Render_Group *group, String text) {
  DEBUG_FUNCTION_BEGIN();
  
  render_save(group);
  render_scale(group, v3_invert(V3(PIXELS_PER_METER, PIXELS_PER_METER, 1)));
  
  for (u32 char_index = 0; char_index < text.count; char_index++) {
    char ch = text.data[char_index];
    
#if 0
    assert(ch >= font->first_codepoint_index && 
           ch < font->first_codepoint_index + font->codepoint_count);
#endif
    
    Codepoint_Metrics metrics = font_get_metrics(group->state.font, ch);
    Sprite spr = font_get_sprite(group->state.font, ch);
    
    rect2 rect = sprite_get_rect(spr);
    v2 size_pixels = v2_hadamard(rect2_get_size(rect), 
                                 V2((f32)spr.atlas->bmp.width,
                                    (f32)spr.atlas->bmp.height));
    
    Transform letter_t = transform_default();
    letter_t.scale = v2_to_v3(size_pixels, 0);
    push_sprite(group, spr, letter_t);
    
    render_translate(group, V3(metrics.advance, 0, 0));
  }
  
  render_restore(group);
  
  DEBUG_FUNCTION_END();
}


void quad_renderer_init(Quad_Renderer *renderer, State *state) {
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
  gl.BufferData(GL_ARRAY_BUFFER, array_count(vertices)*sizeof(Quad_Vertex), 
                vertices, GL_STATIC_DRAW);
  
  gl.GenTextures(1, &renderer->texture);
  gl.BindTexture(GL_TEXTURE_2D, renderer->texture);
  gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
}

void quad_renderer_destroy(Quad_Renderer *renderer) {
  gl.DeleteBuffers(1, &renderer->instance_vbo);
  gl.DeleteBuffers(1, &renderer->vertex_vbo);
  gl.DeleteVertexArrays(1, &renderer->vao);
  gl.DeleteTextures(1, &renderer->texture);
  zero_memory_slow(renderer, sizeof(Quad_Renderer));
}

void quad_renderer_draw(Quad_Renderer *renderer, Bitmap *bmp,
                        mat4x4 model_mat, Quad_Instance *instances, u32 instance_count) {
  DEBUG_FUNCTION_BEGIN();
  
  DEBUG_SECTION_BEGIN(_buffer_data);
  gl.BindBuffer(GL_ARRAY_BUFFER, renderer->instance_vbo);
  gl.BufferData(GL_ARRAY_BUFFER, instance_count*sizeof(Quad_Instance),
                instances, GL_DYNAMIC_DRAW);
  DEBUG_SECTION_END(_buffer_data);
  
  
  DEBUG_SECTION_BEGIN(_set_texture);
  gl.TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, bmp->width, bmp->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, bmp->data);
  DEBUG_SECTION_END(_set_texture);
  
  
  gl.UseProgram(renderer->shader);
  gl_set_uniform_mat4x4(gl, renderer->shader, "u_view", &model_mat, 1);
  
  DEBUG_SECTION_BEGIN(_draw_call);
  gl.BindVertexArray(renderer->vao);
  gl.DrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, instance_count);
  DEBUG_SECTION_END(_draw_call);
  
  DEBUG_FUNCTION_END();
}




void render_group_init(Arena *arena, State *state, Render_Group *group,
                       i32 item_capacity, v2 screen_size) {
  DEBUG_FUNCTION_BEGIN();
  
  group->items = arena_push_array(arena, Render_Item, item_capacity);
  group->item_count = 0;
  group->screen_size = screen_size;
  group->state.matrix = mat4x4_identity();
  group->state.color = V4(1, 1, 1, 1);
  group->item_capacity = item_capacity;
  group->state_stack_count = 0;
  group->debug_atlas = &state->debug_atlas;
  
  DEBUG_FUNCTION_END();
}

void render_group_output(Arena *arena, Render_Group *group, Quad_Renderer *renderer) {
  DEBUG_FUNCTION_BEGIN();
  
  assert(group->state_stack_count == 0);
  
  DEBUG_SECTION_BEGIN(_push_instances);
  if (group->item_count != 0) {
    Quad_Instance *instances = arena_push_array(arena, Quad_Instance, group->item_count);
    i32 instance_count = 0;
    
    Texture_Atlas *atlas = 0;
    
    for (i32 item_index = 0; item_index < group->item_count; item_index++) {
      Render_Item *item = group->items + item_index;
      switch (item->type) {
        case Render_Type_Sprite: {
          Sprite sprite = item->Sprite.sprite;
          
          // TODO(lvl5): remove duplicate code
          if (atlas && atlas != sprite.atlas) {
            v2 screen_size_v2 = group->screen_size;
            v2 screen_size_in_meters = v2_div_s(screen_size_v2, PIXELS_PER_METER);
            v2 gl_units_per_meter = v2_mul_s(v2_invert(screen_size_in_meters), 2);
            
            Transform view_transform;
            view_transform.p = V3(0, 0, 0);
            view_transform.angle = 0;
            view_transform.scale = v2_to_v3(gl_units_per_meter, 1.0f);
            
            mat4x4 view_matrix = transform_apply(mat4x4_identity(), 
                                                 view_transform);
            quad_renderer_draw(renderer, &atlas->bmp, view_matrix,
                               instances, instance_count);
            instance_count = 0;
          }
          
          
          mat4x4 model_m = item->state.matrix;
          rect2 tex_rect = sprite_get_rect(sprite);
          
          Quad_Instance inst = {0};
          inst.model = mat4x4_transpose(model_m);
          inst.tex_offset = tex_rect.min;
          inst.tex_scale = rect2_get_size(tex_rect);
          inst.color = item->state.color;
          
          instances[instance_count++] = inst;
          
          atlas = sprite.atlas;
        } break;
        
        case Render_Type_Rect: {
          
        } break;
        
        default: assert(false);
      }
    }
    DEBUG_SECTION_END(_push_instances);
    
    v2 screen_size_v2 = group->screen_size;
    v2 screen_size_in_meters = v2_div_s(screen_size_v2, PIXELS_PER_METER);
    v2 gl_units_per_meter = v2_mul_s(v2_invert(screen_size_in_meters), 2);
    
    Transform view_transform;
    view_transform.p = V3(0, 0, 0);
    view_transform.angle = 0;
    view_transform.scale = v2_to_v3(gl_units_per_meter, 1.0f);
    
    mat4x4 view_matrix = transform_apply(mat4x4_identity(), view_transform);
    quad_renderer_draw(renderer, &atlas->bmp, view_matrix, 
                       instances, instance_count);
  }
  
  DEBUG_FUNCTION_END();
}
