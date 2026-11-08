#version 430 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
uniform mat4 uProj, uModelView;
uniform mat3 uNormalMat;
out vec3 vViewPos, vNormal;
void main() {
  vec4 vp = uModelView * vec4(aPos, 1.0);
  vViewPos = vp.xyz;
  vNormal = normalize(uNormalMat * aNormal);
  gl_Position = uProj * vp;
}
