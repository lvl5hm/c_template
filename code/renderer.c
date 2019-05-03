
#include "renderer.h"
#define PIXELS_PER_METER 96


mat4x4 transform_apply(mat4x4 matrix, Transform t) {
  mat4x4 result = matrix;
  result = mat4x4_translate(result, t.p);
  result = mat4x4_rotate(result, t.angle);
  result = mat4x4_scale(result, t.scale);
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
  render_save(group);
  render_transform(group, t);
  render_translate(group, v2_to_v3(v2_mul_s(sprite.origin, -1), 0));
  
  Render_Sprite *item = push_render_item(group, Sprite);
  item->sprite = sprite;
  render_restore(group);
}

void push_rect(Render_Group *group, rect2 rect, v4 color) {
  Transform t;
  t.angle = 0;
  t.p = v2_to_v3(rect2_get_center(rect), 0);
  t.scale = v2_to_v3(rect2_get_size(rect), 0);
  
  Sprite spr;
  spr.atlas = group->debug_atlas;
  spr.index = 0;
  spr.origin = V2(0.5f, 0.5f);
  
  render_save(group);
  render_color(group, color);
  push_sprite(group, spr, t);
  render_restore(group);
}

void push_text(Render_Group *group, Font font, String text, Transform t) {
  render_save(group);
  render_transform(group, t);
  
  for (u32 char_index = 0; char_index < text.count; char_index++) {
    char ch = text.data[char_index];
    if (ch == ' ') {
      render_translate(group, V3(0.14f, 0, 0));
      continue;
    }
    
    assert(ch >= font.first_codepoint_index && 
           ch < font.first_codepoint_index + font.codepoint_count);
    
    i32 font_index = ch - font.first_codepoint_index;
    Codepoint_Metrics metrics = font.metrics[font_index];
    
    Sprite spr;
    spr.index = font_index;
    spr.origin = metrics.origin;
    spr.atlas = &font.atlas;
    
    rect2 rect = sprite_get_rect(spr);
    v2 size_pixels = v2_hadamard(rect2_get_size(rect), V2((f32)spr.atlas->bmp.width, (f32)spr.atlas->bmp.height));
    
    v2 size_meters = v2_div_s(size_pixels, PIXELS_PER_METER);
    
    render_save(group);
    Transform letter_t = transform_default();
    letter_t.scale = v2_to_v3(size_meters, 0);
    push_sprite(group, spr, letter_t);
    
    render_restore(group);
    
    render_translate(group, V3(size_meters.x, 0, 0));
  }
  
  render_restore(group);
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
  gl.BufferData(GL_ARRAY_BUFFER, array_count(vertices)*sizeof(Quad_Vertex), vertices, GL_STATIC_DRAW);
  
  
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

void quad_renderer_draw(Quad_Renderer *renderer, Bitmap *bmp, mat4x4 model_mat, Quad_Instance *instances, u32 instance_count) {
  gl.BindBuffer(GL_ARRAY_BUFFER, renderer->instance_vbo);
  gl.BufferData(GL_ARRAY_BUFFER, instance_count*sizeof(Quad_Instance),
                instances, GL_DYNAMIC_DRAW);
  
  gl.UseProgram(renderer->shader);
  gl_set_uniform_mat4x4(gl, renderer->shader, "u_view", &model_mat, 1);
  
  gl.TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, bmp->width, bmp->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, bmp->data);
  
  gl.BindVertexArray(renderer->vao);
  gl.DrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, instance_count);
}




void render_group_init(Arena *arena, State *state, Render_Group *group, i32 item_capacity, v2 screen_size) {
  group->items = arena_push_array(arena, Render_Item, item_capacity);
  group->item_count = 0;
  group->screen_size = screen_size;
  group->state.matrix = mat4x4_identity();
  group->state.color = V4(1, 1, 1, 1);
  group->item_capacity = item_capacity;
  group->state_stack_count = 0;
  group->debug_atlas = &state->debug_atlas;
}

void render_group_output(State *state, Render_Group *group, Quad_Renderer *renderer) {
  assert(group->state_stack_count == 0);
  
  if (group->item_count != 0) {
    Quad_Instance *instances = sb_init(&state->temp, Quad_Instance, group->item_count, false);
    
    Texture_Atlas *atlas = 0;
    
    for (i32 item_index = 0; item_index < group->item_count; item_index++) {
      Render_Item *item = group->items + item_index;
      switch (item->type) {
        case Render_Type_Sprite: {
          Sprite sprite = item->Sprite.sprite;
          
          if (atlas && atlas != sprite.atlas) {
            v2 screen_size_v2 = group->screen_size;
            v2 screen_size_in_meters = v2_div_s(screen_size_v2, PIXELS_PER_METER);
            v2 gl_units_per_meter = v2_mul_s(v2_invert(screen_size_in_meters), 2);
            
            Transform view_transform;
            view_transform.p = V3(0, 0, 0);
            view_transform.angle = 0;
            view_transform.scale = v2_to_v3(gl_units_per_meter, 1.0f);
            
            mat4x4 view_matrix = transform_apply(mat4x4_identity(), view_transform);
            
            quad_renderer_draw(renderer, &atlas->bmp, view_matrix, instances, sb_count(instances));
            
            sb_count(instances) = 0;
          }
          
          
          mat4x4 model_m = item->state.matrix;
          rect2 tex_rect = sprite_get_rect(sprite);
          
          Quad_Instance inst = {0};
          inst.model = mat4x4_transpose(model_m);
          inst.tex_offset = tex_rect.min;
          inst.tex_scale = rect2_get_size(tex_rect);
          inst.color = item->state.color;
          
          sb_push(instances, inst);
          
          
          atlas = sprite.atlas;
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
    
    quad_renderer_draw(renderer, &atlas->bmp, view_matrix, instances, sb_count(instances));
  }
}
