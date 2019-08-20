#shader vertex
#version 330 core

layout (location = 0) in vec3 v_pos;

layout (location = 1) in mat4x4 inst_model;
layout (location = 5) in vec2 inst_tex_offset;
layout (location = 6) in vec2 inst_tex_scale;
layout (location = 7) in vec4 inst_color;

out vec2 fr_tex_coord;
out vec4 fr_color;

uniform mat4x4 u_view;
uniform mat4x4 u_projection;

void main() {
  vec4 p = u_projection*u_view*inst_model*vec4(v_pos, 1.0f);
  gl_Position = p;
  
  fr_tex_coord = inst_tex_offset + v_pos.xy*inst_tex_scale;
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
