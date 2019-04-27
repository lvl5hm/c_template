#shader vertex
#version 330 core

layout (location = 0) in vec2 pos;
layout (location = 1) in vec2 v_tex_coord;
layout (location = 2) in vec4 color;
layout (location = 3) in float transform_index;

out vec2 tex_coord;
out vec4 v_color;

uniform mat3x3 u_view;
uniform mat3x3 u_model_arr[32];

void main() {
  mat3x3 u_model = u_model_arr[int(transform_index)];
  
  vec3 p = u_view*u_model*vec3(pos, 1.0f);
  gl_Position = vec4(p, 1.0);
  tex_coord = v_tex_coord;
  v_color = color;
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
