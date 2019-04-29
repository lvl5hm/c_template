#shader vertex
#version 330 core

layout (location = 0) in vec3 pos;
layout (location = 1) in mat4x4 model;
layout (location = 5) in vec2 tex_offset;
layout (location = 6) in vec2 tex_scale;
layout (location = 7) in vec4 color;

out vec2 tex_coord;
out vec4 v_color;

uniform mat4x4 u_view;

void main() {
  mat4x4 u_model = transpose(model);
  vec4 p = u_view*u_model*vec4(pos, 1.0f);
  tex_coord = tex_offset + pos.xy*tex_scale;
  
  v_color = color;
  gl_Position = p;
}




#shader fragment
#version 330 core


in vec2 tex_coord;
in vec4 v_color;

uniform sampler2D texture_image;

out vec4 FragColor;

void main() {
  FragColor = texture(texture_image, tex_coord)*v_color;
}
