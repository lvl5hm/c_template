#ifndef DEBUG_H

typedef enum {
  Debug_Type_NONE,
  Debug_Type_BEGIN_TIMER,
  Debug_Type_END_TIMER,
} Debug_Type;

typedef struct {
  i16 id;
  u8 type;
  u8 thread_index;
  char *name;
  u64 cycles;
} Debug_Event;

typedef struct {
  Debug_Event events[20000];
  u32 event_count;
  
  i32 timer_count;
} Debug_Frame;

typedef enum {
  Debug_View_Type_NONE,
  Debug_View_Type_TREE,
  Debug_View_Type_FLAT,
} Debug_View_Type;

typedef struct {
  Debug_View_Type type;
  u32 event_index;
  u64 duration;
  i32 first_child_index;
  i32 one_past_last_child_index;
  i32 parent_index; // 0 is for no parent
  i32 depth;
  u64 self_duration;
} Debug_View_Node;

typedef struct {
  Arena arena;
  i32 selected_frame_index; // -1 == no frame selected
  Debug_View_Node *nodes; // 0th element is reserved
  i32 node_count;
  
  u64 node_memory;
} Debug_GUI;


typedef struct {
  Debug_Frame frames[60];
  i32 frame_index;
  b32 pause;
  
  Debug_GUI gui;
} Debug_State;


globalvar Debug_State debug_state = {0};

#define DEBUG_FUNCTION_BEGIN() \
u8 __debug_id = __COUNTER__; \
debug_log_event(__debug_id, Debug_Type_BEGIN_TIMER, __func__)

#define DEBUG_FUNCTION_END() \
debug_log_event(__debug_id, Debug_Type_END_TIMER, __func__)

#define DEBUG_SECTION_BEGIN(name) \
u8 __debug_id_##name = __COUNTER__; \
debug_log_event(__debug_id_##name, Debug_Type_BEGIN_TIMER, #name)

#define DEBUG_SECTION_END(name) \
debug_log_event(__debug_id_##name, Debug_Type_END_TIMER, #name)

void debug_begin_frame() {
  if (!debug_state.pause) {
    Debug_Frame *frame = debug_state.frames + debug_state.frame_index;
    frame->event_count = 0;
    frame->timer_count = 0;
  }
}

void debug_end_frame() {
  if (!debug_state.pause) {
    debug_state.frame_index = (debug_state.frame_index+1) % 
      array_count(debug_state.frames);
  }
}

void debug_log_event(i16 id, Debug_Type type, char *name) {
  if (!debug_state.pause) {
    Debug_Frame *frame = debug_state.frames + debug_state.frame_index;
    Debug_Event *e = frame->events + frame->event_count++;
    e->id = id;
    e->type = type;
    e->thread_index = get_thread_id();
    e->name = name;
    e->cycles = __rdtsc();
    
    if (type == Debug_Type_END_TIMER) {
      frame->timer_count++;
    }
  }
}

#define DEBUG_H
#endif