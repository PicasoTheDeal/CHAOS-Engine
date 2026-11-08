// CHAOS engine v0.6: the dimension, the post stack, the panel, the poster.
#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <cstdio>
#include <cstring>
#include <string>
#include <algorithm>
#include <thread>
#include <chrono>

#include "app.h"
#include "platform/window.h"
#include "render/renderer.h"
#include "seed/seed.h"
#include "seed/base58.h"
#include "seed/bip39.h"
#include "scene/dimension.h"
#include "util/screenshot.h"
#include "util/png.h"
#include "vault/vault.h"
#include "chaos_file/format.h"
#include "server_pubkey.h"
#include "ui/panel.h"
#include "wallpaper.h"
#include "wallpaper_seed.h"
#include <ctime>
#include "model/cheng.h"
#include "platform/power.h"
#include <thread>
#include <cstdlib>
#include <filesystem>
#include <future>
#include "cli.h"
#include "sync/api.h"
#include "sync/session_flow.h"
#include "vault/activation.h"
#include "settings.h"

using namespace chaos;


static App* gApp = nullptr;
static Panel* gPanel = nullptr;

static void keyCallback(GLFWwindow*, int key, int, int action, int mods) {
    if (action != GLFW_PRESS || !gApp) return;
    if (gPanel && gPanel->wantsKeyboard() && key != GLFW_KEY_ESCAPE) return;
    App& a = *gApp; Dimension& d = *a.dim;
    switch (key) {
    case GLFW_KEY_ESCAPE: a.settings.panelOpen = !a.settings.panelOpen; break;
    case GLFW_KEY_R: a.reqReroll = true; break;
    case GLFW_KEY_K: a.reqKeep = true; break;
    case GLFW_KEY_M: d.cycleMood(); a.status = std::string("mood -> ") + moodNameOf(d.moodTarget); break;
    case GLFW_KEY_N: d.setDisplayMode(Mode((int(d.displayMode) + 1) % MODE_COUNT)); a.status = std::string("mode -> ") + modeName(d.displayMode); break;
    case GLFW_KEY_1: case GLFW_KEY_2: case GLFW_KEY_3: case GLFW_KEY_4:
        d.setDisplayMode(Mode(key - GLFW_KEY_1)); a.status = std::string("mode -> ") + modeName(d.displayMode); break;
    case GLFW_KEY_SPACE: a.settings.paused = !a.settings.paused; break;
    case GLFW_KEY_F: a.reqFullscreen = true; break;
    case GLFW_KEY_Q: if (mods & GLFW_MOD_CONTROL) a.reqQuit = true; break;
    }
}

// roll a dimension, record the stages for the poster, rebuild the renderer
// the model on the stage, if one is on: replaces the main shape of the current dimension and rebuilds the renderer
static void putModelOn(App& a, int w, int h) {
    if (!a.persisted.modelOn || !a.model || !a.dim) return;
    static unsigned salt = unsigned(std::time(nullptr));
    float rgb[3] = { 0.9f, 0.93f, 0.91f }; parseHexColor(a.persisted.modelColor, rgb);
    applyModel(*a.dim, *a.model, modelSkinIndex(a.persisted.modelSkin, salt), glm::vec3(rgb[0], rgb[1], rgb[2]));
    if (!a.persisted.modelSkinSeed.empty()) {
        try { Dimension src = rollDimension(seedFromHex(a.persisted.modelSkinSeed)); copyLook(*a.dim, src); }
        catch (...) { a.status = "skin seed is not valid hex, using the current seed's look"; }
    }
    a.renderer.reset();
    a.renderer = std::make_unique<Renderer>(*a.dim, w, h);
}
static bool loadModelFile(App& a) {
    a.model.reset(); a.modelName.clear();
    if (a.persisted.modelPath.empty()) return false;
    std::string name; auto m = model::readCheng(a.persisted.modelPath, &name);
    if (!m) return false;
    a.model = std::move(*m); a.modelName = name.empty() ? std::filesystem::path(a.persisted.modelPath).stem().string() : name;
    return true;
}

static void rollInto(App& a, const Seed32& seed, int forcedShape, float forcedMood, bool hasMode, Mode forcedMode, int w, int h) {
    a.boot.clear();
    a.bootStart = glfwGetTime();
    a.firstFrameAt = a.bootShown ? glfwGetTime() - 2.0 : -1;      // after the first boot the poster is already faded: no sheet over a roll or a load
    auto stage = [&](const char* key, const char* headline, const std::string& detail) { a.boot.push_back({ key, headline, detail, glfwGetTime() }); };
    stage("seed", "SEEDED", toShortHex(seed) + "…  /  " + (a.hasTicket ? "ticketed" : "preview"));
    a.renderer.reset();
    a.dim = std::make_unique<Dimension>(rollDimension(seed, forcedShape, forcedMood));
    Dimension& d = *a.dim;
    if (hasMode) d.setDisplayMode(forcedMode);
    char buf[256];
    std::snprintf(buf, sizeof buf, "%s %.2f  /  %s native", moodNameOf(d.moodTarget), d.moodTarget, modeName(d.nativeMode)); stage("mood", moodNameOf(d.moodTarget), buf);
    std::snprintf(buf, sizeof buf, "tempo %.2f%s", d.paletteTempo, d.unlocks.thirdStop ? "  /  three stops" : ""); stage("pal", "DUOTONE", buf);
    std::snprintf(buf, sizeof buf, "%02d/%d  /  %zu verts  /  r %.1f  /  %s", d.shapeType, SHAPE_COUNT - 1, d.mainMesh.vertexCount(), d.mainRadius, tierName(d.tier)); stage("shape", shapeName(d.shapeType), buf);
    std::string lin; for (size_t i = 0; i < d.lineage.size(); i++) lin += (i ? " -> " : "") + d.lineage[i];
    stage("mut", "MUTATING", lin);
    std::snprintf(buf, sizeof buf, "%zu satellites  /  %zu particles  /  %zu rings", d.sats.size(), d.particles.pos.size(), d.rings.size()); stage("orbit", "ORBITS", buf);
    a.renderer = std::make_unique<Renderer>(d, w, h);
    putModelOn(a, w, h);
    if (a.persisted.modelOn && a.model) stage("model", "YOUR MODEL", a.modelName + "  /  " + std::to_string(a.model->positions.size()) + " verts  /  " + a.persisted.modelSkin);
    stage("gpu", "COMPILING", "berry skin  /  duotone  /  bloom  /  afterimage");
    a.kept.reset();
    a.simTime = 0;
    std::printf("seed     %s\n", toHex(seed).c_str());
    std::printf("base58   %s\n", base58Encode(seed.data(), 32).c_str());
    std::printf("words    %s\n", bip39Encode(seed).c_str());
    std::printf("rarity   %s%s  /  family %d %s  /  score %.2f\n", tierName(d.tier), d.unlocks.gold ? " (gold)" : "", d.family, familyName(d.family), d.score);
    std::printf("shape    %02d/%d %s  /  %zu verts  /  r %.2f\n", d.shapeType, SHAPE_COUNT - 1, shapeName(d.shapeType), d.mainMesh.vertexCount(), d.mainRadius);
    std::printf("lineage  %s\n", lin.c_str());
    std::printf("mode     %s native%s\n", modeName(d.nativeMode), d.displayMode != d.nativeMode ? (std::string(", showing ") + modeName(d.displayMode)).c_str() : "");
    std::printf("mood     %s %.2f%s\n", moodNameOf(d.moodTarget), d.moodTarget, d.moodLocked ? " (pinned)" : "");
}

static bool loadTicket(App& a, const std::string& path, const std::string& revealPath) {
    cf::Ticket t{};
    if (!cf::readTicket(path, t)) { a.status = "cannot read ticket " + path; return false; }
    if (!cf::verifyTicketSig(t, CHAOS_SERVER_PUBKEY)) { a.status = "ticket signature does not check against the registry key"; return false; }
    if (!cf::readReveal(revealPath, a.clientRandom)) { a.status = "cannot read reveal " + revealPath + " (made by --commit)"; return false; }
    if (std::memcmp(cf::commitOf(a.clientRandom).data(), t.commit, 32) != 0) { a.status = "this reveal is not the one this ticket was issued for"; return false; }
    a.ticket = t; a.ticketPath = path; a.revealPath = revealPath; a.hasTicket = true;
    return true;
}

int main(int argc, char** argv) {
    cli::Options o; int rc = 0;
    if (cli::parse(argc, argv, o, rc)) return rc;
    cli::RegistryState reg = cli::probeRegistry(o);
    if (cli::runCommands(o, reg, rc)) return rc;

    // the window (or the offscreen render) from here. the names below are what the loop grew up with.
    Seed32 seed = o.seed;
    const std::string &seedArg = o.seedArg, &shotPath = o.shotPath, &ticketPath = o.ticketPath, &revealPath = o.revealPath, &uiShotPath = o.uiShotPath, &registryUrl = reg.url;
    const int forcedShape = o.forcedShape; const float forcedMood = o.forcedMood; const Mode forcedMode = o.forcedMode;
    const bool hasMode = o.hasMode, keep = o.keep, noPanel = o.noPanel, noPoster = o.noPoster, x11 = o.x11, noLibdecor = o.noLibdecor, devOffline = o.devOffline;
    const bool online = reg.online, canGenerate = reg.canGenerate();
    cf::Ticket ticket = o.ticket; bool hasTicket = o.hasTicket; uint8_t clientRandom[32]; std::memcpy(clientRandom, o.clientRandom, 32);
    auto token = api::sessionToken();
    std::string startNote;
    if (!canGenerate) {
        // no session (or no registry): nothing is generated. a kept or activated seed may play; otherwise the lobby, so the
        // window still opens and the account tab is reachable. a ticket roll is the one thing that cannot wait
        if (!ticketPath.empty()) { std::fprintf(stderr, "a claimable roll needs the registry and a session: sign in at %s/login?app=1 and run chaos --login-token <token>\n", registryUrl.c_str()); return 3; }
        auto picked = pickWallpaperSeed(seedArg, false, startNote);
        if (!picked) { picked = lobbySeed(); startNote = online ? "sign in (account tab) to roll. this is the lobby." : "offline: nothing is generated. this is the lobby."; }
        seed = *picked;
    }

    const bool offscreen = !shotPath.empty();
    const int W0 = offscreen ? 1920 : 1280, H0 = offscreen ? 1080 : 720;
    WindowOptions wo; wo.width = W0; wo.height = H0; wo.visible = !offscreen && uiShotPath.empty(); wo.forceX11 = x11; wo.noLibdecor = noLibdecor;
    Window window(wo);
    App app(window);
    app.persisted = loadSettings();
    if (!app.persisted.vaultDir.empty()) vault::setDir(app.persisted.vaultDir);
    if (!o.modelArg.empty()) {
        // --model <file>: a .cheng, or anything the converter reads (converted into the models folder first)
        std::string err; std::filesystem::path mp = o.modelArg;
        if (mp.extension() != ".cheng") { auto out = model::convertToCheng(mp, err); if (!out) { std::fprintf(stderr, "%s\n", err.c_str()); return 2; } mp = *out; std::printf("converted -> %s\n", mp.c_str()); }
        app.persisted.modelPath = mp.string(); app.persisted.modelOn = true;
    }
    loadModelFile(app);
    if (autostartEnabled()) { setAutostart(false); std::printf("removed the old autostart entry (chaos --wallpaper): on kde the plasma plugin is the wallpaper now\n"); }
    app.settings.vsync = app.persisted.vsync; app.settings.fpsCap = app.persisted.fpsCap; app.settings.poster = app.persisted.poster;
    app.registryUrl = registryUrl; app.online = online; app.devOffline = devOffline;
    app.settings.panelOpen = !noPanel;
    if (noPoster) app.settings.poster = false;
    app.token = token;
    if (online && token) { auto who = api::me(registryUrl, *token); if (who) app.me = *who.value; }
    if (!startNote.empty()) app.status = startNote;
    if (hasTicket) { app.ticket = ticket; std::memcpy(app.clientRandom, clientRandom, 32); app.hasTicket = true; }
    if (!ticketPath.empty()) {
        if (revealPath.empty()) { std::fprintf(stderr, "--ticket needs --reveal <file from --commit>\n"); return 1; }
        if (!loadTicket(app, ticketPath, revealPath)) { std::fprintf(stderr, "%s\n", app.status.c_str()); return 2; }
        seed = cf::deriveSeed(app.ticket, app.clientRandom);
        std::printf("ticket   user %llu, issued %llu  ->  claimable roll\n", (unsigned long long)app.ticket.user_id, (unsigned long long)app.ticket.issued_ms);
    }
    rollInto(app, seed, forcedShape, forcedMood, hasMode, forcedMode, W0, H0);

    if (offscreen) {
        GLuint fbo, color, depth;
        glGenFramebuffers(1, &fbo); glGenRenderbuffers(1, &color); glGenRenderbuffers(1, &depth);
        glBindRenderbuffer(GL_RENDERBUFFER, color); glRenderbufferStorage(GL_RENDERBUFFER, GL_RGB8, W0, H0);
        glBindRenderbuffer(GL_RENDERBUFFER, depth); glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, W0, H0);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, color);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth);
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) { std::fprintf(stderr, "fbo incomplete\n"); return 1; }
        if (o.bench > 0) {
            // the benchmark: N frames at 1080p with the configured quality, gpu time per frame and resident memory
            app.renderer->setLayers(layersFromName(app.persisted.layers));
            app.renderer->setQuality({ app.persisted.renderScale, app.persisted.msaa, app.persisted.detail == "lite" });
            app.renderer->resize(W0, H0);
            for (int i = 0; i < 10; i++) app.renderer->frame(1.0f + i / 60.f, 1 / 60.f, fbo);
            glFinish();
            auto t0 = std::chrono::steady_clock::now();
            for (int i = 0; i < o.bench; i++) app.renderer->frame(2.0f + i / 60.f, 1 / 60.f, fbo);
            glFinish();
            double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count() / o.bench;
            long rssKb = 0; if (FILE* f = std::fopen("/proc/self/status", "r")) { char line[256]; while (std::fgets(line, sizeof line, f)) if (std::sscanf(line, "VmRSS: %ld", &rssKb) == 1) break; std::fclose(f); }
            std::printf("bench  %d frames  %.2f ms/frame  (%.0f fps)  scale %.2f  msaa %d  detail %s  layers '%s'  rss %ld MB\n", o.bench, ms, 1000.0 / ms, app.persisted.renderScale, app.persisted.msaa, app.persisted.detail.c_str(), app.persisted.layers.c_str(), rssKb / 1024);
            return 0;
        }
        for (int i = 0; i < 12; i++) app.renderer->frame(1.0f + i / 60.f, 1 / 60.f, fbo);
        glFinish();
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        auto px = readFramebufferRgb(W0, H0);
        auto png = encodePng(px.data(), W0, H0);
        bool ok = writeFile(shotPath, png.data(), png.size());
        std::printf("%s %s\n", ok ? "wrote" : "failed", shotPath.c_str());
        if (ok && keep) { auto e = vault::keep(*app.dim, png, app.hasTicket ? &app.ticket : nullptr, app.clientRandom); std::printf("kept  %s (%s)\n", e.chaosPath.c_str(), app.hasTicket ? "rolled" : "preview"); }
        return ok ? 0 : 1;
    }

    gApp = &app;
    glfwSetKeyCallback(window.handle(), keyCallback);      // before the panel, so imgui chains to it
    Panel panel(window.handle());
    gPanel = &panel;

    int winX = 100, winY = 100, winW = W0, winH = H0;
    double lastT = glfwGetTime(), fpsAcc = 0; int fpsN = 0;
    while (!window.shouldClose() && !app.reqQuit) {
        window.poll();
        int w, h; window.framebufferSize(w, h);
        if (w == 0 || h == 0) continue;

        // ---- requests ----
        if (app.reqFullscreen) {
            app.reqFullscreen = false;
            GLFWwindow* win = window.handle();
            if (!app.settings.fullscreen) {
                glfwGetWindowPos(win, &winX, &winY); glfwGetWindowSize(win, &winW, &winH);
                int count = 0; GLFWmonitor** mons = glfwGetMonitors(&count);
                GLFWmonitor* mon = count > 0 ? mons[std::clamp(app.settings.monitor, 0, count - 1)] : glfwGetPrimaryMonitor();
                const GLFWvidmode* vm = glfwGetVideoMode(mon);
                glfwSetWindowMonitor(win, mon, 0, 0, vm->width, vm->height, vm->refreshRate);
                app.settings.fullscreen = true;
            } else {
                glfwSetWindowMonitor(win, nullptr, winX, winY, winW, winH, 0);
                app.settings.fullscreen = false;
            }
            continue;
        }
        if (!app.reqConnectToken.empty()) {
            auto who = api::me(app.registryUrl, app.reqConnectToken);
            if (who) { api::saveSession(app.reqConnectToken); app.token = app.reqConnectToken; app.me = *who.value; app.status = "connected as " + who.value->name; }
            else app.status = "that token does not work here: " + (who.error ? who.error->message : "?");
            app.reqConnectToken.clear();
        }
        if (app.reqLogout) { app.reqLogout = false; api::clearSession(); app.token.reset(); app.me.reset(); app.status = "signed out"; }
        // the registry, asked again every 30 s on a worker (and on request), so "online" is a live fact, not a boot-time one
        {
            static std::future<bool> probe; static double lastProbe = 0;
            double tnow = glfwGetTime();
            if (!probe.valid() && (app.reqCheckOnline || tnow - lastProbe > 30.0) && !app.devOffline) {
                lastProbe = tnow; app.reqCheckOnline = false;
                std::string url = app.registryUrl;
                probe = std::async(std::launch::async, [url] { try { return api::health(url).has_value(); } catch (...) { return false; } });
            }
            if (probe.valid() && probe.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
                bool on = probe.get();
                if (on != app.online) { app.online = on; app.status = on ? "registry online: " + app.registryUrl : "registry offline: " + app.registryUrl; }
                if (on && app.token && !app.me) { auto who = api::me(app.registryUrl, *app.token); if (who) app.me = *who.value; }
            }
        }
        if (!app.reqVaultDir.empty()) {
            std::string want = app.reqVaultDir; app.reqVaultDir.clear();
            if (want == "?") {
                // a folder dialog from whatever the desktop has; the path comes back on stdout
                const char* cmds[] = { "kdialog --getexistingdirectory \"$HOME\" 2>/dev/null", "zenity --file-selection --directory 2>/dev/null" };
                want.clear();
                for (const char* c : cmds) { if (FILE* f = popen(c, "r")) { char buf[4096]; if (std::fgets(buf, sizeof buf, f)) want = buf; pclose(f); if (!want.empty()) break; } }
                while (!want.empty() && (want.back() == '\n' || want.back() == '\r')) want.pop_back();
                if (want.empty()) app.status = "no folder picked (needs kdialog or zenity; or type the path and press use)";
            }
            if (!want.empty()) {
                std::error_code ec; std::filesystem::create_directories(want, ec);
                if (ec) app.status = "cannot create " + want + ": " + ec.message();
                else { app.persisted.vaultDir = want; vault::setDir(want); saveSettings(app.persisted); app.status = "vault folder: " + want; }
            }
        }
        if (!app.reqModelImport.empty()) {
            std::string want = app.reqModelImport; app.reqModelImport.clear();
            if (want == "?") {
                const char* cmds[] = { "kdialog --getopenfilename \"$HOME\" 'Models (*.cheng *.obj *.stl *.gltf *.glb *.blend *.fbx *.dae *.step *.stp *.usd *.usdz)' 2>/dev/null", "zenity --file-selection 2>/dev/null" };
                want.clear();
                for (const char* c : cmds) { if (FILE* f = popen(c, "r")) { char buf[4096]; if (std::fgets(buf, sizeof buf, f)) want = buf; pclose(f); if (!want.empty()) break; } }
                while (!want.empty() && (want.back() == '\n' || want.back() == '\r')) want.pop_back();
                if (want.empty()) app.status = "no file picked (needs kdialog or zenity; or type the path)";
            }
            if (!want.empty()) {
                std::string err; std::filesystem::path mp = want;
                if (mp.extension() != ".cheng") {
                    app.status = "converting " + mp.filename().string() + (model::needsBlender(mp) ? " through blender..." : "...");
                    auto out = model::convertToCheng(mp, err);
                    if (!out) { app.status = err; mp.clear(); } else mp = *out;
                }
                if (!mp.empty()) {
                    app.persisted.modelPath = mp.string(); app.persisted.modelOn = true;
                    if (loadModelFile(app)) {
                        if (app.model->hasOwnLook() && app.persisted.modelSkin != "own") app.persisted.modelSkin = "own";   // it brought a look: show it
                        saveSettings(app.persisted);
                        app.status = "model: " + app.modelName + ", " + std::to_string(app.model->positions.size()) + " verts" + (app.model->material.hasTexture() ? ", textured" : app.model->hasOwnLook() ? ", coloured" : "") + "  /  " + mp.string(); app.reqModelApply = true;
                    }
                    else app.status = "could not read " + mp.string();
                }
            }
        }
        if (app.reqModelRandom) {
            app.reqModelRandom = false;
            const char* skins[] = { "berry", "mono", "wire", "own" };
            app.persisted.modelSkin = skins[unsigned(glfwGetTime() * 1000) % (app.model && app.model->hasOwnLook() ? 4 : 3)];
            char hex[8]; std::snprintf(hex, sizeof hex, "#%02x%02x%02x", 90 + rand() % 166, 90 + rand() % 166, 90 + rand() % 166);
            app.persisted.modelColor = hex;
            app.reqModelApply = true;
        }
        if (app.reqModelApply) {
            app.reqModelApply = false;
            if (!app.persisted.modelOn || !app.model) { Seed32 s = app.dim->seed; rollInto(app, s, -1, -1, false, Mode::Chaos, w, h); }   // back to the seed's own shape
            else putModelOn(app, w, h);
        }
        if (app.reqSetDesktop) {
            app.reqSetDesktop = false;
            // kde: tell plasmashell to use the plugin on every desktop with this seed. qdbus6, qdbus or gdbus, whichever is there
            std::string hex = toHex(app.dim->seed), regUrl = app.registryUrl, modelPathSafe = app.persisted.modelPath;
            for (char& ch : regUrl) if (ch == '\'' || ch == '"' || ch == '\\') ch = '_';
            for (char& ch : modelPathSafe) if (ch == '\'' || ch == '"' || ch == '\\') ch = '_';     // a url never needs these; the script must stay well formed
            // the js uses double quotes only, so the whole thing can ride inside one single-quoted shell argument
            std::string script = "var ds = desktops(); for (var i = 0; i < ds.length; i++) { var d = ds[i]; d.wallpaperPlugin = \"org.chaos.wallpaper\"; d.currentConfigGroup = [\"Wallpaper\", \"org.chaos.wallpaper\", \"General\"]; d.writeConfig(\"Seed\", \"" + hex + "\"); d.writeConfig(\"Registry\", \"" + regUrl + "\"); d.writeConfig(\"ModelOn\", " + std::string(app.persisted.modelOn && app.model ? "true" : "false") + "); d.writeConfig(\"Model\", \"" + modelPathSafe + "\"); d.writeConfig(\"ModelSkin\", \"" + app.persisted.modelSkin + "\"); d.writeConfig(\"ModelColor\", \"" + app.persisted.modelColor + "\"); d.writeConfig(\"ModelSkinSeed\", \"" + app.persisted.modelSkinSeed + "\"); d.reloadConfig(); }";
            std::string q = "'" + script + "'";
            const char* tools[] = {
                "busctl --user call org.kde.plasmashell /PlasmaShell org.kde.PlasmaShell evaluateScript s ",
                "qdbus6 org.kde.plasmashell /PlasmaShell org.kde.PlasmaShell.evaluateScript ",
                "qdbus org.kde.plasmashell /PlasmaShell org.kde.PlasmaShell.evaluateScript ",
                "gdbus call --session --dest org.kde.plasmashell --object-path /PlasmaShell --method org.kde.PlasmaShell.evaluateScript " };
            bool ok = false; std::string lastErr;
            for (const char* tool : tools) {
                std::string cmd = std::string(tool) + q + " 2>&1";
                std::string out; if (FILE* f = popen(cmd.c_str(), "r")) { char b[512]; while (std::fgets(b, sizeof b, f)) out += b; int prc = pclose(f); if (prc == 0) { ok = true; break; } }
                if (!out.empty()) lastErr = out.substr(0, 160);
            }
            app.status = ok ? "desktop wallpaper set to " + toShortHex(app.dim->seed) + " (kde, every desktop). the plugin's own page holds draw / size / fps" : "could not tell plasmashell (" + (lastErr.empty() ? std::string("no busctl / qdbus6 / qdbus / gdbus found") : lastErr) + "). set it by hand: configure desktop, wallpaper type CHAOS";
        }
        if (app.reqSync) {
            app.reqSync = false;
            if (app.loggedIn()) { app.status = flow::syncVault(app.registryUrl, *app.token).message; }
            else app.status = "sync needs a connected session";
        }
        if (app.reqClaimSeed || app.reqClaimCurrent) {
            Seed32 target = app.reqClaimSeed ? *app.reqClaimSeed : app.dim->seed;
            app.reqClaimSeed.reset(); app.reqClaimCurrent = false;
            if (!app.loggedIn()) app.status = "claiming needs a connected session";
            else {
                bool found = false;
                for (const auto& e : vault::list()) if (std::memcmp(e.file.seed_full, target.data(), 32) == 0) { found = true; app.status = flow::claimFile(app.registryUrl, *app.token, e.file).message; break; }
                if (!found) app.status = "keep the roll first (K), then claim it";
                if (app.me) { auto who = api::me(app.registryUrl, *app.token); if (who) app.me = *who.value; }
            }
        }
        if (app.reqClaimableRoll) {
            app.reqClaimableRoll = false;
            if (!app.loggedIn()) app.status = "a claimable roll needs a connected session";
            else {
                auto r = flow::rollClaimable(app.registryUrl, *app.token);
                if (!r) app.status = r.error ? r.error->message : "roll failed";
                else {
                    app.ticket = r.value->ticket; std::memcpy(app.clientRandom, r.value->clientRandom, 32); app.hasTicket = true;
                    rollInto(app, r.value->seed, -1, -1, false, Mode::Chaos, w, h);
                    app.status = "claimable roll. keep it (K), then claim it.";
                    auto who = api::me(app.registryUrl, *app.token); if (who) app.me = *who.value;
                }
            }
        }
        if (app.reqReroll || app.reqRerollTicket || app.reqLoad) {
            Seed32 s;
            if (!app.canGenerate() && (app.reqReroll || app.reqRerollTicket || (app.reqLoad && !activation::isActivated(*app.reqLoad) && !vault::has(*app.reqLoad)))) {
                app.status = app.online ? "sign in first (account tab): nothing is generated without an account." : "offline: only chaos kept or loaded online once can be used. nothing is generated offline.";
                app.reqReroll = app.reqRerollTicket = false; app.reqLoad.reset();
                continue;
            }
            if (app.reqLoad) { s = *app.reqLoad; app.hasTicket = false; }
            else if (app.reqRerollTicket) {
                if (!loadTicket(app, app.ticketPath, app.revealPath)) { app.reqRerollTicket = false; continue; }
                s = cf::deriveSeed(app.ticket, app.clientRandom);
            } else { s = randomSeed(); app.hasTicket = false; }
            app.reqReroll = app.reqRerollTicket = false; app.reqLoad.reset();
            bool wasLoad = !app.hasTicket && std::memcmp(s.data(), app.dim ? app.dim->seed.data() : s.data(), 32) != 0;
            rollInto(app, s, -1, -1, false, Mode::Chaos, w, h);
            app.status = app.hasTicket ? "claimable roll" : "preview roll";
            if (wasLoad && app.online) {
                // loading a vault file online: ask the registry, remember the answer. that is what unlocks offline use.
                for (const auto& e : vault::list()) if (std::memcmp(e.file.seed_full, s.data(), 32) == 0) {
                    if (auto r = api::check(app.registryUrl, reinterpret_cast<const uint8_t*>(&e.file), sizeof e.file); r && r->valid && r->known) {
                        activation::record(s, { cf::nowMs(), r->owner, r->state, r->current, r->valueCents, "" });
                        app.status = "activated: owner " + r->owner + ", " + (r->current ? "current" : "STALE copy") + ", value " + std::to_string(r->valueCents / 100) + "." + std::to_string(r->valueCents % 100);
                    }
                    break;
                }
            }
        }

        if (glfwGetWindowAttrib(window.handle(), GLFW_ICONIFIED)) { std::this_thread::sleep_for(std::chrono::milliseconds(120)); lastT = glfwGetTime(); continue; }   // minimised: no frames
        glfwSwapInterval(app.settings.vsync ? 1 : 0);
        app.renderer->setLayers(layersFromName(app.persisted.layers));
        app.renderer->setQuality({ app.persisted.renderScale, app.persisted.msaa, app.persisted.detail == "lite" });
        app.renderer->resize(w, h);
        double now = glfwGetTime();
        float dt = float(std::min(0.1, now - lastT)); lastT = now;
        if (!app.settings.paused) app.simTime += dt;
        app.renderer->frame(float(app.simTime), app.settings.paused ? 0.f : dt, 0);
        if (app.firstFrameAt < 0) app.firstFrameAt = now + (app.settings.poster ? 0.9 : -1.0);   // poster holds a beat after the first real frame
        if (!app.bootShown && app.firstFrameAt >= 0 && now > app.firstFrameAt + 0.7) app.bootShown = true;

        if (app.reqKeep) {
            app.reqKeep = false;
            glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
            auto px = readFramebufferRgb(w, h);
            app.kept = vault::keep(*app.dim, encodePng(px.data(), w, h), app.hasTicket ? &app.ticket : nullptr, app.clientRandom);
            app.status = "kept " + app.kept->chaosPath.string();
            std::printf("kept  %s\n", app.kept->chaosPath.c_str());
        }

        panel.beginFrame();
        if (app.settings.poster) panel.drawPoster(app, w, h);
        panel.draw(app, w, h);
        panel.endFrame();
        if (!uiShotPath.empty()) {
            static int frames = 0, sinceKey = -1;
            const char* uiKey = std::getenv("CHAOS_UI_KEY");
            // screenshots after a roll or a load: the key fires once the boot poster has played, the shot follows three frames later
            if (uiKey && sinceKey < 0 && (app.bootShown || !app.settings.poster) && frames > 2) { if (uiKey[0] == 'R') app.reqReroll = true; else if (uiKey[0] == 'L' && !app.persisted.seed.empty()) { try { app.reqLoad = seedFromHex(app.persisted.seed); } catch (...) {} } sinceKey = 0; }
            else if (sinceKey >= 0) sinceKey++;
            frames++;
            if (uiKey ? sinceKey >= 3 : frames >= 8) {
                glFinish();
                glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
                glReadBuffer(GL_BACK);
                auto px = readFramebufferRgb(w, h);
                auto png = encodePng(px.data(), w, h);
                std::printf("%s %s\n", writeFile(uiShotPath, png.data(), png.size()) ? "wrote" : "failed", uiShotPath.c_str());
                return 0;
            }
            continue;   // never swap: a hidden wayland surface blocks on it
        }
        window.swap();

        fpsAcc += dt; fpsN++;
        if (fpsAcc >= 0.5) { app.fps = float(fpsN / fpsAcc); fpsAcc = 0; fpsN = 0; }
        // on battery the app caps itself at 30 fps whatever the setting says (checked every 2 s)
        static double lastPower = -10; static bool onBattery = false;
        if (now - lastPower > 2.0) { lastPower = now; onBattery = power::onBattery(); }
        int cap = app.settings.fpsCap; if (app.persisted.batteryCap && onBattery && (cap == 0 || cap > 30)) cap = 30;
        if (cap > 0) {
            double target = 1.0 / cap, used = glfwGetTime() - now;
            if (used < target) std::this_thread::sleep_for(std::chrono::duration<double>(target - used));
        }
    }
    gPanel = nullptr; gApp = nullptr;
    return 0;
}
