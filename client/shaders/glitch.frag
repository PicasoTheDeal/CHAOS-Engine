#version 430 core
uniform vec3 uBaseColor; uniform float uTime, uInvert, uBrightness, uGlitchAmt, uSpeed; uniform int uMode;
in vec3 vPos, vNormal;
out vec4 fragColor;
void main() {
  vec3 col = uBaseColor;
  if (uMode == 0) { col = vec3(0.5 + 0.5 * sin(uTime * uSpeed + vNormal.y * 3.0)); }
  else if (uMode == 1) { float r = sin(vPos.x * 20.0 + uTime * 15.0 * uSpeed) * uGlitchAmt; col = uBaseColor + r; }
  else if (uMode == 2) { float angle = atan(vPos.z, vPos.x); float radius = length(vPos.xy); col = vec3(0.5 + 0.5 * sin(radius * 10.0 - angle * 4.0 + uTime * uSpeed)); }
  else if (uMode == 3) { float wave = sin(vPos.x * 5.0 + vPos.y * 5.0 + uTime * uSpeed); col = mix(uBaseColor, 1.0 - uBaseColor, abs(wave) * 0.8); }
  else { float sparkle = sin(vPos.x * 30.0 + vPos.y * 30.0 + uTime * 20.0 * uSpeed); col = mix(uBaseColor, vec3(1.0), 0.7 * abs(sparkle)); }
  col = mix(col, 1.0 - col, uInvert);
  col *= uBrightness;
  fragColor = vec4(col, 0.9);
}
