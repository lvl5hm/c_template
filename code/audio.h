#ifndef AUDIO_H

#include "lvl5_types.h"

typedef struct {
  i16 *samples[2];
  u32 count;
} Sound;


typedef struct {
  i32 index;
  Sound *wav;
  f32 position;
  v2 volume;
  v2 target_volume;
  v2 volume_change_per_second;
  
  f32 speed;
} Playing_Sound;

typedef struct {
  Playing_Sound sounds[64];
  i32 sound_count;
} Sound_State;

#define AUDIO_H
#endif