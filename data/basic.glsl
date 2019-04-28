#shader vertex
#version 330 core

layout (location = 0) in vec3 pos;
layout (location = 1) in mat4x4 model;
layout (location = 5) in vec2 tex_offset;
layout (location = 6) in vec2 tex_scale;
layout (location = 7) in vec4 color;
layout (location = 8) in vec2 origin;

out vec2 tex_coord;
out vec4 v_color;

uniform mat4x4 u_view;

void main() {
  vec4 p = u_view*transpose(model)*vec4(pos-vec3(origin, 0), 1.0f);
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
  //FragColor = vec4(1, 1, 1, 1);
}
