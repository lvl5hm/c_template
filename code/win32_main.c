#include <stdio.h>
#include <malloc.h>
#include <math.h>

#define LVL5_DEBUG
#include "lvl5_math.h"
#include "lvl5_types.h"
#include "lvl5_stretchy_buffer.h"

#include <Windows.h>
#include "platform.h"
#include <dsound.h>

#include "lvl5_opengl_win32.h"

#define TARGET_FPS 60
#include "lvl5_context.h"



typedef struct {
  i32 buffer_sample_count;
  i32 samples_per_second;
  WORD single_sample_size;
  WORD channel_count;
  i32 current_sample_index;
  LPDIRECTSOUNDBUFFER direct_buffer;
} win32_Sound;

typedef enum {
  Replay_State_NONE,
  Replay_State_WRITE,
  Replay_State_PLAY,
} Replay_State;

typedef struct {
  Replay_State state;
  
  byte *data;
  Input inputs[TARGET_FPS*60];
  i32 count;
  i32 play_index;
} win32_Replay;


typedef struct {
  b32 window_resized;
  u64 performance_frequency;
  f32 dt;
  b32 running;
  win32_Sound sound;
  Sound_Buffer game_sound_buffer;
  
  win32_Replay replay;
  Arena scratch;
  Arena arena;
} win32_State;

win32_State state;


u64 win32_get_last_write_time(String file_name) {
  WIN32_FIND_DATAA find_data;
  HANDLE file_handle = FindFirstFileA(
    to_c_string(&state.scratch, file_name),
    &find_data);
  u64 result = 0;
  if (file_handle != INVALID_HANDLE_VALUE) {
    FindClose(file_handle);
    FILETIME write_time = find_data.ftLastWriteTime;
    
    result = write_time.dwHighDateTime;
    result = result << 32;
    result = result | write_time.dwLowDateTime;
  }
  return result;
}

void win32_replay_begin_write(Memory memory) {
  win32_Replay *r = &state.replay;
  assert(r->state == Replay_State_NONE);
  r->state = Replay_State_WRITE;
  copy_memory_slow(r->data, memory.perm, memory.perm_size);
  r->count = 0;
}

void win32_replay_begin_play(Memory memory) {
  win32_Replay *r = &state.replay;
  r->state = Replay_State_PLAY;
  r->play_index = 0;
  copy_memory_slow(memory.perm, r->data, memory.perm_size);
}

void win32_replay_save_input(Input input, Memory memory) {
  win32_Replay *r = &state.replay;
  assert(r->state == Replay_State_WRITE);
  
  r->inputs[r->count++] = input;
  if (r->count == array_count(r->inputs)) {
    win32_replay_begin_play(memory);
  }
}

Input win32_replay_get_next_input(Memory memory) {
  win32_Replay *r = &state.replay;
  assert(r->state == Replay_State_PLAY);
  assert(r->play_index < r->count);
  Input result = r->inputs[r->play_index++];
  if (r->play_index == r->count) {
    win32_replay_begin_play(memory);
  }
  return result;
}

typedef struct {
  HANDLE handle;
  Mem_Size size;
  b32 no_errors;
} win32_File;

PLATFORM_OPEN_FILE(win32_open_file) {
  HANDLE handle = CreateFileA((LPCSTR)to_c_string(&state.scratch, file_name),
                              GENERIC_READ,
                              FILE_SHARE_READ,
                              0,
                              OPEN_EXISTING,
                              FILE_ATTRIBUTE_NORMAL,
                              0);
  
  win32_File *result = (win32_File *)malloc(sizeof(win32_File));
  result->handle = INVALID_HANDLE_VALUE;
  result->size = 0;
  result->no_errors = false;
  
  if (handle != INVALID_HANDLE_VALUE)
  {
    LARGE_INTEGER file_size_li;
    
    if (GetFileSizeEx(handle, &file_size_li))
    {
      u64 file_size = file_size_li.QuadPart;
      
      result->handle = handle;
      result->size = file_size;
      result->no_errors = true;
    }
  }
  
  return (File_Handle)result;
}


PLATFORM_FILE_ERROR(win32_file_error) {
  ((win32_File *)file)->no_errors = false;
}

PLATFORM_FILE_HAS_NO_ERRORS(win32_file_has_no_errors) {
  b32 result = ((win32_File *)file)->no_errors;
  return result;
}

PLATFORM_READ_FILE(win32_read_file) {
  if (win32_file_has_no_errors(file)) {
    OVERLAPPED overlapped = {0};
    overlapped.Offset = (u32)((offset >> 0) & 0xFFFFFFFF);
    overlapped.OffsetHigh = (u32)((offset >> 32) & 0xFFFFFFFF);
    
    DWORD bytes_read;
    ReadFile(
      ((win32_File *)file)->handle,
      dst,
      (u32)size,
      &bytes_read,
      &overlapped);
    assert(bytes_read == size);
  }
}

PLATFORM_CLOSE_FILE(win32_close_file) {
  CloseHandle(((win32_File *)file)->handle);
  free(file);
}

u32 win32_sound_get_multisample_size(win32_Sound *snd) {
  u32 result = snd->single_sample_size*snd->channel_count;
  return result;
}

u32 win32_sound_get_buffer_size_in_bytes(win32_Sound *snd) {
  u32 result = snd->buffer_sample_count*win32_sound_get_multisample_size(snd);
  return result;
}

u32 win32_sound_get_bytes_per_second(win32_Sound *snd) {
  u32 result = snd->samples_per_second*win32_sound_get_multisample_size(snd);
  return result;
}



#define DIRECT_SOUND_CREATE(name) HRESULT WINAPI name(LPGUID lpGuid, \
LPDIRECTSOUND *ppDS, LPUNKNOWN pUnkOuter)
typedef DIRECT_SOUND_CREATE(Direct_Sound_Create);

LPDIRECTSOUNDBUFFER win32_init_dsound(HWND window, win32_Sound *win32_sound) {
  HMODULE direct_sound_library = LoadLibraryA("dsound.dll");
  assert(direct_sound_library);
  
  Direct_Sound_Create *DirectSoundCreate = (Direct_Sound_Create *)GetProcAddress(direct_sound_library, "DirectSoundCreate");
  assert(DirectSoundCreate);
  
  LPDIRECTSOUND direct_sound;
  HRESULT direct_sound_created = DirectSoundCreate(null, &direct_sound, null);
  assert(direct_sound_created == DS_OK);
  
  HRESULT cooperative_level_set = IDirectSound_SetCooperativeLevel(direct_sound, 
                                                                   window, DSSCL_PRIORITY);
  assert(cooperative_level_set == DS_OK);
  
  // NOTE(lvl5): primary buffer
  DSBUFFERDESC primary_desc = {0};
  primary_desc.dwSize = sizeof(primary_desc);
  primary_desc.dwFlags = DSBCAPS_PRIMARYBUFFER;
  
  LPDIRECTSOUNDBUFFER primary_buffer;
  
  
  HRESULT primary_buffer_created = IDirectSound_CreateSoundBuffer(direct_sound, &primary_desc, &primary_buffer, null);
  assert(primary_buffer_created == DS_OK);
  
  WAVEFORMATEX wave_format = {0};
  wave_format.wFormatTag = WAVE_FORMAT_PCM;
  wave_format.nChannels = win32_sound->channel_count;
  wave_format.nSamplesPerSec = win32_sound->samples_per_second;
  wave_format.wBitsPerSample = win32_sound->single_sample_size*8;
  wave_format.nBlockAlign = wave_format.nChannels*wave_format.wBitsPerSample/8;
  wave_format.nAvgBytesPerSec = wave_format.nSamplesPerSec*wave_format.nBlockAlign;
  
  HRESULT format_set = IDirectSoundBuffer_SetFormat(primary_buffer,  &wave_format);
  assert(format_set == DS_OK);
  
  
  // NOTE(lvl5): secondary buffer
  DSBUFFERDESC secondary_desc = {0};
  secondary_desc.dwSize = sizeof(secondary_desc);
  secondary_desc.dwFlags = DSBCAPS_GLOBALFOCUS;
  secondary_desc.dwBufferBytes = win32_sound_get_buffer_size_in_bytes(win32_sound);
  secondary_desc.lpwfxFormat = &wave_format;
  
  LPDIRECTSOUNDBUFFER secondary_buffer;
  
  HRESULT secondary_buffer_created = IDirectSound_CreateSoundBuffer(direct_sound, &secondary_desc, &secondary_buffer, null);
  assert(secondary_buffer_created == DS_OK);
  
  HRESULT is_playing = IDirectSoundBuffer_Play(secondary_buffer, null, null, DSBPLAY_LOOPING);
  assert(is_playing == DS_OK);
  
  return secondary_buffer;
}


String win32_get_build_dir() {
  String full_path;
  full_path.data = arena_push_array(&state.scratch, char, MAX_PATH);
  full_path.count = GetModuleFileNameA(0, full_path.data, MAX_PATH);
  assert(full_path.count);
  
  u32 last_slash_index = find_last_index(full_path, const_string("\\"));
  String result = substring(full_path, 0, last_slash_index + 1);
  
  return result;
}

String win32_get_work_dir() {
#if 0
  String build_dir = win32_get_build_dir();
#endif
  String path_end = const_string("..\\data\\");
  
  String result = path_end;// concat(&state.scratch, build_dir, path_end);
  return result;
}

PLATFORM_READ_ENTIRE_FILE(win32_read_entire_file) {
  String path = win32_get_work_dir();
  String full_name = concat(&state.scratch, path, file_name);
  char *c_file_name = to_c_string(&state.scratch, full_name);
  HANDLE file = CreateFileA(c_file_name,
                            GENERIC_READ,
                            FILE_SHARE_READ,
                            0,
                            OPEN_EXISTING,
                            FILE_ATTRIBUTE_NORMAL,
                            0);
  
  assert(file != INVALID_HANDLE_VALUE);
  
  LARGE_INTEGER file_size_li;
  GetFileSizeEx(file, &file_size_li);
  u64 file_size = file_size_li.QuadPart;
  
  byte *buffer = (byte *)malloc(file_size);
  u32 bytes_read;
  ReadFile(file, buffer, (DWORD)file_size, (LPDWORD)&bytes_read, 0);
  assert(bytes_read == file_size);
  
  CloseHandle(file);
  
  Buffer result;
  result.data = buffer;
  result.size = file_size;
  
  return result;
}

DWORD win32_sound_get_write_start() {
  win32_Sound win32_sound = state.sound;
  
  DWORD write_start = win32_sound.current_sample_index*
    win32_sound_get_multisample_size(&win32_sound) % 
    win32_sound_get_buffer_size_in_bytes(&win32_sound);
  
  return write_start;
}

Sound_Buffer *win32_request_sound_buffer() {
  win32_Sound *sound = &state.sound;
  Sound_Buffer *game_sound_buffer = &state.game_sound_buffer;
  
  DWORD write_start = win32_sound_get_write_start();
  DWORD write_length = 0;
  
  DWORD write_cursor;
  HRESULT got_position = IDirectSoundBuffer_GetCurrentPosition(sound->direct_buffer,
                                                               null, &write_cursor);
  assert(got_position == DS_OK);
  
  u32 bytes_per_frame = win32_sound_get_bytes_per_second(sound)/60;
  
#define OVERWRITE_BYTES align_pow_2(sound->samples_per_second/TARGET_FPS*win32_sound_get_multisample_size(sound)*2, 32)
  
  DWORD target_cursor = align_pow_2((write_cursor + bytes_per_frame + OVERWRITE_BYTES) %
                                    win32_sound_get_buffer_size_in_bytes(sound), 32);
  
  if (target_cursor == write_start) {
    write_length = 0;
  } else {
    if (target_cursor > write_start) {
      write_length = target_cursor - write_start;
    } else {
      write_length = win32_sound_get_buffer_size_in_bytes(sound) - 
        write_start + target_cursor;
    }
  }
  
  game_sound_buffer->count = write_length/win32_sound_get_multisample_size(sound);
  game_sound_buffer->overwrite_count = OVERWRITE_BYTES/
    win32_sound_get_multisample_size(sound);
  
  return game_sound_buffer;
}



void win32_fill_audio_buffer(win32_Sound *win32_sound, Sound_Buffer *src_buffer) {
  DWORD write_start = win32_sound_get_write_start();
  u32 multisample_size = win32_sound_get_multisample_size(win32_sound);
  u32 size_to_lock = src_buffer->count*multisample_size;
  
  i16 *first_region;
  DWORD first_bytes;
  i16 *second_region;
  DWORD second_bytes;
  
  HRESULT is_locked = IDirectSoundBuffer_Lock(win32_sound->direct_buffer, write_start, size_to_lock,
                                              (LPVOID)&first_region, &first_bytes,
                                              (LPVOID)&second_region, &second_bytes, null);
  assert(is_locked == DS_OK);
  
  i32 first_sample_count = first_bytes/multisample_size;
  i32 second_sample_count = second_bytes/multisample_size;
  assert(first_sample_count + second_sample_count == src_buffer->count);
  
  i16 *src = src_buffer->samples;
  
  for (i32 sample_index = 0; sample_index < first_sample_count; sample_index++) {
    first_region[sample_index*2] = *src++;
    first_region[sample_index*2 + 1] = *src++;
    win32_sound->current_sample_index++;
  }
  
  for (i32 sample_index = 0; sample_index < second_sample_count; sample_index++) {
    second_region[sample_index*2] = *src++;
    second_region[sample_index*2 + 1] = *src++;
    win32_sound->current_sample_index++;
  }
  
  win32_sound->current_sample_index -= src_buffer->overwrite_count;
  
  assert(src == src_buffer->samples + src_buffer->count*2);
  
  HRESULT is_unlocked = IDirectSoundBuffer_Unlock(win32_sound->direct_buffer, first_region, first_bytes, second_region, second_bytes);
  assert(is_unlocked == DS_OK);
}


f64 win32_get_time() {
  LARGE_INTEGER time_li;
  QueryPerformanceCounter(&time_li);
  f64 result = ((f64)time_li.QuadPart/(f64)state.performance_frequency);
  return result;
}


void win32_handle_button(Button *button, b32 is_down) {
  if (is_down) {
    button->pressed = true;
  }
  if (button->is_down && !is_down) {
    button->went_up = true;
  } else if (!button->is_down && is_down) {
    button->went_down = true;
  }
  button->is_down = (bool)is_down;
}





LRESULT CALLBACK WindowProc(HWND window,
                            UINT message,
                            WPARAM wParam,
                            LPARAM lParam) {
  switch (message) {
    case WM_SIZE: {
      state.window_resized = true;
    } break;
    
    case WM_DESTROY:
    case WM_CLOSE:
    case WM_QUIT: 
    {
      state.running = false;
    } break;
    
    default: {
      return DefWindowProcA(window, message, wParam, lParam);
    }
  }
  return 0;
}


File_List win32_get_files_in_folder(String str) {
  File_List result = {0};
  
  // TODO(lvl5): this needs to be freed at some point
  result.files = sb_init(&state.arena, String, 8, true);
  
  WIN32_FIND_DATAA findData;
  String wildcard = concat(&state.scratch, win32_get_work_dir(), concat(&state.scratch, str, const_string("\\*.*")));
  char *wildcard_c = to_c_string(&state.scratch, wildcard);
  HANDLE file = FindFirstFileA(wildcard_c, &findData);
  
  while (file != INVALID_HANDLE_VALUE) {
    if (!c_string_compare(findData.cFileName, ".") &&
        !c_string_compare(findData.cFileName, "..")) {
      
      char *src = findData.cFileName;
      i32 name_length = c_string_length(src);
      char *dst = (char *)malloc(name_length);
      copy_memory_slow(dst, src, name_length);
      
      sb_push(result.files, make_string(dst, name_length));
      result.count++;
    }
    
    b32 next_file_found = FindNextFileA(file, &findData);
    if (!next_file_found) {
      break;
    }
  }
  
  return result;
}




typedef struct {
  volatile i32 write_cursor;
  volatile i32 read_cursor;
  Work_Queue_Entry entries[32];
  HANDLE semaphore;
} win32_Work_Queue;


typedef struct {
  win32_Work_Queue *queue;
  i32 thread_index;
} win32_Thread_Info;

PLATFORM_ADD_WORK_QUEUE_ENTRY(win32_add_queue_entry) {
  win32_Work_Queue *queue = (win32_Work_Queue *)queue_ptr;
  
  Work_Queue_Entry *entry = queue->entries + queue->write_cursor;
  entry->fn = fn;
  entry->data = data;
  
  complete_past_writes_before_future_writes();
  i32 new_write_cursor = (queue->write_cursor + 1) % array_count(queue->entries);
  assert(new_write_cursor != queue->read_cursor);
  queue->write_cursor = new_write_cursor;
  ReleaseSemaphore(queue->semaphore, 1, 0);
}

b32 win32_do_next_queue_entry(win32_Work_Queue *queue) {
  b32 result = true;
  
  i32 original_read_cursor = queue->read_cursor;
  i32 new_read_cursor = (original_read_cursor + 1) % 
    array_count(queue->entries);
  
  if (original_read_cursor != queue->write_cursor) {
    i32 index = InterlockedCompareExchange((volatile LONG *)&queue->read_cursor,
                                           new_read_cursor,
                                           original_read_cursor);
    if (index == original_read_cursor) {
      Work_Queue_Entry *entry = queue->entries + index;
      entry->fn(entry->data);
    }
  } else {
    result = false;
  }
  
  return result;
}

DWORD WINAPI ThreadProc(void *data) {
  win32_Thread_Info *info = (win32_Thread_Info *)data;
  win32_Work_Queue *queue = info->queue;
  
  while (true) {
    char buffer[128];
    sprintf_s(buffer, array_count(buffer), "thread %d: ", info->thread_index);
    OutputDebugStringA(buffer);
    b32 should_try_again = win32_do_next_queue_entry(queue);
    if (!should_try_again) {
      WaitForSingleObjectEx(queue->semaphore, INFINITE, false);
    }
  }
  
  return 0;
}


#define THREAD_COUNT 8

int CALLBACK WinMain(HINSTANCE instance,
                     HINSTANCE prevInstance,
                     LPSTR commandLine,
                     int showCommandLine) {
  win32_Work_Queue high_queue = {0};
  high_queue.semaphore = CreateSemaphoreA(null, 0, array_count(high_queue.entries), null);
  win32_Thread_Info thread_infos[THREAD_COUNT];
  
  for (i32 thread_index = 0; thread_index < THREAD_COUNT; thread_index++) {
    win32_Thread_Info *info = thread_infos + thread_index;
    info->queue = &high_queue;
    info->thread_index = thread_index;
    CreateThread(null, 0, ThreadProc, info, 0, null);
  }
  
  LARGE_INTEGER performance_frequency_li;
  QueryPerformanceFrequency(&performance_frequency_li);
  state.performance_frequency = performance_frequency_li.QuadPart;
  
  MMRESULT sheduler_granularity_set = timeBeginPeriod(1);
  assert(sheduler_granularity_set == TIMERR_NOERROR);
  
  Mem_Size scratch_size = kilobytes(40);
  arena_init(&state.scratch, malloc(scratch_size), scratch_size);
  
  Mem_Size arena_size = megabytes(2);
  arena_init(&state.arena, malloc(arena_size), arena_size);
  
  // NOTE(lvl5): windows init
  WNDCLASSA window_class = {0};
  window_class.style = CS_HREDRAW|CS_VREDRAW|CS_OWNDC;
  window_class.lpfnWndProc = WindowProc;
  window_class.hInstance = instance;
  window_class.hCursor = LoadCursor((HINSTANCE)0, IDC_ARROW);
  window_class.lpszClassName = "testThingWindowClass";
  
  if (!RegisterClassA(&window_class)) return 0;
  
#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600
  HWND window = CreateWindowA(window_class.lpszClassName,
                              "Awesome window name",
                              WS_OVERLAPPEDWINDOW|WS_VISIBLE,
                              CW_USEDEFAULT, // x
                              CW_USEDEFAULT, // y
                              WINDOW_WIDTH,
                              WINDOW_HEIGHT,
                              0,
                              0,
                              instance,
                              0);
  if (!window) {
    return 0;
  }
  
  // NOTE(lvl5): init openGL
  win32_init_opengl(window);
  
  // NOTE(lvl5): init sound
  {
    win32_Sound win32_sound;
    win32_sound.current_sample_index = 0;
    win32_sound.samples_per_second = SAMPLES_PER_SECOND;
    win32_sound.channel_count = 2;
    win32_sound.single_sample_size = sizeof(i16);
    win32_sound.buffer_sample_count = align_pow_2(win32_sound.samples_per_second, 16);
    
    win32_sound.direct_buffer = win32_init_dsound(window, &win32_sound);
    
    Sound_Buffer game_sound_buffer = {0};
    game_sound_buffer.samples = (i16 *)malloc(sizeof(i16)*win32_sound.buffer_sample_count*win32_sound.channel_count);
    zero_memory_slow(game_sound_buffer.samples, win32_sound_get_buffer_size_in_bytes(&win32_sound));
    
    
    state.game_sound_buffer = game_sound_buffer;
    state.sound = win32_sound;
  }
  
  HDC device_context = GetDC(window);
  
  Memory game_memory;
  game_memory.perm_size = megabytes(64);
  game_memory.temp_size = gigabytes(1);
  game_memory.debug_size = megabytes(512);
  Mem_Size total_size = game_memory.perm_size + game_memory.temp_size + game_memory.debug_size;
  byte *total_memory = (byte *)VirtualAlloc((void *)terabytes(2),
                                            total_size,
                                            MEM_COMMIT|MEM_RESERVE,
                                            PAGE_READWRITE);
  assert(total_memory);
  
  game_memory.perm = total_memory;
  game_memory.temp = game_memory.perm + game_memory.perm_size;
  game_memory.debug = game_memory.temp + game_memory.temp_size;
  
  zero_memory_slow(total_memory, game_memory.perm_size);
  
  state.replay.data = malloc(game_memory.perm_size);
  
  Input game_input = {0};
  
  state.running = true;
  state.dt = 0;
  
  f64 last_time = win32_get_time();
  u64 last_cycles = __rdtsc();
  
  Platform platform;
  platform.request_sound_buffer = win32_request_sound_buffer;
  platform.read_entire_file = win32_read_entire_file;
  platform.gl = gl;
  platform.get_files_in_folder = win32_get_files_in_folder;
  platform.open_file = win32_open_file;
  platform.file_error = win32_file_error;
  platform.file_has_no_errors = win32_file_has_no_errors;
  platform.read_file = win32_read_file;
  platform.close_file = win32_close_file;
  platform.add_work_queue_entry = win32_add_queue_entry;
  platform.high_queue = (Work_Queue)&high_queue;
  
  HMODULE game_lib = 0;
  Game_Update *game_update = 0;
  u64 last_game_dll_write_time = 0;
  
  MSG message;
  while (state.running) {
    {
      String build_dir = win32_get_build_dir();
      String lock_path = concat(&state.scratch, build_dir,
                                const_string("lock.tmp"));
      WIN32_FILE_ATTRIBUTE_DATA ignored;
      b32 lock_file_exists = 
        GetFileAttributesExA(to_c_string(&state.scratch, lock_path), GetFileExInfoStandard, &ignored);
      String dll_path = concat(&state.scratch, build_dir,
                               const_string("game.dll"));
      
      u64 current_write_time = win32_get_last_write_time(dll_path);
      if (!lock_file_exists && 
          current_write_time &&
          last_game_dll_write_time != current_write_time) {
        if (game_lib) {
          FreeLibrary(game_lib);
        }
        
        String copy_dll_path = 
          concat(&state.scratch, build_dir, const_string("game_temp.dll"));
        
        char *src = to_c_string(&state.scratch, dll_path);
        char *dst = to_c_string(&state.scratch, copy_dll_path);
        b32 copy_success = CopyFileA(src, dst, false);
        assert(copy_success);
        
        game_lib = LoadLibraryA("game_temp.dll");
        assert(game_lib);
        game_update = (Game_Update *)GetProcAddress(game_lib, "game_update");
        assert(game_update);
        
        last_game_dll_write_time = current_write_time;
        game_memory.is_reloaded = true;
      } else {
        game_memory.is_reloaded = false;
      }
    }
    
    if (state.window_resized) {
      game_memory.window_resized = true;
      state.window_resized = false;
    } else {
      game_memory.window_resized = false;
    }
    
    
    f64 time_frame_start = win32_get_time();
    u64 cycles_frame_start = __rdtsc();
    
    for (u32 button_index = 0; 
         button_index < array_count(game_input.buttons);
         button_index++) {
      Button *b = game_input.buttons + button_index;
      b->went_down = false;
      b->went_up = false;
      b->pressed = false;
    }
    
    for (u32 button_index = 0; 
         button_index < array_count(game_input.keys);
         button_index++) {
      Button *b = game_input.keys + button_index;
      b->went_down = false;
      b->went_up = false;
      b->pressed = false;
    }
    
    game_input.mouse.left.went_up = false;
    game_input.mouse.left.went_down = false;
    game_input.mouse.right.went_up = false;
    game_input.mouse.right.went_down = false;
    
    game_input.char_code = 0;
    
    
    RECT client_rect;
    GetClientRect(window, &client_rect);
    v2 game_screen = V2((f32)(client_rect.right - client_rect.left),
                        (f32)(client_rect.bottom - client_rect.top));
    
    POINT mouse_p;
    GetCursorPos(&mouse_p);
    ScreenToClient(window, &mouse_p);
    game_input.mouse.p.x = (f32)mouse_p.x;
    game_input.mouse.p.y = (f32)(client_rect.bottom - mouse_p.y);
    game_input.mouse.scroll = 0;
    
    
    while (PeekMessage(&message, window, 0, 0, PM_REMOVE)) {
      switch (message.message) {
        case WM_MOUSEWHEEL: {
          game_input.mouse.scroll = GET_WHEEL_DELTA_WPARAM(message.wParam)/WHEEL_DELTA;
        } break;
        
        case WM_RBUTTONDOWN: {
          win32_handle_button(&game_input.mouse.right, true);
          win32_handle_button(&game_input.skills[1], true);
        } break;
        case WM_LBUTTONDOWN: {
          win32_handle_button(&game_input.mouse.left, true);
          win32_handle_button(&game_input.skills[0], true);
        } break;
        case WM_LBUTTONUP: {
          win32_handle_button(&game_input.mouse.left, false);
          win32_handle_button(&game_input.skills[0], false);
        } break;
        case WM_RBUTTONUP: {
          win32_handle_button(&game_input.mouse.right, false);
          win32_handle_button(&game_input.skills[1], false);
        } break;
        
        case WM_KEYDOWN:
        case WM_KEYUP:
        case WM_SYSKEYDOWN:
        case WM_SYSKEYUP: {
#define KEY_WAS_DOWN_BIT 1 << 30
#define KEY_IS_UP_BIT 1 << 31
#define SCAN_CODE_MASK 0x0000FF00
          
          b32 key_was_down = (message.lParam & KEY_WAS_DOWN_BIT);
          b32 key_is_down = !(message.lParam & KEY_IS_UP_BIT);
          b32 key_went_down = !key_was_down && key_is_down;
          WPARAM key_code = message.wParam;
          
          if (key_is_down) {
            u32 scan_code = message.lParam & SCAN_CODE_MASK;
            byte keyboard_state[256];
            GetKeyboardState(keyboard_state);
            u16 char_code_u16[2];
            i32 got_keycode = ToAscii((UINT)key_code, scan_code,
                                      keyboard_state, char_code_u16, 0);
            
            if (got_keycode > 0) {
              game_input.char_code = (char)char_code_u16[0];
            }
          }
          
          if (key_code < array_count(game_input.keys)) {
            win32_handle_button(&game_input.keys[key_code], key_is_down);
          }
          
          switch (key_code)
          {
            case VK_LEFT:
            win32_handle_button(&game_input.move_left, key_is_down);
            break;
            case VK_RIGHT:
            win32_handle_button(&game_input.move_right, key_is_down);
            break;
            case VK_UP:
            win32_handle_button(&game_input.move_up, key_is_down);
            break;
            case VK_DOWN:
            win32_handle_button(&game_input.move_down, key_is_down);
            break;
            
            
            case 'Q':
            win32_handle_button(&game_input.skills[2], key_is_down);
            break;
            case 'E':
            win32_handle_button(&game_input.skills[3], key_is_down);
            break;
            case 'A':
            win32_handle_button(&game_input.move_left, key_is_down);
            break;
            case 'D':
            win32_handle_button(&game_input.move_right, key_is_down);
            break;
            case 'W':
            win32_handle_button(&game_input.move_up, key_is_down);
            break;
            case 'S':
            win32_handle_button(&game_input.move_down, key_is_down);
            break;
            case VK_SPACE:
            win32_handle_button(&game_input.start, key_is_down);
            break;
            case VK_F1:
            if (key_went_down && state.replay.state == Replay_State_NONE)
              win32_replay_begin_write(game_memory);
            break;
            case VK_F2:
            if (key_went_down && state.replay.state == Replay_State_WRITE)
              win32_replay_begin_play(game_memory);
            break;
          }
        } break;
        
        default:
        {
          TranslateMessage(&message);
          DispatchMessage(&message);
        }
      }
    }
    
    if (state.replay.state == Replay_State_WRITE) {
      win32_replay_save_input(game_input, game_memory);
    } else if (state.replay.state == Replay_State_PLAY) {
      game_input = win32_replay_get_next_input(game_memory);
    }
    
    game_update(game_screen, game_memory, &game_input, state.dt, platform);
    
    if (state.game_sound_buffer.count) {
      win32_fill_audio_buffer(&state.sound, &state.game_sound_buffer);
    }
    
    
    f64 current_time = win32_get_time();
    f32 time_used = (f32)(current_time - time_frame_start);
    
    //state.dt = (f32)(current_time - last_time);
    state.dt = 1.0f/TARGET_FPS;
    
    
    u64 current_cycles = __rdtsc();
    u64 cycles_per_frame = current_cycles - last_cycles;
    u64 cycles_used = current_cycles - cycles_frame_start;
    
    last_time = current_time;
    last_cycles = current_cycles;
    
    arena_set_mark(&state.scratch, 0);
    SwapBuffers(device_context);
  }
  
  return 0;
}
