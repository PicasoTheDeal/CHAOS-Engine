#version 430 core
uniform sampler2D tDiffuse;
in vec2 vUv; out vec4 fragColor;
void main() { fragColor = texture(tDiffuse, vUv); }
