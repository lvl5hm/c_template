#include "audio.h"

#define make_riff_id(str) ((str[0] << 0)|(str[1] << 8)|(str[2] << 16)|(str[3] << 24))

typedef u32 Riff_Id;

#define WAVE_FORMAT_PCM 0x0001

#if 0

void write_sine_wave(game_Sound_Buffer *sound_buffer, f32 dt) {
  for (u32 sample_index = 0; sample_index < sound_buffer->count; sample_index++) {
    i16 sample = (i16)(sinf((f32)sine_wave_sample_index*0.03f)*32767);
    sound_buffer->samples[sample_index*2] = sample;
    sound_buffer->samples[sample_index*2+1] = sample;
    sine_wave_sample_index++;
  }
  
  sine_wave_sample_index -= sound_buffer->overwrite_count;
}
#endif


#pragma pack(push, 1)
typedef struct {
  Riff_Id id;
  u32 size;
} Riff_Chunk;

typedef struct {
  Riff_Chunk main;
  Riff_Id id;
} Wav_Header;

typedef struct {
  Riff_Chunk chunk;
  u16 wFormatTag;
  u16 nChannels;
  u32 nSamplesPerSec;
  u32 nAvgBytesPerSec;
  u16 nBlockAlign;
  u16 WBitsPerSample;
  u16 cbSize;
  u16 wValidBitsPerSample;
  u32 dwChannelMask;
  byte SubFormat[16];
} Wav_Fmt_Chunk;
#pragma pack(pop)


Sound load_wav(Arena *arena, String file_name) {
  Riff_Id Riff_Id_RIFF = make_riff_id("RIFF");
  Riff_Id Riff_Id_WAVE = make_riff_id("WAVE");
  Riff_Id Riff_Id_fmt = make_riff_id("fmt ");
  Riff_Id Riff_Id_data = make_riff_id("data");
  Riff_Id Riff_Id_fact = make_riff_id("fact");
  
  Buffer file = platform.read_entire_file(file_name);
  
  Wav_Header *wav_header = (Wav_Header *)file.data;
  assert(wav_header->main.id == Riff_Id_RIFF);
  assert(wav_header->id == Riff_Id_WAVE);
  
  Wav_Fmt_Chunk *fmt = (Wav_Fmt_Chunk *)(file.data + sizeof(Wav_Header));
  assert(fmt->chunk.id == Riff_Id_fmt);
  assert(fmt->wFormatTag == WAVE_FORMAT_PCM);
  assert(fmt->nChannels == 2);
  assert(fmt->nSamplesPerSec == SAMPLES_PER_SECOND);
  
  Riff_Chunk *data_chunk = (Riff_Chunk *)(file.data + sizeof(Wav_Header) + sizeof(Riff_Chunk) + fmt->chunk.size);
  assert(data_chunk->id == Riff_Id_data);
  
  Sound result = {0};
  result.count = data_chunk->size / 4; // 4 is double sample size
  
  result.samples[0] = arena_push_array(arena, i16, result.count);
  result.samples[1] = arena_push_array(arena, i16, result.count);
  i16 *interleaved_samples = (i16 *)(data_chunk + 1);
  for (u32 sample_index = 0; sample_index < result.count; sample_index++) {
    result.samples[0][sample_index] = interleaved_samples[sample_index*2];
    result.samples[1][sample_index] = interleaved_samples[sample_index*2+1];
  }
  
  return result;
}

Playing_Sound *sound_play(Sound_State *sound_state, Sound *wav) {
  assert(sound_state->sound_count < array_count(sound_state->sounds));
  Playing_Sound *snd = sound_state->sounds + sound_state->sound_count++;
  snd->wav = wav;
  snd->position = 0;
  snd->volume = V2(1, 1);
  snd->target_volume = V2(1, 1);
  snd->index = sound_state->sound_count - 1;
  snd->speed = 1;
  
  return snd;
}

void sound_set_volume(Playing_Sound *snd, v2 target_volume, f32 seconds) {
  snd->target_volume = target_volume;
  snd->volume_change_per_second = v2_div_s(v2_sub(target_volume, snd->volume), seconds);
}

void sound_stop(Sound_State *state, i32 sound_index) {
  Playing_Sound *last = state->sounds + state->sound_count - 1;
  Playing_Sound *removed = state->sounds + sound_index;
  *removed = *last;
  state->sound_count--;
}

void sound_mix_playing_sounds(Sound_Buffer *dst, Sound_State *sound_state,
                              Arena *temp, f32 dt) {
  DEBUG_FUNCTION_BEGIN();
  u64 mixing_memory = arena_get_mark(temp);
  
  DEBUG_SECTION_BEGIN(_clear_buffer);
  f32 *left = arena_push_array(temp, f32, dst->count);
  f32 *right = arena_push_array(temp, f32, dst->count);
  for (i32 i = 0; i < dst->count; i++) {
    left[i] = 0;
    right[i] = 0;
  }
  DEBUG_SECTION_END(_clear_buffer);
  
  for (i32 sound_index = 0; sound_index < sound_state->sound_count; sound_index++) {
    DEBUG_SECTION_BEGIN(_mix_single_sound);
    Playing_Sound *snd = sound_state->sounds + sound_index;
    Sound *wav = snd->wav;
    
    i32 samples_to_mix = dst->count;
    i32 samples_left_in_sound = round_f32_i32((wav->count - snd->position)/snd->speed);
    if (samples_left_in_sound < samples_to_mix) {
      samples_to_mix = samples_left_in_sound;
    }
    
    v2 volume_change_per_frame = v2_mul_s(v2_sub(snd->target_volume, snd->volume), dt);
    v2 volume_change_per_sample = v2_div_s(volume_change_per_frame, (f32)samples_to_mix);
    
    for (i32 sample_index = 0; sample_index < samples_to_mix; sample_index++) {
      f32 pos = snd->position + (f32)sample_index*snd->speed;
      i32 src_index = round_f32_i32(pos);
      
      v2 volume = v2_add(snd->volume, 
                         v2_mul_s(volume_change_per_sample, (f32)sample_index));
      f32 src_l = wav->samples[0][src_index];
      f32 src_r = wav->samples[1][src_index];
      left[sample_index] += src_l*volume.x;
      right[sample_index] += src_r*volume.y;
    }
    
    snd->volume = v2_add(snd->volume, volume_change_per_frame);
    snd->position += samples_to_mix*snd->speed;
    snd->position -= dst->overwrite_count*snd->speed;
    
    if (samples_to_mix == samples_left_in_sound) {
      sound_stop(sound_state, sound_index);
    }
    DEBUG_SECTION_END(_mix_single_sound);
  }
  
  DEBUG_SECTION_BEGIN(_fill_sound_buffer);
  for (i32 i = 0; i < dst->count; i++) {
    dst->samples[i*2] = round_f32_i16(left[i]);
    dst->samples[i*2+1] = round_f32_i16(right[i]);
  }
  DEBUG_SECTION_END(_fill_sound_buffer);
  
  arena_set_mark(temp, mixing_memory);
  
  DEBUG_FUNCTION_END();
}
