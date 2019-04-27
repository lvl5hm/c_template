#ifndef LVL5_STRETCHY_BUFFER
#define LVL5_STRETCHY_BUFFER_VERSION 0

#ifndef LVL5_CONTEXT

#ifndef LVL5_MALLOC
#include <malloc.h>
#define LVL5_REALLOC(old_ptr, size) realloc(old_ptr, size)
#define LVL5_FREE(ptr) free(ptr)
#endif

#endif

typedef struct {
  u32 count;
  u32 capacity;
  
#ifdef LVL5_DEBUG
  b32 is_growable;
#endif
  
#ifdef LVL5_CONTEXT
  Allocator *allocator;
  void *allocator_data;
#endif
} sb_Header;


#ifndef LVL5_STRETCHY_BUFFER_GROW_FACTOR
#define LVL5_STRETCHY_BUFFER_GROW_FACTOR 2
#endif


#define __get_header(arr) ((sb_Header *)(arr)-1)
#define sb_count(arr) (__get_header(arr)->count)
#define sb_capacity(arr) (__get_header(arr)->capacity)

#ifdef LVL5_CONTEXT
#define sb_allocator(arr) (__get_header(arr)->allocator)
#define sb_allocator_data(arr) (__get_header(arr)->allocator_data)
#endif

#define __need_grow(arr) (!arr || (sb_count(arr) == sb_capacity(arr)))

#define sb_push(arr, item) __need_grow(arr) ? __grow(&(arr), sizeof(item), 32, true) : 0, (arr)[sb_count(arr)++] = item

#define sb_free(arr) __free(arr), arr = 0

#define sb_reserve(arr, capacity, is_growable) __need_grow(arr) ? __grow(&(arr), sizeof(arr[0]), capacity, is_growable) : 0

void *__grow(void **arr_ptr, u64 item_size, i32 reserve_count, b32 is_growable) {
  void *arr = *arr_ptr;
  
  sb_Header *header = __get_header(arr);
  
#ifdef LVL5_DEBUG
  if (arr) {
    assert(header->is_growable == is_growable);
  }
#endif
  
  void *result = arr;
  
#ifdef LVL5_CONTEXT
  Allocator *allocator;
  void *allocator_data;
  
  if (arr) {
    allocator = header->allocator;
    allocator_data = header->allocator_data;
  } else {
    Context ctx = get_context();
    allocator = ctx.allocator;
    allocator_data = ctx.allocator_data;
  }
  
  i32 old_capacity = arr ? header->capacity : 0;
  i32 new_capacity = arr ? header->capacity*LVL5_STRETCHY_BUFFER_GROW_FACTOR : reserve_count;
  i32 old_count = arr ? header->count : 0;
  
  u32 header_size = sizeof(sb_Header);
  byte *old_ptr = arr ? (byte *)arr - header_size : 0;
  result = allocator(new_capacity*item_size + header_size, old_ptr, old_capacity*item_size+header_size, 4, Allocator_Mode_REALLOC, allocator_data) + header_size;
  
  sb_count(result) = old_count;
  sb_capacity(result) = new_capacity;
  sb_allocator(result) = allocator;
  sb_allocator_data(result) = allocator_data;
#else
  i32 new_capacity = arr ? header->capacity*LVL5_STRETCHY_BUFFER_GROW_FACTOR : reserve_count;
  i32 old_count = arr ? header->count : 0;
  
  u32 header_size = sizeof(sb_Header);
  byte *old_ptr = arr ? (byte *)arr - header_size : 0;
  result = (byte *)LVL5_REALLOC(old_ptr, new_capacity*item_size + header_size) + header_size;
  
  sb_count(result) = old_count;
  sb_capacity(result) = new_capacity;
#endif
  *arr_ptr = result;
  
#ifdef LVL5_DEBUG
  __get_header(result)->is_growable = is_growable;
#endif
  
  return 0;
}

void __free(void *arr) {
  if (arr) {
    byte *old_ptr = (byte *)arr - sizeof(sb_Header);
#ifdef LVL5_CONTEXT
    sb_Header *header = __get_header(arr);
    header->allocator(0, old_ptr, 0, 0, Allocator_Mode_FREE, header->allocator_data);
#else
    LVL5_FREE(old_ptr);
#endif
  }
}

#define LVL5_STRETCHY_BUFFER
#endif