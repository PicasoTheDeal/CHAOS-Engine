#version 430 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
uniform mat4 uProj, uModelView;
uniform float uTime, uChaos, uNoiseScale, uDispStrength, uMutate;
uniform float uCellScale, uDomeR, uDomeAmp;
uniform vec3 uSeedOff;
out vec3 vModelPos, vNormalModel, vCenter;
out vec4 vMask;      // x dome height, y crease openness, z per-sac random, w d1
out float vDisplacement;
#include <berry_common.glsl>
void main() {
  vec3 pos = aPos;
  float n = fbm(pos * uNoiseScale + uTime * 0.15);
  float wave = sin(pos.x * 2.5 + uTime * 0.4) * 0.12 + cos(pos.y * 2.5 + uTime * 0.3) * 0.12;
  float chaosWave = sin(pos.x * uChaos + uTime * 0.4) * 0.15 + cos(pos.z * uChaos + uTime * 0.3) * 0.15;
  float n2 = fbm(pos * (uNoiseScale * 2.6 + uMutate * 1.5) - uTime * 0.07 + 19.0);
  float d = (n * 1.2 + wave + chaosWave) * uDispStrength * (1.0 + uMutate * 1.2) + (n2 - 0.45) * uMutate * uDispStrength * 2.5;

  vec3 q = pos * uCellScale * (1.0 + uMutate * 0.4) + uSeedOff;
  float d1, d2; vec3 c;
  voronoi(q, d1, d2, c);
  float h = sqrt(max(uDomeR * uDomeR - d1 * d1, 0.0)) / uDomeR;
  float crease = smoothstep(0.0, 0.35, d2 - d1);
  float dome = h * (0.35 + 0.65 * crease);
  vec3 rnd = hash3(c);
  float sacSize = 0.75 + 0.5 * rnd.x;

  pos += aNormal * (d + dome * uDomeAmp * sacSize);
  vDisplacement = d;
  vMask = vec4(h, crease, rnd.x, d1);
  vModelPos = pos;
  vNormalModel = aNormal;
  vCenter = (c - uSeedOff) / uCellScale - aNormal * (uDomeR / uCellScale) * 0.35;
  gl_Position = uProj * uModelView * vec4(pos, 1.0);
}
