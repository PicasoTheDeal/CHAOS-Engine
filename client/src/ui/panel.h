#pragma once
// Dear ImGui control panel: vault, roll, view, settings, account. plus the boot poster.
#include <string>
#include <vector>
#include "app.h"
#include "ui/thumbs.h"
#include "vault/vault.h"
#include "seed/seed.h"
struct GLFWwindow;
struct ImFont;

namespace chaos {

class Panel {
public:
    explicit Panel(GLFWwindow* win);
    ~Panel();
    void beginFrame();
    void draw(App& app, int w, int h);        // panel + overlays, when open
    void drawPoster(App& app, int w, int h);  // boot poster, full bleed
    void endFrame();
    bool wantsKeyboard() const;
    static std::string assetDir();
private:
    void tabSetup(App& app);
    void tabVault(App& app);
    void tabRoll(App& app);
    void tabView(App& app);
    void tabModel(App& app);
    void tabSettings(App& app);
    void tabAccount(App& app);
    void refreshVault();
    ImFont* headline_ = nullptr;
    ImFont* mono_ = nullptr;
    ImFont* big_ = nullptr;
    ImFont* counter_ = nullptr;
    ImFont* label_ = nullptr;     // jetbrains mono 600: labels, keys, tags
    ImFont* serif_ = nullptr;     // instrument serif italic: the one subtitle voice
    int tab_ = 0;                 // 0 = setup; main.cpp leaves it there until the user is signed in and has rolled
    Thumbs thumbs_;
    struct VaultRow { vault::Entry e; bool ok = false; std::string why; bool activated = false; Seed32 seed{}; };
    std::vector<VaultRow> entries_;      // verified once per refresh, not per frame
    bool vaultDirty_ = true, keptSeen_ = false;
    char ticketBuf_[512] = {};
    char revealBuf_[512] = {};
    char seedBuf_[512] = {};
};

} // namespace chaos
