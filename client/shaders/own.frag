#version 430 core
// the model's own material: base colour x vertex colour x (texture, weighted per vertex), rough / metallic blinn-phong
// under the dimension's three lights, its glow, its fog. same light rig as phong.frag so a model sits in the scene
uniform vec3 uBase, uEmissive, uAmbient;
uniform float uRoughness, uMetallic;
uniform int uHasTex; uniform sampler2D uTex;
uniform vec3 uLightDirV[3];
uniform vec3 uLightCol[3];
uniform vec3 uFogColor; uniform float uFogNear, uFogFar;
in vec3 vViewPos, vNormal; in vec2 vUv; in vec4 vColor;
out vec4 fragColor;
void main() {
  vec3 albedo = uBase * vColor.rgb;
  if (uHasTex == 1) albedo *= mix(vec3(1.0), texture(uTex, vUv).rgb, vColor.a);
  vec3 N = normalize(gl_FrontFacing ? vNormal : -vNormal);
  vec3 V = normalize(-vViewPos);
  const float PI = 3.141593;
  float r = clamp(uRoughness, 0.05, 1.0);
  float shin = clamp(2.0 / (r * r * r * r) - 2.0, 4.0, 400.0);
  vec3 F0 = mix(vec3(0.04), albedo, uMetallic);
  vec3 diff = albedo * (1.0 - uMetallic);
  vec3 col = uEmissive + diff * uAmbient / PI + F0 * uAmbient * uMetallic * 0.5;
  for (int i = 0; i < 3; i++) {
    float nl = max(dot(N, uLightDirV[i]), 0.0);
    vec3 H = normalize(uLightDirV[i] + V);
    float spec = pow(max(dot(N, H), 0.0), shin) * (shin + 2.0) / (8.0 * PI);
    float fw = pow(1.0 - max(dot(H, V), 0.0), 5.0);
    vec3 F = F0 + (1.0 - F0) * fw;
    col += uLightCol[i] * nl * (diff / PI + spec * F);
  }
  float f = clamp((length(vViewPos) - uFogNear) / (uFogFar - uFogNear), 0.0, 1.0);
  col = mix(col, uFogColor, f);
  fragColor = vec4(col, 0.0);     // alpha 0: the duotone pass leaves this pixel its real colour (post true colour mode)
}
