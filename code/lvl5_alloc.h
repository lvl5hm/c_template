#ifndef LVL5_ALLOC
#define LVL5_ALLOC_VERSION 0

#include "lvl5_types.h"

typedef struct {
  byte *data;
  u64 size;
  u64 capacity;
  
#ifdef LVL5_DEBUG
  u32 marks_taken;
#endif
} Arena;


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

void arena_init(Arena *arena, void *data, u64 capacity) {
  arena->data = (byte *)data;
  arena->capacity = capacity;
  arena->size = 0;
}

#define arena_push_array(arena, T, count) \
(T *)_arena_push_memory(arena, sizeof(T)*count, 16)

#define arena_push_struct(arena, T) \
(T *)_arena_push_memory(arena, sizeof(T), 16)

#define arena_push_size(arena, size) \
_arena_push_memory(arena, size, 16)

byte *_arena_push_memory(Arena *arena, u64 size, u64 align) {
  byte *result = 0;
  assert(arena->size + size <= arena->capacity);
  
  u64 data_u64 = (u64)(arena->data + arena->size);
  u64 data_u64_aligned = align_pow_2(data_u64, align);
  result = (byte *)data_u64_aligned;
  arena->size += align_pow_2(size, align);
  
  return result;
}

u64 arena_get_mark(Arena *arena) {
  u64 result = arena->size;
  
#ifdef LVL5_DEBUG
  arena->marks_taken++;
#endif
  
  return result;
}

void arena_set_mark(Arena *arena, u64 mark) {
  assert(mark <= arena->capacity);
  arena->size = mark;
  
#ifdef LVL5_DEBUG
  arena->marks_taken--;
#endif
}

void arena_check_no_marks(Arena *arena) {
#ifdef LVL5_DEBUG
  assert(arena->marks_taken == 0);
#endif
}

void arena_init_subarena(Arena *parent, Arena *child,
                         u64 capacity) {
  byte *child_memory = arena_push_array(parent, byte, capacity);
  arena_init(child, child_memory, capacity);
}


//
// Dynamic allocator
//

typedef struct Used_Block Used_Block;

struct Used_Block {
  u64 size;
  Used_Block *prev;
  Used_Block *next;
};

typedef struct {
  byte *data;
  u64 capacity;
  
  Used_Block sentinel;
} Dynamic_Allocator;

void dynamic_init(Dynamic_Allocator *d, 
                  void *data, u64 capacity) {
  d->data = (byte *)data;
  d->capacity = capacity;
  d->sentinel.size = 0;
  d->sentinel.prev = &d->sentinel;
  d->sentinel.next = &d->sentinel;
}



byte *dynamic_push_memory(Dynamic_Allocator *d, u64 size, u64 align) {
  Used_Block *block = d->sentinel.next;
  byte *result = 0;
  
  while (true) {
    byte *block_end = (byte *)block + block->size;
    byte *next_block_start = (byte *)block->next;
    if (block == &d->sentinel) {
      block_end = d->data;
    }
    if (block->next == &d->sentinel) {
      next_block_start = d->data + d->capacity;
    }
    
    u64 free_size = next_block_start - block_end;
    u64 aligned_size = align_pow_2(size, align);
    
    if (free_size >= aligned_size + sizeof(Used_Block)) {
      u64 ptr_u64 = (u64)(block_end + sizeof(Used_Block));
      result = (byte *)align_pow_2(ptr_u64, align);
      
      Used_Block *new_block = (Used_Block *)result - 1;
      
      new_block->prev = block;
      new_block->next = block->next;
      
      new_block->prev->next = new_block;
      new_block->next->prev = new_block;
      
      new_block->size = aligned_size;
      break;
    } else if (block->next != &d->sentinel) {
      block = block->next;
    } else {
      break;
    }
  }
  
  return result;
}

void dynamic_free(void *ptr) {
  Used_Block *block = (Used_Block *)ptr - 1;
  block->prev->next = block->next;
  block->next->prev = block->prev;
}


#define LVL5_ALLOC
#endif