#version 430 core
// MeshPhongMaterial stand-in: 3 directional lights + ambient + emissive + linear fog, double sided
uniform vec3 uColor, uEmissive, uAmbient;
uniform vec3 uLightDirV[3];     // view space, towards the light
uniform vec3 uLightCol[3];
uniform float uShininess, uOpacity;
uniform vec3 uFogColor; uniform float uFogNear, uFogFar;
in vec3 vViewPos, vNormal;
out vec4 fragColor;
void main() {
  vec3 N = normalize(gl_FrontFacing ? vNormal : -vNormal);
  vec3 V = normalize(-vViewPos);
  const float PI = 3.141593;
  vec3 col = uEmissive + uColor * uAmbient / PI;          // BRDF_Lambert: albedo / pi
  for (int i = 0; i < 3; i++) {
    float nl = max(dot(N, uLightDirV[i]), 0.0);
    vec3 H = normalize(uLightDirV[i] + V);
    float spec = pow(max(dot(N, H), 0.0), uShininess) * (uShininess + 2.0) / (8.0 * PI);   // BRDF_BlinnPhong, specular 0.11 like the three default
    col += uLightCol[i] * nl * (uColor / PI + spec * 0.11);
  }
  float f = clamp((length(vViewPos) - uFogNear) / (uFogFar - uFogNear), 0.0, 1.0);
  col = mix(col, uFogColor, f);
  fragColor = vec4(col, uOpacity);
}
