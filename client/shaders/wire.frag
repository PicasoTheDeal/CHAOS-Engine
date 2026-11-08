#version 430 core
uniform float uAlpha;
out vec4 fragColor;
void main() { fragColor = vec4(1.0, 1.0, 1.0, uAlpha); }
