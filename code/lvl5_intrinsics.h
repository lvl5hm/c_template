#ifndef LVL5_INTRINSICS_H

#include <intrin.h>
#include "lvl5_types.h"

#define MEM(dst, index) ((f32 *)&dst)[index]
#define MEMi(dst, index) ((i32 *)&dst)[index]

u8 get_thread_id() {
  byte *thread_local_storage = (byte *)__readgsqword(0x30);
  u8 result = *(u8 *)(thread_local_storage + 0x48);
  
  return result;
}

#define LVL5_INTRINSICS_H
#endif