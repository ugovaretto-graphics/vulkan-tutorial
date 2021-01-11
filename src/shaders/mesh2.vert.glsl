#version 450

#pragma shader_stage(vertex)

struct Vertex {
  float vx, vy, vz;
  float nx, ny, nz;
  float tu, tv;
};

layout(binding = 0) readonly buffer Vertices
{
  Vertex vertices[];
};

layout(location = 0) out vec4 color; 

void main() {
  Vertex v = vertices[gl_VertexIndex];
  vec3 position = vec3(v.vx, v.vy, v.vz);
  vec3 ormal = vec3(v.nx, v.ny, v.nz);
  vec2 texCoord = vec2(v.tu, v.tv);
  gl_Position = vec4(position + vec3(0, 0, 0.5), 1.0);
  color = vec4(position * 0.5 + vec3(0.5), 1.0);
}
