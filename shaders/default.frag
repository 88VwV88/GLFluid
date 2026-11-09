#version 460 core

in  vec2 TexCoord;
out vec4 fragColor;

uniform sampler2D uQuadTexture;

void main() {
  fragColor = texture(uQuadTexture, TexCoord);
}