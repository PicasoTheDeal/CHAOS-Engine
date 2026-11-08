#version 430 core
layout(location = 0) in vec3 aPos;
uniform mat4 uProj, uModelView;
void main() { gl_Position = uProj * uModelView * vec4(aPos, 1.0); }
