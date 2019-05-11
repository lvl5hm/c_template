#include <stdio.h>
#include "lvl5_types.h"
#include "debug.h"
#include "renderer.c"
#include "platform.h"

Platform platform;


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

Bitmap load_bmp(String file_name) {
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
#define PADDING_Y 4
  
  for (i32 bitmap_index = 0; bitmap_index < count; bitmap_index++) {
    Bitmap *bmp = bitmaps + bitmap_index;
    if (bmp->width > max_width) {
      max_width = bmp->width;
    }
    total_height += bmp->height + PADDING_Y;
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

Texture_Atlas make_texture_atlas_from_folder(Arena *temp, Arena *perm, String folder) {
  File_List dir = platform.get_files_in_folder(folder);
  u64 loaded_bitmaps_memory = arena_get_mark(temp);
  
  Bitmap *bitmaps = arena_push_array(temp, Bitmap, dir.count);
  
  for (i32 file_index = 0; file_index < dir.count; file_index++) {
    String file_name = dir.files[file_index];
    String full_name = concat(temp, folder, concat(temp, const_string("\\"), file_name));
    Bitmap sprite_bmp = load_bmp(full_name);
    bitmaps[file_index] = sprite_bmp;
  }
  
  Texture_Atlas result = make_texture_atlas_from_bitmaps(perm, bitmaps, dir.count); 
  arena_set_mark(temp, loaded_bitmaps_memory);
  
  return result;
}

#define FONT_HEIGHT 16

Font load_ttf(Arena *temp, Arena *perm, String file_name) {
  Font result;
  
  stbtt_fontinfo font;
  Buffer font_file = platform.read_entire_file(file_name);
  const unsigned char *font_buffer = (const unsigned char *)font_file.data;
  stbtt_InitFont(&font, font_buffer, stbtt_GetFontOffsetForIndex(font_buffer, 0));
  
  u64 loaded_bitmaps_memory = arena_get_mark(temp);
  result.first_codepoint_index = ' ';
  i32 last_codepoint_index = '~';
  result.codepoint_count = last_codepoint_index - result.first_codepoint_index;
  result.metrics = sb_init(perm, Codepoint_Metrics,
                           result.codepoint_count, false);
  
  
  Bitmap *bitmaps = sb_init(temp, Bitmap, result.codepoint_count, false);
  for (char ch = result.first_codepoint_index; ch < last_codepoint_index; ch++) {
    i32 width;
    i32 height;
    f32 scale = stbtt_ScaleForPixelHeight(&font, FONT_HEIGHT);
    byte *single_bitmap = stbtt_GetCodepointBitmap(&font, 0, scale, ch, &width, &height, 0, 0);
    Bitmap bitmap = make_empty_bitmap(temp, width, height);
    
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
    
    metrics.kerning = arena_push_array(perm, f32, result.codepoint_count);
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
  
  arena_set_mark(temp, loaded_bitmaps_memory);
  
  Texture_Atlas atlas = make_texture_atlas_from_bitmaps(perm, bitmaps, sb_count(bitmaps));
  result.atlas = atlas;
  
  return result;
}


typedef enum {
  Key_Code_BACK = 0x08,
  Key_Code_TAB = 0x09,
  Key_Code_ENTER = 0x0D,
  Key_Code_SHIFT = 0x10,
  Key_Code_CTRL = 0x11,
  Key_Code_ESCAPE = 0x1B,
  Key_Code_SPACE = 0x20,
  Key_Code_DELETE = 0x2E,
  Key_Code_LEFT = 0x25,
  Key_Code_UP = 0x26,
  Key_Code_RIGHT = 0x27,
  Key_Code_DOWN = 0x28,
} Key_Code;


v2 get_mouse_p_meters(game_Input *input, v2 screen_size) {
  v2 screen_size_meters = v2_div_s(screen_size, PIXELS_PER_METER);
  v2 half_screen_size_meters = v2_div_s(screen_size_meters, 2);
  v2 mouse_p_meters = v2_sub(v2_div_s(input->mouse.p, PIXELS_PER_METER),
                             half_screen_size_meters);
  return mouse_p_meters;
}


b32 point_in_rect(v2 point, rect2 rect) {
  b32 result = point.x > rect.min.x &&
    point.x < rect.max.x &&
    point.y > rect.min.y &&
    point.y < rect.max.y;
  return result;
}

typedef enum {
  Debug_Var_Name_PERF,
} Debug_Var_Name;

void debug_init(Arena *temp, Arena *perm) {
  arena_init_subarena(temp, &debug_state.arena, megabytes(32));
  arena_init_subarena(&debug_state.arena, &debug_state.gui.arena, kilobytes(32));
  
  Debug_GUI *gui = &debug_state.gui;
  gui->font = load_ttf(temp, perm, const_string("Inconsolata-Regular.ttf"));
  
  // NOTE(lvl5): variables
  debug_state.vars[Debug_Var_Name_PERF] = (Debug_Var){const_string("perf"), 0};
  
  // NOTE(lvl5): terminal
  Debug_Terminal *term = &gui->terminal;
  term->input_capacity = 512;
  term->input_data = arena_push_array(&debug_state.arena, char, 
                                      term->input_capacity);
  term->input_count = 0;
  term->cursor = 0;
}

void debug_draw_terminal(Debug_Terminal *term, Render_Group *group, game_Input *input,
                         v2 screen_meters) {
#define LETTER_SIZE 0.0795f
  if (input->keys[Key_Code_BACK].pressed) {
    if (term->input_count > 0) {
      if (term->cursor > 0) {
        for (i32 i = term->cursor; i < term->input_count; i++) {
          term->input_data[i-1] = term->input_data[i];
        }
        term->cursor--;
        term->input_count--;
      }
    }
  } else if (input->char_code) {
    assert(term->input_count < term->input_capacity);
    for (i32 i = term->input_count; i >= term->cursor; i--) {
      term->input_data[i+1] = term->input_data[i];
    }
    term->input_data[term->cursor++] = input->char_code;
    term->input_count++;
  }
  
  if (input->keys[Key_Code_RIGHT].pressed) {
    term->cursor++;
  } else if (input->keys[Key_Code_LEFT].pressed) {
    term->cursor--;
  }
  
  if (input->keys[Key_Code_ENTER].went_down) {
    String str = make_string(term->input_data, term->input_count);
    if (starts_with(str, const_string("set"))) {
      i32 space_index = find_index(str, const_string(" "), 4);
      String var_name = substring(str, 4, space_index);
      String value_str = substring(str, space_index+1, str.count-1);
      
      for (i32 var_index = 0; var_index < array_count(debug_state.vars); var_index++) {
        Debug_Var *var = debug_state.vars + var_index;
        if (string_compare(var_name, var->name)) {
          i32 value = string_to_i32(value_str);
          var->value = value;
        }
      }
    }
    
    term->input_count = 0;
    term->cursor = 0;
  }
  String terminal_string = make_string(term->input_data, term->input_count);
  
  render_save(group);
  render_translate(group, V3(-0.5f*screen_meters.x, 0, 0));
  push_rect(group, rect2_center_size(V2(term->cursor*LETTER_SIZE, 0.05f),
                                     V2(0.02f, 0.1f)), V4(1, 1, 1, 1));
  
  push_text(group, &debug_state.gui.font, terminal_string, transform_default());
  
  
  render_restore(group);
}

void debug_draw_gui(State *state, v2 screen_size, game_Input *input) {
  Debug_GUI *gui = &debug_state.gui;
  
  u64 debug_render_memory = arena_get_mark(&debug_state.arena);
  Render_Group _debug_render_group;
  Render_Group *group = &_debug_render_group;
  
  render_group_init(&debug_state.arena, state, group, 10000, screen_size); 
  
  v2 screen_meters = v2_div_s(screen_size, PIXELS_PER_METER);
  
  debug_draw_terminal(&gui->terminal, group, input, screen_meters);
  
  if (debug_state.vars[Debug_Var_Name_PERF].value != 0) {
    
    f32 total_width = 3.0f;
    f32 rect_width = total_width/array_count(debug_state.frames);
    
#define MAX_CYCLES 40891803
    
    render_save(group);
    for (i32 i = 0; i < array_count(debug_state.frames); i++) {
      i32 frame_index = (debug_state.frame_index + i) % array_count(debug_state.frames);
      
      Debug_Frame *frame = debug_state.frames + frame_index;
      if (frame->event_count && frame_index != debug_state.frame_index) {
        u64 begin_cycles = frame->events[0].cycles;
        u64 end_cycles = frame->events[frame->event_count-1].cycles;
        u64 duration = end_cycles - begin_cycles;
        
        f32 rect_height = (f32)duration/MAX_CYCLES*10;
        
        rect2 rect = rect2_min_size(V2(-screen_meters.x*0.5f, 
                                       screen_meters.y*0.5f-1), 
                                    V2(rect_width, rect_height));
        v4 color = V4(0, 1, 0, 1);
        rect2 mouse_rect = rect;
        mouse_rect.max.y = mouse_rect.min.y+1;
        mouse_rect = rect2_apply_matrix(mouse_rect, group->state.matrix);
        v2 mouse_p_meters = get_mouse_p_meters(input, screen_size);
        if (point_in_rect(mouse_p_meters, mouse_rect)) {
          color = V4(1, 0, 0, 1);
          debug_state.pause = true;
          
          if (input->mouse.left.went_down) {
            if (gui->selected_frame_index != -1) {
              arena_set_mark(&gui->arena, gui->node_memory);
            }
            gui->selected_frame_index = frame_index;
            gui->node_memory = arena_get_mark(&gui->arena);
            
            Debug_Frame *frame = debug_state.frames + gui->selected_frame_index;
            gui->nodes = arena_push_array(&gui->arena, 
                                          Debug_View_Node, frame->timer_count+1);
            i32 node_index = 0;
            gui->node_count = 1;
            i32 depth = 0;
            
            Debug_View_Node *node = gui->nodes + 0;
            node->duration = MAX_CYCLES;
            
            for (u32 event_index = 0;
                 event_index < frame->event_count; 
                 event_index++) {
              Debug_Event *event = frame->events + event_index;
              
              if (event->type == Debug_Type_BEGIN_TIMER) {
                i32 new_index =  gui->node_count++;
                Debug_View_Node *node = gui->nodes + new_index;
                node->type = Debug_View_Type_NONE;
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
                
                Debug_View_Node *parent = gui->nodes + node->parent_index;
                parent->self_duration -= node->duration;
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
    render_restore(group);
    
    if (debug_state.gui.selected_frame_index >= 0) {
      Debug_Frame *frame = debug_state.frames +
        debug_state.gui.selected_frame_index;
      
      v2 half_screen_size_meters = v2_div_s(screen_size, PIXELS_PER_METER*2);
      
      render_save(group);
      v3 text_p = V3(-half_screen_size_meters.x + 0.1f, 
                     half_screen_size_meters.y - 1.2f, 0);
      render_translate(group, text_p);
      
      
      i32 node_index = 1;
      
      while (node_index < gui->node_count) {
        Debug_View_Node *node = gui->nodes + node_index;
        Debug_View_Node *parent = gui->nodes + node->parent_index;
        u64 parent_duration = parent->duration;
        
        f32 percent = (f32)node->duration/(f32)parent_duration*100;
        f32 self_percent = (f32)node->self_duration/(f32)parent_duration*100;
        
        Debug_Event *event = frame->events + node->event_index;
        
        char buffer[256];
        sprintf_s(buffer, array_count(buffer), "%s: %.2f%% (self %.2f%%)  (%lld)", 
                  event->name, percent, self_percent, node->duration);
        
        String str = from_c_string(buffer);
        rect2 rect = rect2_apply_matrix(rect2_min_size(V2(0, 0), V2(3, 0.2f)), 
                                        group->state.matrix);
        
        v2 mouse_p_meters = get_mouse_p_meters(input, group->screen_size);
        i32 child_indices_count = node->one_past_last_child_index - node->first_child_index;
        
        render_save(group);
        
        if (child_indices_count > 0 && 
            point_in_rect(mouse_p_meters, rect)) {
          render_color(group, V4(0, 1, 0, 1));
          if (input->mouse.left.went_down) {
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
    }
  }
  
  
  render_group_output(&debug_state.arena, group, &state->renderer);
  arena_set_mark(&debug_state.arena, debug_render_memory);
}




