#shader vertex
#version 330 core

layout (location = 0) in vec2 pos;
layout (location = 1) in mat3x3 model;
layout (location = 4) in vec2 tex_offset;
layout (location = 5) in vec2 tex_scale;
layout (location = 6) in vec4 color;
layout (location = 7) in vec2 origin;

out vec2 tex_coord;
out vec4 v_color;

uniform mat3x3 u_view;

void main() {
  vec3 p = u_view*transpose(model)*vec3(pos-origin, 1.0f);
  tex_coord = tex_offset + pos*tex_scale;
  
  v_color = color;
  gl_Position = vec4(p, 1.0);
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
