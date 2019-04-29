#ifndef LVL5_CONTEXT
#define LVL5_CONTEXT_VERSION 0

#include "lvl5_types.h"
#include "lvl5_alloc.h"

typedef enum {
  Allocator_Mode_ALLOC,
  Allocator_Mode_FREE,
  Allocator_Mode_REALLOC,
} Allocator_Mode;

#define ALLOCATOR(name) byte *name(u64 size, void *old_ptr, u64 old_size, \
u64 align, Allocator_Mode mode, void *allocator_data)

typedef ALLOCATOR(Allocator);


void copy_memory_slow(void *dst, void *src, u64 size) {
  for (u64 i = 0; i < size; i++) {
    ((byte *)dst)[i] = ((byte *)src)[i];
  }
}

void zero_memory_slow(void *dst, u64 size) {
  for (u64 i = 0; i < size; i++) {
    ((byte *)dst)[i] = 0;
  }
}

ALLOCATOR(arena_allocator) {
  byte *result = 0;
  
  Arena *arena = (Arena *)allocator_data;
  
  switch (mode)
  {
    case Allocator_Mode_ALLOC: {
      result = arena_push_size(arena, size, align);
    } break;
    
    case Allocator_Mode_FREE: {
      // NOTE(lvl5): arenas don't free
    } break;
    
    case Allocator_Mode_REALLOC: {
      result = arena_push_size(arena, size, align);
      if (old_ptr) {
        copy_memory_slow(result, old_ptr, old_size ? old_size : size);
      }
    } break;
  }
  
  return result;
}

ALLOCATOR(dynamic_allocator) {
  byte *result = 0;
  
  Dynamic_Allocator *dyn = (Dynamic_Allocator *)allocator_data;
  
  switch (mode)
  {
    case Allocator_Mode_ALLOC: {
      result = dynamic_push_memory(dyn, size, align);
    } break;
    
    case Allocator_Mode_FREE: {
      dynamic_free(old_ptr);
      // NOTE(lvl5): arenas don't free
    } break;
    
    case Allocator_Mode_REALLOC: {
      result = dynamic_push_memory(dyn, size, align);
      if (old_ptr) {
        copy_memory_slow(result, old_ptr, old_size ? old_size : size);
      }
    } break;
  }
  
  return result;
}


typedef struct {
  Allocator *allocator;
  void *allocator_data;
  
  Arena *scratch_memory;
} Context;


// TODO(lvl5): make this multithreaded
Context __global_context_stack[64];
u32 __global_context_count;

Context get_context() {
  Context result = __global_context_stack[__global_context_count-1];
  return result;
}

void push_context(Context ctx) {
  assert(__global_context_count < array_count(__global_context_stack));
  __global_context_stack[__global_context_count++] = ctx;
}

void pop_context() {
  __global_context_count--;
}

Arena make_scratch_memory(void *data, u64 capacity) {
  Arena result;
  arena_init(&result, data, capacity);
  return result;
}

#define alloc_struct(T, align) (T *)alloc(sizeof(T), align)
#define alloc_array(T, count, align) (T *)alloc(sizeof(T)*(count), align)

byte *alloc(u64 size, u64 align) {
  Context ctx = get_context();
  byte *result = ctx.allocator(size, 0, 0, align, Allocator_Mode_ALLOC,
                               ctx.allocator_data);
  return result;
}


byte *reallocate(void *ptr, u64 old_size, u64 new_size, u64 align) {
  Context ctx = get_context();
  byte *result = ctx.allocator(new_size, ptr, old_size, align, Allocator_Mode_REALLOC,
                               ctx.allocator_data);
  return result;
}


void free(void *ptr) {
  Context ctx = get_context();
  ctx.allocator(0, ptr, 0, 0, Allocator_Mode_FREE, ctx.allocator_data);
}



#define scratch_alloc_struct(T, align) (T *)scratch_alloc(sizeof(T), align)
#define scratch_alloc_array(T, count, align) (T *)scratch_alloc(sizeof(T)*(count), align)


byte *scratch_alloc(u64 size, u64 align) {
  Context ctx = get_context();
  byte *result = arena_allocator(size, 0, 0, align, Allocator_Mode_ALLOC,
                                 ctx.scratch_memory);
  return result;
}


void scratch_clear() {
  Context ctx = get_context();
  ctx.scratch_memory->size = 0;
}

#define LVL5_CONTEXT
#endif