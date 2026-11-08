#pragma once
// .cheng: the engine's own mesh file. one object, already cleaned, decimated and sized for the stage, so loading it
// costs nothing: positions quantized to 16 bits inside the bounding box, normals as 16-bit octahedral pairs, triangle
// indices, and (v2) what the model brought with it: uvs, vertex colours, one material and one base-colour texture
// as a png. a 20k-vertex model is about 300 KB plus its texture. made from .obj / .stl / .gltf / .glb (and .blend /
// .fbx / .dae / .step through blender when it is installed) by `chaos --cheng` or the app's model tab. never a seed,
// never claimable.
#include <string>
#include <optional>
#include <filesystem>
#include <glm/glm.hpp>
#include "shape/parametric.h"

namespace chaos::model {

constexpr int DEFAULT_MAX_VERTS = 120000;     // the gpu does not care at this size; detail (seams, grooves, lettering) does. 120k verts is about 1.4 MB as a .cheng
constexpr int MAX_TEXTURE = 1024;             // a base-colour texture is halved until it fits this: a wallpaper never shows more

// read any supported source into a raw mesh. err says what went wrong. blender is shelled out to for its formats.
std::optional<Mesh> importModel(const std::filesystem::path& path, std::string& err);
bool needsBlender(const std::filesystem::path& path);
bool blenderAvailable();

// weld exact duplicates only (keeping hard edges, texture seams and material edges), source normals kept, decimate by
// grid clustering only when the count is over maxVerts (and then as finely as will fit), centre, scale to `radius`
Mesh prepareModel(Mesh m, int maxVerts = DEFAULT_MAX_VERTS, float radius = 4.5f);

bool writeCheng(const std::filesystem::path& path, const Mesh& m, const std::string& name);
std::optional<Mesh> readCheng(const std::filesystem::path& path, std::string* name = nullptr);

// where the app keeps converted models: <vault>/../models (next to the vault folder)
std::filesystem::path modelsDir();
// one call for the app and the cli: import, prepare, write <modelsDir>/<stem>.cheng. returns the .cheng path
std::optional<std::filesystem::path> convertToCheng(const std::filesystem::path& src, std::string& err, int maxVerts = DEFAULT_MAX_VERTS);

} // namespace chaos::model
