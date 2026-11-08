#version 430 core
// a model's own look: uv and colour ride along. a mesh without them leaves the attributes disabled and the
// generic values below stand in (uv 0, colour white with weight 1)
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUv;
layout(location = 3) in vec4 aColor;
uniform mat4 uProj, uModelView;
uniform mat3 uNormalMat;
out vec3 vViewPos, vNormal; out vec2 vUv; out vec4 vColor;
void main() {
  vec4 vp = uModelView * vec4(aPos, 1.0);
  vViewPos = vp.xyz;
  vNormal = normalize(uNormalMat * aNormal);
  vUv = aUv; vColor = aColor;
  gl_Position = uProj * vp;
}
