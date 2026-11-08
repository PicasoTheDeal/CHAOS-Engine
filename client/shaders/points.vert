#version 430 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in float aBright;
uniform mat4 uProj, uModelView;
uniform float uSize, uViewportH;
out float vBright;
void main() {
  vec4 vp = uModelView * vec4(aPos, 1.0);
  gl_Position = uProj * vp;
  gl_PointSize = max(1.0, uSize * uViewportH * 0.5 / max(-vp.z, 0.1));   // sizeAttenuation
  vBright = aBright;
}
