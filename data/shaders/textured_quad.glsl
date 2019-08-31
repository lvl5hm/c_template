#shader vertex
#version 330 core

layout (location = 0) in vec3 v_pos;

layout (location = 1) in mat4x4 inst_model;
layout (location = 5) in vec4 inst_tex;
layout (location = 6) in vec4 inst_color;

out vec2 fr_tex_coord;
out vec4 fr_color;

uniform mat4x4 u_view;
uniform mat4x4 u_projection;
uniform sampler2D texture_image;

void main() {
  mat4x4 u_model = transpose(inst_model);
  vec4 p = u_projection*u_view*u_model*vec4(v_pos, 1.0f);
  gl_Position = p;
  
  ivec2 atlas_size = textureSize(texture_image, 0);
  
  vec2 tex_pos = vec2(inst_tex.x/atlas_size.x, inst_tex.y/atlas_size.y);
  vec2 tex_size = vec2(inst_tex.z/atlas_size.x, inst_tex.w/atlas_size.y);
  
  fr_tex_coord = tex_pos + v_pos.xy*tex_size;
  fr_color = inst_color;
}




#shader fragment
#version 330 core


in vec2 fr_tex_coord;
in vec4 fr_color;

uniform sampler2D texture_image;

out vec4 FragColor;

void main() {
  FragColor = texture(texture_image, fr_tex_coord)*fr_color;
}
