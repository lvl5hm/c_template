#include "font.h"


typedef union {
  struct {
    byte r;
    byte g;
    byte b;
    byte a;
  };
  u32 full;
} Pixel;

#pragma pack(push, 1)
typedef struct {
  u32 info_header_size;
  u32 width;
  u32 height;
  u16 planes;
  u16 bits_per_pixel;
  u32 compression;
  u32 image_size;
  u32 x_pixels_per_meter;
  u32 y_pixels_per_meter;
  u32 colors_used;
  u32 important_colors;
} Bmp_Info_Header;

typedef struct {
  u16 signature;
  u32 file_size;
  u32 reserved;
  u32 data_offset;
  Bmp_Info_Header info;
} Bmp_File_Header;
#pragma pack(pop)

Bitmap load_bmp(String file_name) {
  Buffer file = platform.read_entire_file(file_name);
  assert(file.data);
  Bmp_File_Header *header = (Bmp_File_Header *)file.data;
  assert(header->signature == (('B' << 0) | ('M' << 8)));
  assert(header->file_size == file.size);
  assert(header->info.planes == 1);
  assert(header->info.bits_per_pixel == 32);
  
  byte *data = file.data + header->data_offset;
  
#define R_MASK 0x00FF0000
#define G_MASK 0x0000FF00
#define B_MASK 0x000000FF
#define A_MASK 0xFF000000
  
  Bitmap result = {0};
  result.data = data;
  result.width = header->info.width;
  result.height = header->info.height;
  
  for (i32 pixel_index = 0; 
       pixel_index < result.width*result.height;
       pixel_index++) {
    u32 *pixel = (u32 *)result.data + pixel_index;
    u32 swizzled = *pixel;
    
    u32 r = (R_MASK & swizzled) >> 16;
    u32 g = (G_MASK & swizzled) >> 8;
    u32 b = (B_MASK & swizzled) >> 0;
    u32 a = (A_MASK & swizzled) >> 24;
    u32 unswizzled = (r << 0)|(g << 8)|(b << 16)|(a << 24);
    *pixel = unswizzled;
  }
  
  return result;
}

Bitmap make_empty_bitmap(Arena *arena, i32 width, i32 height) {
  Bitmap result;
  result.width = width;
  result.height = height;
  result.data = (byte *)arena_push_array(arena, u32, width*height);
  zero_memory_slow(result.data, width*height*sizeof(u32));
  return result;
}



Texture_Atlas make_texture_atlas_from_bitmaps(Arena *arena, Bitmap *bitmaps, i32 count) {
  Texture_Atlas result = {0};
  result.sprite_count = count;
  result.rects = arena_push_array(arena, rect2, count);
  
  i32 max_width = 0;
  i32 total_height = 0;
#define PADDING_Y 4
  
  for (i32 bitmap_index = 0; bitmap_index < count; bitmap_index++) {
    Bitmap *bmp = bitmaps + bitmap_index;
    if (bmp->width > max_width) {
      max_width = bmp->width;
    }
    total_height += bmp->height + PADDING_Y;
  }
  
  result.bmp = make_empty_bitmap(arena, max_width, total_height);
  i32 current_y = 0;
  
  for (i32 bitmap_index = 0; bitmap_index < count; bitmap_index++) {
    Bitmap *bmp = bitmaps + bitmap_index;
    rect2 *rect = result.rects + bitmap_index;
    
    rect->min = V2(0, (f32)current_y/(f32)total_height);
    rect->max = V2(bmp->width/(f32)max_width, rect->min.y + (f32)bmp->height/(f32)total_height);
    
    u32 *row = (u32 *)result.bmp.data + current_y*max_width;
    for (i32 y = 0; y < bmp->height; y++) {
      u32 *dst = row;
      for (i32 x = 0; x < bmp->width; x++) {
        *dst++ = ((u32 *)bmp->data)[y*bmp->width + x];
      }
      row += max_width;
    }
    
    current_y += bmp->height;
  }
  
  return result;
}

Texture_Atlas make_texture_atlas_from_folder(Arena *temp, Arena *perm, String folder) {
  File_List dir = platform.get_files_in_folder(folder);
  u64 loaded_bitmaps_memory = arena_get_mark(temp);
  
  Bitmap *bitmaps = arena_push_array(temp, Bitmap, dir.count);
  
  for (i32 file_index = 0; file_index < dir.count; file_index++) {
    String file_name = dir.files[file_index];
    String full_name = concat(temp, folder, concat(temp, const_string("\\"), file_name));
    Bitmap sprite_bmp = load_bmp(full_name);
    bitmaps[file_index] = sprite_bmp;
  }
  
  Texture_Atlas result = make_texture_atlas_from_bitmaps(perm, bitmaps, dir.count); 
  arena_set_mark(temp, loaded_bitmaps_memory);
  
  return result;
}

#define FONT_HEIGHT 16

Font load_ttf(Arena *temp, Arena *perm, String file_name) {
  Font result;
  
  stbtt_fontinfo font;
  Buffer font_file = platform.read_entire_file(file_name);
  const unsigned char *font_buffer = (const unsigned char *)font_file.data;
  stbtt_InitFont(&font, font_buffer, stbtt_GetFontOffsetForIndex(font_buffer, 0));
  
  u64 loaded_bitmaps_memory = arena_get_mark(temp);
  result.first_codepoint_index = ' ';
  i32 last_codepoint_index = '~';
  result.codepoint_count = last_codepoint_index - result.first_codepoint_index;
  result.metrics = sb_init(perm, Codepoint_Metrics,
                           result.codepoint_count, false);
  
  
  Bitmap *bitmaps = sb_init(temp, Bitmap, result.codepoint_count, false);
  for (char ch = result.first_codepoint_index; ch < last_codepoint_index; ch++) {
    i32 width;
    i32 height;
    f32 scale = stbtt_ScaleForPixelHeight(&font, FONT_HEIGHT);
    byte *single_bitmap = stbtt_GetCodepointBitmap(&font, 0, scale, ch, &width, &height, 0, 0);
    Bitmap bitmap = make_empty_bitmap(temp, width, height);
    
    i32 x0, y0, x1, y1;
    stbtt_GetCodepointBitmapBox(&font, ch, scale, scale, &x0,&y0,&x1,&y1);
    
#if 0    
    if (ch == '.') {
      int fpp = 23;
    }
#endif
    
    u32 *row = (u32 *)bitmap.data + (height-1)*width;
    for (i32 y = 0; y < height; y++) {
      u32 *dst = row;
      for (i32 x = 0; x < width; x++) {
        byte a = single_bitmap[y*width + x];
        Pixel pixel;
        pixel.r = 255;
        pixel.g = 255;
        pixel.b = 255;
        pixel.a = a;
        *dst++ = pixel.full;
      }
      row -= width;
    }
    
    Codepoint_Metrics metrics;
    metrics.origin = V2(-(f32)x0/width, (f32)y1/height);
    
    i32 advance, _lsb;
    stbtt_GetCodepointHMetrics(&font, ch, &advance, &_lsb);
    metrics.advance = advance*scale;
    
    metrics.kerning = arena_push_array(perm, f32, result.codepoint_count);
    for (char codepoint_index = 0;
         codepoint_index< result.codepoint_count;
         codepoint_index++) {
      char other_ch = result.first_codepoint_index + codepoint_index;
      f32 kern = scale*stbtt_GetCodepointKernAdvance(&font, ch, 
                                                     other_ch);
      metrics.kerning[codepoint_index] = kern;
    }
    
    sb_push(bitmaps, bitmap);
    sb_push(result.metrics, metrics);
  }
  
  arena_set_mark(temp, loaded_bitmaps_memory);
  
  Texture_Atlas atlas = make_texture_atlas_from_bitmaps(perm, bitmaps, sb_count(bitmaps));
  result.atlas = atlas;
  
  return result;
}

Codepoint_Metrics font_get_metrics(Font *font, char ch) {
  i32 font_index = ch - font->first_codepoint_index;
  Codepoint_Metrics metrics = font->metrics[font_index];
  return metrics;
}

Sprite font_get_sprite(Font *font, char ch) {
  Codepoint_Metrics metrics = font_get_metrics(font, ch);
  i32 font_index = ch - font->first_codepoint_index;
  Sprite spr;
  spr.index = font_index;
  spr.origin = metrics.origin;
  spr.atlas = &font->atlas;
  assert(spr.atlas->rects);
  return spr;
}


// NOTE(lvl5): width is in pixels
f32 font_get_text_width(Font *font, String text) {
  f32 result = 0;
  for (u32 i = 0; i < text.count; i++) {
    Codepoint_Metrics metrics = font_get_metrics(font, text.data[i]);
    result += metrics.advance;
  }
  return result;
}
