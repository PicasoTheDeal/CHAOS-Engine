#version 430 core
// UnrealBloomPass luminosity high pass
uniform sampler2D tDiffuse;
uniform float uThreshold, uSmoothWidth;
in vec2 vUv; out vec4 fragColor;
void main() {
  vec4 texel = texture(tDiffuse, vUv);
  float v = dot(texel.rgb, vec3(0.299, 0.587, 0.114));
  float alpha = smoothstep(uThreshold, uThreshold + uSmoothWidth, v);
  fragColor = mix(vec4(0.0), texel, alpha);
}
