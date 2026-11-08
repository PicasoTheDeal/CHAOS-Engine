#version 430 core
uniform vec3 uColorA, uColorB;
uniform float uTime, uSeed, uDensity;
uniform int uOctaves;     // 5 in full detail, 3 in lite: the two hyperspace fields are the whole-screen cost
in vec3 vPos;
out vec4 fragColor;

mat2 rot(float a) { float c = cos(a), s = sin(a); return mat2(c, -s, s, c); }

float hyperspace(vec2 p, float t) {
  float accum = 0.0;
  float amp = 0.5;
  vec2 shift = vec2(100.0);
  for (int i = 0; i < uOctaves; i++) {
    p = rot(1.2 + uSeed * 0.0001) * p * 1.4 + shift;
    p += sin(p.yx * 2.0 + t * 0.3) * 0.6;
    accum += sin(p.x + p.y) * amp;
    amp *= 0.5;
  }
  return accum;
}

void main() {
  vec3 dir = normalize(vPos);
  vec2 uv = dir.xy / (1.0 + abs(dir.z));
  uv *= uDensity;
  float t = uTime * 0.15;
  vec2 p = uv + vec2(sin(t * 0.2), cos(t * 0.3)) * 0.5;
  float warp1 = hyperspace(p, t);
  float warp2 = hyperspace(p * 1.5 - warp1, -t * 0.8);
  float pattern = sin(warp1 * 4.0 + warp2 * 3.0 + t) * 0.5 + 0.5;
  float structure = cos(warp2 * 6.0 - warp1 * 2.0 - t * 0.5) * 0.5 + 0.5;
  float v = smoothstep(0.1, 0.9, pattern) * 0.65 + structure * 0.35;
  float grid = abs(sin(uv.x * 10.0 + warp1 * 3.0) * cos(uv.y * 20.0 + warp2 * 3.0));
  grid = pow(grid, 4.0) * 0.25 * (0.5 + 0.5 * sin(t));
  float vignette = 1.0 - 0.4 * length(dir.xy);
  vec3 col = mix(uColorA * 0.12, uColorA * 0.5, v);
  col = mix(col, uColorB * 0.55, smoothstep(0.75, 1.0, v) * 0.6 + grid);
  fragColor = vec4(col * vignette, 1.0);
}
