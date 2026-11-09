#version 460 core

layout (location = 0) in vec2 aPosition;
layout (location = 1) in vec2 aTexCoord;

uniform vec2 uResolution;
uniform vec2 uScale;

out vec2 vTexCoord;

void main() {
  const float aspect = uResolution.x / uResolution.y;
  gl_Position = vec4(aPosition.x * uScale.x, aPosition.y * uScale.y, 0.0, 1.0);
  vTexCoord = aTexCoord;
}