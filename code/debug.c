#include <stdio.h>
#include "lvl5_types.h"
#include "debug.h"
#include "renderer.c"
#include "platform.h"


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
  Key_Code_TILDE = 0xC0,
} Key_Code;


v2 get_mouse_p_meters(Input *input, v2 screen_size) {
  v2 screen_size_meters = v2_div(screen_size, PIXELS_PER_METER);
  v2 half_screen_size_meters = v2_div(screen_size_meters, 2);
  v2 mouse_p_meters = v2_sub(v2_div(input->mouse.p, PIXELS_PER_METER),
                             half_screen_size_meters);
  return mouse_p_meters;
}

i32 debug_get_var_i32(Debug_Var_Name name) {
  i32 result = debug_state->vars[name].value;
  return result;
}

void debug_init(Arena *temp, byte *debug_memory) {
  arena_init(&debug_state->arena, debug_memory, megabytes(32));
  arena_init_subarena(&debug_state->arena, &debug_state->gui.arena, megabytes(4));
  
  Debug_GUI *gui = &debug_state->gui;
  gui->font = load_ttf(temp, &debug_state->arena, 
                       const_string("fonts/Inconsolata-Regular.ttf"));
  
  // NOTE(lvl5): variables
  debug_state->vars[Debug_Var_Name_PERF] = (Debug_Var){const_string("perf"), 0};
  debug_state->vars[Debug_Var_Name_COLLIDERS] = (Debug_Var){const_string("colliders"), 1};
  debug_state->vars[Debug_Var_Name_MEMORY] = (Debug_Var){const_string("memory"), 0};
  
  
  // NOTE(lvl5): terminal
  Debug_Terminal *term = &gui->terminal;
  arena_init_subarena(&debug_state->arena, &term->arena, megabytes(2));
  term->lines = sb_init(&term->arena, String, 128, true);
  term->history = sb_init(&term->arena, String, 64, true);
  term->input_capacity = 512;
  term->input_data = arena_push_array(&debug_state->arena, char, 
                                      term->input_capacity);
  term->input_data[0] = '>';
  term->input_count = 1;
  term->cursor = 1;
}

void debug_add_arena(Arena *arena, String name) {
  Debug_Arena *da = debug_state->gui.arenas + debug_state->gui.arena_count++;
  da->name = name;
  da->arena = arena;
}

#define LINE_INTERVAL 20
#define DEBUG_BG_COLOR V4(0, 0, 0, 0.7f)


void debug_terminal_history_save(Debug_Terminal *term) {
  String entry = alloc_string(&term->arena, term->input_data + 1, term->input_count - 2);
  sb_push(term->history, entry);
}

void debug_terminal_history_restore(Debug_Terminal *term) {
  String hist = term->history[term->history_index];
  copy_memory_slow(term->input_data + 1, hist.data, hist.count);
  term->input_count = hist.count + 1;
  term->cursor = hist.count + 1;
}

void debug_log_string(String str) {
  Debug_Terminal *term = &debug_state->gui.terminal;
  String line = alloc_string(&term->arena, str.data, str.count);
  
  if (sb_count(term->lines) == sb_capacity(term->lines)) {
    for (u32 i = 0; i < sb_count(term->lines)-1; i++) {
      term->lines[i] = term->lines[i+1];
    }
    term->lines[sb_count(term->lines)-1] = line;
  } else {
    sb_push(term->lines, line);
  }
}

void debug_log(char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  
  char *buffer = arena_push_array(scratch, char, 256);
  vsprintf_s(buffer, 256, fmt, args);
  String fmt_string = make_string(buffer, c_string_length(buffer));
  
  va_end(args);
  
  debug_log_string(fmt_string);
}

void debug_draw_terminal(Debug_Terminal *term, Render_Group *group, Input *input,
                         v2 screen_size) {
  DEBUG_FUNCTION_BEGIN();
  
  if (input->keys[Key_Code_TILDE].went_down) {
    if (!term->is_shown) {
      term->is_shown = true;
      term->is_active = true;
    } else {
      term->is_shown = false;
      term->is_active = false;
    }
    return;
  }
  
  if (!term->is_shown) return;
  
  if (term->is_active) {
    if (input->keys[Key_Code_BACK].pressed) {
      if (term->input_count > 1) {
        if (term->cursor > 1) {
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
      term->cursor = clamp_i32(term->cursor + 1, 1, term->input_count);;
    } else if (input->keys[Key_Code_LEFT].pressed) {
      term->cursor = clamp_i32(term->cursor - 1, 1, term->input_count);;
    } else if (input->keys[Key_Code_UP].pressed) {
      if (term->history_index == -1) {
        if (term->input_count > 1) {
          debug_terminal_history_save(term);
          term->history_index = sb_count(term->history) - 2;
        } else {
          term->history_index = sb_count(term->history) - 1;
        }
      } else {
        term->history_index = clamp_i32(term->history_index - 1, 0, sb_count(term->history));
      }
      
      debug_terminal_history_restore(term);
    } else if (input->keys[Key_Code_DOWN].pressed) {
      term->history_index = clamp_i32(term->history_index + 1, 0, sb_count(term->history));
      debug_terminal_history_restore(term);
    }
    
    
    if (input->keys[Key_Code_ENTER].pressed) {
      debug_terminal_history_save(term);
      term->history_index = -1;
      
      String str = make_string(term->input_data+1, term->input_count-2);
      debug_log_string(str);
      
      term->input_count = 1;
      term->cursor = 1;
      
      if (starts_with(str, const_string("set"))) {
        i32 space_index = find_index(str, const_string(" "), 4);
        String var_name = substring(str, 4, space_index);
        String value_str = substring(str, space_index+1, str.count);
        
        for (i32 var_index = 0; var_index < array_count(debug_state->vars); var_index++) {
          Debug_Var *var = debug_state->vars + var_index;
          if (string_compare(var_name, var->name)) {
            i32 value = string_to_i32(value_str);
            var->value = value;
          }
        }
      }
    }
  }
  
  Codepoint_Metrics metrics = font_get_metrics(group->state.font, 'a');
  f32 letter_size = metrics.advance; // assumed to be monospace
  
  String terminal_string = make_string(term->input_data, term->input_count);
  
  render_save(group);
  render_translate(group, V3(-0.5f*screen_size.x,
                             0.5f*screen_size.y,
                             0));
  
  render_color(group, DEBUG_BG_COLOR);
  i32 shown_line_count = 8;
  f32 terminal_height = LINE_INTERVAL*(f32)shown_line_count;
  
  rect2 terminal_rect = rect2_min_size(V2(0, -terminal_height), V2(screen_size.x, terminal_height));
  rect2 mouse_rect = rect2_apply_matrix(terminal_rect, group->state.matrix);
  v2 mouse_p = v2_sub(input->mouse.p, v2_mul(screen_size, 0.5f));
  if (input->mouse.left.went_down) {
    if (point_in_rect(mouse_p, mouse_rect)) {
      term->is_active = true;
    } else {
      term->is_active = false;
    }
  }
  
  push_rect(group, terminal_rect);
  
  
  render_translate(group, V3(0, -terminal_height + 5, 0));
  
  if (term->is_active) {
    render_color(group, V4(0, 1, 0, 0.7f));
  } else {
    render_color(group, V4(0, 0, 0, 0));
  }
  push_rect(group, rect2_min_size(V2(term->cursor*letter_size, -3),
                                  V2(letter_size, 15)));
  
  render_color(group, COLOR_WHITE);
  push_text(group, terminal_string);
  
  
  u32 line_count = sb_count(term->lines);
  for (u32 shown_line_index = 0;
       shown_line_index < min_u32(line_count, shown_line_count);
       shown_line_index++) {
    u32 line_index = line_count - shown_line_index - 1;
    render_translate(group, V3(0, LINE_INTERVAL, 0));
    push_text(group, term->lines[line_index]);
  }
  
  render_restore(group);
  DEBUG_FUNCTION_END();
}


void debug_draw_gui(State *state, v2 screen_size, Input *input, f32 dt) {
  DEBUG_FUNCTION_BEGIN();
  
  Debug_GUI *gui = &debug_state->gui;
  
  u64 debug_render_memory = arena_get_mark(&debug_state->arena);
  Render_Group _debug_render_group;
  Render_Group *group = &_debug_render_group;
  
  Camera gui_camera = {
    .far = 10.0f,
    .near = 0.0f,
    .width = screen_size.x,
    .height = screen_size.y,
    .p = v3_zero(),
    .angle = 0,
  };
  render_group_init(&debug_state->arena, state, group, 10000, &gui_camera); 
  render_font(group, &gui->font);
  
  debug_draw_terminal(&gui->terminal, group, input, screen_size);
  
  if (debug_get_var_i32(Debug_Var_Name_MEMORY) != 0) {
    render_save(group);
    render_translate(group, V3(-screen_size.x*0.5f, 
                               screen_size.y*0.5f-1.2f,
                               0));
    
    for (i32 i = 0; i < gui->arena_count; i++) {
      Debug_Arena *debug_arena = gui->arenas + i;
      char buffer[256];
      sprintf_s(buffer, array_count(buffer), "[%s]: %lld/%lld", 
                to_c_string(&debug_state->arena, debug_arena->name),
                debug_arena->arena->size, debug_arena->arena->capacity);
      String str = from_c_string(buffer);
      push_text(group, str);
      render_translate(group, V3(0, -LINE_INTERVAL, 0));
    }
    
    
    
    render_restore(group);
  }
  
  if (debug_get_var_i32(Debug_Var_Name_PERF) != 0) {
    f32 total_width = 300;
    f32 total_heigt = 50;
    render_translate(group, V3(-screen_size.x*0.5f, 
                               screen_size.y*0.5f - total_heigt,
                               0));
    render_color(group, DEBUG_BG_COLOR);
    push_rect(group, rect2_min_size(V2(0, 0), V2(total_width, total_heigt)));
    
#define MAX_CYCLES 40891803
    {
      Debug_Frame *frame = debug_state->frames + debug_state->frame_index - 1;
      u64 begin_cycles = frame->events[0].cycles;
      u64 end_cycles = frame->events[frame->event_count-1].cycles;
      u64 duration = end_cycles - begin_cycles;
      
      render_save(group);
      render_color(group, COLOR_BLACK);
      render_translate(group, V3(total_width + 10, total_heigt - 14, 0));
      char *buffer = arena_push_array(scratch, char, 256);
      sprintf_s(buffer, 256, "%.2f ms", 
                (f32)(end_cycles - begin_cycles)/(f32)MAX_CYCLES*16.6f);
      
      String str = from_c_string(buffer);
      push_text(group, str);
      render_restore(group);
    }
    
    f32 rect_width = total_width/array_count(debug_state->frames);
    
    render_save(group);
    b32 any_frame_selected = false;
    
    v2 mouse_p = v2_sub(input->mouse.p, V2(gui_camera.width*0.5f, gui_camera.height*0.5f));
    
    for (i32 i = 0; i < array_count(debug_state->frames); i++) {
      i32 frame_index = (debug_state->frame_index + i) % array_count(debug_state->frames);
      
      Debug_Frame *frame = debug_state->frames + frame_index;
      if (frame->event_count && frame_index != debug_state->frame_index) {
        u64 begin_cycles = frame->events[0].cycles;
        u64 end_cycles = frame->events[frame->event_count-1].cycles;
        u64 duration = end_cycles - begin_cycles;
        
        f32 rect_height = (f32)duration/MAX_CYCLES*total_heigt;
        
        rect2 rect = rect2_min_size(V2(0, 0), 
                                    V2(rect_width, rect_height));
        v4 color = COLOR_GREEN;
        
        rect2 mouse_rect = rect;
        mouse_rect.max.y = mouse_rect.min.y+total_heigt;
        mouse_rect = rect2_apply_matrix(mouse_rect, group->state.matrix);
        
        if (point_in_rect(mouse_p, mouse_rect)) {
          color = COLOR_RED;
          debug_state->pause = true;
          any_frame_selected = true;
          
          if (input->mouse.left.went_down) {
            if (gui->selected_frame_index != -1) {
              arena_set_mark(&gui->arena, gui->node_memory);
            }
            gui->selected_frame_index = frame_index;
            gui->node_memory = arena_get_mark(&gui->arena);
            
            Debug_Frame *frame = debug_state->frames + gui->selected_frame_index;
            i32 node_capacity = frame->timer_count+1;
            gui->nodes = arena_push_array(&gui->arena,
                                          Debug_View_Node, node_capacity);
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
                node->name = alloc_string(&gui->arena, event->name,
                                          c_string_length(event->name));
                node->first_child_index = new_index + 1;
                node->parent_index = node_index;
                node->depth = depth++;
                node->self_duration = 0;
                node->id = event->id;
                node->duration = event->cycles;
                
                node_index = new_index;
              } else if (event->type == Debug_Type_END_TIMER) {
                Debug_View_Node *node = gui->nodes + node_index;
                assert(node->id == event->id);
                node->one_past_last_child_index = gui->node_count;
                node->duration = event->cycles - node->duration;
                
                Debug_View_Node *parent = gui->nodes + node->parent_index;
                parent->self_duration -= node->duration;
                node->self_duration += node->duration;
                
                node_index = node->parent_index;
                depth--;
              }
            }
          }
        }
        
        render_color(group, color);
        push_rect(group, rect);
        render_translate(group, V3(rect_width, 0, 0));
      }
    }
    render_restore(group);
    
    if (debug_state->gui.selected_frame_index >= 0) {
      Debug_Frame *frame = debug_state->frames +
        debug_state->gui.selected_frame_index;
      
      render_save(group);
      v3 text_p = V3(0, -20, 0);
      render_translate(group, text_p);
      i32 node_index = 1;
      
      while (node_index < gui->node_count) {
        Debug_View_Node *node = gui->nodes + node_index;
        Debug_View_Node *parent = gui->nodes + node->parent_index;
        
        u64 parent_duration = parent->duration;
        
        f32 percent = (f32)node->duration/(f32)parent_duration*100;
        f32 self_percent = (f32)node->self_duration/(f32)parent_duration*100;
        
        char *name = to_c_string(&debug_state->arena, node->name);
        char *buffer = arena_push_array(scratch, char, 256);
        sprintf_s(buffer, 256, "%s: %.2f%% (self %.2f%%)  (%lld)", 
                  name, percent, self_percent, node->duration);
        
        String str = from_c_string(buffer);
        
        render_save(group);
        v3 indent_trans = V3(10.0f*node->depth, 0, 0);
        render_translate(group, indent_trans);
        
        f32 rect_width = font_get_text_width_pixels(&gui->font, str);
        rect2 on_screen_rect = rect2_min_size(V2(0, 0), V2(rect_width, LINE_INTERVAL));
        rect2 rect = rect2_apply_matrix(on_screen_rect, 
                                        group->state.matrix);
        
        render_color(group, DEBUG_BG_COLOR);
        push_rect(group, on_screen_rect);
        
        i32 child_indices_count = node->one_past_last_child_index - 
          node->first_child_index;
        
        render_color(group, COLOR_WHITE);
        
        if (point_in_rect(mouse_p, rect)) {
          any_frame_selected = true;
          if (child_indices_count > 0) {
            render_color(group, COLOR_GREEN);
            if (input->mouse.left.went_down) {
              node->type++;
              if (node->type > Debug_View_Type_FLAT) {
                node->type = Debug_View_Type_NONE;
              }
            }
          }
        }
        
        render_translate(group, V3(0, 5, 0));
        push_text(group, str);
        render_restore(group);
        
        render_translate(group, V3(0, -LINE_INTERVAL, 0));
        
        if (node->type == Debug_View_Type_FLAT) {
          // NOTE(lvl5): merge all descendant by event id
          Debug_View_Node *flat_children = sb_init(&debug_state->arena,
                                                   Debug_View_Node, child_indices_count, false);
          
          for (i32 child_index = node->first_child_index;
               child_index < node->one_past_last_child_index;
               child_index++) {
            Debug_View_Node *child = gui->nodes + child_index;
            Debug_View_Node *rec = 0;
            
            for (u32 flat_index = 0; 
                 flat_index < sb_count(flat_children);
                 flat_index++) {
              Debug_View_Node *flat_child = flat_children + flat_index;
              if (flat_child->id == child->id) {
                rec = flat_child;
              }
            }
            if (!rec) {
              Debug_View_Node new_rec = {0};
              new_rec.id = child->id;
              new_rec.name = child->name;
              sb_push(flat_children, new_rec);
              rec = flat_children + sb_count(flat_children) - 1;
            }
            
            rec->self_duration += child->self_duration;
            rec->count++;
          }
          
          {
            // NOTE(lvl5): sort the flat children by self_duration
            u32 i = 1;
            while (i < sb_count(flat_children)) {
              u32 j = i;
              while (j > 0 && 
                     (flat_children[j-1].self_duration < 
                      flat_children[j].self_duration)) {
                Debug_View_Node temp = flat_children[j];
                flat_children[j] = flat_children[j-1];
                flat_children[j-1] = temp;
                j--;
              }
              i++;
            }
          }
          
          Debug_View_Node *parent = node;
          render_translate(group, V3(0.1f*(node->depth+1), 0, 0));
          
          // NOTE(lvl5): draw flat children
          for (u32 flat_index = 0;
               flat_index < sb_count(flat_children);
               flat_index++) {
            Debug_View_Node *node = flat_children + flat_index;
            
            u64 parent_duration = parent->duration;
            
            f32 percent = (f32)node->duration/(f32)parent_duration*100;
            f32 self_percent = (f32)node->self_duration/(f32)parent_duration*100;
            
            char *name = to_c_string(&debug_state->arena, node->name);
            char *buffer = arena_push_array(scratch, char, 256);
            sprintf_s(buffer, 256, 
                      "%s: %.2f%% (%lld) %d hits (%lld cy/h)", 
                      name, self_percent, node->self_duration,
                      node->count, node->self_duration/node->count);
            
            String str = from_c_string(buffer);
            
            render_save(group);
            v3 indent_trans = V3(10.0f*node->depth, 0, 0);
            render_translate(group, indent_trans);
            
            f32 rect_width = font_get_text_width_pixels(&gui->font, str);
            rect2 on_screen_rect = 
              rect2_min_size(V2(0, 0), V2(rect_width, LINE_INTERVAL));
            
            render_color(group, DEBUG_BG_COLOR);
            push_rect(group, on_screen_rect);
            
            i32 child_indices_count = node->one_past_last_child_index -
              node->first_child_index;
            
            render_color(group, COLOR_WHITE);
            
            render_translate(group, V3(0, 5, 0));
            push_text(group, str);
            render_restore(group);
            
            render_translate(group, V3(0, -LINE_INTERVAL, 0));
          }
          render_translate(group, V3(-0.1f*(node->depth+1), 0, 0));
        }
        
        if (node->type == Debug_View_Type_TREE)
          node_index++;
        else
          node_index = node->one_past_last_child_index;
      }
      
      render_restore(group);
    }
    
    if (!any_frame_selected) {
      if (debug_state->gui.selected_frame_index >= 0) {
        if (input->mouse.left.went_down) {
          debug_state->gui.selected_frame_index = -1;
          arena_set_mark(&gui->arena, gui->node_memory);
        }
      } else {
        debug_state->pause = false;
      }
    }
  }
  
  render_group_output(&debug_state->arena, group, &state->renderer);
  arena_set_mark(&debug_state->arena, debug_render_memory);
  
  DEBUG_FUNCTION_END();
}
