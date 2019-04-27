#define make_riff_id(str) ((str[0] << 0)|(str[1] << 8)|(str[2] << 16)|(str[3] << 24))

typedef u32 Riff_Id;

Riff_Id Riff_Id_RIFF;
Riff_Id Riff_Id_WAVE;
Riff_Id Riff_Id_fmt;
Riff_Id Riff_Id_data;
Riff_Id Riff_Id_fact;

#define WAVE_FORMAT_PCM 0x0001


void audio_init() {
  Riff_Id_RIFF = make_riff_id("RIFF");
  Riff_Id_WAVE = make_riff_id("WAVE");
  Riff_Id_fmt = make_riff_id("fmt ");
  Riff_Id_data = make_riff_id("data");
  Riff_Id_fact = make_riff_id("fact");
}

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


Sound load_wav(Platform platform, String file_name) {
  String file = platform.read_entire_file(file_name);
  
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
  
  result.samples[0] = lvl5_alloc_array(i16, result.count, 4*2);
  result.samples[1] = lvl5_alloc_array(i16, result.count, 4*2);
  i16 *interleaved_samples = (i16 *)(data_chunk + 1);
  for (u32 sample_index = 0; sample_index < result.count; sample_index++) {
    result.samples[0][sample_index] = interleaved_samples[sample_index*2];
    result.samples[1][sample_index] = interleaved_samples[sample_index*2+1];
  }
  
  return result;
}
