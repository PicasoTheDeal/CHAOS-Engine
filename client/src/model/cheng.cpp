#include "model/cheng.h"
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <map>
#include <cstring>
#include <cstdlib>
#include <cmath>
#include <algorithm>
#include <nlohmann/json.hpp>
#include "stb_image.h"
#include "stb_image_write.h"
#include "vault/vault.h"

namespace chaos::model {
namespace fs = std::filesystem;
using json = nlohmann::json;

// ---------------------------------------------------------------- import
static std::string lower(std::string s) { for (char& c : s) c = char(std::tolower((unsigned char)c)); return s; }
static std::string ext(const fs::path& p) { return lower(p.extension().string()); }

bool needsBlender(const fs::path& p) { std::string e = ext(p); return e == ".blend" || e == ".fbx" || e == ".dae" || e == ".step" || e == ".stp" || e == ".usd" || e == ".usdz" || e == ".3ds" || e == ".x3d" || e == ".abc"; }
bool blenderAvailable() { return std::system("command -v blender >/dev/null 2>&1") == 0; }

// ---- textures: decode anything stb reads, keep rows top first, box-shrink to the cap so a .cheng stays small
static bool decodeImage(const uint8_t* bytes, size_t len, Material& mat) {
    int w, h, n; unsigned char* px = stbi_load_from_memory(bytes, int(len), &w, &h, &n, 4);
    if (!px || w <= 0 || h <= 0) { if (px) stbi_image_free(px); return false; }
    std::vector<uint8_t> rgba(px, px + size_t(w) * size_t(h) * 4); stbi_image_free(px);
    while (w > MAX_TEXTURE || h > MAX_TEXTURE) {                 // halve until it fits: 2x2 box average
        int nw = std::max(1, w / 2), nh = std::max(1, h / 2); std::vector<uint8_t> out(size_t(nw) * size_t(nh) * 4);
        for (int y = 0; y < nh; y++) for (int x = 0; x < nw; x++) for (int c = 0; c < 4; c++) {
            int s = 0, k = 0;
            for (int dy = 0; dy < 2; dy++) for (int dx = 0; dx < 2; dx++) { int sx = std::min(w - 1, x * 2 + dx), sy = std::min(h - 1, y * 2 + dy); s += rgba[(size_t(sy) * w + sx) * 4 + c]; k++; }
            out[(size_t(y) * nw + x) * 4 + c] = uint8_t(s / k);
        }
        rgba.swap(out); w = nw; h = nh;
    }
    mat.texW = w; mat.texH = h; mat.rgba = std::move(rgba);
    return true;
}
static bool loadImageFile(const fs::path& p, Material& mat) {
    std::ifstream in(p, std::ios::binary); if (!in) return false;
    std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(in)), {});
    return !bytes.empty() && decodeImage(bytes.data(), bytes.size(), mat);
}

// a source material before it is folded into the mesh: colour, optional texture, how rough it is
struct SrcMat { glm::vec3 kd{ 1.f }; glm::vec3 ke{ 0.f }; float rough = 0.6f, metal = 0.f; Material tex; bool hasTex() const { return tex.hasTexture(); } };

// several materials in, one Material + per-vertex colours out. the texture that covers the most triangles is the one
// the .cheng carries; vertices of other materials get their colour and a texture weight of 0.
static void foldMaterials(Mesh& m, const std::vector<SrcMat>& mats, const std::vector<int>& matOfVertex) {
    if (mats.empty()) return;
    std::vector<size_t> tris(mats.size(), 0);
    for (size_t t = 0; t + 2 < m.indices.size(); t += 3) { int mi = matOfVertex[m.indices[t]]; if (mi >= 0 && mi < int(mats.size())) tris[mi]++; }
    int chosen = -1; size_t best = 0;
    for (size_t i = 0; i < mats.size(); i++) if (mats[i].hasTex() && tris[i] > best) { best = tris[i]; chosen = int(i); }
    if (chosen < 0) for (size_t i = 0; i < mats.size(); i++) if (tris[i] >= best) { best = tris[i]; chosen = int(i); }
    if (chosen < 0) return;
    const SrcMat& c = mats[chosen];
    m.material.roughness = c.rough; m.material.metallic = c.metal; m.material.emissive = c.ke;
    if (c.hasTex()) { m.material.texW = c.tex.texW; m.material.texH = c.tex.texH; m.material.rgba = c.tex.rgba; }
    bool uniform = true; for (int mi : matOfVertex) if (mi != chosen) { uniform = false; break; }
    if (uniform) { m.material.base = c.kd; return; }          // one material: no colour array needed
    m.material.base = glm::vec3(1.f);
    std::vector<glm::vec4> col(m.positions.size(), glm::vec4(c.kd, c.hasTex() ? 1.f : 0.f));
    for (size_t i = 0; i < m.positions.size(); i++) { int mi = matOfVertex[i]; if (mi >= 0 && mi < int(mats.size()) && mi != chosen) col[i] = glm::vec4(mats[mi].kd, 0.f); }
    if (m.hasColors()) for (size_t i = 0; i < col.size(); i++) { col[i] = glm::vec4(glm::vec3(col[i]) * glm::vec3(m.colors[i]), col[i].a); }
    m.colors = std::move(col);
    if (!c.hasTex()) m.uvs.clear();
}

static std::vector<SrcMat> readMtl(const fs::path& p, std::map<std::string, int>& byName) {
    std::vector<SrcMat> mats; std::ifstream in(p); std::string line;
    while (std::getline(in, line)) {
        std::istringstream ss(line); std::string k; ss >> k;
        if (k == "newmtl") { std::string n; ss >> n; byName[n] = int(mats.size()); mats.push_back({}); continue; }
        if (mats.empty()) continue;
        SrcMat& m = mats.back();
        if (k == "Kd") ss >> m.kd.r >> m.kd.g >> m.kd.b;
        else if (k == "Ke") ss >> m.ke.r >> m.ke.g >> m.ke.b;
        else if (k == "Ns") { float ns = 0; ss >> ns; m.rough = glm::clamp(1.f - std::sqrt(glm::clamp(ns, 0.f, 1000.f) / 1000.f), 0.05f, 1.f); }
        else if (k == "Pr") ss >> m.rough;
        else if (k == "Pm") ss >> m.metal;
        else if (k == "map_Kd") { std::string rest; std::getline(ss, rest); size_t a = rest.find_first_not_of(" \t"); if (a != std::string::npos) { rest = rest.substr(a); while (!rest.empty() && (rest.back() == '\r' || rest.back() == ' ')) rest.pop_back(); fs::path tp = rest; if (tp.is_relative()) tp = p.parent_path() / tp; loadImageFile(tp, m.tex); } }
    }
    return mats;
}

static std::optional<Mesh> importObj(const fs::path& p, std::string& err) {
    std::ifstream in(p);
    if (!in) { err = "cannot read " + p.string(); return std::nullopt; }
    std::vector<glm::vec3> pos, nrm, vcol; std::vector<glm::vec2> uv; bool anyVcol = false;
    std::vector<SrcMat> mats; std::map<std::string, int> matByName; int curMat = -1; std::vector<int> matOf;
    Mesh m;
    std::unordered_map<std::string, uint32_t> seen;      // "v/vt/vn|mat" -> vertex
    std::string line;
    auto vertex = [&](const std::string& tok) -> uint32_t {
        std::string key = tok + "|" + std::to_string(curMat);
        auto it = seen.find(key);
        if (it != seen.end()) return it->second;
        int vi = 0, ti = 0, ni = 0; char buf[128]; std::strncpy(buf, tok.c_str(), sizeof buf - 1); buf[sizeof buf - 1] = 0;
        char* s = buf; char* f1 = std::strchr(s, '/');
        vi = std::atoi(s);
        if (f1) { char* f2 = std::strchr(f1 + 1, '/'); ti = std::atoi(f1 + 1); if (f2) ni = std::atoi(f2 + 1); }
        if (vi < 0) vi = int(pos.size()) + vi + 1;
        if (ti < 0) ti = int(uv.size()) + ti + 1;
        if (ni < 0) ni = int(nrm.size()) + ni + 1;
        if (vi < 1 || vi > int(pos.size())) return UINT32_MAX;
        m.positions.push_back(pos[vi - 1]);
        m.normals.push_back(ni >= 1 && ni <= int(nrm.size()) ? nrm[ni - 1] : glm::vec3(0));
        m.uvs.push_back(ti >= 1 && ti <= int(uv.size()) ? glm::vec2(uv[ti - 1].x, 1.f - uv[ti - 1].y) : glm::vec2(0));   // obj's v runs up, ours runs down
        m.colors.push_back(glm::vec4(vcol[vi - 1], 1.f));
        matOf.push_back(curMat);
        uint32_t id = uint32_t(m.positions.size() - 1); seen.emplace(key, id); return id;
    };
    bool anyNormal = false, anyUv = false;
    while (std::getline(in, line)) {
        if (line.size() < 2) continue;
        std::istringstream ss(line); std::string k; ss >> k;
        if (k == "v") { glm::vec3 v; ss >> v.x >> v.y >> v.z; pos.push_back(v); glm::vec3 c(1.f); if (ss >> c.r >> c.g >> c.b) anyVcol = true; else c = glm::vec3(1.f); vcol.push_back(c); }
        else if (k == "vn") { glm::vec3 v; ss >> v.x >> v.y >> v.z; nrm.push_back(v); anyNormal = true; }
        else if (k == "vt") { glm::vec2 v; ss >> v.x >> v.y; uv.push_back(v); anyUv = true; }
        else if (k == "mtllib") { std::string rest; std::getline(ss, rest); size_t a = rest.find_first_not_of(" \t"); if (a != std::string::npos) { rest = rest.substr(a); while (!rest.empty() && (rest.back() == '\r' || rest.back() == ' ')) rest.pop_back(); fs::path mp = rest; if (mp.is_relative()) mp = p.parent_path() / mp; std::map<std::string, int> names; auto more = readMtl(mp, names); int base = int(mats.size()); for (auto& [n, i] : names) matByName[n] = base + i; mats.insert(mats.end(), more.begin(), more.end()); } }
        else if (k == "usemtl") { std::string n; ss >> n; auto it = matByName.find(n); curMat = it == matByName.end() ? -1 : it->second; }
        else if (k == "f") {
            std::vector<uint32_t> poly; std::string tok;
            while (ss >> tok) { uint32_t id = vertex(tok); if (id != UINT32_MAX) poly.push_back(id); }
            for (size_t i = 2; i < poly.size(); i++) { m.indices.push_back(poly[0]); m.indices.push_back(poly[i - 1]); m.indices.push_back(poly[i]); }   // fan
        }
    }
    if (m.indices.empty()) { err = "no faces in " + p.filename().string(); return std::nullopt; }
    if (!anyNormal) m.computeVertexNormals();
    if (!anyUv) m.uvs.clear();
    if (!anyVcol) m.colors.clear();
    for (int& mi : matOf) if (mi < 0 && !mats.empty()) mi = 0;        // faces before any usemtl: the first material
    foldMaterials(m, mats, matOf);
    return m;
}

static std::optional<Mesh> importStl(const fs::path& p, std::string& err) {
    std::ifstream in(p, std::ios::binary);
    if (!in) { err = "cannot read " + p.string(); return std::nullopt; }
    std::vector<char> bytes((std::istreambuf_iterator<char>(in)), {});
    Mesh m;
    auto tri = [&](const float* f) { for (int k = 0; k < 3; k++) { m.positions.push_back(glm::vec3(f[3 + 3 * k], f[4 + 3 * k], f[5 + 3 * k])); m.normals.push_back(glm::vec3(0)); m.indices.push_back(uint32_t(m.positions.size() - 1)); } };
    bool ascii = bytes.size() > 5 && std::strncmp(bytes.data(), "solid", 5) == 0;
    if (!ascii && bytes.size() >= 84) {
        uint32_t n; std::memcpy(&n, bytes.data() + 80, 4);
        if (84 + size_t(n) * 50 <= bytes.size()) { for (uint32_t i = 0; i < n; i++) { float f[12]; std::memcpy(f, bytes.data() + 84 + size_t(i) * 50, 48); tri(f); } }
    }
    if (m.indices.empty()) {
        // ascii: "vertex x y z" lines, three per facet
        std::istringstream ss(std::string(bytes.begin(), bytes.end())); std::string tok; std::vector<glm::vec3> v;
        while (ss >> tok) if (tok == "vertex") { glm::vec3 q; ss >> q.x >> q.y >> q.z; v.push_back(q); if (v.size() == 3) { for (auto& x : v) { m.positions.push_back(x); m.normals.push_back(glm::vec3(0)); m.indices.push_back(uint32_t(m.positions.size() - 1)); } v.clear(); } }
    }
    if (m.indices.empty()) { err = "no triangles in " + p.filename().string(); return std::nullopt; }
    m.computeVertexNormals();
    return m;
}

static std::vector<uint8_t> b64decode(const std::string& s) {
    static const std::string A = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::vector<uint8_t> out; int val = 0, bits = -8;
    for (char c : s) { size_t i = A.find(c); if (i == std::string::npos) break; val = (val << 6) + int(i); bits += 6; if (bits >= 0) { out.push_back(uint8_t((val >> bits) & 0xff)); bits -= 8; } }
    return out;
}

static std::optional<Mesh> importGltf(const fs::path& p, std::string& err) {
    std::ifstream in(p, std::ios::binary);
    if (!in) { err = "cannot read " + p.string(); return std::nullopt; }
    std::vector<uint8_t> file((std::istreambuf_iterator<char>(in)), {});
    json doc; std::vector<uint8_t> glbBin;
    if (file.size() >= 12 && std::memcmp(file.data(), "glTF", 4) == 0) {
        size_t off = 12;
        while (off + 8 <= file.size()) {
            uint32_t len, type; std::memcpy(&len, &file[off], 4); std::memcpy(&type, &file[off + 4], 4); off += 8;
            if (off + len > file.size()) break;
            if (type == 0x4E4F534A) doc = json::parse(file.begin() + off, file.begin() + off + len, nullptr, false);
            else if (type == 0x004E4942) glbBin.assign(file.begin() + off, file.begin() + off + len);
            off += len;
        }
    } else doc = json::parse(file.begin(), file.end(), nullptr, false);
    if (doc.is_discarded() || !doc.contains("meshes")) { err = "not a gltf mesh: " + p.filename().string(); return std::nullopt; }
    // buffers
    std::vector<std::vector<uint8_t>> buffers;
    for (auto& b : doc.value("buffers", json::array())) {
        std::string uri = b.value("uri", "");
        if (uri.empty()) buffers.push_back(glbBin);
        else if (uri.rfind("data:", 0) == 0) { auto c = uri.find(','); buffers.push_back(b64decode(c == std::string::npos ? "" : uri.substr(c + 1))); }
        else { std::ifstream bi(p.parent_path() / uri, std::ios::binary); buffers.emplace_back((std::istreambuf_iterator<char>(bi)), std::istreambuf_iterator<char>()); }
    }
    auto accessorBytes = [&](int ai, int& count, int& comps, int& ctype, int& stride) -> const uint8_t* {
        const json& a = doc["accessors"].at(ai); const json& bv = doc["bufferViews"].at(a.value("bufferView", 0));
        count = a.value("count", 0); ctype = a.value("componentType", 5126);
        std::string t = a.value("type", "SCALAR"); comps = t == "VEC3" ? 3 : t == "VEC2" ? 2 : t == "VEC4" ? 4 : 1;
        int csize = ctype == 5126 || ctype == 5125 ? 4 : ctype == 5123 || ctype == 5122 ? 2 : 1;
        stride = bv.value("byteStride", 0); if (!stride) stride = csize * comps;
        size_t off = size_t(bv.value("byteOffset", 0)) + size_t(a.value("byteOffset", 0));
        const auto& buf = buffers.at(bv.value("buffer", 0));
        if (off + size_t(count) * stride > buf.size() + size_t(stride)) return nullptr;
        return buf.data() + off;
    };
    // one normalized float out of a component, whatever its type (uv and colour accessors may be u8 / u16 normalized)
    auto comp = [](const uint8_t* d, int ctype, int k) -> float {
        switch (ctype) { case 5126: { float f; std::memcpy(&f, d + k * 4, 4); return f; }
                         case 5123: { uint16_t v; std::memcpy(&v, d + k * 2, 2); return v / 65535.f; }
                         case 5121: return d[k] / 255.f; default: return 0.f; }
    };
    // materials: images by index, decoded once
    std::vector<SrcMat> mats; std::unordered_map<int, Material> images;
    auto imageOf = [&](int texIndex) -> const Material* {
        if (texIndex < 0 || !doc.contains("textures") || texIndex >= int(doc["textures"].size())) return nullptr;
        int src = doc["textures"][texIndex].value("source", -1);
        if (src < 0 || !doc.contains("images") || src >= int(doc["images"].size())) return nullptr;
        auto it = images.find(src); if (it != images.end()) return it->second.hasTexture() ? &it->second : nullptr;
        Material mat; const json& im = doc["images"][src];
        if (im.contains("bufferView")) { const json& bv = doc["bufferViews"].at(im["bufferView"]); const auto& buf = buffers.at(bv.value("buffer", 0)); size_t off = size_t(bv.value("byteOffset", 0)), len = size_t(bv.value("byteLength", 0)); if (off + len <= buf.size()) decodeImage(buf.data() + off, len, mat); }
        else { std::string uri = im.value("uri", ""); if (uri.rfind("data:", 0) == 0) { auto c = uri.find(','); auto bytes = b64decode(c == std::string::npos ? "" : uri.substr(c + 1)); decodeImage(bytes.data(), bytes.size(), mat); } else if (!uri.empty()) loadImageFile(p.parent_path() / uri, mat); }
        auto& slot = images[src]; slot = std::move(mat);
        return slot.hasTexture() ? &slot : nullptr;
    };
    for (auto& jm : doc.value("materials", json::array())) {
        SrcMat sm; const json& pbr = jm.value("pbrMetallicRoughness", json::object());
        if (pbr.contains("baseColorFactor") && pbr["baseColorFactor"].size() >= 3) sm.kd = glm::vec3(pbr["baseColorFactor"][0], pbr["baseColorFactor"][1], pbr["baseColorFactor"][2]);
        sm.rough = pbr.value("roughnessFactor", 1.f); sm.metal = pbr.value("metallicFactor", 1.f);
        if (!pbr.contains("metallicFactor")) sm.metal = 0.f;        // the spec default is 1, which makes every untextured blender material a mirror; a plain colour is what people meant
        if (jm.contains("emissiveFactor") && jm["emissiveFactor"].size() >= 3) sm.ke = glm::vec3(jm["emissiveFactor"][0], jm["emissiveFactor"][1], jm["emissiveFactor"][2]);
        if (pbr.contains("baseColorTexture")) if (const Material* im = imageOf(pbr["baseColorTexture"].value("index", -1))) sm.tex = *im;
        mats.push_back(std::move(sm));
    }
    Mesh m; std::vector<int> matOf; bool anyUv = false, anyColor = false;
    for (auto& mesh : doc["meshes"]) for (auto& prim : mesh.value("primitives", json::array())) {
        if (prim.value("mode", 4) != 4 || !prim.contains("attributes") || !prim["attributes"].contains("POSITION")) continue;
        int n, c, ct, st; const uint8_t* pd = accessorBytes(prim["attributes"]["POSITION"], n, c, ct, st);
        if (!pd || ct != 5126) continue;
        uint32_t base = uint32_t(m.positions.size()); int mi = prim.value("material", -1);
        for (int i = 0; i < n; i++) { glm::vec3 v; std::memcpy(&v, pd + size_t(i) * st, 12); m.positions.push_back(v); m.normals.push_back(glm::vec3(0)); m.uvs.push_back(glm::vec2(0)); m.colors.push_back(glm::vec4(1.f)); matOf.push_back(mi); }
        if (prim["attributes"].contains("NORMAL")) { int nn, nc, nct, nst; const uint8_t* nd = accessorBytes(prim["attributes"]["NORMAL"], nn, nc, nct, nst); if (nd && nct == 5126 && nn == n) for (int i = 0; i < n; i++) std::memcpy(&m.normals[base + i], nd + size_t(i) * nst, 12); }
        if (prim["attributes"].contains("TEXCOORD_0")) { int tn, tc, tct, tst; const uint8_t* td = accessorBytes(prim["attributes"]["TEXCOORD_0"], tn, tc, tct, tst); if (td && tn == n && tc == 2) { anyUv = true; for (int i = 0; i < n; i++) m.uvs[base + i] = glm::vec2(comp(td + size_t(i) * tst, tct, 0), comp(td + size_t(i) * tst, tct, 1)); } }
        if (prim["attributes"].contains("COLOR_0")) { int cn, cc, cct, cst; const uint8_t* cd = accessorBytes(prim["attributes"]["COLOR_0"], cn, cc, cct, cst); if (cd && cn == n && (cc == 3 || cc == 4)) { anyColor = true; for (int i = 0; i < n; i++) m.colors[base + i] = glm::vec4(comp(cd + size_t(i) * cst, cct, 0), comp(cd + size_t(i) * cst, cct, 1), comp(cd + size_t(i) * cst, cct, 2), 1.f); } }
        if (prim.contains("indices")) {
            int ni, ic, ict, ist; const uint8_t* id = accessorBytes(prim["indices"], ni, ic, ict, ist);
            if (id) for (int i = 0; i < ni; i++) { uint32_t v = ict == 5125 ? *reinterpret_cast<const uint32_t*>(id + size_t(i) * ist) : ict == 5123 ? *reinterpret_cast<const uint16_t*>(id + size_t(i) * ist) : *(id + size_t(i) * ist); m.indices.push_back(base + v); }
        } else for (int i = 0; i < n; i++) m.indices.push_back(base + uint32_t(i));
    }
    if (m.indices.empty()) { err = "no triangles in " + p.filename().string(); return std::nullopt; }
    bool anyNormal = false; for (auto& nv : m.normals) if (glm::dot(nv, nv) > 0.1f) { anyNormal = true; break; }
    if (!anyNormal) m.computeVertexNormals();
    if (!anyUv) m.uvs.clear();
    if (!anyColor) m.colors.clear();
    foldMaterials(m, mats, matOf);
    return m;
}

static std::optional<Mesh> importViaBlender(const fs::path& p, std::string& err) {
    if (!blenderAvailable()) { err = "blender is needed to read " + ext(p) + " files (install it, or export .glb / .obj yourself)"; return std::nullopt; }
    fs::path out = fs::temp_directory_path() / ("chaos-import-" + std::to_string(std::rand()) + ".glb");
    std::string e = ext(p);
    std::string imp = e == ".blend" ? "" :
        e == ".fbx" ? "bpy.ops.import_scene.fbx(filepath=src)" :
        e == ".dae" ? "bpy.ops.wm.collada_import(filepath=src)" :
        (e == ".step" || e == ".stp") ? "bpy.ops.import_mesh.step(filepath=src)" :
        (e == ".usd" || e == ".usdz") ? "bpy.ops.wm.usd_import(filepath=src)" :
        e == ".x3d" ? "bpy.ops.import_scene.x3d(filepath=src)" : "bpy.ops.wm.alembic_import(filepath=src)";
    // materials come along: blender bakes its base colour / image node into the glb (one base colour texture per material)
    std::string py = "import bpy\nsrc=r'" + p.string() + "'\n" + (imp.empty() ? "" : "bpy.ops.wm.read_factory_settings(use_empty=True)\n" + imp + "\n") +
                     "bpy.ops.export_scene.gltf(filepath=r'" + out.string() + "', export_format='GLB', export_apply=True, export_normals=True, export_materials='EXPORT', export_image_format='AUTO', export_animations=False)\n";
    fs::path script = fs::temp_directory_path() / ("chaos-import-" + std::to_string(std::rand()) + ".py");
    { std::ofstream s(script); s << py; }
    std::string cmd = "blender -b " + (e == ".blend" ? "'" + p.string() + "' " : std::string()) + "--python '" + script.string() + "' >/dev/null 2>&1";
    int rc = std::system(cmd.c_str());
    std::error_code ec; fs::remove(script, ec);
    if (rc != 0 || !fs::exists(out)) { err = "blender could not convert " + p.filename().string(); return std::nullopt; }
    auto m = importGltf(out, err);
    fs::remove(out, ec);
    return m;
}

std::optional<Mesh> importModel(const fs::path& p, std::string& err) {
    std::error_code ec;
    if (!fs::exists(p, ec)) { err = "no such file: " + p.string(); return std::nullopt; }
    std::string e = ext(p);
    if (e == ".cheng") return readCheng(p);
    if (e == ".obj") return importObj(p, err);
    if (e == ".stl") return importStl(p, err);
    if (e == ".gltf" || e == ".glb") return importGltf(p, err);
    if (needsBlender(p)) return importViaBlender(p, err);
    err = "unknown model format " + e + " (obj, stl, gltf, glb; blend, fbx, dae, step, usd through blender)";
    return std::nullopt;
}

// ---------------------------------------------------------------- prepare
// a vertex key: position snapped to a cell, plus the normal's rough direction, so a seam's two sides stay two vertices.
// `extra` folds in the uv and colour (the exact weld only): a texture seam or a material edge is two vertices too
static uint64_t keyOf(const glm::vec3& p, const glm::vec3& lo, float cell, const glm::vec3& n, bool withNormal, uint64_t extra) {
    glm::ivec3 g = glm::ivec3(glm::floor((p - lo) / cell));
    uint64_t k = (uint64_t(uint32_t(g.x) & 0xfffff) << 44) | (uint64_t(uint32_t(g.y) & 0xfffff) << 24) | (uint64_t(uint32_t(g.z) & 0xfffff) << 4);
    if (withNormal) { int nx = n.x > 0.5f ? 1 : n.x < -0.5f ? 2 : 0, ny = n.y > 0.5f ? 1 : n.y < -0.5f ? 2 : 0, nz = n.z > 0.5f ? 1 : n.z < -0.5f ? 2 : 0; k |= uint64_t(nx) | uint64_t(ny) << 2; k ^= uint64_t(nz) << 1; }   // 4 bits of direction
    if (extra) { k ^= (extra * 0x9E3779B97F4A7C15ull) ^ (extra >> 29); }
    return k;
}
static uint64_t surfaceKey(const Mesh& m, size_t i) {
    uint64_t e = 0;
    if (m.hasUvs()) { e = (uint64_t(uint16_t(std::lround(m.uvs[i].x * 4096.f))) << 16) | uint64_t(uint16_t(std::lround(m.uvs[i].y * 4096.f))); e |= 1ull << 63; }
    if (m.hasColors()) { for (int k = 0; k < 4; k++) e = (e << 8) ^ uint64_t(uint8_t(std::lround(glm::clamp(m.colors[i][k], 0.f, 1.f) * 255.f))); e |= 1ull << 62; }
    return e;
}
static Mesh cluster(const Mesh& m, const glm::vec3& lo, float cell, bool keepEdges, bool keepSurface) {
    Mesh out; out.material = m.material;
    const bool uv = m.hasUvs(), col = m.hasColors();
    std::unordered_map<uint64_t, uint32_t> cells;
    std::vector<uint32_t> remap(m.positions.size());
    std::vector<glm::vec3> sumP, sumN; std::vector<glm::vec2> sumT; std::vector<glm::vec4> sumC; std::vector<int> cnt;
    for (size_t i = 0; i < m.positions.size(); i++) {
        const glm::vec3 n = m.normals.size() > i ? m.normals[i] : glm::vec3(0);
        uint64_t key = keyOf(m.positions[i], lo, cell, n, keepEdges, keepSurface ? surfaceKey(m, i) : 0);
        auto it = cells.find(key);
        if (it == cells.end()) { it = cells.emplace(key, uint32_t(sumP.size())).first; sumP.push_back(glm::vec3(0)); sumN.push_back(glm::vec3(0)); sumT.push_back(glm::vec2(0)); sumC.push_back(glm::vec4(0)); cnt.push_back(0); }
        sumP[it->second] += m.positions[i]; sumN[it->second] += n; if (uv) sumT[it->second] += m.uvs[i]; if (col) sumC[it->second] += m.colors[i]; cnt[it->second]++;
        remap[i] = it->second;
    }
    out.positions.resize(sumP.size()); out.normals.resize(sumP.size());
    if (uv) out.uvs.resize(sumP.size());
    if (col) out.colors.resize(sumP.size());
    for (size_t k = 0; k < sumP.size(); k++) {
        out.positions[k] = sumP[k] / float(cnt[k]); glm::vec3 n = sumN[k]; out.normals[k] = glm::dot(n, n) > 1e-12f ? glm::normalize(n) : glm::vec3(0);
        if (uv) out.uvs[k] = sumT[k] / float(cnt[k]);
        if (col) out.colors[k] = sumC[k] / float(cnt[k]);
    }
    for (size_t t = 0; t + 2 < m.indices.size(); t += 3) {
        uint32_t a = remap[m.indices[t]], b = remap[m.indices[t + 1]], c = remap[m.indices[t + 2]];
        if (a == b || b == c || a == c) continue;
        out.indices.push_back(a); out.indices.push_back(b); out.indices.push_back(c);
    }
    return out;
}

Mesh prepareModel(Mesh m, int maxVerts, float radius) {
    if (m.positions.empty()) return m;
    glm::vec3 lo(1e30f), hi(-1e30f);
    for (auto& p : m.positions) { lo = glm::min(lo, p); hi = glm::max(hi, p); }
    glm::vec3 size = glm::max(hi - lo, glm::vec3(1e-6f));
    const float extent = std::max(size.x, std::max(size.y, size.z));
    bool hadNormals = false; for (auto& n : m.normals) if (glm::dot(n, n) > 0.1f) { hadNormals = true; break; }
    // 1. weld exact duplicates only (a cell of 1/65536 of the model: the .cheng cannot tell finer apart anyway),
    //    and keep a vertex per normal direction, per uv, per colour, so creases, seams and material edges keep their two sides
    Mesh out = cluster(m, lo, extent / 65536.f, hadNormals, true);
    // 2. only if that is still over the cap: cluster on a real grid, starting fine and coarsening until it fits
    float cell = extent / 4096.f;
    for (int iter = 0; iter < 40 && int(out.positions.size()) > maxVerts; iter++) { out = cluster(m, lo, cell, hadNormals, false); cell *= 1.25f; }
    // 3. drop vertices no triangle uses
    const bool uv = out.hasUvs(), col = out.hasColors();
    std::vector<uint32_t> used(out.positions.size(), UINT32_MAX); Mesh packed; packed.material = std::move(out.material);
    for (uint32_t& i : out.indices) { if (used[i] == UINT32_MAX) { used[i] = uint32_t(packed.positions.size()); packed.positions.push_back(out.positions[i]); packed.normals.push_back(out.normals[i]); if (uv) packed.uvs.push_back(out.uvs[i]); if (col) packed.colors.push_back(out.colors[i]); } i = used[i]; }
    packed.indices = std::move(out.indices);
    // 4. normals: the source's when it had them (blender's smoothing groups and hard edges survive), else computed
    bool flat = true; for (auto& n : packed.normals) if (glm::dot(n, n) > 0.1f) { flat = false; break; }
    if (flat || packed.normals.empty()) packed.computeVertexNormals();
    packed.center();
    float r = packed.boundingRadius();
    if (r > 1e-6f) for (auto& p : packed.positions) p *= radius / r;
    return packed;
}

// ---------------------------------------------------------------- .cheng
// v1: magic, counts, bounds, name, 16-bit positions + octahedral normals, u32 indices.
// v2 adds, after the name: a flags byte (1 uv, 2 colours, 4 texture), the material (base rgb, roughness, metallic,
// emissive rgb: 8 floats), then per stream: uvs as 16-bit inside their own bounds, colours as rgba8, the texture
// as a png (u16 w, u16 h, u32 length, bytes). v1 files still read.
static const char MAGIC_V1[6] = { 'C', 'H', 'E', 'N', 'G', 1 };
static const char MAGIC_V2[6] = { 'C', 'H', 'E', 'N', 'G', 2 };
static glm::vec2 octEncode(glm::vec3 n) {
    n /= (std::abs(n.x) + std::abs(n.y) + std::abs(n.z) + 1e-9f);
    glm::vec2 e(n.x, n.y);
    if (n.z < 0) e = (1.f - glm::abs(glm::vec2(e.y, e.x))) * glm::vec2(e.x >= 0 ? 1 : -1, e.y >= 0 ? 1 : -1);
    return e;
}
static glm::vec3 octDecode(glm::vec2 e) {
    glm::vec3 n(e.x, e.y, 1.f - std::abs(e.x) - std::abs(e.y));
    if (n.z < 0) { float ox = n.x, oy = n.y; n.x = (1.f - std::abs(oy)) * (ox >= 0 ? 1 : -1); n.y = (1.f - std::abs(ox)) * (oy >= 0 ? 1 : -1); }
    return glm::normalize(n);
}
template <class T> static void put(std::ostream& o, const T& v) { o.write(reinterpret_cast<const char*>(&v), sizeof v); }
template <class T> static bool get(std::istream& i, T& v) { return bool(i.read(reinterpret_cast<char*>(&v), sizeof v)); }

bool writeCheng(const fs::path& path, const Mesh& m, const std::string& name) {
    std::ofstream out(path, std::ios::binary);
    if (!out) return false;
    glm::vec3 lo(1e30f), hi(-1e30f);
    for (auto& p : m.positions) { lo = glm::min(lo, p); hi = glm::max(hi, p); }
    glm::vec3 size = glm::max(hi - lo, glm::vec3(1e-6f));
    uint32_t nv = uint32_t(m.positions.size()), ni = uint32_t(m.indices.size());
    uint8_t nameLen = uint8_t(std::min<size_t>(name.size(), 255));
    const bool uv = m.hasUvs(), col = m.hasColors(), tex = m.material.hasTexture();
    out.write(MAGIC_V2, 6); put(out, nv); put(out, ni); put(out, lo); put(out, hi);
    put(out, nameLen); out.write(name.data(), nameLen);
    uint8_t flags = uint8_t((uv ? 1 : 0) | (col ? 2 : 0) | (tex ? 4 : 0)); put(out, flags);
    float mat[8] = { m.material.base.r, m.material.base.g, m.material.base.b, m.material.roughness, m.material.metallic, m.material.emissive.r, m.material.emissive.g, m.material.emissive.b };
    out.write(reinterpret_cast<const char*>(mat), sizeof mat);
    std::vector<uint16_t> q; q.reserve(size_t(nv) * 5);
    for (uint32_t i = 0; i < nv; i++) {
        glm::vec3 t = (m.positions[i] - lo) / size;
        for (int k = 0; k < 3; k++) q.push_back(uint16_t(std::lround(glm::clamp(t[k], 0.f, 1.f) * 65535.f)));
        glm::vec2 e = octEncode(m.normals.size() > i ? m.normals[i] : glm::vec3(0, 1, 0));
        q.push_back(uint16_t(std::lround((glm::clamp(e.x, -1.f, 1.f) * 0.5f + 0.5f) * 65535.f)));
        q.push_back(uint16_t(std::lround((glm::clamp(e.y, -1.f, 1.f) * 0.5f + 0.5f) * 65535.f)));
    }
    out.write(reinterpret_cast<const char*>(q.data()), std::streamsize(q.size() * 2));
    if (uv) {
        glm::vec2 tlo(1e30f), thi(-1e30f); for (auto& t : m.uvs) { tlo = glm::min(tlo, t); thi = glm::max(thi, t); }
        glm::vec2 tsize = glm::max(thi - tlo, glm::vec2(1e-6f)); put(out, tlo); put(out, thi);
        std::vector<uint16_t> tq; tq.reserve(size_t(nv) * 2);
        for (auto& t : m.uvs) { glm::vec2 u = (t - tlo) / tsize; tq.push_back(uint16_t(std::lround(glm::clamp(u.x, 0.f, 1.f) * 65535.f))); tq.push_back(uint16_t(std::lround(glm::clamp(u.y, 0.f, 1.f) * 65535.f))); }
        out.write(reinterpret_cast<const char*>(tq.data()), std::streamsize(tq.size() * 2));
    }
    if (col) {
        std::vector<uint8_t> cq; cq.reserve(size_t(nv) * 4);
        for (auto& c : m.colors) for (int k = 0; k < 4; k++) cq.push_back(uint8_t(std::lround(glm::clamp(c[k], 0.f, 1.f) * 255.f)));
        out.write(reinterpret_cast<const char*>(cq.data()), std::streamsize(cq.size()));
    }
    if (tex) {
        std::vector<uint8_t> png;
        stbi_write_png_to_func([](void* ctx, void* data, int n) { auto* v = static_cast<std::vector<uint8_t>*>(ctx); v->insert(v->end(), (uint8_t*)data, (uint8_t*)data + n); }, &png, m.material.texW, m.material.texH, 4, m.material.rgba.data(), m.material.texW * 4);
        uint16_t w = uint16_t(m.material.texW), h = uint16_t(m.material.texH); uint32_t len = uint32_t(png.size());
        put(out, w); put(out, h); put(out, len); out.write(reinterpret_cast<const char*>(png.data()), std::streamsize(png.size()));
    }
    out.write(reinterpret_cast<const char*>(m.indices.data()), std::streamsize(ni * 4));
    return bool(out);
}

std::optional<Mesh> readCheng(const fs::path& path, std::string* name) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return std::nullopt;
    char magic[6]; in.read(magic, 6);
    if (!in) return std::nullopt;
    int version = std::memcmp(magic, MAGIC_V1, 6) == 0 ? 1 : std::memcmp(magic, MAGIC_V2, 6) == 0 ? 2 : 0;
    if (!version) return std::nullopt;
    uint32_t nv, ni; glm::vec3 lo, hi; uint8_t nameLen;
    get(in, nv); get(in, ni); get(in, lo); get(in, hi); get(in, nameLen);
    if (!in || nv == 0 || nv > 2'000'000 || ni > 6'000'000 || ni % 3) return std::nullopt;
    std::string nm(nameLen, '\0'); in.read(nm.data(), nameLen); if (name) *name = nm;
    Mesh m; uint8_t flags = 0;
    if (version >= 2) {
        get(in, flags); float mat[8]; in.read(reinterpret_cast<char*>(mat), sizeof mat); if (!in) return std::nullopt;
        m.material.base = glm::vec3(mat[0], mat[1], mat[2]); m.material.roughness = glm::clamp(mat[3], 0.f, 1.f); m.material.metallic = glm::clamp(mat[4], 0.f, 1.f); m.material.emissive = glm::vec3(mat[5], mat[6], mat[7]);
    }
    std::vector<uint16_t> q(size_t(nv) * 5); in.read(reinterpret_cast<char*>(q.data()), std::streamsize(q.size() * 2));
    m.positions.resize(nv); m.normals.resize(nv); m.indices.resize(ni);
    if (flags & 1) {
        glm::vec2 tlo, thi; get(in, tlo); get(in, thi); std::vector<uint16_t> tq(size_t(nv) * 2); in.read(reinterpret_cast<char*>(tq.data()), std::streamsize(tq.size() * 2));
        glm::vec2 tsize = thi - tlo; m.uvs.resize(nv);
        for (uint32_t i = 0; i < nv; i++) m.uvs[i] = tlo + tsize * glm::vec2(tq[i * 2] / 65535.f, tq[i * 2 + 1] / 65535.f);
    }
    if (flags & 2) {
        std::vector<uint8_t> cq(size_t(nv) * 4); in.read(reinterpret_cast<char*>(cq.data()), std::streamsize(cq.size())); m.colors.resize(nv);
        for (uint32_t i = 0; i < nv; i++) m.colors[i] = glm::vec4(cq[i * 4] / 255.f, cq[i * 4 + 1] / 255.f, cq[i * 4 + 2] / 255.f, cq[i * 4 + 3] / 255.f);
    }
    if (flags & 4) {
        uint16_t w, h; uint32_t len; get(in, w); get(in, h); get(in, len);
        if (!in || len == 0 || len > 64u * 1024 * 1024) return std::nullopt;
        std::vector<uint8_t> png(len); in.read(reinterpret_cast<char*>(png.data()), std::streamsize(len));
        if (!in || !decodeImage(png.data(), png.size(), m.material) || m.material.texW != w || m.material.texH != h) return std::nullopt;
    }
    in.read(reinterpret_cast<char*>(m.indices.data()), std::streamsize(ni * 4));
    if (!in) return std::nullopt;
    glm::vec3 size = hi - lo;
    for (uint32_t i = 0; i < nv; i++) {
        m.positions[i] = lo + size * glm::vec3(q[i * 5] / 65535.f, q[i * 5 + 1] / 65535.f, q[i * 5 + 2] / 65535.f);
        m.normals[i] = octDecode(glm::vec2(q[i * 5 + 3] / 65535.f * 2 - 1, q[i * 5 + 4] / 65535.f * 2 - 1));
    }
    for (uint32_t i : m.indices) if (i >= nv) return std::nullopt;
    return m;
}

fs::path modelsDir() { fs::path p = vault::dir().parent_path() / "models"; std::error_code ec; fs::create_directories(p, ec); return p; }

std::optional<fs::path> convertToCheng(const fs::path& src, std::string& err, int maxVerts) {
    auto raw = importModel(src, err);
    if (!raw) return std::nullopt;
    Mesh m = prepareModel(std::move(*raw), maxVerts);
    if (m.indices.empty()) { err = "nothing left after cleaning " + src.filename().string(); return std::nullopt; }
    fs::path out = modelsDir() / (src.stem().string() + ".cheng");
    if (!writeCheng(out, m, src.stem().string())) { err = "cannot write " + out.string(); return std::nullopt; }
    return out;
}

} // namespace chaos::model
