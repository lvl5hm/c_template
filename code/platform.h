#ifndef PLATFORM_H
#include "lvl5_string.h"
#include "lvl5_math.h"
#include "lvl5_opengl.h"

#define SAMPLES_PER_SECOND 48000



typedef struct {
  i16 *samples;
  u32 count;
  u32 overwrite_count;
} game_Sound_Buffer;

typedef struct {
  i16 *samples[2];
  u32 count;
} Sound;

typedef struct {
  b32 is_down;
  b32 went_down;
  b32 went_up;
} Button;

typedef struct {
  v2 p;
  Button left;
  Button right;
} Mouse;

typedef struct {
  Mouse mouse;
  
  union {
    Button buttons[5];
    struct {
      Button move_left;
      Button move_right;
      Button move_up;
      Button move_down;
      Button start;
    };
  };
} game_Input;


typedef struct {
  byte *data;
  u64 size;
} game_Memory;

typedef struct {
  byte *data;
  u64 size;
} Buffer;

String buffer_to_string(Buffer b) {
  String result = make_string((char *)b.data, (u32)b.size);
  return result;
}

#define PLATFORM_READ_ENTIRE_FILE(name) Buffer name(String file_name)
typedef PLATFORM_READ_ENTIRE_FILE(Platform_Read_Entire_File);


#define PLATFORM_REQUEST_SOUND_BUFFER(name) game_Sound_Buffer *name()
typedef PLATFORM_REQUEST_SOUND_BUFFER(Platform_Request_Sound_Buffer);


typedef struct {
  String *files;
  i32 count;
} File_List;

#define PLATFORM_GET_FILES_IN_FOLDER(name) File_List name(String dir_name)
typedef PLATFORM_GET_FILES_IN_FOLDER(Platform_Get_Files_In_Folder);

typedef struct {
  Platform_Read_Entire_File *read_entire_file;
  Platform_Request_Sound_Buffer *request_sound_buffer;
  gl_Funcs gl;
  Platform_Get_Files_In_Folder *get_files_in_folder;
} Platform;



#define GAME_UPDATE(name) void name(v2i screen_size, game_Memory memory, game_Input input, f32 dt, Platform platform)

typedef GAME_UPDATE(Game_Update);


#define PLATFORM_H
#endif