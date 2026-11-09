#version 460 core

layout (triangles) in;
layout (triangle_strip, max_vertices = 3) out;

in  vec2 vTexCoord[];
out vec2 TexCoord;

void main() {
  gl_Position = gl_in[0].gl_Position;
  TexCoord = vTexCoord[0];
  EmitVertex();
  gl_Position = gl_in[1].gl_Position;
  TexCoord = vTexCoord[1];
  EmitVertex();
  gl_Position = gl_in[2].gl_Position;
  TexCoord = vTexCoord[2];
  EmitVertex();
}