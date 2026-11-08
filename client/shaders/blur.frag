#version 430 core
// separable gaussian, kernel radius uRadius (3,5,7,9,11 per mip like UnrealBloomPass)
uniform sampler2D tDiffuse;
uniform vec2 uDirection;     // (1/w, 0) or (0, 1/h)
uniform int uRadius;
in vec2 vUv; out vec4 fragColor;
float gauss(float x, float sigma) { return 0.39894 * exp(-0.5 * x * x / (sigma * sigma)) / sigma; }
void main() {
  float sigma = float(uRadius);
  float w0 = gauss(0.0, sigma);
  vec3 sum = texture(tDiffuse, vUv).rgb * w0;
  float wsum = w0;
  for (int i = 1; i < uRadius; i++) {
    float x = float(i), w = gauss(x, sigma);
    vec2 o = uDirection * x;
    sum += (texture(tDiffuse, vUv + o).rgb + texture(tDiffuse, vUv - o).rgb) * w;
    wsum += 2.0 * w;
  }
  fragColor = vec4(sum / wsum, 1.0);
}
