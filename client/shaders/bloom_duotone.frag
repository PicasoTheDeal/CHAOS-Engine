#version 430 core
// one pass: scene + the five bloom mips (UnrealBloomPass weights), then the whole frame's luminance onto the
// 4-stop ramp built only from A and B. used to be two passes with a 16-bit frame between them.
uniform sampler2D tScene, tBlur0, tBlur1, tBlur2, tBlur3, tBlur4;
uniform float uStrength, uRadius;
uniform vec3 uColA, uColB, uColC;
uniform int uTritone;
uniform float uExposure, uContrast, uSplit, uGrainT, uGrain, uKick;
uniform int uTrueColor;        // 1: scene alpha 0 marks pixels that keep their real colour (a model's own textures)
in vec2 vUv; out vec4 fragColor;
float lerpFactor(float f) { return mix(f, 1.2 - f, uRadius); }
float hash12(vec2 p) { vec3 p3 = fract(vec3(p.xyx) * 0.1031); p3 += dot(p3, p3.yzx + 33.33); return fract((p3.x + p3.y) * p3.z); }
void main() {
  vec3 bloom = lerpFactor(1.0) * texture(tBlur0, vUv).rgb
             + lerpFactor(0.8) * texture(tBlur1, vUv).rgb
             + lerpFactor(0.6) * texture(tBlur2, vUv).rgb
             + lerpFactor(0.4) * texture(tBlur3, vUv).rgb
             + lerpFactor(0.2) * texture(tBlur4, vUv).rgb;
  vec4 scene = texture(tScene, vUv);
  vec3 src = (scene.rgb + uStrength * bloom) * uExposure;
  src = (src * (2.51 * src + 0.03)) / (src * (2.43 * src + 0.59) + 0.14);   // ACES fit
  float keep = uTrueColor == 1 ? 1.0 - clamp(scene.a, 0.0, 1.0) : 0.0;
  float lum = dot(clamp(src, 0.0, 1.0), vec3(0.2126, 0.7152, 0.0722));
  lum = pow(lum, 1.0 / 1.8);
  lum = clamp((lum - 0.5) * uContrast + 0.5, 0.0, 1.0);
  lum += (hash12(vUv * 1731.0 + uGrainT) - 0.5) * uGrain;
  lum = clamp(lum, 0.0, 1.0);
  vec3 k0 = uColA * 0.05, k1 = uColA, k2 = uColB, k3 = mix(uColB, vec3(1.0), 0.35);
  float s = uSplit;
  vec3 col;
  float hi = s + (1.0 - s) * 0.55;
  if (uTritone == 1) {
    // rare and up: C sits between A and B, five stops instead of four
    if (lum < s * 0.75) col = mix(k0, k1, smoothstep(0.0, s * 0.75, lum));
    else if (lum < s) col = mix(k1, uColC, smoothstep(s * 0.75, s, lum));
    else if (lum < hi) col = mix(uColC, k2, smoothstep(s, hi, lum));
    else col = mix(k2, k3, smoothstep(hi, 1.0, lum));
  } else {
    if (lum < s * 0.75) col = mix(k0, k1, smoothstep(0.0, s * 0.75, lum));
    else if (lum < hi) col = mix(k1, k2, smoothstep(s * 0.75, hi, lum));
    else col = mix(k2, k3, smoothstep(hi, 1.0, lum));
  }
  col = mix(col, uColB, uKick * uKick * 0.25);
  col = mix(col, clamp(src * 1.15, 0.0, 1.0), keep);
  fragColor = vec4(col, 1.0);
}
