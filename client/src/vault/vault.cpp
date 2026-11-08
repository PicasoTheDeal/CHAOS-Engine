#include "vault/vault.h"
#include "seed/base58.h"
#include "blake3.h"
#include <fstream>
#include <cstdlib>
#include <cstring>
#include <chrono>
#include <stdexcept>

namespace chaos::vault {

namespace fs = std::filesystem;

static fs::path xdg(const char* var, const char* fallback) {
    const char* v = std::getenv(var);
    if (v && *v) return fs::path(v);
    const char* home = std::getenv("HOME");
    return fs::path(home ? home : ".") / fallback;
}

static fs::path g_dir;
void setDir(const fs::path& p) { g_dir = p; }
fs::path dir() {
    fs::path p = g_dir.empty() ? xdg("XDG_DATA_HOME", ".local/share") / "chaos" / "vault" : g_dir;
    std::error_code ec; fs::create_directories(p, ec);
    return p;
}

fs::path configDir() {
    fs::path p = xdg("XDG_CONFIG_HOME", ".config") / "chaos";
    fs::create_directories(p);
    return p;
}

std::array<uint8_t, 16> instanceId() {
    static std::array<uint8_t, 16> id{};
    static bool loaded = false;
    if (loaded) return id;
    fs::path p = configDir() / "instance";
    std::ifstream in(p, std::ios::binary);
    if (in && in.read(reinterpret_cast<char*>(id.data()), 16)) { loaded = true; return id; }
    std::ifstream ur("/dev/urandom", std::ios::binary);
    if (!ur.read(reinterpret_cast<char*>(id.data()), 16)) throw std::runtime_error("cannot read /dev/urandom");
    std::ofstream out(p, std::ios::binary);
    out.write(reinterpret_cast<const char*>(id.data()), 16);
    loaded = true;
    return id;
}

chaos_file_t makeFile(const Dimension& d, const std::vector<uint8_t>& png, const cf::Ticket* ticket, const uint8_t* clientRandom) {
    chaos_file_t f{};
    f.magic = CHAOS_MAGIC;
    f.version = CHAOS_VERSION;
    f.state = ticket ? CHAOS_STATE_ROLLED : CHAOS_STATE_PREVIEW;
    std::memcpy(f.seed_full, d.seed.data(), 32);
    auto inst = instanceId();
    std::memcpy(f.instance_id, inst.data(), 16);
    if (ticket) {
        cf::putTicket(f, *ticket);
        std::memcpy(f.client_random, clientRandom, 32);
    } else {
        // a preview roll has no ticket: the seed came straight from os entropy, so that is what client_random holds
        std::memcpy(f.client_random, d.seed.data(), 32);
    }
    f.family = uint8_t(d.family);
    f.tier = uint8_t(int(d.tier));
    f.mode = uint8_t(int(d.nativeMode));
    std::memcpy(&f.mutation_seed, d.seed.data(), 4);
    f.claimed_at_ms = 0;
    f.owner_id = 0;
    std::string disp = base58Encode(d.seed.data(), 32);
    f.display_len = uint8_t(disp.size());
    std::memcpy(f.seed_display, disp.data(), disp.size());
    if (!png.empty()) {
        blake3_hasher h; blake3_hasher_init(&h);
        blake3_hasher_update(&h, png.data(), png.size());
        blake3_hasher_finalize(&h, f.preview_hash, 32);
    }
    return f;
}

std::filesystem::path pathFor(const chaos_file_t& f) {
    Seed32 s; std::memcpy(s.data(), f.seed_full, 32);
    return dir() / (toShortHex(s) + ".chaos");
}

KeptEntry keep(const Dimension& d, const std::vector<uint8_t>& png, const cf::Ticket* ticket, const uint8_t* clientRandom) {
    return store(makeFile(d, png, ticket, clientRandom), png);
}

KeptEntry store(const chaos_file_t& f, const std::vector<uint8_t>& png) {
    KeptEntry e{ pathFor(f), pathFor(f).replace_extension(".png") };
    std::ofstream c(e.chaosPath, std::ios::binary);
    if (!c.write(reinterpret_cast<const char*>(&f), sizeof f)) throw std::runtime_error("cannot write " + e.chaosPath.string());
    if (!png.empty()) {
        std::ofstream p(e.pngPath, std::ios::binary);
        if (!p.write(reinterpret_cast<const char*>(png.data()), std::streamsize(png.size()))) throw std::runtime_error("cannot write " + e.pngPath.string());
    }
    return e;
}

std::vector<Entry> list() {
    std::vector<Entry> out;
    for (const auto& ent : fs::directory_iterator(dir())) {
        if (ent.path().extension() != ".chaos") continue;
        std::ifstream in(ent.path(), std::ios::binary);
        chaos_file_t f{};
        if (!in.read(reinterpret_cast<char*>(&f), sizeof f)) continue;
        if (f.magic != CHAOS_MAGIC || f.version != CHAOS_VERSION) continue;
        out.push_back({ ent.path().stem().string(), f });
    }
    return out;
}

bool has(const Seed32& seed) {
    for (const auto& e : list()) if (std::memcmp(e.file.seed_full, seed.data(), 32) == 0) return true;
    return false;
}

} // namespace chaos::vault
