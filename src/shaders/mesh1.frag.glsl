#version 450

#pragma shader_stage(fragment)

layout (location = 0 ) out vec4 outputColor;

layout (location = 0) in vec4 color;

void main() {
  outputColor = color;
}