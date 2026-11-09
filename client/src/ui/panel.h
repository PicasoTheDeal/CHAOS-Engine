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
struct ImDrawList;
struct ImVec2;

namespace chaos {

class Panel {
public:
    explicit Panel(GLFWwindow* win);
    ~Panel();
    void beginFrame();
    void draw(App& app, int w, int h, unsigned previewTex = 0, int pw = 0, int ph = 0);   // panel + overlays, when open. with a preview texture: the desk layout (stage on the left, the column on the right)
    bool wantsPreview() const { return tab_ == 1 || tab_ == 3 || tab_ == 4; }   // vault, view, model show the chaos. the other pages are plain
    static void previewSize(int w, int h, int& pw, int& ph);   // how big the preview is for this window in the desk layout. 0 = too small, use the full canvas
    static void clearColor(const App& app, float rgb[3]);      // the desk's paper, for the gl clear behind the ui
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
    void column(App& app, float W, ImDrawList* dl);   // hud, tabs, body, footer: the panel proper
    void desk(App& app, int w, int h, unsigned tex, int pw, int ph);     // the desk: header, tabs across, the page (with the preview on the tabs that want it), footer
    void preview(App& app, unsigned tex, int pw, int ph);                // the chaos as an image with its line under it
    void refreshVault();
    ImFont* headline_ = nullptr;
    ImFont* mono_ = nullptr;
    ImFont* big_ = nullptr;
    ImFont* counter_ = nullptr;
    ImFont* label_ = nullptr;     // jetbrains mono 600: labels, keys, tags
    ImFont* serif_ = nullptr;     // instrument serif italic: the one subtitle voice
    int tab_ = 0;                 // 0 = setup; main.cpp leaves it there till you're signed in and have rolled
    Thumbs thumbs_;
    struct VaultRow { vault::Entry e; bool ok = false; std::string why; bool activated = false; Seed32 seed{}; };
    std::vector<VaultRow> entries_;      // verified once per refresh, not per frame
    bool vaultDirty_ = true, keptSeen_ = false;
    char ticketBuf_[512] = {};
    char revealBuf_[512] = {};
    char seedBuf_[512] = {};
};

} // namespace chaos
