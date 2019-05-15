#include "audio.h"
#include "lvl5_intrinsics.h"

typedef u32 Riff_Id;
#define make_riff_id(str) ((str[0] << 0)|(str[1] << 8)|(str[2] << 16)|(str[3] << 24))

#define WAVE_FORMAT_PCM 0x0001

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
  
  Riff_Chunk *data_chunk = (Riff_Chunk *)(file.data + sizeof(Wav_Header) + 
                                          sizeof(Riff_Chunk) + fmt->chunk.size);
  assert(data_chunk->id == Riff_Id_data);
  
  Sound result = {0};
  result.count = data_chunk->size / 4; // 4 is double sample size
  
#define PADDING 128
  result.samples[0] = (i16 *)_arena_push_memory(arena, sizeof(i16)*result.count + PADDING, 32);
  result.samples[1] = (i16 *)_arena_push_memory(arena, sizeof(i16)*result.count + PADDING, 32);
  
  i16 *interleaved_samples = (i16 *)(data_chunk + 1);
  for (u32 sample_index = 0; sample_index < result.count; sample_index++) {
    result.samples[0][sample_index] = interleaved_samples[sample_index*2];
    result.samples[1][sample_index] = interleaved_samples[sample_index*2+1];
  }
  
  return result;
}

void sound_init(Sound_State *sound_state) {
  sound_state->volume_master = 1;
  sound_state->volumes[Sound_Type_MUSIC] = 1;
  sound_state->volumes[Sound_Type_EFFECTS] = 1;
  sound_state->volumes[Sound_Type_INTERFACE] = 1;
}

Playing_Sound *sound_play(Sound_State *sound_state, Sound *wav, Sound_Type type) {
  assert(sound_state->sound_count < array_count(sound_state->sounds));
  Playing_Sound *snd = sound_state->sounds + sound_state->sound_count++;
  snd->wav = wav;
  snd->position = 0;
  snd->volume = V2(1, 1);
  snd->target_volume = V2(1, 1);
  snd->index = sound_state->sound_count - 1;
  snd->speed = 1;
  snd->type = type;
  
  return snd;
}

void sound_set_volume(Playing_Sound *snd, v2 target_volume, f32 seconds) {
  snd->target_volume = target_volume;
  if (seconds) {
    snd->volume_change_per_second = v2_div_s(v2_sub(target_volume, snd->volume), seconds);
  } else {
    snd->volume = snd->target_volume;
  }
}

void sound_stop(Sound_State *state, i32 sound_index) {
  Playing_Sound *last = state->sounds + state->sound_count - 1;
  Playing_Sound *removed = state->sounds + sound_index;
  *removed = *last;
  state->sound_count--;
}

#if 1

i32 sine_wave_sample_index = 0;

void write_sine_wave(Sound_Buffer *sound_buffer, f32 dt) {
  for (i32 sample_index = 0; sample_index < sound_buffer->count; sample_index++) {
    i16 sample = (i16)(sinf((f32)sine_wave_sample_index*0.03f)*32767);
    sound_buffer->samples[sample_index*2] = sample;
    sound_buffer->samples[sample_index*2+1] = sample;
    sine_wave_sample_index++;
  }
  
  sine_wave_sample_index -= sound_buffer->overwrite_count;
}
#endif


void sound_mix_playing_sounds(Sound_Buffer *dst, Sound_State *sound_state,
                              Arena *temp, f32 dt) {
  DEBUG_FUNCTION_BEGIN();
  u64 mixing_memory = arena_get_mark(temp);
  assert(dst->count % 8 == 0);
  
  i32 count_div_4 = dst->count/4;
  __m128 zero_4 = _mm_set_ps1(0);
  
  DEBUG_SECTION_BEGIN(_clear_buffer);
  
  __m128 *left = arena_push_array(temp, __m128, count_div_4);
  __m128 *right = arena_push_array(temp, __m128, count_div_4);
  for (i32 i = 0; i < count_div_4; i++) {
    _mm_store_ps((float *)(left+i), zero_4); 
    _mm_store_ps((float *)(right+i), zero_4); 
  }
  
  DEBUG_SECTION_END(_clear_buffer);
  
  for (i32 sound_index = 0; sound_index < sound_state->sound_count; sound_index++) {
    DEBUG_SECTION_BEGIN(_mix_single_sound);
    Playing_Sound *snd = sound_state->sounds + sound_index;
    Sound *wav = snd->wav;
    //assert(wav->count % 8 == 0);
    
    i32 samples_to_mix = dst->count;
    i32 samples_left_in_sound = round_f32_i32((wav->count - snd->position)/snd->speed);
    if (samples_left_in_sound < samples_to_mix) {
      samples_to_mix = samples_left_in_sound;
    }
    
    assert(samples_to_mix > 0);
    
    v2 volume_change_per_frame = v2_mul_s(v2_sub(snd->target_volume, snd->volume), dt);
    v2 volume_change_per_sample = v2_div_s(volume_change_per_frame, (f32)samples_to_mix);
    
    __m128 speed_4 = _mm_set_ps1(snd->speed);
    __m128 initial_pos_4 = _mm_set_ps1(snd->position);
    
    // NOTE(lvl5): apply global volume settings
    __m128 global_volume = _mm_mul_ps(_mm_set_ps1(sound_state->volume_master),_mm_set_ps1(sound_state->volumes[snd->type]));
    
    __m128 initial_volume_l_4 = _mm_mul_ps(_mm_set_ps1(snd->volume.x), global_volume);
    __m128 initial_volume_r_4 = _mm_mul_ps(_mm_set_ps1(snd->volume.y), global_volume);
    
    __m128 volume_change_per_sample_l_4 = _mm_mul_ps(_mm_set_ps1(volume_change_per_sample.x), global_volume);
    __m128 volume_change_per_sample_r_4 = _mm_mul_ps(_mm_set_ps1(volume_change_per_sample.y), global_volume);
    
    for (i32 sample_index = 0; sample_index < samples_to_mix; sample_index += 4) {
      __m128 sample_index_ps = _mm_setr_ps((f32)sample_index+0,
                                           (f32)sample_index+1,
                                           (f32)sample_index+2,
                                           (f32)sample_index+3);
      __m128 pos = _mm_add_ps(initial_pos_4, _mm_mul_ps(sample_index_ps, speed_4));
      
      __m128i src_index = _mm_cvtps_epi32(pos);
      
      __m128 volume_l = _mm_add_ps(initial_volume_l_4, 
                                   _mm_mul_ps(sample_index_ps,
                                              volume_change_per_sample_l_4));
      __m128 volume_r = _mm_add_ps(initial_volume_r_4, 
                                   _mm_mul_ps(sample_index_ps,
                                              volume_change_per_sample_r_4));
      
      __m128 sample_l = _mm_cvtepi32_ps(_mm_setr_epi32(wav->samples[0][MEMi(src_index, 0)],
                                                       wav->samples[0][MEMi(src_index, 1)],
                                                       wav->samples[0][MEMi(src_index, 2)],
                                                       wav->samples[0][MEMi(src_index, 3)]));
      
      __m128 sample_r = _mm_cvtepi32_ps(_mm_setr_epi32(wav->samples[1][MEMi(src_index, 0)],
                                                       wav->samples[1][MEMi(src_index, 1)],wav->samples[1][MEMi(src_index, 2)],wav->samples[1][MEMi(src_index, 3)]));
      
      __m128 sample_with_volume_l = _mm_mul_ps(sample_l, volume_l);
      __m128 sample_with_volume_r = _mm_mul_ps(sample_r, volume_r);
      
      i32 sample_index_div_4 = sample_index/4;
      __m128 dst_l = _mm_load_ps((float *)(left + sample_index_div_4));
      __m128 dst_r = _mm_load_ps((float *)(right + sample_index_div_4));
      
      __m128 mixed_sample_l = _mm_add_ps(dst_l, sample_with_volume_l);
      __m128 mixed_sample_r = _mm_add_ps(dst_r, sample_with_volume_r);
      
      _mm_store_ps((float *)(left + sample_index_div_4), mixed_sample_l);
      _mm_store_ps((float *)(right + sample_index_div_4), mixed_sample_r);
    }
    
    snd->volume = v2_add(snd->volume, volume_change_per_frame);
    snd->position += samples_to_mix*snd->speed;
    snd->position -= dst->overwrite_count*snd->speed;
    
    if (samples_to_mix == samples_left_in_sound) {
      sound_stop(sound_state, sound_index);
      sound_index--;
    }
    DEBUG_SECTION_END(_mix_single_sound);
  }
  
  DEBUG_SECTION_BEGIN(_fill_sound_buffer);
  
  for (i32 i = 0; i < count_div_4; i++) {
    __m128i l = _mm_cvtps_epi32(left[i]);
    __m128i r = _mm_cvtps_epi32(right[i]);
    
    __m128i lr0 = _mm_unpacklo_epi32(l, r);
    __m128i lr1 = _mm_unpackhi_epi32(l, r);
    
    __m128i packed = _mm_packs_epi32(lr0, lr1);
    _mm_store_si128((__m128i *)dst->samples+i, packed);
  }
  
  DEBUG_SECTION_END(_fill_sound_buffer);
  arena_set_mark(temp, mixing_memory);
  
  DEBUG_FUNCTION_END();
}
