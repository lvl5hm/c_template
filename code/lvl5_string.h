#ifndef LVL5_STRING
#define LVL5_STRING_VERSION 0

#include "lvl5_types.h"
#include "lvl5_context.h"
#include "stdarg.h"

typedef struct {
  char *data;
  u32 count;
} String;

#define const_string(const_char) make_string(const_char, array_count(const_char)-1)

String make_string(char *data, u32 count) {
  String result;
  result.data = data;
  result.count = count;
  return result;
}

i32 find_last_index(String str, String substr) {
  for (u32 i = str.count - substr.count - 1; i >= 0; i--)
  {
    u32 srcIndex = i;
    u32 testIndex = 0;
    while (str.data[srcIndex] == substr.data[testIndex])
    {
      srcIndex++;
      testIndex++;
      if (testIndex == substr.count)
      {
        return i;
      }
    }
  }
  return -1;
}

i32 find_index(String str, String substr) {
  for (u32 i = 0; i < str.count - substr.count; i++)
  {
    u32 srcIndex = i;
    u32 testIndex = 0;
    while (str.data[srcIndex] == substr.data[testIndex])
    {
      srcIndex++;
      testIndex++;
      if (testIndex == substr.count)
      {
        return i;
      }
    }
  }
  return -1;
}

String substring(String s, u32 begin, u32 end) {
  String result;
  result.data = s.data + begin;
  result.count = end - begin;
  return result;
}

String scratch_concat(String a, String b) {
  String result;
  result.data = (char *)scratch_alloc(a.count + b.count, 4);
  result.count = a.count + b.count;
  copy_memory_slow(result.data, a.data, a.count);
  copy_memory_slow(result.data + a.count, b.data, b.count);
  return result;
}

char *scratch_c_string(String a) {
  char *result = (char *)scratch_alloc(a.count + 1, 4);
  copy_memory_slow(result, a.data, a.count);
  result[a.count] = '\0';
  return result;
}

// zero terminator does NOT count!
i32 c_string_length(char *str) {
  i32 result = 0;
  while (*str++) result++;
  return result;
}

b32 c_string_compare(char *a, char *b) {
  while (*a) {
    if (*a++ != *b++) {
      return false; 
    }
  }
  if (!*b) return true;
  return false;
}

String i32_to_string(i32 num)
{
  char *str = scratch_alloc_array(char, 11, 4);
  String result = {0};
  
  i32 n = num;
  
  if (n == 0) {
    str[10 - result.count] = '0';
    result.count = 1;
  } else {
    if (n < 0) {
      n *= -1;
    }
    
    while (n != 0) {
      str[10 - result.count] = '0' + (n % 10);
      n /= 10;
      result.count++;
    }
    
    if (num < 0) {
      str[10 - result.count] = '-';
      result.count++;
    }
  }
  
  result.data = str + 11 - result.count;
  return result;
}


String scratch_sprintf(String fmt, ...) {
  String result = {0};
  
  va_list args;
  va_start(args, fmt);
  
  
  for (u32 i = 0; i < fmt.count; i++) {
    if (fmt.data[i] == '%') {
      if (fmt.data[i+1] == 'i') {
        i32 num = va_arg(args, i32);
        String str = i32_to_string(num);
        
        result = scratch_concat(result, str);
        i += 1;
      }
    } else {
      result = scratch_concat(result, make_string(&fmt.data[i], 1));
    }
  }
  
  va_end(args);
  
  return result;
}


#define LVL5_STRING
#endif