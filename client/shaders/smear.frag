#version 430 core
// bleed only. chromatic split along the kick direction plus a short directional smear.
// the split never fully closes: bleed always bleeds a little, and it tears open when a palette lands.
uniform sampler2D tDiffuse;
uniform float uTime, uKick, uAmount;
in vec2 vUv; out vec4 fragColor;
void main() {
  float a = uTime * 0.37;
  vec2 dir = vec2(cos(a), sin(a));
  float split = (0.0015 + 0.012 * uKick * uKick) * uAmount;
  vec2 d = dir * split;
  vec3 col;
  col.r = texture(tDiffuse, vUv + d).r;
  col.g = texture(tDiffuse, vUv).g;
  col.b = texture(tDiffuse, vUv - d).b;
  // smear: a few taps dragged along the same direction, weighted toward the centre
  vec3 sm = vec3(0.0);
  float w = 0.0;
  for (int i = -3; i <= 3; i++) {
    float k = 1.0 - abs(float(i)) / 4.0;
    sm += texture(tDiffuse, vUv + d * float(i) * 2.5).rgb * k;
    w += k;
  }
  sm /= w;
  col = mix(col, sm, 0.35 * uAmount * (0.3 + 0.7 * uKick));
  fragColor = vec4(col, 1.0);
}
