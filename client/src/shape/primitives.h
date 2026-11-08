#pragma once
// three.js geometry equivalents. All return an indexed Mesh with smooth normals.
#include "shape/parametric.h"
#include <glm/glm.hpp>
#include <vector>

namespace chaos {

Mesh boxGeo(float w, float h, float d, int ws = 1, int hs = 1, int ds = 1);
Mesh planeGeo(float w, float h, int ws = 1, int hs = 1);
Mesh cylinderGeo(float rTop, float rBottom, float height, int radialSeg, int heightSeg = 1, bool open = false);
inline Mesh coneGeo(float r, float h, int radialSeg, int heightSeg = 1) { return cylinderGeo(0, r, h, radialSeg, heightSeg); }
Mesh ringGeo(float inner, float outer, int thetaSeg, int phiSeg);
Mesh torusGeo(float r, float tube, int radialSeg, int tubularSeg, float arc = 6.283185307f);
Mesh torusKnotGeo(float r, float tube, int tubularSeg, int radialSeg, int p, int q);
Mesh latheGeo(const std::vector<glm::vec2>& pts, int segments);
Mesh tubeGeo(const std::vector<glm::vec3>& path, int tubularSeg, float radius, int radialSeg, bool closed);

// PolyhedronGeometry: base solid subdivided `detail` times and pushed onto a sphere of `radius`.
Mesh icosahedronGeo(float radius, int detail);
Mesh octahedronGeo(float radius, int detail);
Mesh tetrahedronGeo(float radius, int detail);
Mesh dodecahedronGeo(float radius, int detail);

// CatmullRom (centripetal) through `pts`, sampled into `n` points.
std::vector<glm::vec3> catmullRom(const std::vector<glm::vec3>& pts, int n, bool closed);

// BufferGeometryUtils
Mesh mergeMeshes(const std::vector<Mesh>& parts);
void transformMesh(Mesh& m, const glm::mat4& mat);
void translateMesh(Mesh& m, const glm::vec3& t);
Mesh weld(const Mesh& m, float eps);            // mergeVertices: snap positions, drop duplicates, smooth normals

} // namespace chaos
