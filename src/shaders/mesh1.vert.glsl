#version 450

#pragma shader_stage(vertex)

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal;
layout(location = 2) in vec3 texcoord;

layout(location = 0) out vec4 color;

void main() {
  gl_Position = vec4(position + vec3(0, 0, 0.5), 1.0);
  color = vec4(position* 0.5 + vec3(0.5), 1.0);
}
