#ifndef DEBUG_H

typedef enum {
  Debug_Type_NONE,
  Debug_Type_BEGIN_TIMER,
  Debug_Type_END_TIMER,
} Debug_Type;

typedef struct {
  i32 id;
  Debug_Type type;
  i32 thread_index;
  char *name;
  u64 cycles;
} Debug_Event;

globalvar Debug_Event debug_events[20000];
globalvar u32 debug_event_count = 0;

#define DEBUG_COUNTER_BEGIN() \
i32 __debug_id = __COUNTER__; \
debug_log_event(__debug_id, Debug_Type_BEGIN_TIMER, __func__, __rdtsc())

#define DEBUG_COUNTER_END() \
debug_log_event(__debug_id, Debug_Type_END_TIMER, __func__, __rdtsc())

void debug_log_event(i32 id, Debug_Type type, char *name, u64 cycles) {
  Debug_Event *e = debug_events + debug_event_count++;
  e->id = id;
  e->type = type;
  e->thread_index = get_thread_id();
  e->name = name;
  e->cycles = cycles;
}

i32 opened_debugs[16];

typedef struct Debug_Node Debug_Node;

struct Debug_Node {
  b32 is_open;
  u64 cycle_count;
  String name;
  Debug_Node *children;
};

#define DEBUG_H
#endif