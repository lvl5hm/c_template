#include "renderer.h"
#include "debug.h"
#define PIXELS_PER_METER 32
#include "font.c"

mat4 transform_apply(mat4 matrix, Transform t) {
  mat4 result = matrix;
  result = mat4_mul_mat4(result, mat4_translated(t.p));
  result = mat4_mul_mat4(result, mat4_rotated(t.angle));
  result = mat4_mul_mat4(result, mat4_scaled(t.scale));
  
  return result;
}

mat4 transform_apply_inverse(mat4 matrix, Transform t) {
  mat4 result = matrix;
  result = mat4_mul_mat4(result, mat4_scaled(v3_invert(t.scale)));
  result = mat4_mul_mat4(result, mat4_rotated(-t.angle));
  result = mat4_mul_mat4(result, mat4_translated(v3_mul(t.p, -1)));
  
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
  group->state.matrix = mat4_mul_mat4(group->state.matrix, mat4_translated(p));
}

void render_scale(Render_Group *group, v3 scale) {
  group->state.matrix = mat4_mul_mat4(group->state.matrix,mat4_scaled(scale));
}

void render_rotate(Render_Group *group, f32 angle) {
  group->state.matrix = mat4_mul_mat4(group->state.matrix, mat4_rotated(angle));
}

void render_transform(Render_Group *group, Transform t) {
  group->state.matrix = transform_apply(group->state.matrix, t);
}

void render_transform_inverse(Render_Group *group, Transform t) {
  group->state.matrix = transform_apply_inverse(group->state.matrix, t);
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
  group->expected_quad_count++;
  return item;
}

void push_sprite(Render_Group *group, Sprite sprite, Transform t) {
  DEBUG_FUNCTION_BEGIN();
  if (group->item_count == 20) {
    assert(sprite.atlas->rects);
  }
  
  render_save(group);
  render_transform(group, t);
  render_translate(group, v2_to_v3(v2_mul(sprite.origin, -1), 0));
  
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


void push_circle_outline(Render_Group *group, v2 center, f32 radius, f32 thick) {
  i32 segment_count = 20;
  f32 angle_per_segment = 2*PI/segment_count;
  
  for (i32 segment_index = 0; segment_index < segment_count; segment_index++) {
    f32 angle_start = segment_index*angle_per_segment;
    f32 angle_end = (segment_index+1)*angle_per_segment;
    
    v2 r = v2_mul(v2_right(), radius);
    v2 start = v2_add(center, v2_rotate(r, angle_start));
    v2 end = v2_add(center, v2_rotate(r, angle_end));
    
    push_line(group, start, end, thick);
  }
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

f32 text_get_size(Render_Group *group, String text) {
  f32 result = 0;
  for (u32 char_index = 0; char_index < text.count; char_index++) {
    char ch = text.data[char_index];
    Sprite spr = font_get_sprite(group->state.font, ch);
    rect2 rect = sprite_get_rect(spr);
    v2 size_pixels = v2_hadamard(rect2_get_size(rect), 
                                 V2((f32)spr.atlas->bmp.width,
                                    (f32)spr.atlas->bmp.height));
    v2 size_meters = v2_div(size_pixels, PIXELS_PER_METER);
    result += size_meters.x;
  }
  return result;
}

void push_text(Render_Group *group, String text) {
  DEBUG_FUNCTION_BEGIN();
  
  Render_Text *entry = push_render_item(group, Text);
  entry->text = text;
  group->expected_quad_count += text.count - 1; // 1 is automatically pushed by push_render_item()
  
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
                        mat4 view_mat, mat4 projection_mat, Quad_Instance *instances, u32 instance_count) {
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
  gl_set_uniform_mat4(gl, renderer->shader, "u_projection", &projection_mat, 1);
  gl_set_uniform_mat4(gl, renderer->shader, "u_view", &view_mat, 1);
  
  DEBUG_SECTION_BEGIN(_draw_call);
  gl.BindVertexArray(renderer->vao);
  gl.DrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, instance_count);
  DEBUG_SECTION_END(_draw_call);
  
  DEBUG_FUNCTION_END();
}



void render_group_init(Arena *arena, State *state, Render_Group *group,
                       i32 item_capacity, Camera *camera, v2 screen_size) {
  DEBUG_FUNCTION_BEGIN();
  
  Render_Group zero_group = {0};
  *group = zero_group;
  
  group->items = arena_push_array(arena, Render_Item, item_capacity);
  group->item_count = 0;
  group->screen_size = screen_size;
  group->camera = camera;
  group->state.matrix = mat4_identity();
  group->state.color = V4(1, 1, 1, 1);
  group->item_capacity = item_capacity;
  group->state_stack_count = 0;
  group->debug_atlas = &state->debug_atlas;
  
  DEBUG_FUNCTION_END();
}


mat4 camera_get_view_matrix(Camera *camera) {
  mat4 result = mat4_identity();
  result = mat4_mul_mat4(result, mat4_rotated(-camera->angle));
  result = mat4_mul_mat4(result, mat4_translated(v3_mul(camera->p, -1)));
  
  return result;
}

mat4 camera_get_projection_matrix(Camera *camera, v2 screen_size) {
  f32 w = camera->width;
  f32 h = camera->height;
  mat4 result = mat4_orthographic(-w*0.5f, w*0.5f,
                                  -h*0.5f, h*0.5f,
                                  camera->near, camera->far);
  
  return result;
}

void render_group_output(Arena *arena, Render_Group *group, Quad_Renderer *renderer) {
  DEBUG_FUNCTION_BEGIN();
  
  assert(group->state_stack_count == 0);
  
  if (group->item_count == 0) {
    return;
  }
  Quad_Instance *instances = sb_init(arena, Quad_Instance, group->expected_quad_count, true);
  
  Texture_Atlas *atlas = 0;
  
  DEBUG_SECTION_BEGIN(_push_instances);
  for (i32 item_index = 0; item_index < group->item_count; item_index++) {
    Render_Item *item = group->items + item_index;
    switch (item->type) {
      case Render_Type_Sprite: {
        Sprite sprite = item->Sprite.sprite;
        
        // TODO(lvl5): remove duplicate code
        if (atlas && atlas != sprite.atlas) {
          mat4 view_matrix = camera_get_view_matrix(group->camera);
          mat4 projection_matrix = camera_get_projection_matrix(group->camera, group->screen_size);
          quad_renderer_draw(renderer, &atlas->bmp, view_matrix,
                             projection_matrix, instances, sb_count(instances));
          sb_count(instances) = 0;
        }
        
        
        mat4 model_m = item->state.matrix;
        rect2 tex_rect = sprite_get_rect(sprite);
        
        Quad_Instance inst = {0};
        inst.model = mat4_transpose(model_m);
        inst.tex_offset = tex_rect.min;
        inst.tex_scale = rect2_get_size(tex_rect);
        inst.color = item->state.color;
        
        sb_push(instances, inst);
        
        atlas = sprite.atlas;
      } break;
      
      case Render_Type_Rect: {
        
      } break;
      
      case Render_Type_Text: {
        if (atlas && sb_count(instances)) {
          mat4 view_matrix = camera_get_view_matrix(group->camera);
          mat4 projection_matrix = camera_get_projection_matrix(group->camera, group->screen_size);
          quad_renderer_draw(renderer, &atlas->bmp, view_matrix,
                             projection_matrix, instances, sb_count(instances));
          sb_count(instances) = 0;
        }
        
        Font *font = item->state.font;
        String text = item->Text.text;
        mat4 model_m = item->state.matrix;
        v3 scale = v3_invert(V3(PIXELS_PER_METER, PIXELS_PER_METER, 1.0f));
        model_m = mat4_mul_mat4(model_m, mat4_scaled(scale));
        
        for (u32 char_index = 0; char_index < text.count; char_index++) {
          char ch = text.data[char_index];
          
          assert(ch >= font->first_codepoint_index && 
                 ch < font->first_codepoint_index + font->codepoint_count);
          
          Codepoint_Metrics metrics = font_get_metrics(font, ch);
          Sprite spr = font_get_sprite(font, ch);
          rect2 tex_rect = sprite_get_rect(spr);
          
          v2 size_pixels = v2_hadamard(rect2_get_size(tex_rect), 
                                       V2((f32)spr.atlas->bmp.width,
                                          (f32)spr.atlas->bmp.height));
          
          mat4 self_m = model_m;
          
          self_m.e00 *= size_pixels.x;
          self_m.e11 *= size_pixels.y;
          
          self_m.e30 -= spr.origin.x/PIXELS_PER_METER;
          self_m.e31 -= spr.origin.y/PIXELS_PER_METER;
          
          Quad_Instance inst = {0};
          inst.model = mat4_transpose(self_m);
          inst.tex_offset = tex_rect.min;
          inst.tex_scale = rect2_get_size(tex_rect);
          inst.color = item->state.color;
          
          sb_push(instances, inst);
          
          f32 advance_meters = metrics.advance/PIXELS_PER_METER;
          model_m.e30 += advance_meters;
        }
        atlas = &font->atlas;
      } break;
      
      default: assert(false);
    }
  }
  DEBUG_SECTION_END(_push_instances);
  
  mat4 view_matrix = camera_get_view_matrix(group->camera);
  mat4 projection_matrix = camera_get_projection_matrix(group->camera, group->screen_size);
  
  quad_renderer_draw(renderer, &atlas->bmp, view_matrix, projection_matrix, 
                     instances, sb_count(instances));
  
  DEBUG_FUNCTION_END();
}
