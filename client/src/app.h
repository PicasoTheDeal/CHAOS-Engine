#pragma once
// everything the window loop and the panel share. the panel only sets requests; main acts on them.
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include "platform/window.h"
#include "render/renderer.h"
#include "scene/dimension.h"
#include "shape/parametric.h"
#include "chaos_file/format.h"
#include "sync/api.h"
#include "vault/vault.h"
#include "settings.h"

namespace chaos {

struct Settings {
    bool vsync = true;
    int fpsCap = 0;            // 0 = uncapped (vsync still applies)
    bool panelOpen = true;
    bool paused = false;
    bool fullscreen = false;
    int monitor = 0;
    bool poster = true;
    bool desk = true;          // desk: the ui is the window and the chaos is a preview in it. off: the chaos fills the window, the panel floats on the right (v toggles)
};

struct BootStage { std::string key, headline, detail; double at; };

struct App {
    Window& window;
    std::unique_ptr<Dimension> dim;
    std::unique_ptr<Renderer> renderer;
    Settings settings;
    Persisted persisted;        // settings.json, edited in the panel

    // online policy: generation needs the registry, usage of activated files does not
    std::string registryUrl;
    bool online = false;
    bool devOffline = false;    // --dev-offline: lift the gate for development. a client-side policy, not a wall
    bool canGenerate() const { return devOffline || (online && token && me); }   // signup is a must: nothing is generated without a session
    std::optional<std::string> token;      // registry session, from ~/.config/chaos/session
    std::optional<api::Me> me;              // filled when the token checks out
    bool loggedIn() const { return online && token && me; }
    bool reqClaimableRoll = false, reqClaimCurrent = false, reqSync = false, reqLogout = false;
    std::string reqConnectToken;            // panel: paste a token
    bool reqCheckOnline = false;            // panel: ask the registry now
    bool reqSetDesktop = false;             // panel: hand the current seed to the plasma wallpaper plugin
    std::string reqVaultDir;                // panel: move the vault here (a path), "?" = open a folder dialog
    int setupStep = 0;                      // the setup tab's progress, derived each frame
    std::optional<Seed32> reqClaimSeed;     // vault tab: claim this rolled file

    // boot poster
    double bootStart = 0;
    bool bootShown = false;                 // the poster plays once, at boot. rolls and loads after that cut straight to the frame
    double firstFrameAt = -1;
    std::vector<BootStage> boot;

    // the current roll's provenance
    bool hasTicket = false;
    cf::Ticket ticket{};
    uint8_t clientRandom[32] = {};
    std::string ticketPath, revealPath;
    std::optional<vault::KeptEntry> kept;     // where the current roll was kept, if it was

    // clock
    double simTime = 0;
    float fps = 0;
    std::string status;

    // requests from the panel or the keys, consumed by main each frame
    bool reqReroll = false, reqRerollTicket = false, reqKeep = false, reqFullscreen = false, reqQuit = false;
    std::optional<Seed32> reqLoad;
    std::optional<Mesh> model;              // the .cheng, decoded once
    // the model tab: import a file ("?" = open a file dialog), re-apply after a skin/colour change, pick at random
    std::string reqModelImport; bool reqModelApply = false, reqModelRandom = false;
    std::string modelName;                  // what is loaded, for the panel

    explicit App(Window& w) : window(w) {}
};

} // namespace chaos
