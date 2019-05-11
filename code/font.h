#ifndef FONT_H

#include "lvl5_types.h"


typedef struct {
  i32 width;
  i32 height;
  byte *data;
} Bitmap;


typedef struct {
  Bitmap bmp;
  rect2 *rects;
  i32 sprite_count;
} Texture_Atlas;


typedef struct {
  v2 origin;
  f32 advance;
  f32 *kerning;
} Codepoint_Metrics;

typedef struct {
  Texture_Atlas atlas;
  char first_codepoint_index;
  Codepoint_Metrics *metrics;
  i32 codepoint_count;
} Font;


#define FONT_H
#endif