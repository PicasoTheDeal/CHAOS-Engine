#version 430 core
uniform float uOpacity;
in float vBright;
out vec4 fragColor;
void main() {
  vec2 d = gl_PointCoord - 0.5;
  if (dot(d, d) > 0.25) discard;
  fragColor = vec4(vec3(vBright), uOpacity);
}
