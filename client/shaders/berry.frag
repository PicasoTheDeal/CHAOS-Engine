#version 430 core
uniform float uTime;
uniform vec3 uAlbedo, uAlbedo2;
uniform vec3 uTransColor;
uniform float uTransDepth;
uniform vec3 uBackdrop;
uniform vec3 uCamM;
uniform vec3 uLightDir[3];
uniform vec3 uLightCol[3];
uniform vec3 uAmbient;
uniform float uBead, uSeeThrough, uSeedAmt, uSSS;
uniform float uRoughness, uF0, uSpecular;
uniform float uSheen; uniform vec3 uSheenColor;
uniform float uPoreScale, uPoreStrength;
uniform float uBloom, uBloomScale;
uniform float uCellScale, uDomeR;
uniform float uPulse;
uniform int uLite;      // 1: no pore normal detail, no wax: ten fewer noise evaluations per pixel
in vec3 vModelPos, vNormalModel, vCenter;
in vec4 vMask;
in float vDisplacement;
out vec4 fragColor;
#include <berry_common.glsl>
#define saturate(a) clamp(a, 0.0, 1.0)
const float PI = 3.141593;

// Karis' analytic fit of the split-sum environment BRDF
vec2 envBRDF(float roughness, float NoV) {
  const vec4 c0 = vec4(-1.0, -0.0275, -0.572, 0.022);
  const vec4 c1 = vec4(1.0, 0.0425, 1.04, -0.04);
  vec4 r = roughness * c0 + c1;
  float a004 = min(r.x * r.x, exp2(-9.28 * NoV)) * r.x + r.y;
  return vec2(-1.04, 1.04) * a004 + r.zw;
}
float D_GGX(float NoH, float a) { float a2 = a * a; float d = NoH * NoH * (a2 - 1.0) + 1.0; return a2 / (PI * d * d); }
float V_Smith(float NoV, float NoL, float a) { float a2 = a * a; float gv = NoL * sqrt(NoV * NoV * (1.0 - a2) + a2); float gl = NoV * sqrt(NoL * NoL * (1.0 - a2) + a2); return 0.5 / max(gv + gl, 1e-4); }
vec3 studioEnv(vec3 R, float rough) {
  vec3 e = uAmbient * (0.55 + 0.45 * R.y);
  for (int i = 0; i < 3; i++) {
    float c = saturate(dot(R, uLightDir[i]));
    e += uLightCol[i] * pow(c, mix(48.0, 2.0, rough)) * mix(0.9, 0.25, rough);
  }
  return e;
}

void main() {
  if (!gl_FrontFacing) {
    fragColor = vec4(uTransColor * uAmbient * 0.08, 1.0);
    return;
  }
  vec3 toC = vModelPos - vCenter;
  float beadR = max(length(toC), 1e-3);
  vec3 nBead = toC / beadR;
  float domeW = smoothstep(0.05, 0.6, vMask.x) * (0.5 + 0.5 * vMask.y);
  vec3 nBase = normalize(mix(normalize(vNormalModel), nBead, domeW));
  vec3 pp = vModelPos * uPoreScale;
  vec3 g = vec3(0.0);
  float dh = 0.5;
  if (uLite == 0) {
    float e = 0.35;
    g = vec3(noise(pp + vec3(e, 0, 0)) - noise(pp - vec3(e, 0, 0)),
             noise(pp + vec3(0, e, 0)) - noise(pp - vec3(0, e, 0)),
             noise(pp + vec3(0, 0, e)) - noise(pp - vec3(0, 0, e)));
    dh = noise(pp);
  }
  vec3 N = normalize(nBase + g * uPoreStrength);
  vec3 V = normalize(uCamM - vModelPos);
  float NoV = saturate(dot(N, V));

  float ao = mix(0.28, 1.0, vMask.y) * mix(0.75, 1.0, vMask.x);
  float roughness = clamp(uRoughness + (1.0 - vMask.y) * 0.25 + (dh - 0.5) * 0.12, 0.045, 1.0);

  float t = clamp(vDisplacement * 3.0 + 0.5, 0.0, 1.0);
  vec3 albedo = mix(uAlbedo, uAlbedo2, t);
  vec3 irr = uAmbient * (0.6 + 0.4 * nBase.y) * PI;
  for (int i = 0; i < 3; i++) {
    float nl = dot(nBase, uLightDir[i]);
    float wrap = saturate((nl + 0.45) / 1.45);
    irr += uLightCol[i] * wrap * wrap * PI;
  }
  vec3 aoC = pow(vec3(ao), vec3(1.0) - 0.75 * albedo / max(max(albedo.r, max(albedo.g, albedo.b)), 1e-3));
  vec3 sss = pow(albedo, vec3(0.85, 1.25, 1.25)) * irr * (0.75 / PI) * aoC;

  vec3 sigma = -log(clamp(uTransColor, vec3(0.002), vec3(0.999))) / uTransDepth;
  float open = 0.3 + 0.7 * ao;
  vec3 nS = nBead;
  vec3 outward = normalize(vNormalModel);
  vec3 bead = vec3(0.0);
  for (int i = 0; i < 3; i++) {
    vec3 Lm = uLightDir[i];
    float nl = dot(nS, Lm);
    float chord = 2.0 * beadR * max(-nl, 0.0) + 0.10;
    float past = smoothstep(0.45, -0.30, nl);
    float reach = smoothstep(-0.45, 0.20, dot(outward, Lm));
    bead += uLightCol[i] * exp(-sigma * chord) * past * reach;
  }
  bead *= 0.07 * uBead * open * 2.5;
  vec3 Tr = refract(-V, nBase, 1.0 / 1.33);
  float chordV = max(2.0 * dot(-toC, Tr), 0.0);
  float leave = smoothstep(0.30, 0.85, 1.0 - saturate(dot(outward, V)));
  leave *= smoothstep(0.12, 0.60, ao);
  vec3 through = uBackdrop * exp(-sigma * (chordV + 0.06)) * leave * (0.25 + 0.75 * vMask.z) * uSeeThrough * 0.55;
  float SEED_R = beadR * 0.3;
  float sb = dot(-toC, Tr);
  float sh = sb * sb - (beadR * beadR - SEED_R * SEED_R);
  float seedHit = smoothstep(0.0, SEED_R * SEED_R * 0.9, sh) * step(0.0, sb);
  float sd = max(sb - sqrt(max(sh, 0.0)), 0.0);
  vec3 seedLight = uAmbient * open + bead * 2.0;
  vec3 seed = vec3(0.80, 0.56, 0.30) * seedLight * exp(-sigma * (sd + 0.03)) * seedHit * uSeedAmt;

  vec3 glow = bead + through + seed;
  float seeThrough = leave * saturate(uSeeThrough);
  vec3 diffuse = sss * uSSS * (1.0 - 0.35 * seeThrough) + glow;

  vec3 spec = vec3(0.0);
  float a = roughness * roughness;
  for (int i = 0; i < 3; i++) {
    vec3 L = uLightDir[i];
    vec3 H = normalize(L + V);
    float NoL = saturate(dot(N, L)), NoH = saturate(dot(N, H)), VoH = saturate(dot(V, H));
    float F = uF0 + (1.0 - uF0) * pow(1.0 - VoH, 5.0);
    spec += uLightCol[i] * D_GGX(NoH, a) * V_Smith(NoV, NoL, a) * F * NoL;
  }
  vec3 R = reflect(-V, N);
  R = normalize(mix(R, N, roughness * roughness));
  vec2 dfg = envBRDF(roughness, NoV);
  float so = saturate(pow(NoV + ao, exp2(-16.0 * roughness - 1.0)) - 1.0 + ao);
  float horizon = saturate(1.0 + dot(R, nBase));
  so *= horizon * horizon;
  float cavity = mix(1.0, 0.55 + 0.45 * dh, saturate(uPoreStrength * 2.0));
  spec += studioEnv(R, roughness) * (uF0 * dfg.x + dfg.y) * so * cavity;
  spec *= uSpecular;

  float fres = pow(1.0 - NoV, 3.0);
  float localLight = dot(sss, vec3(0.33)) * 2.2;
  vec3 sheen = uSheenColor * localLight * fres * uSheen * (0.4 + 0.6 * ao);

  vec3 col = diffuse + spec + sheen;

  if (uLite == 0) {
    float wax = smoothstep(0.35, 0.8, fbm(vModelPos * uBloomScale + 7.0)) * uBloom;
    wax *= (0.5 + 0.5 * pow(1.0 - NoV, 1.5)) * mix(0.6, 1.0, vMask.y);
    vec3 waxCol = vec3(0.78, 0.80, 0.88) * (uAmbient + 0.25 * (uLightCol[0] + uLightCol[1] + uLightCol[2])) * 0.5;
    col = mix(col, waxCol, saturate(wax));
  }

  col += albedo * 0.02 * uPulse;
  fragColor = vec4(max(col, 0.0), 1.0);
}
