#ifndef AUDIO_H

#include "lvl5_types.h"

typedef struct {
  i16 *samples[2];
  u32 count;
} Sound;

typedef enum {
  Sound_Type_MUSIC,
  Sound_Type_EFFECTS,
  Sound_Type_INTERFACE,
  
  Sound_Type_count,
} Sound_Type;

typedef struct {
  b32 is_active;
  
  i32 index;
  Sound *wav;
  f32 position;
  v2 volume;
  v2 target_volume;
  v2 volume_change_per_second;
  
  f32 speed;
  Sound_Type type;
} Playing_Sound;

typedef struct {
  v3 p;
  Playing_Sound *snd;
} Sound_Emitter;

typedef struct {
  Playing_Sound sounds[64];
  i32 sound_count;
  
  i32 empty_sound_slots[64];
  i32 empty_sound_slot_count;
  
  Sound_Emitter emitters[64];
  i32 emitter_count;
  
  f32 volume_master;
  f32 volumes[Sound_Type_count];
} Sound_State;

#define AUDIO_H
#endif