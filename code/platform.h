#ifndef PLATFORM_H
#include "lvl5_string.h"
#include "lvl5_math.h"
#include "lvl5_opengl.h"

#define SAMPLES_PER_SECOND 44100

#define complete_past_writes_before_future_writes() _WriteBarrier()
#define complete_past_reads_before_future_reads() _ReadBarrier()



typedef struct {
  i16 *samples;
  i32 count;
  i32 overwrite_count;
} Sound_Buffer;

typedef struct {
  bool is_down;
  bool went_down;
  bool went_up;
  bool pressed;
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
  
  Button keys[200];
  
  char char_code;
} Input;


typedef struct {
  b32 is_reloaded;
  b32 window_resized;
  
  byte *perm;
  u64 perm_size;
  
  byte *temp;
  u64 temp_size;
  
  byte *debug;
  u64 debug_size;
} Memory;

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


#define PLATFORM_REQUEST_SOUND_BUFFER(name) Sound_Buffer *name()
typedef PLATFORM_REQUEST_SOUND_BUFFER(Platform_Request_Sound_Buffer);


typedef struct {
  String *files;
  i32 count;
} File_List;

#define PLATFORM_GET_FILES_IN_FOLDER(name) File_List name(String dir_name)
typedef PLATFORM_GET_FILES_IN_FOLDER(Platform_Get_Files_In_Folder);

typedef struct File *File_Handle;

#define PLATFORM_OPEN_FILE(name) File_Handle name(String file_name)
typedef PLATFORM_OPEN_FILE(Platform_Open_File);

#define PLATFORM_FILE_ERROR(name) void name(File_Handle file)
typedef PLATFORM_FILE_ERROR(Platform_File_Error);

#define PLATFORM_FILE_HAS_NO_ERRORS(name) b32 name(File_Handle file)
typedef PLATFORM_FILE_HAS_NO_ERRORS(Platform_File_Has_No_Errors);

#define PLATFORM_READ_FILE(name) void name(File_Handle file, void *dst, u64 size, u64 offset)
typedef PLATFORM_READ_FILE(Platform_Read_File);

#define PLATFORM_CLOSE_FILE(name) void name(File_Handle file)
typedef PLATFORM_CLOSE_FILE(Platform_Close_File);

#define WORKER_FN(name) void name(void *data)
typedef WORKER_FN(Worker_Fn);

typedef struct {
  Worker_Fn *fn;
  void *data;
} Work_Queue_Entry;


typedef struct _Work_Queue *Work_Queue;

#define PLATFORM_ADD_WORK_QUEUE_ENTRY(name) void name(Work_Queue queue_ptr, Worker_Fn *fn, void *data)
typedef PLATFORM_ADD_WORK_QUEUE_ENTRY(Platform_Add_Work_Queue_Entry);

#define PLATFORM_GET_TIME(name) f64 name()
typedef PLATFORM_GET_TIME(Platform_Get_Time);

typedef struct {
  Platform_Get_Time *get_time;
  Platform_Read_Entire_File *read_entire_file;
  Platform_Request_Sound_Buffer *request_sound_buffer;
  gl_Funcs gl;
  Platform_Get_Files_In_Folder *get_files_in_folder;
  Platform_Open_File *open_file;
  Platform_File_Error *file_error;
  Platform_File_Has_No_Errors *file_has_no_errors;
  Platform_Read_File *read_file;
  Platform_Close_File *close_file;
  Platform_Add_Work_Queue_Entry *add_work_queue_entry;
  Work_Queue high_queue;
  Work_Queue low_queue;
} Platform;


globalvar gl_Funcs gl;
globalvar Platform platform;

#define GAME_UPDATE(name) void name(v2 screen_size, Memory memory, \
Input input, f32 dt, Platform _platform)
typedef GAME_UPDATE(Game_Update);


#define PLATFORM_H
#endif