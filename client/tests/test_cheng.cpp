#include <catch2/catch_test_macros.hpp>
#include <fstream>
#include <filesystem>
#include <cstdlib>
#include "model/cheng.h"
#include "shape/parametric.h"

using namespace chaos;
namespace fs = std::filesystem;

TEST_CASE("obj in, cheng out, same shape back") {
    fs::path tmp = fs::temp_directory_path() / "chaos-cheng-test"; fs::create_directories(tmp);
    setenv("XDG_DATA_HOME", tmp.c_str(), 1);
    // a cube with quads and negative-free indices, no normals
    std::ofstream o(tmp / "cube.obj");
    o << "v -1 -1 -1\nv 1 -1 -1\nv 1 1 -1\nv -1 1 -1\nv -1 -1 1\nv 1 -1 1\nv 1 1 1\nv -1 1 1\n"
         "f 1 2 3 4\nf 5 8 7 6\nf 1 5 6 2\nf 2 6 7 3\nf 3 7 8 4\nf 5 1 4 8\n";
    o.close();
    std::string err;
    auto out = model::convertToCheng(tmp / "cube.obj", err);
    REQUIRE(out.has_value());
    REQUIRE(fs::exists(*out));
    std::string name;
    auto m = model::readCheng(*out, &name);
    REQUIRE(m.has_value());
    REQUIRE(name == "cube");
    REQUIRE(m->indices.size() == 36);                       // six quads, two triangles each
    REQUIRE(m->positions.size() <= 24);
    REQUIRE(std::abs(m->boundingRadius() - 4.5f) < 0.02f);  // sized for the stage
    for (auto& n : m->normals) REQUIRE(std::abs(glm::length(n) - 1.f) < 0.01f);
}

TEST_CASE("decimation lands under the cap") {
    Mesh s = sphere(1.f, 200, 200);                         // 40k+ vertices
    Mesh p = model::prepareModel(s, 5000);
    REQUIRE(p.positions.size() <= 5000);
    REQUIRE(p.indices.size() % 3 == 0);
    INFO("verts " << p.positions.size() << " idx " << p.indices.size() << " from " << s.positions.size() << "/" << s.indices.size());
    REQUIRE(p.indices.size() > 1000);
}

TEST_CASE("garbage is refused") {
    fs::path tmp = fs::temp_directory_path() / "chaos-cheng-test"; fs::create_directories(tmp);
    { std::ofstream o(tmp / "bad.cheng"); o << "not a cheng"; }
    REQUIRE_FALSE(model::readCheng(tmp / "bad.cheng").has_value());
    std::string err;
    REQUIRE_FALSE(model::importModel(tmp / "missing.obj", err).has_value());
    REQUIRE(err.rfind("no such file", 0) == 0);
    { std::ofstream o(tmp / "nope.xyz"); o << "?"; }
    REQUIRE_FALSE(model::importModel(tmp / "nope.xyz", err).has_value());
    REQUIRE(err.find("unknown model format") != std::string::npos);
}

#include "util/png.h"

TEST_CASE("a textured obj keeps its uvs, texture and material through the cheng") {
    fs::path tmp = fs::temp_directory_path() / "chaos-cheng-test"; fs::create_directories(tmp);
    setenv("XDG_DATA_HOME", tmp.c_str(), 1);
    // a 4x4 texture, red on the left half, blue on the right
    std::vector<uint8_t> rgb; for (int y = 0; y < 4; y++) for (int x = 0; x < 4; x++) { rgb.push_back(x < 2 ? 255 : 0); rgb.push_back(0); rgb.push_back(x < 2 ? 0 : 255); }
    auto png = encodePng(rgb.data(), 4, 4); REQUIRE(writeFile((tmp / "skin.png").string(), png.data(), png.size()));
    { std::ofstream o(tmp / "quad.mtl"); o << "newmtl skin\nKd 0.5 0.6 0.7\nNs 250\nmap_Kd skin.png\n"; }
    { std::ofstream o(tmp / "quad.obj"); o << "mtllib quad.mtl\nusemtl skin\nv 0 0 0\nv 1 0 0\nv 1 1 0\nv 0 1 0\nvt 0 0\nvt 1 0\nvt 1 1\nvt 0 1\nf 1/1 2/2 3/3 4/4\n"; }
    std::string err;
    auto out = model::convertToCheng(tmp / "quad.obj", err);
    REQUIRE(out.has_value());
    auto m = model::readCheng(*out);
    REQUIRE(m.has_value());
    REQUIRE(m->hasUvs());
    REQUIRE_FALSE(m->hasColors());                                   // one material: the colour sits in the material, no array
    REQUIRE(m->material.hasTexture());
    REQUIRE(m->material.texW == 4); REQUIRE(m->material.texH == 4);
    REQUIRE(m->material.rgba[0] == 255); REQUIRE(m->material.rgba[3 * 4 + 2] == 255);   // left red, right blue, rows top first
    REQUIRE(std::abs(m->material.base.g - 0.6f) < 0.01f);
    REQUIRE(m->material.roughness < 0.6f);                           // Ns 250 is fairly shiny
    REQUIRE(m->hasOwnLook());
    // obj's v runs up, the cheng's runs down: the corner at vt (0,0) reads back as (0,1)
    bool corner = false; for (auto& t : m->uvs) if (std::abs(t.x) < 0.01f && std::abs(t.y - 1.f) < 0.01f) corner = true;
    REQUIRE(corner);
}

TEST_CASE("two materials fold into vertex colours, and a plain mesh carries no look") {
    fs::path tmp = fs::temp_directory_path() / "chaos-cheng-test"; fs::create_directories(tmp);
    setenv("XDG_DATA_HOME", tmp.c_str(), 1);
    { std::ofstream o(tmp / "two.mtl"); o << "newmtl a\nKd 1 0 0\nnewmtl b\nKd 0 0 1\n"; }
    { std::ofstream o(tmp / "two.obj"); o << "mtllib two.mtl\nv 0 0 0\nv 1 0 0\nv 1 1 0\nv 0 1 0\nv 2 0 0\nv 2 1 0\nusemtl a\nf 1 2 3 4\nusemtl b\nf 2 5 6 3\n"; }
    std::string err;
    auto out = model::convertToCheng(tmp / "two.obj", err);
    REQUIRE(out.has_value());
    auto m = model::readCheng(*out);
    REQUIRE(m.has_value());
    REQUIRE(m->hasColors());
    REQUIRE_FALSE(m->material.hasTexture());
    int red = 0, blue = 0; for (auto& c : m->colors) { if (c.r > 0.9f && c.b < 0.1f) red++; if (c.b > 0.9f && c.r < 0.1f) blue++; }
    REQUIRE(red == 4); REQUIRE(blue == 4);                           // the shared edge stays two vertices per side: a material edge is a seam
    REQUIRE(m->hasOwnLook());
    Mesh plain = model::prepareModel(sphere(1.f, 8, 8), 5000);
    REQUIRE_FALSE(plain.hasOwnLook());
}
