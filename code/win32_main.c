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

#include "KHR/wglext.h"

#define DIRECT_SOUND_CREATE(name) HRESULT WINAPI name(LPGUID lpGuid, LPDIRECTSOUND *ppDS, LPUNKNOWN pUnkOuter)
typedef DIRECT_SOUND_CREATE(Direct_Sound_Create);

#define TARGET_FPS 60


void *get_any_gl_func_address(const char *name)
{
  void *p = (void *)wglGetProcAddress(name);
  if(p == 0 ||
     (p == (void*)0x1) || (p == (void*)0x2) || (p == (void*)0x3) ||
     (p == (void*)-1) )
  {
    HMODULE module = LoadLibraryA("opengl32.dll");
    p = (void *)GetProcAddress(module, name);
  }
  
  return p;
}


gl_Funcs gl_load_functions()
{
  gl_Funcs gl = {0};
  
#define load_opengl_proc(name) *(u64 *)&(gl.name) = (u64)get_any_gl_func_address("gl"#name)
  
  load_opengl_proc(BindBuffer);
  load_opengl_proc(GenBuffers);
  load_opengl_proc(BufferData);
  load_opengl_proc(VertexAttribPointer);
  load_opengl_proc(EnableVertexAttribArray);
  load_opengl_proc(CreateShader);
  load_opengl_proc(ShaderSource);
  load_opengl_proc(CompileShader);
  load_opengl_proc(GetShaderiv);
  load_opengl_proc(GetShaderInfoLog);
  load_opengl_proc(CreateProgram);
  load_opengl_proc(AttachShader);
  load_opengl_proc(LinkProgram);
  load_opengl_proc(ValidateProgram);
  load_opengl_proc(DeleteShader);
  load_opengl_proc(UseProgram);
  load_opengl_proc(DebugMessageCallback);
  load_opengl_proc(Enablei);
  load_opengl_proc(DebugMessageControl);
  load_opengl_proc(GetUniformLocation);
  load_opengl_proc(GenVertexArrays);
  load_opengl_proc(BindVertexArray);
  load_opengl_proc(DeleteBuffers);
  load_opengl_proc(DeleteVertexArrays);
  
  load_opengl_proc(Uniform4f);
  load_opengl_proc(Uniform3f);
  load_opengl_proc(Uniform2f);
  load_opengl_proc(Uniform1f);
  
  load_opengl_proc(UniformMatrix2fv);
  load_opengl_proc(UniformMatrix3fv);
  load_opengl_proc(UniformMatrix4fv);
  
  load_opengl_proc(ClearColor);
  load_opengl_proc(Clear);
  load_opengl_proc(DrawArrays);
  load_opengl_proc(DrawElements);
  load_opengl_proc(TexParameteri);
  load_opengl_proc(GenTextures);
  load_opengl_proc(BindTexture);
  load_opengl_proc(TexImage2D);
  load_opengl_proc(GenerateMipmap);
  load_opengl_proc(BlendFunc);
  load_opengl_proc(Enable);
  load_opengl_proc(DeleteTextures);
  
  return gl;
}




PFNWGLSWAPINTERVALEXTPROC wglSwapIntervalEXT;




typedef struct {
  u32 buffer_sample_count;
  u32 samples_per_second;
  WORD single_sample_size;
  WORD channel_count;
  u32 current_sample_index;
  LPDIRECTSOUNDBUFFER direct_buffer;
} win32_Sound;

typedef struct {
  u64 performance_frequency;
  f32 dt;
  b32 running;
  win32_Sound sound;
  game_Sound_Buffer game_sound_buffer;
} win32_State;

win32_State state;



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



String win32_get_work_dir()
{
  String full_path;
  full_path.data = (char *)temp_alloc(MAX_PATH, 4);
  full_path.count = GetModuleFileNameA(0, full_path.data, MAX_PATH);
  assert(full_path.count);
  
  u32 last_slash_index = find_last_index(full_path, const_string("\\"));
  String result = substring(full_path, 0, last_slash_index + 1);
  
  String path_end = const_string("../data/");
  
  result = temp_concat(result, path_end);
  return result;
}

PLATFORM_READ_ENTIRE_FILE(win32_read_entire_file) {
  String path = win32_get_work_dir();
  String full_name = temp_concat(path, file_name);
  char *c_file_name = temp_c_string(full_name);
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
  
  byte *buffer = (byte *)alloc(file_size, 4);
  u32 bytes_read;
  ReadFile(file, buffer, (DWORD)file_size, (LPDWORD)&bytes_read, 0);
  assert(bytes_read == file_size);
  
  CloseHandle(file);
  
  Buffer result;
  result.data = buffer;
  result.size = file_size;
  
  return result;
}

ALLOCATOR(heap_allocator) {
  byte *result = 0;
  
  switch (mode)
  {
    case Allocator_Mode_ALLOC: {
      result = malloc(size);
    } break;
    
    case Allocator_Mode_FREE: {
      free(old_ptr);
    } break;
    
    case Allocator_Mode_REALLOC: {
      result = realloc(old_ptr, size);
    } break;
  }
  
  return result;
}



DWORD win32_sound_get_write_start() {
  win32_Sound win32_sound = state.sound;
  
  DWORD write_start = win32_sound.current_sample_index*win32_sound_get_multisample_size(&win32_sound) % win32_sound_get_buffer_size_in_bytes(&win32_sound);
  
  return write_start;
}

game_Sound_Buffer *win32_request_sound_buffer() {
  win32_Sound *sound = &state.sound;
  game_Sound_Buffer *game_sound_buffer = &state.game_sound_buffer;
  
  DWORD write_start = win32_sound_get_write_start();
  DWORD write_length = 0;
  
  DWORD write_cursor;
  HRESULT got_position = IDirectSoundBuffer_GetCurrentPosition(sound->direct_buffer, null, &write_cursor);
  assert(got_position == DS_OK);
  
  u32 bytes_per_frame = win32_sound_get_bytes_per_second(sound)/60;
  u32 overwrite_bytes = game_sound_buffer->overwrite_count*win32_sound_get_multisample_size(sound);
  
  DWORD target_cursor = (write_cursor + bytes_per_frame + overwrite_bytes) % win32_sound_get_buffer_size_in_bytes(sound);
  
  if (target_cursor == write_start) {
    write_length = 0;
  } else {
    if (target_cursor > write_start) {
      write_length = target_cursor - write_start;
    } else {
      write_length = win32_sound_get_buffer_size_in_bytes(sound) - write_start + target_cursor;
    }
  }
  
  game_sound_buffer->count = write_length/win32_sound_get_multisample_size(sound);
  game_sound_buffer->overwrite_count = sound->samples_per_second/TARGET_FPS;
  assert(game_sound_buffer->count < sound->buffer_sample_count);
  
  return game_sound_buffer;
}



void win32_fill_audio_buffer(win32_Sound *win32_sound, game_Sound_Buffer *src_buffer) {
  DWORD write_start = win32_sound_get_write_start();
  u32 multisample_size = win32_sound_get_multisample_size(win32_sound);
  u32 size_to_lock = src_buffer->count*multisample_size;
  
  i16 *first_region;
  DWORD first_bytes;
  i16 *second_region;
  DWORD second_bytes;
  
  HRESULT is_locked = IDirectSoundBuffer_Lock(win32_sound->direct_buffer, write_start, size_to_lock,
                                              &first_region, &first_bytes,
                                              &second_region, &second_bytes, null);
  assert(is_locked == DS_OK);
  
  u32 first_sample_count = first_bytes/multisample_size;
  u32 second_sample_count = second_bytes/multisample_size;
  assert(first_sample_count + second_sample_count == src_buffer->count);
  
  i16 *src = src_buffer->samples;
  
  for (u32 sample_index = 0; sample_index < first_sample_count; sample_index++) {
    first_region[sample_index*2] = *src++;
    first_region[sample_index*2 + 1] = *src++;
    win32_sound->current_sample_index++;
  }
  
  for (u32 sample_index = 0; sample_index < second_sample_count; sample_index++) {
    second_region[sample_index*2] = *src++;
    second_region[sample_index*2 + 1] = *src++;
    win32_sound->current_sample_index++;
  }
  
  assert(src == src_buffer->samples + src_buffer->count*2);
  
  HRESULT is_unlocked = IDirectSoundBuffer_Unlock(win32_sound->direct_buffer, first_region, first_bytes, second_region, second_bytes);
  assert(is_unlocked == DS_OK);
  
  win32_sound->current_sample_index -= src_buffer->overwrite_count;
}


f64 win32_get_time() {
  LARGE_INTEGER time_li;
  QueryPerformanceCounter(&time_li);
  f64 result = ((f64)time_li.QuadPart/(f64)state.performance_frequency);
  return result;
}


void win32_handle_button(Button *button, b32 is_down) {
  if (button->is_down && !is_down) {
    button->went_up = true;
  } else if (!button->is_down && is_down) {
    button->went_down = true;
  }
  button->is_down = is_down;
}





void APIENTRY opengl_debug_callback(GLenum source,
                                    GLenum type,
                                    GLuint id,
                                    GLenum severity,
                                    GLsizei length,
                                    const GLchar* message,
                                    const void* userParam)
{
  OutputDebugStringA(message);
  __debugbreak();
}

LRESULT CALLBACK WindowProc(HWND window,
                            UINT message,
                            WPARAM wParam,
                            LPARAM lParam) {
  switch (message)
  {
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
  Context heap_ctx = get_context();
  heap_ctx.allocator = heap_allocator;
  File_List result = {0};
  
  push_context(heap_ctx); {
    sb_reserve(result.files, 8, true);
    
    WIN32_FIND_DATAA findData;
    String wildcard = temp_concat(str, const_string("\\*.*"));
    char *wildcard_c = temp_c_string(wildcard);
    HANDLE file = FindFirstFileA(wildcard_c, &findData);
    
    while (file != INVALID_HANDLE_VALUE) {
      if (!c_string_compare(findData.cFileName, ".") &&
          !c_string_compare(findData.cFileName, "..")) {
        
        char *src = findData.cFileName;
        i32 name_length = c_string_length(src);
        char *dst = alloc_array(char, name_length, 4);
        copy_memory_slow(dst, src, name_length);
        
        sb_push(result.files, make_string(dst, name_length));
        result.count++;
      }
      
      b32 nextFileFound = FindNextFileA(file, &findData);
      if (!nextFileFound)
      {
        break;
      }
    }
  } pop_context();
  return result;
}


int CALLBACK WinMain(HINSTANCE instance,
                     HINSTANCE prevInstance,
                     LPSTR commandLine,
                     int showCommandLine) {
  
  LARGE_INTEGER performance_frequency_li;
  QueryPerformanceFrequency(&performance_frequency_li);
  state.performance_frequency = performance_frequency_li.QuadPart;
  
  MMRESULT sheduler_granularity_set = timeBeginPeriod(1);
  assert(sheduler_granularity_set == TIMERR_NOERROR);
  
  printf("Hello, world! %zd\n", sizeof(i64));
  
  Context heap_ctx;
  heap_ctx.allocator = heap_allocator;
  heap_ctx.allocator_data = 0;
  
  Arena scratch = make_temp_memory(malloc(kilobytes(32)), kilobytes(32));
  heap_ctx.temp_memory = &scratch;
  
  push_context(heap_ctx);
  
  
  HMODULE game_lib = LoadLibraryA("game.dll");
  assert(game_lib);
  Game_Update *game_update = (Game_Update *)GetProcAddress(game_lib, "game_update");
  assert(game_update);
  
  
  //global_sound = win32_load_wav(const_string("bloop_00.wav"));
  //global_sound = win32_load_wav(const_string("noragami.wav"));
  
  // NOTE(lvl5): windows init
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
  if (!window) 
  {
    return 0;
  }
  
  // NOTE(lvl5): init openGL
  HDC device_context = GetDC(window);
  
  PIXELFORMATDESCRIPTOR pixel_format_descriptor = {
    sizeof(PIXELFORMATDESCRIPTOR),
    1,
    PFD_DRAW_TO_WINDOW|PFD_SUPPORT_OPENGL|PFD_DOUBLEBUFFER,    // Flags
    PFD_TYPE_RGBA,        // The kind of framebuffer. RGBA or palette.
    32,                   // Colordepth of the framebuffer.
    0, 0, 0, 0, 0, 0,
    0,
    0,
    0,
    0, 0, 0, 0,
    24,                   // Number of bits for the depthbuffer
    8,                    // Number of bits for the stencilbuffer
    0,                    // Number of Aux buffers in the framebuffer.
    PFD_MAIN_PLANE,
    0,
    0, 0, 0
  };
  
  int pixel_format = ChoosePixelFormat(device_context, &pixel_format_descriptor);
  if (!pixel_format) return 0;
  
  if (!SetPixelFormat(device_context, pixel_format, 
                      &pixel_format_descriptor)) return 0;
  
  HGLRC opengl_context = wglCreateContext(device_context);
  if (!opengl_context) return 0;
  
  if (!wglMakeCurrent(device_context, opengl_context)) return 0;
  
  
  gl_Funcs gl = gl_load_functions();
  
  
  wglSwapIntervalEXT = (PFNWGLSWAPINTERVALEXTPROC)wglGetProcAddress("wglSwapIntervalEXT");
  
  b32 interval_set = wglSwapIntervalEXT(1);
  
  glEnable(GL_DEBUG_OUTPUT);
  gl.DebugMessageCallback(opengl_debug_callback, 0);
  GLuint unusedIds = 0;
  gl.DebugMessageControl(GL_DONT_CARE,
                         GL_DONT_CARE,
                         GL_DONT_CARE,
                         0,
                         &unusedIds,
                         true);
  
  
  v2i game_screen = V2i(WINDOW_WIDTH, WINDOW_HEIGHT);
  
  
  // NOTE(lvl5): init sound
  {
    win32_Sound win32_sound;
    win32_sound.current_sample_index = 0;
    win32_sound.samples_per_second = SAMPLES_PER_SECOND;
    win32_sound.channel_count = 2;
    win32_sound.single_sample_size = sizeof(i16);
    win32_sound.buffer_sample_count = win32_sound.samples_per_second;
    
    win32_sound.direct_buffer = win32_init_dsound(window, &win32_sound);
    
    game_Sound_Buffer game_sound_buffer = {0};
    game_sound_buffer.samples = alloc_array(i16, win32_sound.buffer_sample_count*win32_sound.channel_count, win32_sound_get_multisample_size(&win32_sound)*4);
    zero_memory_slow(game_sound_buffer.samples, win32_sound_get_buffer_size_in_bytes(&win32_sound));
    
    
    state.game_sound_buffer = game_sound_buffer;
    state.sound = win32_sound;
  }
  
  game_Memory game_memory;
  game_memory.size = gigabytes(1);
  game_memory.data = alloc(game_memory.size, 8);
  zero_memory_slow(game_memory.data, megabytes(8));
  
  game_Input game_input = {0};
  
  state.running = true;
  state.dt = 0;
  
  f64 last_time = win32_get_time();
  
  Platform platform;
  platform.request_sound_buffer = win32_request_sound_buffer;
  platform.read_entire_file = win32_read_entire_file;
  platform.gl = gl;
  platform.get_files_in_folder = win32_get_files_in_folder;
  
  MSG message;
  while (state.running) {
    f64 time_frame_start = win32_get_time();
    
    for (u32 button_index = 0; 
         button_index < array_count(game_input.buttons);
         button_index++) {
      Button *b = game_input.buttons + button_index;
      b->went_down = false;
      b->went_up = false;
    }
    
    while (PeekMessage(&message, window, 0, 0, PM_REMOVE)) 
    {
      switch (message.message)
      {
        case WM_KEYDOWN:
        case WM_KEYUP:
        case WM_SYSKEYDOWN:
        case WM_SYSKEYUP:
        {
#define KEY_WAS_DOWN_BIT 1 << 30
#define KEY_IS_UP_BIT 1 << 31
          b32 key_is_down = !(message.lParam & KEY_IS_UP_BIT);
          WPARAM key_code = message.wParam;
          
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
            case VK_SPACE:
            win32_handle_button(&game_input.start, key_is_down);
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
    
    
    game_update(game_screen, game_memory, game_input, state.dt, platform);
    
    if (state.game_sound_buffer.count) {
      win32_fill_audio_buffer(&state.sound, &state.game_sound_buffer);
    }
    
    
    f64 current_time = win32_get_time();
    f32 time_used = (f32)(current_time - time_frame_start);
    
    state.dt = (f32)(current_time - last_time);
    if (state.dt > 0.1f) state.dt = 1.0f/TARGET_FPS;
    
    last_time = current_time;
    
    char buffer[256];
    sprintf_s(buffer, 256, "%.4f ms  %d samples\n", time_used*1000.0f, state.game_sound_buffer.count);
    OutputDebugStringA(buffer);
    
    temp_clear();
    SwapBuffers(device_context);
  }
  
  return 0;
}