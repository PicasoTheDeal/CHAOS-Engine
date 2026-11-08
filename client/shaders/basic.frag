#version 430 core
uniform vec3 uColor; uniform float uAlpha;
out vec4 fragColor;
void main() { fragColor = vec4(uColor, uAlpha); }
