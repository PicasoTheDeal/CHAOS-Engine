#version 430 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
uniform mat4 uProj, uModelView;
uniform mat3 uNormalMat;
out vec3 vPos, vNormal;
void main() { vPos = aPos; vNormal = normalize(uNormalMat * aNormal); gl_Position = uProj * uModelView * vec4(aPos, 1.0); }
