#version 430 core
// three.js AfterimageShader with the FilmShader (r163, grayscale off) folded in: the new frame gets its grain here,
// then decays against the old one. one pass where there were two.
uniform sampler2D tOld, tNew;
uniform float uDamp, uTime, uIntensity;
in vec2 vUv; out vec4 fragColor;
vec4 when_gt(vec4 x, float y) { return max(sign(x - y), 0.0); }
float rand(vec2 uv) { float a = 12.9898, b = 78.233, c = 43758.5453; float dt = dot(uv, vec2(a, b)), sn = mod(dt, 3.14); return fract(sin(sn) * c); }
void main() {
  vec4 texelOld = texture(tOld, vUv);
  vec3 base = texture(tNew, vUv).rgb;
  float noise = rand(fract(vUv + uTime));
  vec3 filmed = mix(base, base + base * clamp(0.1 + noise, 0.0, 1.0), uIntensity);
  texelOld *= uDamp * when_gt(texelOld, 0.1);
  fragColor = max(vec4(filmed, 1.0), texelOld);
}
