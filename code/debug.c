#include "debug.h"
#include "renderer.c"
#include <stdio.h>

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

void debug_draw_gui(State *state, v2 screen_size, game_Input *input) {
  Debug_GUI *gui = &debug_state.gui;
  
  u64 debug_render_memory = arena_get_mark(&state->debug_arena);
  Render_Group _debug_render_group;
  Render_Group *group = &_debug_render_group;
  
  render_group_init(&state->debug_arena, state, group, 10000, screen_size); 
  f32 total_width = 3.0f;
  f32 rect_width = total_width/array_count(debug_state.frames);
  
#define MAX_CYCLES 40891803
  
  for (i32 i = 0; i < array_count(debug_state.frames); i++) {
    i32 frame_index = (debug_state.frame_index + i) % array_count(debug_state.frames);
    
    Debug_Frame *frame = debug_state.frames + frame_index;
    if (frame->event_count && frame_index != debug_state.frame_index) {
      u64 begin_cycles = frame->events[0].cycles;
      u64 end_cycles = frame->events[frame->event_count-1].cycles;
      u64 duration = end_cycles - begin_cycles;
      
      f32 rect_height = (f32)duration/MAX_CYCLES*10;
      v2 screen_meters = v2_div_s(screen_size, PIXELS_PER_METER);
      
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
  
  render_group_output(&state->debug_arena, group, &state->renderer);
  arena_set_mark(&state->debug_arena, debug_render_memory);
  
  
  if (debug_state.gui.selected_frame_index >= 0) {
    Debug_Frame *frame = debug_state.frames +
      debug_state.gui.selected_frame_index;
    
    u64 debug_render_memory = arena_get_mark(&state->debug_arena);
    v2 half_screen_size_meters = v2_div_s(screen_size, PIXELS_PER_METER*2);
    
    Render_Group _group;
    Render_Group *group = &_group;
    render_group_init(&state->debug_arena, state, group, 10000, screen_size); 
    
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
    render_group_output(&state->debug_arena, group, &state->renderer);
    
    arena_set_mark(&state->debug_arena, debug_render_memory);
  }
}
