#ifndef LVL5_STRETCHY_BUFFER
#define LVL5_STRETCHY_BUFFER_VERSION 0

#include "lvl5_alloc.h"

typedef struct {
  u32 count;
  u32 capacity;
  
#ifdef LVL5_DEBUG
  b32 is_growable;
#endif
  Arena *arena;
} sb_Header;


#ifndef LVL5_STRETCHY_BUFFER_GROW_FACTOR
#define LVL5_STRETCHY_BUFFER_GROW_FACTOR 2
#endif


#define __get_header(arr) ((sb_Header *)(arr)-1)
#define sb_count(arr) (__get_header(arr)->count)
#define sb_capacity(arr) (__get_header(arr)->capacity)
#define sb_arena(arr) (__get_header(arr)->arena)

#define __need_grow(arr) (!arr || (sb_count(arr) == sb_capacity(arr)))
#define sb_init(arena, T, capacity, is_growable) __sb_init(arena, capacity, is_growable, sizeof(T))
void *__sb_init(Arena *arena, u32 capacity, b32 is_growable, u32 item_size) {
  sb_Header header = {0};
  header.capacity = capacity;
  header.is_growable = is_growable;
  header.arena = arena;
  header.count = 0;
  
  void *memory = arena_push_size(arena, sizeof(sb_Header)+item_size*capacity);
  *(sb_Header *)memory = header;
  return (byte *)memory + sizeof(sb_Header);
}

#define sb_push(arr, item) __need_grow(arr) ? __grow(&(arr), sizeof(item), true) : 0, (arr)[sb_count(arr)++] = item

void *__grow(void **arr_ptr, u64 item_size, b32 is_growable) {
  void *arr = *arr_ptr;
  
  sb_Header *header = __get_header(arr);
  assert(header->arena);
  
#ifdef LVL5_DEBUG
  if (arr) {
    assert(header->is_growable == is_growable);
  }
#endif
  
  void *result = arr;
  
  i32 new_capacity = header->capacity*LVL5_STRETCHY_BUFFER_GROW_FACTOR;
  
  u32 header_size = sizeof(sb_Header);
  result = arena_push_size(header->arena, 
                           new_capacity*item_size + header_size) + header_size;
  copy_memory_slow(result, arr, header->capacity*item_size);
  
  sb_count(result) = header->count;
  sb_capacity(result) = new_capacity;
  *arr_ptr = result;
  
#ifdef LVL5_DEBUG
  __get_header(result)->is_growable = is_growable;
#endif
  
  return 0;
}

#define LVL5_STRETCHY_BUFFER
#endif