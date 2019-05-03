#ifndef LVL5_OPENGL_H

#include "lvl5_string.h"
#define APIENTRY __stdcall
#define WINGDIAPI __declspec(dllimport)

#include <GL/gl.h>
#include "KHR/glext.h"

typedef void FNGLCLEARPROC(u32 buffer_bit);
typedef void FNGLCLEARCOLORPROC(f32 r, f32 g, f32 b, f32 a);
typedef void FNGLDRAWARRAYSPROC(GLenum mode, GLint first, GLsizei count);
typedef void FNGLDRAWELEMENTSPROC(GLenum mode, GLsizei count, GLenum type, GLvoid *indices);

typedef void FNGLTEXPARAMETERI(GLenum thing, GLenum param, GLint value);
typedef void FNGLTEXPARAMETERFV(GLenum thing, GLenum param, GLfloat *value);
typedef void FNGLGENTEXTURES(GLint count, GLuint *id);
typedef void FNGLBINDTEXTURE(GLenum type, GLuint id);
typedef void FNGLTEXIMAGE2D(GLenum target,
                            GLint level,
                            GLint internalFormat,
                            GLsizei width,
                            GLsizei height,
                            GLint border,
                            GLenum format,
                            GLenum type,
                            const GLvoid *data);

typedef void FNGLGENERATEMIPMAPPROC(GLenum thing);
typedef void FNGLENABLEPROC(GLenum thing);
typedef void FNGLBLENDFUNCPROC(GLenum src, GLenum dst);
typedef void FNGLDELETETEXTURESPROC(GLsizei n, GLuint *textures);

typedef struct {
  PFNGLGENBUFFERSPROC GenBuffers;
  PFNGLBINDBUFFERPROC BindBuffer;
  PFNGLBUFFERDATAPROC BufferData;
  PFNGLVERTEXATTRIBPOINTERPROC VertexAttribPointer;
  PFNGLENABLEVERTEXATTRIBARRAYPROC EnableVertexAttribArray;
  PFNGLCREATESHADERPROC CreateShader;
  PFNGLSHADERSOURCEPROC ShaderSource;
  PFNGLCOMPILESHADERPROC CompileShader;
  PFNGLGETSHADERIVPROC GetShaderiv;
  PFNGLGETSHADERINFOLOGPROC GetShaderInfoLog;
  PFNGLCREATEPROGRAMPROC CreateProgram;
  PFNGLATTACHSHADERPROC AttachShader;
  PFNGLLINKPROGRAMPROC LinkProgram;
  PFNGLVALIDATEPROGRAMPROC ValidateProgram;
  PFNGLDELETESHADERPROC DeleteShader;
  PFNGLUSEPROGRAMPROC UseProgram;
  PFNGLUSESHADERPROGRAMEXTPROC UseShaderProgramEXT;
  PFNGLDEBUGMESSAGECALLBACKPROC DebugMessageCallback;
  PFNGLENABLEIPROC Enablei;
  PFNGLDEBUGMESSAGECONTROLPROC DebugMessageControl;
  PFNGLGETUNIFORMLOCATIONPROC GetUniformLocation;
  PFNGLGENVERTEXARRAYSPROC GenVertexArrays;
  PFNGLBINDVERTEXARRAYPROC BindVertexArray;
  PFNGLDELETEBUFFERSPROC DeleteBuffers;
  PFNGLDELETEVERTEXARRAYSPROC DeleteVertexArrays;
  PFNGLVERTEXATTRIBDIVISORPROC VertexAttribDivisor;
  PFNGLDRAWARRAYSINSTANCEDPROC DrawArraysInstanced;
  
  PFNGLUNIFORM4FPROC Uniform4f;
  PFNGLUNIFORM3FPROC Uniform3f;
  PFNGLUNIFORM2FPROC Uniform2f;
  PFNGLUNIFORM1FPROC Uniform1f;
  PFNGLUNIFORMMATRIX2FVPROC UniformMatrix2fv;
  PFNGLUNIFORMMATRIX3FVPROC UniformMatrix3fv;
  PFNGLUNIFORMMATRIX4FVPROC UniformMatrix4fv;
  
  FNGLCLEARCOLORPROC *ClearColor;
  FNGLCLEARPROC *Clear;
  FNGLDRAWARRAYSPROC *DrawArrays;
  FNGLDRAWELEMENTSPROC *DrawElements;
  FNGLTEXPARAMETERI *TexParameteri;
  FNGLGENTEXTURES *GenTextures;
  FNGLBINDTEXTURE *BindTexture;
  FNGLTEXIMAGE2D *TexImage2D;
  FNGLTEXPARAMETERFV *TexParameterfv;
  FNGLGENERATEMIPMAPPROC *GenerateMipmap;
  FNGLENABLEPROC *Enable;
  FNGLBLENDFUNCPROC *BlendFunc;
  FNGLDELETETEXTURESPROC *DeleteTextures;
  
} gl_Funcs;

typedef struct {
  String vertex;
  String fragment;
} gl_Parse_Result;

gl_Parse_Result gl_parse_glsl(String src)
{
  String vertex_search_string = const_string("#shader vertex");
  i32 vertex_index = find_index(src, vertex_search_string);
  String fragment_search_string = const_string("#shader fragment");
  i32 fragment_index = find_index(src, fragment_search_string);
  
  gl_Parse_Result result;
  result.vertex = make_string(src.data + vertex_index +
                              vertex_search_string.count, 
                              fragment_index - vertex_index - 
                              vertex_search_string.count);
  result.fragment = make_string(src.data + fragment_index + 
                                fragment_search_string.count,
                                src.count - fragment_index - 
                                fragment_search_string.count);
  
  return result;
}

u32 gl_compile_shader(Arena *arena, gl_Funcs gl, u32 type, String src)
{
  u32 id = gl.CreateShader(type);
  gl.ShaderSource(id, 1, &src.data, (i32 *)&src.count);
  gl.CompileShader(id);
  
  i32 compile_status;
  gl.GetShaderiv(id, GL_COMPILE_STATUS, &compile_status);
  if (compile_status == GL_FALSE)
  {
    i32 length;
    gl.GetShaderiv(id, GL_INFO_LOG_LENGTH, &length);
    char *message = arena_push_array(arena, char, length);
    gl.GetShaderInfoLog(id, length, &length, message);
    assert(false);
  }
  
  return id;
}

u32 gl_create_shader(Arena *arena, gl_Funcs gl, String vertex_src, String fragment_src)
{
  u32 program = gl.CreateProgram();
  u32 vertex_shader = gl_compile_shader(arena, gl, GL_VERTEX_SHADER, vertex_src);
  u32 fragment_shader = gl_compile_shader(arena, gl, GL_FRAGMENT_SHADER, fragment_src);
  
  gl.AttachShader(program, vertex_shader);
  gl.AttachShader(program, fragment_shader);
  gl.LinkProgram(program);
  gl.ValidateProgram(program);
  
  gl.DeleteShader(vertex_shader);
  gl.DeleteShader(fragment_shader);
  
  return program;
}

void gl_set_uniform_mat3x3(gl_Funcs gl, GLint program, char *name, mat3x3 *value, u32 count) {
  GLint uniform_loc = gl.GetUniformLocation(program, name);
  gl.UniformMatrix3fv(uniform_loc, count, GL_TRUE, (GLfloat *)value);
}


void gl_set_uniform_mat4x4(gl_Funcs gl, GLint program, char *name, mat4x4 *value, u32 count) {
  GLint uniform_loc = gl.GetUniformLocation(program, name);
  gl.UniformMatrix4fv(uniform_loc, count, GL_TRUE, (GLfloat *)value);
}


void gl_set_uniform_v4(gl_Funcs gl, GLint program, char *name, v4 value) {
  GLint uniform_loc = gl.GetUniformLocation(program, name);
  gl.Uniform4f(uniform_loc, value.x, value.y, value.z, value.w);
}

#define LVL5_OPENGL_H
#endif