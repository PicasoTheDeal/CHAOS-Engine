#include "cli.h"
#include "util/duration.h"
#include <filesystem>
#include "model/cheng.h"
#include <cstdio>
#include <cctype>
#include <cstring>
#include <cstdlib>
#include <string>
#include "seed/base58.h"
#include "scene/dimension.h"
#include "vault/vault.h"
#include "vault/activation.h"
#include "settings.h"
#include "sync/api.h"
#include "sync/session_flow.h"
#include "server_pubkey.h"

namespace chaos::cli {

void usage() {
    std::puts("chaos [--seed <64 hex | base58 | 24 words>] [--shape N] [--mood 0..1] [--mode zen|calm|chaos|bleed]\n"
              "      [--commit out.reveal] [--ticket t.ticket --reveal out.reveal] [--shot out.png] [--keep] [--list-shapes] [--vault] [--verify f.chaos] [--import f.chaos]\n"
              "      [--no-panel] [--no-poster] [--x11] [--registry URL] [--dev-offline]\n"
              "  online rule: generating (previews, rerolls, --seed, tickets) needs the registry reachable. a claimed file that\n"
              "  was loaded online once stays usable offline. --dev-offline lifts the gate for development.\n"
              "  --x11     run the normal window through x11 / xwayland instead of the wayland backend\n"
              "  --no-libdecor  wayland without libdecor (if startup hangs: its gtk plugin is the usual reason)\n"
              "chaos --login-token T | --login-dev NAME | --logout | --whoami        the registry session (token from <registry>/login?app=1)\n"
              "chaos --cheng <model file>            convert obj / stl / gltf / glb (blend, fbx, dae, step via blender) to a .cheng\n"
              "chaos --model <file.cheng | model file>   your own model on the stage (offline feature, a display choice)\n"
              "chaos --roll [--shot out.png] [--keep]      claimable roll in one go: commit, ticket, seed (needs a session)\n"
              "chaos --claim f.chaos                       claim a rolled vault file: signed copy stored, activated\n"
              "chaos --sync                                states for every vault seed, fetch what you own but do not hold\n"
              "chaos --wallpaper [--backend auto|wayland|x11] [--seed ...] [--mode ...] [--monitors mirror|each] [--wallpaper-fps N] [--frames N] [--no-pause]\n"
              "  --wallpaper  a real wallpaper: wayland layer shell natively (kde, sway, hyprland, river), else a desktop-type x11 window per monitor\n"
              "  --commit  draw client_random into <out.reveal>, print its commit (hex) to send with the ticket request, exit\n"
              "  --ticket  roll with a registry ticket + the matching --reveal file (the seed derives from both), the kept file is claimable\n"
              "  --keep    with --shot: also write the .chaos + png into the vault\n"
              "  --vault   list what the vault holds, verified against the registry key, and exit\n"
              "  --verify  check one .chaos against the registry key and exit\n"
              "  --import  verify a .chaos (eg one the registry handed back) and store it in the vault\n"
              "  keys: esc panel, R reroll, K keep, M mood, N mode, 1-4 mode, space pause, F fullscreen");
}


static int cmdVault() {
    auto entries = vault::list();
    std::printf("%zu in %s\n", entries.size(), vault::dir().c_str());
    for (const auto& e : entries) {
        auto c = cf::verify(e.file);
        std::printf("  %s  %-8s %-10s %-12s %-6s %s%s\n", e.name.c_str(), cf::stateName(e.file.state), tierName(Tier(e.file.tier)), familyName(e.file.family), modeName(Mode(e.file.mode)), c.ok() ? "valid" : "INVALID ", c.why.c_str());
    }
    return 0;
}

static int cmdVerify(const char* path) {
    chaos_file_t f{};
    if (!cf::read(path, f)) { std::fprintf(stderr, "cannot read %s (need exactly 512 bytes)\n", path); return 1; }
    auto c = cf::verify(f);
    Seed32 s; std::memcpy(s.data(), f.seed_full, 32);
    std::printf("%s  state %s  tier %s  family %s  mode %s  owner %llu\n", toShortHex(s).c_str(), cf::stateName(f.state), tierName(Tier(f.tier)), familyName(f.family), modeName(Mode(f.mode)), (unsigned long long)f.owner_id);
    std::printf("layout %s  ticket %s  commit %s  seed %s  cert %s  ->  %s%s\n", c.layout ? "ok" : "BAD", c.ticket ? "ok" : "BAD", c.commit ? "ok" : "BAD", c.seed ? "ok" : "BAD", c.cert ? "ok" : "BAD", c.ok() ? "VALID" : "INVALID: ", c.why.c_str());
    return c.ok() ? 0 : 2;
}

static std::string registryUrlFor(const Options& o) {
    Persisted ps = loadSettings();
    return !o.registryArg.empty() ? o.registryArg : (!ps.registryUrl.empty() ? ps.registryUrl : api::defaultRegistryUrl());
}

static int cmdImport(const Options& o, const char* path) {
    chaos_file_t f{};
    if (!cf::read(path, f)) { std::fprintf(stderr, "cannot read %s\n", path); return 1; }
    auto c = cf::verify(f);
    if (!c.ok()) { std::fprintf(stderr, "refused: %s\n", c.why.c_str()); return 2; }
    auto e = vault::store(f, {});
    std::printf("stored %s (%s)\n", e.chaosPath.c_str(), cf::stateName(f.state));
    // loading online: ask the registry about this copy and remember the answer, that is what unlocks offline use
    if (auto r = api::check(registryUrlFor(o), reinterpret_cast<const uint8_t*>(&f), sizeof f)) {
        Seed32 s; std::memcpy(s.data(), f.seed_full, 32);
        if (r->valid && r->known) { activation::record(s, { cf::nowMs(), r->owner, r->state, r->current, r->valueCents, "" }); std::printf("activated: owner %s, %s, value %.2f, this copy is %s\n", r->owner.c_str(), r->state.c_str(), r->valueCents / 100.0, r->current ? "current" : "STALE"); }
        else std::printf("registry: %s%s (not activated)\n", r->valid ? "valid but unknown" : "INVALID: ", r->valid ? "" : r->why.c_str());
    } else std::printf("registry unreachable: stored, not activated. load it again online to use it offline.\n");
    return 0;
}

bool parse(int argc, char** argv, Options& o, int& rc) {
    o.seed = randomSeed();
    const char* importPath = nullptr;
    for (int i = 1; i < argc; i++) {
        auto is = [&](const char* f) { return !std::strcmp(argv[i], f); };
        auto next = [&]() -> const char* { return i + 1 < argc ? argv[++i] : nullptr; };
        if (is("--seed") && i + 1 < argc) { o.seedArg = next(); o.seed = seedFromString(o.seedArg); }
        else if (is("--wallpaper")) o.wallpaper = true;
        else if (is("--x11")) o.x11 = true;
        else if (is("--registry") && i + 1 < argc) o.registryArg = next();
        else if (is("--dev-offline")) o.devOffline = true;
        else if (is("--login-token") && i + 1 < argc) o.loginToken = next();
        else if (is("--login-dev") && i + 1 < argc) o.loginDev = next();
        else if (is("--logout")) o.doLogout = true;
        else if (is("--whoami")) o.doWhoami = true;
        else if (is("--roll")) o.doRoll = true;
        else if (is("--claim") && i + 1 < argc) o.claimPath = next();
        else if (is("--sync")) o.doSync = true;
        else if (is("--no-libdecor")) o.noLibdecor = true;
        else if (is("--monitors") && i + 1 < argc) o.wa.monitors = next();
        else if (is("--backend") && i + 1 < argc) o.wa.backend = next();
        else if (is("--wallpaper-fps") && i + 1 < argc) o.wa.fps = std::atoi(next());
        else if (is("--frames") && i + 1 < argc) o.wa.frames = std::atoi(next());
        else if (is("--no-pause")) o.wa.noPause = true;
        else if (is("--keep")) o.keep = true;
        else if (is("--no-panel")) o.noPanel = true;
        else if (is("--no-poster")) o.noPoster = true;
        else if (is("--ticket") && i + 1 < argc) o.ticketPath = next();
        else if (is("--reveal") && i + 1 < argc) o.revealPath = next();
        else if (is("--commit") && i + 1 < argc) o.commitPath = next();
        else if (is("--shot") && i + 1 < argc) o.shotPath = next();
        else if (is("--cheng") && i + 1 < argc) o.chengSrc = next();
        else if (is("--model") && i + 1 < argc) o.modelArg = next();
        else if (is("--bench") && i + 1 < argc) { o.bench = std::atoi(next()); o.shotPath = "/dev/null"; }
        else if (is("--ui-shot") && i + 1 < argc) o.uiShotPath = next();
        else if (is("--shape") && i + 1 < argc) o.forcedShape = std::atoi(next());
        else if (is("--mood") && i + 1 < argc) o.forcedMood = float(std::atof(next()));
        else if (is("--mode") && i + 1 < argc) { o.modeArg = next(); if (!parseMode(o.modeArg, o.forcedMode)) { usage(); rc = 1; return true; } o.hasMode = true; }
        else if (is("--version")) { std::printf("chaos %s (.chaos format v%u)\n", CHAOS_VERSION_STRING, CHAOS_VERSION); rc = 0; return true; }
        else if (is("--list-shapes")) { for (int s = 0; s < SHAPE_COUNT; s++) std::printf("%2d  %s\n", s, shapeName(s)); rc = 0; return true; }
        else if (is("--vault")) { rc = cmdVault(); return true; }
        else if (is("--verify") && i + 1 < argc) { rc = cmdVerify(next()); return true; }
        else if (is("--import") && i + 1 < argc) importPath = next();
        else { usage(); rc = 1; return true; }
    }
    if (importPath) { rc = cmdImport(o, importPath); return true; }
    return false;
}

RegistryState probeRegistry(const Options& o) {
    RegistryState r;
    r.url = registryUrlFor(o);
    r.devOffline = o.devOffline;
    r.online = api::health(r.url).has_value();
    std::printf("registry %s  ->  %s%s\n", r.url.c_str(), r.online ? "online" : "OFFLINE", r.devOffline ? " (dev-offline: gate lifted)" : "");
    if (r.online) { if (auto tok = api::sessionToken()) { auto who = api::me(r.url, *tok); r.signedIn = bool(who); } }
    return r;
}

bool runCommands(Options& o, const RegistryState& reg, int& rc) {
    const std::string& registryUrl = reg.url;
    if (o.doLogout) { api::clearSession(); std::puts("signed out"); rc = 0; return true; }
    if (!o.loginDev.empty()) {
        auto r = api::devLogin(registryUrl, o.loginDev);
        if (!r) { std::fprintf(stderr, "%s\n", r.error ? r.error->message.c_str() : "login failed"); rc = 2; return true; }
        o.loginToken = *r.value;
    }
    if (!o.loginToken.empty()) {
        while (!o.loginToken.empty() && std::isspace((unsigned char)o.loginToken.back())) o.loginToken.pop_back();
        auto who = api::me(registryUrl, o.loginToken);
        if (!who) { std::fprintf(stderr, "that token does not work here: %s\n", who.error ? who.error->message.c_str() : "?"); rc = 2; return true; }
        api::saveSession(o.loginToken);
        std::printf("signed in as %s (%s), %d/%d claimable rolls in the bank\n", who.value->name.c_str(), who.value->email.c_str(), who.value->banked, who.value->max);
        if (!o.doRoll && o.claimPath.empty() && !o.doSync) { rc = 0; return true; }
    }
    auto token = api::sessionToken();
    auto needSession = [&](const char* what) -> int {
        if (!reg.online) { std::fprintf(stderr, "offline: %s needs the registry\n", what); return 3; }
        if (!token) { std::fprintf(stderr, "%s needs a session: sign in at %s/login?app=1 and run chaos --login-token <token>\n", what, registryUrl.c_str()); return 2; }
        return 0;
    };
    if (o.doWhoami) {
        if ((rc = needSession("whoami"))) return true;
        auto who = api::me(registryUrl, *token);
        if (!who) { std::fprintf(stderr, "session rejected: %s\n", who.error ? who.error->message.c_str() : "?"); rc = 2; return true; }
        std::printf("%s (%s)  bank %d/%d%s\n", who.value->name.c_str(), who.value->email.c_str(), who.value->banked, who.value->max, who.value->nextInMs > 0 ? ("  next in " + humanMs(who.value->nextInMs)).c_str() : "");
        rc = 0; return true;
    }
    if (!o.claimPath.empty()) {
        if ((rc = needSession("claim"))) return true;
        chaos_file_t f{};
        if (!cf::read(o.claimPath, f)) { std::fprintf(stderr, "cannot read %s\n", o.claimPath.c_str()); rc = 1; return true; }
        auto out = flow::claimFile(registryUrl, *token, f);
        std::printf("%s\n", out.message.c_str());
        rc = out.ok ? 0 : 2; return true;
    }
    if (o.doSync) {
        if ((rc = needSession("sync"))) return true;
        std::printf("%s\n", flow::syncVault(registryUrl, *token).message.c_str());
        rc = 0; return true;
    }
    if (o.doRoll) {
        if ((rc = needSession("a claimable roll"))) return true;
        auto r = flow::rollClaimable(registryUrl, *token);
        if (!r) { std::fprintf(stderr, "%s\n", r.error ? r.error->message.c_str() : "roll failed"); rc = 2; return true; }
        o.seed = r.value->seed; o.ticket = r.value->ticket; std::memcpy(o.clientRandom, r.value->clientRandom, 32); o.hasTicket = true;
        std::printf("ticket   user %llu  ->  claimable roll (one go)\n", (unsigned long long)o.ticket.user_id);
    }
    if (!o.commitPath.empty()) {
        if (std::filesystem::exists(o.commitPath)) {
            std::fprintf(stderr, "%s already exists from an earlier --commit. if you already took a ticket for it on the vault page, roll with that ticket and this file (chaos --ticket chaos.ticket --reveal %s). a new commit would not match that ticket. if you really want a new one, delete the file first\n", o.commitPath.c_str(), o.commitPath.c_str());
            rc = 2; return true;
        }
        uint8_t cr[32]; cf::randomBytes(cr, 32);
        if (!cf::writeReveal(o.commitPath, cr)) { std::fprintf(stderr, "cannot write %s\n", o.commitPath.c_str()); rc = 1; return true; }
        for (uint8_t b : cf::commitOf(cr)) std::printf("%02x", b);
        std::printf("\n");
        rc = 0; return true;
    }
    if (!o.chengSrc.empty()) {
        std::string err; auto out = model::convertToCheng(o.chengSrc, err);
        if (!out) { std::fprintf(stderr, "%s\n", err.c_str()); rc = 2; return true; }
        std::string name; auto m = model::readCheng(*out, &name);
        std::printf("%s\n%zu verts  /  %zu triangles  /  %lld bytes%s%s\n", out->c_str(), m ? m->positions.size() : 0, m ? m->indices.size() / 3 : 0, (long long)std::filesystem::file_size(*out),
                    m && m->material.hasTexture() ? (std::string("  /  texture ") + std::to_string(m->material.texW) + "x" + std::to_string(m->material.texH)).c_str() : "",
                    m && m->hasColors() ? "  /  vertex colours" : m && m->hasOwnLook() ? "  /  one material" : "");
        rc = 0; return true;
    }
    if (o.wallpaper) { o.wa.seedHex = o.seedArg; o.wa.mode = o.modeArg; o.wa.online = reg.canGenerate(); rc = runWallpaper(o.wa); return true; }
    return false;
}

} // namespace chaos::cli
