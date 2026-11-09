#include "ui/panel.h"
#include "ui/mark.h"
#include "model/cheng.h"
#include "util/duration.h"
#include "render/renderer.h"
#include <ctime>
#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <filesystem>
#include <unistd.h>
#include <cstring>
#include <cmath>
#include "seed/base58.h"
#include "seed/bip39.h"
#include "server_pubkey.h"
#include "vault/activation.h"
#include "sync/api.h"
#include <cstdlib>
#include <cctype>
#include <algorithm>
#include <fstream>
#include <cfloat>

namespace chaos {

namespace fs = std::filesystem;

std::string Panel::assetDir() {
    static std::string dir = [] {
        char buf[4096];
        ssize_t n = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
        if (n > 0) {
            buf[n] = 0;
            auto exeDir = fs::path(buf).parent_path();
            for (auto p : { exeDir / "assets", exeDir / ".." / "share" / "chaos" / "assets" })
                if (fs::is_directory(p)) return fs::weakly_canonical(p).string();
        }
#ifdef CHAOS_DATA_DIR
        if (fs::is_directory(fs::path(CHAOS_DATA_DIR) / "assets")) return (fs::path(CHAOS_DATA_DIR) / "assets").string();
#endif
#ifdef CHAOS_ASSET_DIR
        return std::string(CHAOS_ASSET_DIR);
#else
        return std::string("assets");
#endif
    }();
    return dir;
}

static ImVec4 col(const glm::vec3& c, float a = 1) { return ImVec4(c.r, c.g, c.b, a); }

// ---- the site's tokens (CHAOS/src/routes/+layout.svelte), so the app and the registry read as one thing -------
// the site's tokens (CHAOS/src/routes/+layout.svelte), dark and light, so the app and the registry read as one thing.
// drawing is flat and drafted: hairlines, no rounding, box edges that run a few pixels past their corners.
struct Theme { ImVec4 BG, SILVER, SILVER2, DIM, FAINT, LINE, LINE2, MINT, GOLD, RED, LCD, SEG; bool light; };
static const Theme DARK{ { 0.078f, 0.078f, 0.086f, 1 }, { 0.925f, 0.914f, 0.886f, 1 }, { 0.788f, 0.773f, 0.737f, 1 }, { 0.925f, 0.914f, 0.886f, 0.62f }, { 0.925f, 0.914f, 0.886f, 0.38f },
                         { 0.925f, 0.914f, 0.886f, 0.14f }, { 0.925f, 0.914f, 0.886f, 0.26f }, { 0.941f, 0.706f, 0.235f, 1 }, { 0.780f, 0.639f, 0.275f, 1 }, { 0.898f, 0.282f, 0.302f, 1 },
                         { 0.788f, 0.765f, 0.706f, 1 }, { 0.122f, 0.114f, 0.094f, 1 }, false };     // #141416 slate, #ece9e2 bone, #f0b43c amber, #e5484d red
static const Theme LIGHT{ { 0.953f, 0.945f, 0.925f, 1 }, { 0.090f, 0.090f, 0.102f, 1 }, { 0.294f, 0.290f, 0.275f, 1 }, { 0.090f, 0.090f, 0.102f, 0.64f }, { 0.090f, 0.090f, 0.102f, 0.42f },
                          { 0.090f, 0.090f, 0.102f, 0.16f }, { 0.090f, 0.090f, 0.102f, 0.32f }, { 0.659f, 0.427f, 0.071f, 1 }, { 0.557f, 0.416f, 0.078f, 1 }, { 0.784f, 0.196f, 0.227f, 1 },
                          { 0.788f, 0.765f, 0.706f, 1 }, { 0.122f, 0.114f, 0.094f, 1 }, true };     // #f3f1ec paper, #17171a ink, #a86d12 amber
static const Theme* g_theme = &DARK;
#define tk (*g_theme)

namespace {
ImVec4 g_accent = tk.MINT;   // the live accent: the current dimension's palette, lifted so it reads on the dark
ImFont* g_label = nullptr;    // jetbrains mono 600, the tracked uppercase voice
ImFont* g_body = nullptr;     // jetbrains mono 400
constexpr float LABEL_PX = 10.f, KEY_PX = 11.f, BODY_PX = 14.f, KEY_H = 38.f, OVER = 5.f;   // OVER: how far an edge runs past its corner
// a drafted box: four hairlines, each running OVER px past the corners
void archRect(ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32 col, float over = OVER) {
    dl->AddLine(ImVec2(a.x - over, a.y), ImVec2(b.x + over, a.y), col);
    dl->AddLine(ImVec2(a.x - over, b.y), ImVec2(b.x + over, b.y), col);
    dl->AddLine(ImVec2(a.x, a.y - over), ImVec2(a.x, b.y + over), col);
    dl->AddLine(ImVec2(b.x, a.y - over), ImVec2(b.x, b.y + over), col);
}
// the live accent from the seed's palette. dark: the hue lifted to sit on slate. light: the hue taken down to ink
// strength. a palette with no hue to speak of (near white, near grey) falls back to the theme's own amber: a grey
// accent on paper was the "washed out" light theme
ImVec4 accentFrom(const glm::vec3& c) {
    float mx = std::max(c.r, std::max(c.g, c.b)), mn = std::min(c.r, std::min(c.g, c.b));
    float sat = mx > 0.001f ? (mx - mn) / mx : 0;
    if (sat < 0.28f) return tk.MINT;
    glm::vec3 n = c / mx;
    glm::vec3 a = tk.light ? n * 0.58f : glm::mix(n, glm::vec3(1), 0.30f);
    if (tk.light) { float lum = 0.2126f * a.r + 0.7152f * a.g + 0.0722f * a.b; if (lum > 0.5f) a *= 0.5f / lum; }   // never lighter than mid on paper
    return ImVec4(a.r, a.g, a.b, 1);
}

// uppercase text with letter spacing: imgui has no tracking, so one glyph at a time. returns the width drawn
float trackedWidth(ImFont* f, float px, const char* s, float spacing) {
    float w = 0; char b[2] = { 0, 0 };
    for (const char* c = s; *c; c++) { b[0] = char(std::toupper((unsigned char)*c)); w += f->CalcTextSizeA(px, FLT_MAX, 0, b).x + spacing; }
    return w > 0 ? w - spacing : 0;
}
void trackedAt(ImDrawList* dl, ImFont* f, float px, ImVec2 pos, const char* s, float spacing, ImU32 colour) {
    char b[2] = { 0, 0 };
    for (const char* c = s; *c; c++) { b[0] = char(std::toupper((unsigned char)*c)); dl->AddText(f, px, pos, colour, b); pos.x += f->CalcTextSizeA(px, FLT_MAX, 0, b).x + spacing; }
}
ImFont* labelFont() { return g_label ? g_label : ImGui::GetFont(); }

// .lbl: 9-10px, .22em, faint. a label above a value
void lbl(const char* s, ImVec4 c = tk.FAINT) {
    ImDrawList* dl = ImGui::GetWindowDrawList(); ImVec2 p = ImGui::GetCursorScreenPos();
    trackedAt(dl, labelFont(), LABEL_PX, ImVec2(p.x, p.y + 2), s, LABEL_PX * 0.22f, ImGui::GetColorU32(c));
    ImGui::Dummy(ImVec2(trackedWidth(labelFont(), LABEL_PX, s, LABEL_PX * 0.22f), LABEL_PX + 6));
}
// h2: tracked, dim, a hairline under it
void h2(const char* s) {
    ImGui::Dummy(ImVec2(0, 8));
    ImDrawList* dl = ImGui::GetWindowDrawList(); ImVec2 p = ImGui::GetCursorScreenPos(); float w = ImGui::GetContentRegionAvail().x;
    trackedAt(dl, labelFont(), LABEL_PX, p, s, LABEL_PX * 0.28f, ImGui::GetColorU32(tk.DIM));
    dl->AddLine(ImVec2(p.x, p.y + LABEL_PX + 10), ImVec2(p.x + w, p.y + LABEL_PX + 10), ImGui::GetColorU32(tk.LINE));
    ImGui::Dummy(ImVec2(w, LABEL_PX + 18));
}
// .tier: a bordered tracked tag. gold when asked
void tag(const char* s, ImVec4 c = tk.SILVER, ImVec4 border = tk.LINE2) {
    ImDrawList* dl = ImGui::GetWindowDrawList(); ImVec2 p = ImGui::GetCursorScreenPos();
    float sp = 9.f * 0.22f, w = trackedWidth(labelFont(), 9.f, s, sp) + 16, h = 20;
    archRect(dl, p, ImVec2(p.x + w, p.y + h), ImGui::GetColorU32(border), 3);
    trackedAt(dl, labelFont(), 9.f, ImVec2(p.x + 8, p.y + 6), s, sp, ImGui::GetColorU32(c));
    ImGui::Dummy(ImVec2(w, h));
}
// .key: flat hairline button, fills silver on hover / when on. w: 0 fits, -1 fills the row
// w: 0 fits, -1 fills the row. on: lit in the accent. primary: always filled in the accent (the one thing to press)
float g_keyPx = KEY_PX;   // the tab row draws its keys a size down
float g_keyH = KEY_H;     // the stage's action keys draw taller
bool key(const char* label, float w = 0, bool on = false, bool primary = false) {
    const bool disabled = ImGui::GetStyle().Alpha < 1.f;
    const float px = g_keyPx, sp = px * 0.16f, tw = trackedWidth(labelFont(), px, label, sp);
    ImVec2 size(w > 0 ? w : (w < 0 ? ImGui::GetContentRegionAvail().x : tw + 36), g_keyH);
    ImVec2 p = ImGui::GetCursorScreenPos();
    bool clicked = ImGui::InvisibleButton(label, size);
    bool hov = ImGui::IsItemHovered() && !disabled;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec4 fill = primary ? (hov ? tk.SILVER : g_accent) : on ? g_accent : ImGui::IsItemActive() ? tk.SILVER2 : tk.SILVER;
    bool lit = hov || on || primary;
    if (lit) dl->AddRectFilled(p, ImVec2(p.x + size.x, p.y + size.y), ImGui::GetColorU32(fill));
    archRect(dl, p, ImVec2(p.x + size.x, p.y + size.y), ImGui::GetColorU32(lit ? fill : tk.LINE2));
    trackedAt(dl, labelFont(), px, ImVec2(p.x + (size.x - tw) * 0.5f, p.y + (size.y - px) * 0.5f), label, sp, ImGui::GetColorU32(lit ? tk.BG : tk.SILVER));
    return clicked && !disabled;
}
bool primaryKey(const char* label, float w = -1) { return key(label, w, false, true); }
// one dim line under a key saying what it does
void hint(const char* s) { ImGui::PushTextWrapPos(); ImGui::TextColored(tk.FAINT, "%s", s); ImGui::PopTextWrapPos(); }
// .msg: a bordered line, red when bad
void msg(const char* s, bool bad = false) {
    ImDrawList* dl = ImGui::GetWindowDrawList(); ImVec2 p = ImGui::GetCursorScreenPos(); float w = ImGui::GetContentRegionAvail().x;
    ImVec2 sz = ImGui::CalcTextSize(s, nullptr, false, w - 28);
    archRect(dl, p, ImVec2(p.x + w, p.y + sz.y + 20), ImGui::GetColorU32(bad ? tk.RED : tk.LINE2));
    ImGui::SetCursorScreenPos(ImVec2(p.x + 14, p.y + 10));
    ImGui::PushTextWrapPos(p.x + w - 14); ImGui::TextColored(bad ? tk.RED : tk.SILVER, "%s", s); ImGui::PopTextWrapPos();
    ImGui::SetCursorScreenPos(ImVec2(p.x, p.y + sz.y + 20)); ImGui::Dummy(ImVec2(w, 6));
}
void good(const char* s) { ImGui::TextColored(g_accent, "%s", s); }
// each cell a small box, the grid cutting between them (the gap only once a cell is big enough to show it)
void drawMark(ImDrawList* dl, ImVec2 p, float px, ImU32 colour) {
    const float g = px >= 3 ? px * 0.12f : 0;
    for (int y = 0; y < mark::SIZE; y++) for (int x = 0; x < mark::SIZE; x++) if (mark::on(x, y))
        dl->AddRectFilled(ImVec2(p.x + x * px + g, p.y + y * px + g), ImVec2(p.x + (x + 1) * px - g, p.y + (y + 1) * px - g), colour);
}
std::string clockNow() { char b[16]; std::time_t n = std::time(nullptr); std::tm lt{}; localtime_r(&n, &lt); std::strftime(b, sizeof b, "%H:%M:%S", &lt); return b; }

void applyTheme() {
    ImGuiStyle& s = ImGui::GetStyle();
    ImGui::StyleColorsDark();
    s.WindowRounding = s.FrameRounding = s.ScrollbarRounding = s.GrabRounding = s.TabRounding = s.PopupRounding = s.ChildRounding = 0;
    s.WindowBorderSize = 0; s.FrameBorderSize = 1; s.PopupBorderSize = 1; s.ChildBorderSize = 0;
    s.WindowPadding = ImVec2(20, 18); s.FramePadding = ImVec2(10, 7); s.ItemSpacing = ImVec2(8, 8); s.ItemInnerSpacing = ImVec2(10, 6);
    s.ScrollbarSize = 6; s.GrabMinSize = 8; s.DisabledAlpha = 0.35f;
    auto& c = s.Colors;
    c[ImGuiCol_Text] = tk.SILVER; c[ImGuiCol_TextDisabled] = tk.DIM;
    c[ImGuiCol_WindowBg] = ImVec4(tk.BG.x, tk.BG.y, tk.BG.z, tk.light ? 0.96f : 0.88f);   // paper has to be opaque to read on a bright scene; slate lets a little through c[ImGuiCol_ChildBg] = ImVec4(0, 0, 0, 0); c[ImGuiCol_PopupBg] = tk.BG;
    c[ImGuiCol_Border] = tk.LINE2; c[ImGuiCol_BorderShadow] = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_FrameBg] = ImVec4(0, 0, 0, 0); c[ImGuiCol_FrameBgHovered] = ImVec4(tk.SILVER.x, tk.SILVER.y, tk.SILVER.z, 0.06f); c[ImGuiCol_FrameBgActive] = ImVec4(tk.SILVER.x, tk.SILVER.y, tk.SILVER.z, 0.10f);
    c[ImGuiCol_TitleBg] = c[ImGuiCol_TitleBgActive] = c[ImGuiCol_TitleBgCollapsed] = tk.BG; c[ImGuiCol_MenuBarBg] = tk.BG;
    c[ImGuiCol_ScrollbarBg] = ImVec4(0, 0, 0, 0); c[ImGuiCol_ScrollbarGrab] = tk.LINE2; c[ImGuiCol_ScrollbarGrabHovered] = tk.DIM; c[ImGuiCol_ScrollbarGrabActive] = tk.SILVER;
    c[ImGuiCol_CheckMark] = tk.MINT; c[ImGuiCol_SliderGrab] = tk.SILVER2; c[ImGuiCol_SliderGrabActive] = tk.MINT;
    c[ImGuiCol_FrameBg] = ImVec4(tk.SILVER.x, tk.SILVER.y, tk.SILVER.z, 0.04f);
    c[ImGuiCol_Button] = ImVec4(0, 0, 0, 0); c[ImGuiCol_ButtonHovered] = tk.SILVER; c[ImGuiCol_ButtonActive] = tk.SILVER2;
    c[ImGuiCol_Header] = ImVec4(tk.SILVER.x, tk.SILVER.y, tk.SILVER.z, 0.08f); c[ImGuiCol_HeaderHovered] = ImVec4(tk.SILVER.x, tk.SILVER.y, tk.SILVER.z, 0.12f); c[ImGuiCol_HeaderActive] = ImVec4(tk.SILVER.x, tk.SILVER.y, tk.SILVER.z, 0.18f);
    c[ImGuiCol_FrameBgHovered] = ImVec4(tk.SILVER.x, tk.SILVER.y, tk.SILVER.z, 0.08f); c[ImGuiCol_FrameBgActive] = ImVec4(tk.SILVER.x, tk.SILVER.y, tk.SILVER.z, 0.12f);
    c[ImGuiCol_ScrollbarGrab] = tk.LINE2; c[ImGuiCol_ScrollbarGrabHovered] = tk.DIM; c[ImGuiCol_ScrollbarGrabActive] = tk.SILVER2;
    c[ImGuiCol_PopupBg] = tk.BG; c[ImGuiCol_Border] = tk.LINE2;
    c[ImGuiCol_Separator] = tk.LINE; c[ImGuiCol_SeparatorHovered] = tk.LINE2; c[ImGuiCol_SeparatorActive] = tk.SILVER;
    c[ImGuiCol_ResizeGrip] = c[ImGuiCol_ResizeGripHovered] = c[ImGuiCol_ResizeGripActive] = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_Tab] = ImVec4(0, 0, 0, 0); c[ImGuiCol_TabHovered] = ImVec4(tk.SILVER.x, tk.SILVER.y, tk.SILVER.z, 0.08f); c[ImGuiCol_TabSelected] = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_PlotHistogram] = tk.SILVER2; c[ImGuiCol_PlotHistogramHovered] = tk.SILVER;
    c[ImGuiCol_TextSelectedBg] = ImVec4(tk.SILVER.x, tk.SILVER.y, tk.SILVER.z, 0.25f);
    c[ImGuiCol_NavCursor] = tk.MINT;
}
} // namespace

// the desktop's own fonts: kde's kdeglobals names them, fontconfig finds the files. elsewhere fontconfig's sans / mono.
static std::string run(const std::string& cmd) { std::string out; if (FILE* f = popen(cmd.c_str(), "r")) { char b[1024]; while (std::fgets(b, sizeof b, f)) out += b; pclose(f); } while (!out.empty() && (out.back() == '\n' || out.back() == '\r')) out.pop_back(); return out; }
static std::string kdeFont(const char* keyName) {
    const char* h = std::getenv("XDG_CONFIG_HOME"); const char* home = std::getenv("HOME");
    fs::path kg = (h && *h ? fs::path(h) : fs::path(home ? home : ".") / ".config") / "kdeglobals";
    std::ifstream in(kg); std::string line; bool general = false;
    while (std::getline(in, line)) {
        if (!line.empty() && line[0] == '[') { general = line == "[General]"; continue; }
        if (general && line.rfind(std::string(keyName) + "=", 0) == 0) { std::string v = line.substr(std::strlen(keyName) + 1); auto c = v.find(','); return c == std::string::npos ? v : v.substr(0, c); }
    }
    return "";
}
static std::string fontFile(const std::string& family, const char* fallback) {
    std::string fam = family.empty() ? fallback : family;
    for (char& ch : fam) if (ch == '\'' ) ch = ' ';
    std::string f = run("fc-match -f '%{file}' '" + fam + "' 2>/dev/null");
    if (f.empty() || !fs::exists(f)) f = run(std::string("fc-match -f '%{file}' '") + fallback + "' 2>/dev/null");
    return (f.size() > 4 && (f.ends_with(".ttf") || f.ends_with(".otf") || f.ends_with(".TTF"))) ? f : "";   // imgui's loader wants ttf / otf
}

Panel::Panel(GLFWwindow* win) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    std::string fonts = assetDir() + "/fonts/";
    auto load = [&](const std::string& path, float px) -> ImFont* { return fs::exists(path) ? io.Fonts->AddFontFromFileTTF(path.c_str(), px) : nullptr; };
    // the desktop's fonts first (kde: the general font and the fixed-width one), our shipped ones as the fallback
    std::string sans = fontFile(kdeFont("font"), "sans-serif"), mono = fontFile(kdeFont("fixed"), "monospace");
    if (sans.empty()) sans = fonts + "inter-latin-400-normal.ttf";
    if (mono.empty()) mono = fonts + "jetbrains-mono-latin-400-normal.ttf";
    mono_ = load(sans, BODY_PX);                  // first = default: the desktop's reading font
    label_ = load(mono, LABEL_PX);                // labels, keys, data: the desktop's fixed font
    headline_ = load(sans, 15);
    big_ = load(sans, 56);
    serif_ = load(sans, 17);
    counter_ = load(fonts + "DSEG14Classic-Regular.ttf", 18);
    std::fprintf(stderr, "fonts  %s  /  %s\n", sans.c_str(), mono.c_str());
    if (const char* tb = std::getenv("CHAOS_TAB")) tab_ = std::clamp(std::atoi(tb), 0, 6);   // screenshots of a given tab
    if (!mono_) io.Fonts->AddFontDefault();
    g_label = label_; g_body = mono_;
    applyTheme();
    ImGui_ImplGlfw_InitForOpenGL(win, true);
    ImGui_ImplOpenGL3_Init("#version 430 core");
}

Panel::~Panel() {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

void Panel::beginFrame() { ImGui_ImplOpenGL3_NewFrame(); ImGui_ImplGlfw_NewFrame(); ImGui::NewFrame(); }
void Panel::endFrame() { ImGui::Render(); ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData()); }
bool Panel::wantsKeyboard() const { return ImGui::GetIO().WantCaptureKeyboard || ImGui::GetIO().WantTextInput; }

// ---- boot poster: the page background, the build stages landing as tracked lines, the counter in the lcd font ----
void Panel::drawPoster(App& app, int w, int h) {
    const Dimension& d = *app.dim;
    double now = glfwGetTime(), age = now - app.bootStart;
    float fade = app.firstFrameAt < 0 ? 1.f : float(std::max(0.0, 1.0 - (now - app.firstFrameAt) / 0.7));
    if (fade <= 0) return;
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    auto a = [&](ImVec4 c, float m = 1) { return ImGui::ColorConvertFloat4ToU32(ImVec4(c.x, c.y, c.z, c.w * fade * m)); };
    dl->AddRectFilled(ImVec2(0, 0), ImVec2(float(w), float(h)), a(tk.BG));
    const ImVec4 acc = accentFrom(d.pal.b), acc2 = accentFrom(d.pal.a);
    // grid paper, and a frame whose edges run past the corners
    for (int gx = 24; gx < w; gx += 24) dl->AddLine(ImVec2(float(gx), 0), ImVec2(float(gx), float(h)), a(tk.LINE, 0.35f));
    for (int gy = 24; gy < h; gy += 24) dl->AddLine(ImVec2(0, float(gy)), ImVec2(float(w), float(gy)), a(tk.LINE, 0.35f));
    archRect(dl, ImVec2(24, 24), ImVec2(w - 24.f, h - 24.f), a(tk.LINE2), 12);
    // the mark, big, on its own grid: the ordered side still, the loose cells each drifting on their own slow sine
    {
        const float px = std::min(w, h) * 0.028f, mx = w * 0.70f - px * 8, my = h * 0.5f - px * 8, g = px * 0.12f;
        for (int i = 0; i <= mark::SIZE; i++) {
            dl->AddLine(ImVec2(mx + i * px, my), ImVec2(mx + i * px, my + 16 * px), a(tk.LINE, 0.7f));
            dl->AddLine(ImVec2(mx, my + i * px), ImVec2(mx + 16 * px, my + i * px), a(tk.LINE, 0.7f));
        }
        for (int y = 0; y < mark::SIZE; y++) for (int x = 0; x < mark::SIZE; x++) if (mark::on(x, y)) {
            float dx = 0, dy = 0;
            if (mark::loose(x)) { float k = (x - 4) / 10.f; dx = float(std::sin(age * 0.9 + x * 1.7 + y * 0.6)) * 0.35f * k * px; dy = float(std::cos(age * 0.7 + y * 1.3 + x * 0.4)) * 0.3f * k * px; }
            dl->AddRectFilled(ImVec2(mx + x * px + g + dx, my + y * px + g + dy), ImVec2(mx + (x + 1) * px - g + dx, my + (y + 1) * px - g + dy), a(mark::loose(x) ? acc : acc2));
        }
    }
    const float x0 = w * 0.08f, y0 = h * 0.16f;
    drawMark(dl, ImVec2(x0, 34), 2, a(acc));
    trackedAt(dl, labelFont(), LABEL_PX, ImVec2(x0 + 44, 44), "chaos", LABEL_PX * 0.32f, a(tk.SILVER));
    trackedAt(dl, labelFont(), LABEL_PX, ImVec2(w - 16 - 120, 44), "/boot", LABEL_PX * 0.12f, a(tk.FAINT));
    if (big_) dl->AddText(big_, 56, ImVec2(x0 - 3, y0), a(tk.SILVER), "A dimension, from a seed.");
    if (serif_) dl->AddText(serif_, 17, ImVec2(x0, y0 + 72), a(tk.DIM), "Rolling it now. Nothing here repeats.");
    dl->AddLine(ImVec2(x0, y0 + 112), ImVec2(x0 + w * 0.5f, y0 + 112), a(tk.LINE));
    float y = y0 + 128;
    for (const auto& st : app.boot) {
        if (st.at > now) continue;
        trackedAt(dl, labelFont(), LABEL_PX, ImVec2(x0, y + 2), st.headline.c_str(), LABEL_PX * 0.22f, a(tk.DIM));
        dl->AddText(mono_ ? mono_ : ImGui::GetFont(), BODY_PX, ImVec2(x0 + 230, y), a(tk.SILVER), st.detail.c_str());
        y += 24;
    }
    if (counter_) {
        // the lcd chip from the site: a pale plate, dark segments
        char buf[64]; std::snprintf(buf, sizeof buf, "%06.2f", age);
        ImVec2 p(x0, h - 70.f), sz(118, 34);
        dl->AddRectFilled(p, ImVec2(p.x + sz.x, p.y + sz.y), a(tk.LCD), 4);
        dl->AddRect(ImVec2(p.x - 2, p.y - 2), ImVec2(p.x + sz.x + 2, p.y + sz.y + 2), a(ImVec4(0.486f, 0.498f, 0.47f, 1)), 5.f, 0, 2.f);
        trackedAt(dl, labelFont(), 7.5f, ImVec2(p.x + 10, p.y + 6), "uptime", 7.5f * 0.18f, a(tk.SEG, 0.6f));
        dl->AddText(counter_, 16, ImVec2(p.x + 10, p.y + 15), a(tk.SEG), buf);
        std::string c = clockNow();
        trackedAt(dl, labelFont(), LABEL_PX, ImVec2(w - 16 - 80, h - 52.f), c.c_str(), LABEL_PX * 0.08f, a(tk.FAINT));
    }
}

// ---- the panel --------------------------------------------------------------------------------------------
void Panel::previewSize(int w, int h, int& pw, int& ph) {
    const float M = 16, W = 480, GAP = 24, PAD = 16;
    pw = int(float(w) - 2 * M - W - GAP - 2 * PAD); ph = pw * 9 / 16;
    const int maxH = h - int(2 * M) - 410;                       // the brand line, the status, three rows of keys with their hints
    if (ph > maxH) { ph = maxH; pw = ph * 16 / 9; }
    if (pw < 360 || ph < 200) { pw = ph = 0; }                   // too small for a desk: the chaos fills the window instead
}
void Panel::clearColor(const App& app, float rgb[3]) {
    const Theme& t = app.persisted.theme == "light" ? LIGHT : DARK;
    rgb[0] = t.BG.x; rgb[1] = t.BG.y; rgb[2] = t.BG.z;
}

void Panel::draw(App& app, int w, int h, unsigned previewTex, int pw, int ph) {
    if (!app.settings.panelOpen) return;
    const float W = 480, M = 16;
    const Theme* want = app.persisted.theme == "light" ? &LIGHT : &DARK;
    if (want != g_theme) { g_theme = want; applyTheme(); }
    g_accent = accentFrom(app.dim->pal.b);
    ImGui::GetStyle().Colors[ImGuiCol_CheckMark] = g_accent; ImGui::GetStyle().Colors[ImGuiCol_SliderGrabActive] = g_accent; ImGui::GetStyle().Colors[ImGuiCol_PlotHistogram] = g_accent;
    const ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar;
    if (previewTex) {
        // the desk: one opaque sheet over the whole window. the stage (preview + the big keys) on the left, the column on the right
        ImGui::PushStyleColor(ImGuiCol_WindowBg, tk.BG);
        ImGui::SetNextWindowPos(ImVec2(M, M));
        ImGui::SetNextWindowSize(ImVec2(float(w) - 2 * M, float(h) - 2 * M));
        ImGui::Begin("chaos-desk", nullptr, flags);
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 wp = ImGui::GetWindowPos();
        archRect(dl, wp, ImVec2(wp.x + float(w) - 2 * M, wp.y + float(h) - 2 * M), ImGui::GetColorU32(tk.LINE2), 9);
        { ImVec4 g = g_accent; g.w = 0.07f; dl->AddRectFilledMultiColor(ImVec2(wp.x + float(w) - 2 * M - W, wp.y), ImVec2(wp.x + float(w) - 2 * M, wp.y + 120), ImGui::GetColorU32(g), ImGui::GetColorU32(g), 0, 0); }
        const float stageW = float(w) - 2 * M - W - 24;
        ImGui::BeginChild("stage", ImVec2(stageW, 0), false);
        stage(app, previewTex, pw, ph);
        ImGui::EndChild();
        ImGui::SameLine(0, 24);
        ImVec2 cp = ImGui::GetCursorScreenPos();
        dl->AddLine(ImVec2(cp.x - 12, wp.y + 8), ImVec2(cp.x - 12, wp.y + float(h) - 2 * M - 8), ImGui::GetColorU32(tk.LINE));
        ImGui::BeginChild("column", ImVec2(W - 32, 0), false, ImGuiWindowFlags_NoScrollbar);
        column(app, W - 32, dl);
        ImGui::EndChild();
        ImGui::End();
        ImGui::PopStyleColor();
        return;
    }
    // the canvas: the chaos fills the window, the panel floats on the right and lets it through
    ImGui::SetNextWindowPos(ImVec2(float(w) - W - M, M));
    ImGui::SetNextWindowSize(ImVec2(W, float(h) - 2 * M));
    ImGui::Begin("chaos", nullptr, flags);
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 wp = ImGui::GetWindowPos();
    archRect(dl, wp, ImVec2(wp.x + W, wp.y + h - 2 * M), ImGui::GetColorU32(tk.LINE2), 9);   // the sheet's edge, drafted
    { ImVec4 g = g_accent; g.w = 0.07f; dl->AddRectFilledMultiColor(wp, ImVec2(wp.x + W, wp.y + 120), ImGui::GetColorU32(g), ImGui::GetColorU32(g), 0, 0); }   // a breath of the palette at the top
    column(app, W, dl);
    ImGui::End();
}

// the preview of the chaos and the keys a person actually needs, big, with a line under each saying what it does
void Panel::stage(App& app, unsigned tex, int pw, int ph) {
    Dimension& d = *app.dim;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();
    drawMark(dl, ImVec2(p.x, p.y - 2), 1.6f, ImGui::GetColorU32(g_accent));
    trackedAt(dl, labelFont(), 11, ImVec2(p.x + 36, p.y + 6), "chaos", 11 * 0.32f, ImGui::GetColorU32(tk.SILVER));
    trackedAt(dl, labelFont(), LABEL_PX, ImVec2(p.x + 118, p.y + 7), app.hasTicket ? "/claimable roll" : app.kept ? "/kept" : "/preview", LABEL_PX * 0.12f, ImGui::GetColorU32(tk.FAINT));
    ImGui::Dummy(ImVec2(0, 30));
    ImVec2 ip = ImGui::GetCursorScreenPos();
    ImGui::Image((ImTextureID)(intptr_t)tex, ImVec2(float(pw), float(ph)), ImVec2(0, 1), ImVec2(1, 0));   // gl textures are bottom up
    archRect(dl, ip, ImVec2(ip.x + pw, ip.y + ph), ImGui::GetColorU32(tk.LINE2), 6);
    ImGui::Dummy(ImVec2(0, 10));
    // the line: what is on the stage
    if (d.unlocks.gold) tag(tierName(d.tier), tk.GOLD, tk.GOLD); else tag(tierName(d.tier));
    ImGui::SameLine();
    ImGui::TextColored(tk.SILVER2, "%s  /  %s  /  %s  /  %s", toShortHex(d.seed).c_str(), familyName(d.family), shapeName(d.shapeType), modeName(d.displayMode));
    ImGui::SameLine(float(pw) - 150);
    if (app.online) tag("online", g_accent, g_accent); else if (app.devOffline) tag("dev offline", tk.GOLD, tk.GOLD); else tag("offline", tk.RED, tk.RED);
    ImGui::SameLine();
    char fpsBuf[32]; std::snprintf(fpsBuf, sizeof fpsBuf, "%.0f fps", app.fps);
    tag(app.settings.paused ? "paused" : fpsBuf, tk.DIM);
    if (serif_) ImGui::PushFont(serif_, 15);
    ImGui::PushTextWrapPos(float(pw)); ImGui::TextWrapped("%s", app.status.empty() ? "Quiet." : app.status.c_str()); ImGui::PopTextWrapPos();
    if (serif_) ImGui::PopFont();
    ImGui::Dummy(ImVec2(0, 8));
    // the keys. two per row, a hint under each
    g_keyH = 46; g_keyPx = 12;
    const float kw = (float(pw) - 12) * 0.5f;
    auto pair = [&](const char* a, const char* ha, bool aOn, bool aPrimary, const char* b, const char* hb, bool bOn, bool bPrimary) -> int {
        int hit = 0;
        ImGui::BeginGroup(); ImGui::BeginDisabled(!aOn); if (key(a, kw, false, aPrimary)) hit = 1; ImGui::EndDisabled(); ImGui::TextColored(tk.FAINT, "%s", ha); ImGui::EndGroup();
        ImGui::SameLine(0, 12);
        ImGui::BeginGroup(); ImGui::BeginDisabled(!bOn); if (key(b, kw, false, bPrimary)) hit = 2; ImGui::EndDisabled(); ImGui::TextColored(tk.FAINT, "%s", hb); ImGui::EndGroup();
        ImGui::Dummy(ImVec2(0, 6));
        return hit;
    };
    const bool canRoll = app.canGenerate(), canClaimable = app.loggedIn() && app.me && app.me->banked >= 1;
    std::string claimableLabel = app.loggedIn() && app.me && app.me->banked < 1 ? "next claimable roll in " + humanMs(app.me->nextInMs) : "roll a claimable one";
    int h1 = pair("roll a free preview  [R]", canRoll ? "free, unlimited, can never be claimed" : app.online ? "sign in first (account tab)" : "offline: nothing is generated", canRoll, false,
                  claimableLabel.c_str(), "your one roll this week, signed by the registry", canClaimable, canClaimable);
    if (h1 == 1) app.reqReroll = true;
    if (h1 == 2) app.reqClaimableRoll = true;
    const bool kept = app.kept.has_value();
    int h2 = pair("keep it to my folder  [K]", "writes the .chaos + a png to your folder", true, app.hasTicket && !kept,
                  "claim it", app.hasTicket ? (kept ? "the registry signs it, then it's yours" : "keep it first, then claim") : "only a claimable roll can be claimed", app.hasTicket && kept, app.hasTicket && kept);
    if (h2 == 1) app.reqKeep = true;
    if (h2 == 2) app.reqClaimCurrent = true;
    int h3 = pair("set as my wallpaper", "kde: every desktop switches to this chaos", true, false,
                  "full view  [V]", "the chaos fills the window, esc hides the panel", true, false);
    if (h3 == 1) app.reqSetDesktop = true;
    if (h3 == 2) app.settings.desk = false;
    g_keyH = KEY_H; g_keyPx = KEY_PX;
}

void Panel::column(App& app, float W, ImDrawList* dl) {
    // hud: brand, state tags
    {
        ImVec2 p = ImGui::GetCursorScreenPos();
        drawMark(dl, ImVec2(p.x, p.y - 2), 1.6f, ImGui::GetColorU32(g_accent));
        trackedAt(dl, labelFont(), 11, ImVec2(p.x + 36, p.y + 6), "chaos", 11 * 0.32f, ImGui::GetColorU32(tk.SILVER));
        const char* crumbs[] = { "/setup", "/vault", "/roll", "/view", "/model", "/prefs", "/account" };
        trackedAt(dl, labelFont(), LABEL_PX, ImVec2(p.x + 118, p.y + 7), crumbs[tab_], LABEL_PX * 0.12f, ImGui::GetColorU32(tk.FAINT));
        ImGui::Dummy(ImVec2(200, 22));
        ImGui::SameLine(W - 20 - 150);
        if (app.online) tag("online", g_accent, g_accent); else if (app.devOffline) tag("dev offline", tk.GOLD, tk.GOLD); else tag("offline", tk.RED, tk.RED);
        ImGui::SameLine();
        char fpsBuf[32]; std::snprintf(fpsBuf, sizeof fpsBuf, "%.0f fps", app.fps);
        tag(app.settings.paused ? "paused" : fpsBuf, tk.DIM);
        ImGui::Dummy(ImVec2(0, 2));
        ImVec2 q = ImGui::GetCursorScreenPos();
        dl->AddLine(ImVec2(q.x, q.y), ImVec2(q.x + W - 40, q.y), ImGui::GetColorU32(tk.LINE));
        ImGui::Dummy(ImVec2(0, 8));
    }
    // tabs: a row of pills, the active one in the palette
    {
        const char* names[] = { "[setup]", "[vault]", "[roll]", "[view]", "[model]", "[prefs]", "[account]" };
        float kw = (ImGui::GetContentRegionAvail().x - 6 * 4) / 7;
        g_keyPx = 8.5f;
        for (int i = 0; i < 7; i++) { if (i) ImGui::SameLine(0, 4); if (key(names[i], kw, tab_ == i)) tab_ = i; }
        g_keyPx = KEY_PX;
        ImGui::Dummy(ImVec2(0, 6));
    }
    const float footH = 100;
    ImGui::BeginChild("body", ImVec2(0, -footH), false);
    switch (tab_) {
        case 0: tabSetup(app); break;
        case 1: tabVault(app); break;
        case 2: tabRoll(app); break;
        case 3: tabView(app); break;
        case 4: tabModel(app); break;
        case 5: tabSettings(app); break;
        default: tabAccount(app); break;
    }
    ImGui::EndChild();
    // footer: the status line in the serif voice, the keys in the tracked one
    {
        ImVec2 q = ImGui::GetCursorScreenPos();
        dl->AddLine(ImVec2(q.x, q.y), ImVec2(q.x + W - 40, q.y), ImGui::GetColorU32(tk.LINE));
        ImGui::Dummy(ImVec2(0, 8));
        if (serif_) ImGui::PushFont(serif_, 15);
        ImGui::TextWrapped("%s", app.status.empty() ? "Quiet." : app.status.c_str());
        if (serif_) ImGui::PopFont();
        ImGui::Dummy(ImVec2(0, 2));
        lbl("esc panel / r preview / k keep / m mood / n mode / v view", tk.FAINT);
        lbl("1-4 mode / space pause / f fullscreen", tk.FAINT);
        ImGui::SameLine(W - 20 - 70);
        lbl(clockNow().c_str(), tk.FAINT);
    }
}

// the list is rebuilt only when something changed: verifying two signatures and reading activated.json per entry
// per frame was most of the lag with the vault tab open
void Panel::refreshVault() {
    entries_.clear();
    for (auto& e : vault::list()) {
        VaultRow r; r.e = std::move(e);
        auto c = cf::verify(r.e.file); r.ok = c.ok(); r.why = c.why;
        std::memcpy(r.seed.data(), r.e.file.seed_full, 32);
        r.activated = activation::isActivated(r.seed);
        entries_.push_back(std::move(r));
    }
    vaultDirty_ = false;
}

static void copyButton(const char* id, const std::string& text, App& app) {
    ImGui::PushID(id);
    if (key("copy")) { glfwSetClipboardString(app.window.handle(), text.c_str()); app.status = std::string("copied ") + id; }
    ImGui::PopID();
}

// the whole path, four steps, one key each. a step shows done once it is
void Panel::tabSetup(App& app) {
    const bool signedIn = app.loggedIn();
    const bool haveRoll = app.hasTicket;
    const bool kept = app.kept.has_value();
    auto step = [&](int n, const char* title, bool done, const char* why) {
        char num[8]; std::snprintf(num, sizeof num, "%02d", n);
        ImGui::BeginGroup();
        if (done) tag("done", g_accent, g_accent); else tag(num);
        ImGui::SameLine(); if (headline_) ImGui::PushFont(headline_, 15); ImGui::Text("%s", title); if (headline_) ImGui::PopFont();
        ImGui::PushTextWrapPos(); ImGui::TextColored(tk.DIM, "%s", why); ImGui::PopTextWrapPos();
        ImGui::EndGroup();
    };
    if (serif_) ImGui::PushFont(serif_, 17);
    ImGui::TextWrapped("Four steps from nothing to a dimension of your own on the desktop.");
    if (serif_) ImGui::PopFont();
    ImGui::Dummy(ImVec2(0, 6));
    step(1, "sign in", signedIn, app.online ? "the registry needs an account. one claimable roll a week." : "the registry is not reachable right now (settings tab: registry url).");
    if (!signedIn) { if (primaryKey("sign in (account tab)")) tab_ = 6; }
    ImGui::Dummy(ImVec2(0, 10));
    step(2, "roll a claimable dimension", haveRoll, "commit, ticket and seed in one go. R gives free previews that can never be claimed.");
    ImGui::BeginDisabled(!signedIn || (app.me && app.me->banked < 1));
    if (primaryKey(app.me && app.me->banked < 1 ? ("next claimable roll in " + humanMs(app.me->nextInMs)).c_str() : "roll a claimable one")) app.reqClaimableRoll = true;
    ImGui::EndDisabled();
    ImGui::Dummy(ImVec2(0, 10));
    step(3, "keep it, then claim it", kept && !haveRoll, "keep writes the file to your folder; claim asks the registry to sign it. then it is yours, listed in your vault.");
    ImGui::BeginDisabled(!haveRoll);
    if (!kept) { if (primaryKey("keep it to my folder  [K]")) app.reqKeep = true; } else { if (primaryKey("claim it (the registry signs it)")) app.reqClaimCurrent = true; }
    ImGui::EndDisabled();
    ImGui::Dummy(ImVec2(0, 10));
    step(4, "put it on the desktop", false, "kde: one key. elsewhere, pick the CHAOS wallpaper in your desktop settings and paste the seed.");
    if (primaryKey("set as my wallpaper")) app.reqSetDesktop = true;
    ImGui::Dummy(ImVec2(0, 10));
    lbl("files");
    ImGui::TextWrapped("%s", vault::dir().c_str());
    ImGui::TextDisabled("change it on the account tab.");
    ImGui::Dummy(ImVec2(0, 8));
}

// your own model on the stage. offline, local, a display choice: never a seed, never claimable, never valued
void Panel::tabModel(App& app) {
    Persisted& c = app.persisted;
    if (serif_) ImGui::PushFont(serif_, 17);
    ImGui::TextWrapped("Your own thing on the stage. A .cheng replaces the main shape; the satellites go; the rest stays the seed's.");
    if (serif_) ImGui::PopFont();
    ImGui::TextDisabled("obj, stl, gltf, glb straight in. blend, fbx, dae, step, usd through blender if it is installed. the converter welds, keeps every seam and groove (it only thins a mesh past 120k vertices), sizes it; the .cheng lands in your models folder. it brings the materials along too: base colours, vertex colours and one base-colour texture (the one covering the most of it), roughness, metal, glow. pick 'own' below to see them, or a skin and colour of your own.");
    h2("file");
    static char pathBuf[1024] = {};
    if (pathBuf[0] == 0 && !c.modelPath.empty()) std::strncpy(pathBuf, c.modelPath.c_str(), sizeof pathBuf - 1);
    ImGui::InputText("path", pathBuf, sizeof pathBuf);
    if (primaryKey("choose a file", 0)) app.reqModelImport = "?";
    ImGui::SameLine();
    if (key("use this path")) app.reqModelImport = pathBuf;
    if (!app.reqModelImport.empty()) pathBuf[0] = 0;
    if (app.model) {
        ImGui::Text("%s", app.modelName.c_str()); ImGui::SameLine(); ImGui::TextDisabled("%zu verts  /  %zu triangles", app.model->positions.size(), app.model->indices.size() / 3);
        const Material& mt = app.model->material;
        if (mt.hasTexture()) { ImGui::SameLine(); ImGui::TextDisabled("/  texture %dx%d", mt.texW, mt.texH); }
        if (app.model->hasColors()) { ImGui::SameLine(); ImGui::TextDisabled("/  vertex colours"); }
        if (!app.model->hasOwnLook()) { ImGui::SameLine(); ImGui::TextDisabled("/  no material of its own"); }
    }
    else lbl("no model loaded", tk.FAINT);
    h2("look");
    const char* skins[] = { "own (its textures and colours)", "berry (the seed's skin)", "mono (one colour)", "wire", "random each start" };
    const char* skinKeys[] = { "own", "berry", "mono", "wire", "random" };
    int si = 2; for (int i = 0; i < 5; i++) if (c.modelSkin == skinKeys[i]) si = i;
    if (ImGui::Combo("skin", &si, skins, 5)) { c.modelSkin = skinKeys[si]; app.reqModelApply = true; }
    if (c.modelSkin == "own" && app.model && !app.model->hasOwnLook()) ImGui::TextDisabled("this one came with nothing: it shows mono until you pick another skin.");
    float rgb[3] = { 0.9f, 0.93f, 0.91f }; parseHexColor(c.modelColor, rgb);
    if (ImGui::ColorEdit3("colour", rgb, ImGuiColorEditFlags_NoInputs)) { char hex[8]; std::snprintf(hex, sizeof hex, "#%02x%02x%02x", int(rgb[0] * 255), int(rgb[1] * 255), int(rgb[2] * 255)); c.modelColor = hex; app.reqModelApply = true; }
    ImGui::SameLine(); if (key("random")) app.reqModelRandom = true;
    ImGui::TextDisabled("mono and wire use the colour; berry uses the look below; own ignores both.");
    // the look from a seed: the current one, or any kept in the vault. berry skin, palette, grading, lights come from it
    {
        if (vaultDirty_) refreshVault();
        std::string cur = c.modelSkinSeed;
        std::string label = cur.empty() ? "the current seed" : cur.substr(0, 16);
        for (const auto& row : entries_) if (toHex(row.seed) == cur) { label = row.e.name + "  " + tierName(Tier(row.e.file.tier)) + " / " + familyName(row.e.file.family); break; }
        if (ImGui::BeginCombo("look from", label.c_str())) {
            if (ImGui::Selectable("the current seed", cur.empty())) { c.modelSkinSeed.clear(); app.reqModelApply = true; }
            for (const auto& row : entries_) {
                std::string hex = toHex(row.seed), item = row.e.name + "  " + tierName(Tier(row.e.file.tier)) + " / " + familyName(row.e.file.family) + " / " + modeName(Mode(row.e.file.mode));
                if (ImGui::Selectable(item.c_str(), hex == cur)) { c.modelSkinSeed = hex; app.reqModelApply = true; }
            }
            ImGui::EndCombo();
        }
        ImGui::TextDisabled("a seed's skin, palette, lights and grading on your model. the seed itself is not touched.");
    }
    h2("stage");
    ImGui::BeginDisabled(!app.model);
    if (!c.modelOn) { if (primaryKey("put it on the stage")) { c.modelOn = true; app.reqModelApply = true; } }
    else { if (primaryKey("back to the seed's shape")) { c.modelOn = false; app.reqModelApply = true; } }
    ImGui::EndDisabled();
    if (key("save", -1)) { app.status = saveSettings(c) ? "saved" : "could not save settings"; }
    ImGui::TextDisabled("\"set as desktop wallpaper\" (view tab) carries the model to the plasma wallpaper too.");
    lbl("models folder", tk.FAINT);
    ImGui::TextWrapped("%s", model::modelsDir().c_str());
}

void Panel::tabVault(App& app) {
    if (vaultDirty_) refreshVault();
    if (primaryKey("keep current roll  [K]", 0)) app.reqKeep = true;
    ImGui::SameLine();
    if (key("refresh")) { vaultDirty_ = true; thumbs_.clear(); }
    ImGui::SameLine();
    ImGui::TextDisabled("%zu files", entries_.size());
    lbl(vault::dir().c_str());
    ImGui::BeginChild("entries", ImVec2(0, 0), false);
    for (const auto& row : entries_) {
        const auto& e = row.e;
        ImGui::PushID(e.name.c_str());
        std::string png = (vault::dir() / (e.name + ".png")).string();
        int tw, th; unsigned tex = thumbs_.get(png, tw, th);
        if (tex) ImGui::Image((ImTextureID)(intptr_t)tex, ImVec2(144, 81));
        else ImGui::Dummy(ImVec2(144, 81));
        ImGui::SameLine();
        ImGui::BeginGroup();
        ImGui::TextColored(tk.SILVER2, "%s", e.name.c_str());
        lbl((std::string(cf::stateName(e.file.state)) + " / " + tierName(Tier(e.file.tier)) + " / " + familyName(e.file.family) + " / " + modeName(Mode(e.file.mode))).c_str(), tk.DIM);
        if (row.ok) good("valid"); else ImGui::TextColored(tk.RED, "invalid: %s", row.why.c_str());
        if (e.file.state == CHAOS_STATE_CLAIMED) ImGui::TextDisabled("owner %llu", (unsigned long long)e.file.owner_id);
        const Seed32& es = row.seed;
        bool act = row.activated;
        if (act) good("activated (plays offline)"); else ImGui::TextDisabled(app.online && app.loggedIn() ? "not activated yet: load it to activate" : "kept here: plays, activates once loaded signed in");
        if (key("load")) { app.reqLoad = es; }
        if (e.file.state == CHAOS_STATE_ROLLED && app.loggedIn()) { ImGui::SameLine(); if (key("claim")) app.reqClaimSeed = es; }
        ImGui::EndGroup();
        ImGui::Separator();
        ImGui::PopID();
    }
    ImGui::EndChild();
    if (app.kept && !keptSeen_) { vaultDirty_ = true; keptSeen_ = true; }   // one re-list per keep
    if (!app.kept) keptSeen_ = false;
}

void Panel::tabRoll(App& app) {
    if (!app.canGenerate()) {
        if (app.online) {
            tag("sign in", tk.GOLD, tk.GOLD);
            ImGui::TextWrapped("nothing is generated without an account. sign in on the account tab, then roll here. one claimable roll a week per account; previews are free once signed in.");
        } else {
            tag("offline", tk.RED, tk.RED);
            ImGui::TextWrapped("nothing is generated offline. a chaos kept in the vault, or loaded online once, can be used. reconnect and sign in to roll.");
        }
        ImGui::TextDisabled("registry: %s", app.registryUrl.c_str());
        return;
    }
    h2("preview");
    ImGui::TextWrapped("a preview roll is free and unlimited. it renders, it can be kept, it can never be claimed.");
    if (key("roll a free preview  [R]", -1)) app.reqReroll = true;
    hint("renders, can be kept, can never be claimed. as many as you like.");
    h2("claimable");
    if (app.loggedIn()) {
        ImGui::TextWrapped("claimable roll: commit, ticket, seed, in one go. %d/%d in the bank; one a week.%s", app.me->banked, app.me->max, app.me->nextInMs > 0 ? ("  next one in " + humanMs(app.me->nextInMs) + ".").c_str() : "");
        ImGui::BeginDisabled(app.me->banked < 1);
        if (primaryKey(app.me->banked < 1 ? ("next claimable roll in " + humanMs(app.me->nextInMs)).c_str() : "roll a claimable one")) app.reqClaimableRoll = true;
        ImGui::EndDisabled();
        hint("commit, ticket, seed in one go. one a week per account. keep it, then claim it and it is yours.");
        if (app.hasTicket) {
            bool kept = app.kept.has_value();
            ImGui::BeginDisabled(!kept);
            if (key(kept ? "claim it (the registry signs it)" : "keep it first [K], then claim", -1)) app.reqClaimCurrent = true;
            ImGui::EndDisabled();
        }
        ImGui::Spacing();
        ImGui::TextDisabled("or by hand:");
    } else {
        ImGui::TextWrapped("connect the registry session (account tab) and the claimable roll is one button. by hand:");
    }
    if (ticketBuf_[0] == 0 && !app.ticketPath.empty()) std::strncpy(ticketBuf_, app.ticketPath.c_str(), sizeof ticketBuf_ - 1);
    ImGui::InputText("ticket", ticketBuf_, sizeof ticketBuf_);
    if (revealBuf_[0] == 0 && !app.revealPath.empty()) std::strncpy(revealBuf_, app.revealPath.c_str(), sizeof revealBuf_ - 1);
    ImGui::InputText("reveal", revealBuf_, sizeof revealBuf_);
    if (key("roll with this ticket", -1)) { app.ticketPath = ticketBuf_; app.revealPath = revealBuf_; app.reqRerollTicket = true; }
    hint("the by-hand route: chaos --commit out.reveal prints a commit, the vault page on the site turns it into a .ticket, and that ticket rolls with the SAME out.reveal. don't run --commit again in between, the ticket only matches the reveal it was made for.");
    h2("this roll");
    ImGui::Text("%s", app.hasTicket ? "claimable (ticket)" : "preview");
    if (app.hasTicket) ImGui::TextDisabled("ticket user %llu", (unsigned long long)app.ticket.user_id);
    if (app.kept) ImGui::TextWrapped("kept: %s", app.kept->chaosPath.c_str());
    h2("load");
    ImGui::InputText("seed", seedBuf_, sizeof seedBuf_);
    if (key("load seed (hex / base58 / 24 words)", -1)) {
        try { app.reqLoad = seedFromString(seedBuf_); } catch (const std::exception& ex) { app.status = ex.what(); }
    }
}

void Panel::tabView(App& app) {
    Dimension& d = *app.dim;
    // the three spellings of the seed, computed once per seed, not per frame (bip39 hashes)
    static Seed32 cachedSeed{}; static std::string hex, b58, words;
    if (hex.empty() || std::memcmp(cachedSeed.data(), d.seed.data(), 32) != 0) { cachedSeed = d.seed; hex = toHex(d.seed); b58 = base58Encode(d.seed.data(), 32); words = bip39Encode(d.seed); }
    lbl("hex"); ImGui::TextColored(tk.SILVER2, "%s", hex.c_str()); ImGui::PushTextWrapPos(); ImGui::PopTextWrapPos(); copyButton("hex", hex, app);
    lbl("base58"); ImGui::TextColored(tk.SILVER2, "%s", b58.c_str()); copyButton("base58", b58, app);
    lbl("words"); ImGui::PushTextWrapPos(); ImGui::TextColored(tk.SILVER2, "%s", words.c_str()); ImGui::PopTextWrapPos(); copyButton("words", words, app);
    h2("the dimension");
    if (d.unlocks.gold) tag(tierName(d.tier), tk.GOLD, tk.GOLD); else tag(tierName(d.tier));
    ImGui::SameLine(); ImGui::TextDisabled("/ family %d %s / score %.2f", d.family, familyName(d.family), d.score);
    ImGui::Text("%02d %s / %zu verts / r %.2f", d.shapeType, shapeName(d.shapeType), d.mainMesh.vertexCount(), d.mainRadius);
    std::string lin; for (size_t i = 0; i < d.lineage.size(); i++) lin += (i ? " -> " : "") + d.lineage[i];
    ImGui::TextWrapped("lineage: %s", lin.c_str());
    ImGui::Text("%zu satellites / %zu particles / %zu rings / %zu debris / %zu fold groups", d.sats.size(), d.particles.pos.size(), d.rings.size(), d.debris.size(), d.folds.size());
    h2("display");
    float mood = d.moodTarget;
    if (ImGui::SliderFloat("mood", &mood, 0, 1, "%.2f")) { d.moodTarget = mood; d.moodLocked = true; }
    ImGui::SameLine(); ImGui::TextDisabled("%s", moodNameOf(mood));
    int mode = int(d.displayMode);
    const char* modes[] = { "ZEN", "CALM", "CHAOS", "BLEED" };
    if (ImGui::Combo("mode", &mode, modes, 4)) d.setDisplayMode(Mode(mode));
    ImGui::SameLine(); ImGui::TextDisabled("native %s", modeName(d.nativeMode));
    ImGui::Text("palette"); ImGui::SameLine();
    ImGui::ColorButton("A", col(d.pal.a), 0, ImVec2(40, 20)); ImGui::SameLine();
    ImGui::ColorButton("B", col(d.pal.b), 0, ImVec2(40, 20));
    if (d.unlocks.thirdStop) { ImGui::SameLine(); ImGui::ColorButton("C", col(d.palC), 0, ImVec2(40, 20)); }
    ImGui::SameLine(); ImGui::TextDisabled("tempo %.2f / %d jumps", d.paletteTempo, d.pal.jumps);
    ImGui::ProgressBar(d.mutV / d.mutMax, ImVec2(-1, 0), "mutation");
    h2("desktop");
    if (primaryKey("set as desktop wallpaper")) app.reqSetDesktop = true;
    ImGui::TextDisabled("kde: switches every desktop to the CHAOS wallpaper with this seed. draw / size / fps are on the wallpaper's own page.");
}

void Panel::tabSettings(App& app) {
    Settings& s = app.settings;
    Persisted& c = app.persisted;
    h2("this window");
    if (primaryKey(s.paused ? "resume  [space]" : "pause  [space]")) s.paused = !s.paused;
    ImGui::Checkbox("vsync", &s.vsync);
    ImGui::SliderInt("fps cap (0 = off)", &s.fpsCap, 0, 144);
    ImGui::Checkbox("cap at 30 fps on battery", &c.batteryCap);
    ImGui::Checkbox("wallpaper mode: pause while a fullscreen window has focus (x11)", &c.pauseOnFullscreen);
    ImGui::TextDisabled("both off by default. the plasma wallpaper has its own pause switch on its settings page.");
    int count = 0; GLFWmonitor** mons = glfwGetMonitors(&count);
    if (count > 0) {
        std::vector<const char*> names; for (int i = 0; i < count; i++) names.push_back(glfwGetMonitorName(mons[i]));
        if (s.monitor >= count) s.monitor = 0;
        ImGui::Combo("monitor", &s.monitor, names.data(), count);
    }
    bool fsBefore = s.fullscreen;
    ImGui::Checkbox("fullscreen  [F]", &s.fullscreen);
    if (fsBefore != s.fullscreen) { s.fullscreen = fsBefore; app.reqFullscreen = true; }
    ImGui::Checkbox("boot poster", &s.poster);
    h2("display");
    {
        const char* themeOpts[] = { "dark", "light (paper)" };
        int ti = c.theme == "light" ? 1 : 0;
        if (ImGui::Combo("theme", &ti, themeOpts, 2)) c.theme = ti ? "light" : "dark";
        const char* drawOpts[] = { "everything the seed rolled", "no satellites", "the main shape alone" };
        int di = c.layers == "nosats" ? 1 : c.layers == "main" ? 2 : 0;
        if (ImGui::Combo("draw", &di, drawOpts, 3)) c.layers = di == 1 ? "nosats" : di == 2 ? "main" : "";
        const char* sizeOpts[] = { "full", "3/4", "half" };
        int si = c.renderScale <= 0.5f ? 2 : c.renderScale <= 0.75f ? 1 : 0;
        if (ImGui::Combo("render size", &si, sizeOpts, 3)) c.renderScale = si == 2 ? 0.5f : si == 1 ? 0.75f : 1.f;
        const char* aaOpts[] = { "off", "2x", "4x" };
        int ai = c.msaa >= 4 ? 2 : c.msaa >= 2 ? 1 : 0;
        if (ImGui::Combo("anti-aliasing", &ai, aaOpts, 3)) c.msaa = ai == 2 ? 4 : ai == 1 ? 2 : 0;
        const char* detOpts[] = { "full", "lite (cheaper noise and bloom)" };
        int di2 = c.detail == "lite" ? 1 : 0;
        if (ImGui::Combo("detail", &di2, detOpts, 2)) c.detail = di2 ? "lite" : "full";
        ImGui::TextDisabled("this screen's choice only. the chaos, its value and its certificate do not change.");
        if (key("low-end preset")) { c.renderScale = 0.6f; c.msaa = 0; c.detail = "lite"; c.layers = "nosats"; s.fpsCap = 30; c.fpsCap = 30; app.status = "low-end preset: 60% size, no msaa, lite detail, no satellites, 30 fps. save settings to keep it"; }
        ImGui::SameLine();
        if (key("full preset")) { c.renderScale = 1.f; c.msaa = 2; c.detail = "full"; c.layers = ""; s.fpsCap = 0; c.fpsCap = 0; app.status = "full preset. save settings to keep it"; }
        ImGui::TextDisabled("the wallpaper and the plasma plugin read the same keys.");
    }
    h2("registry");
    static char regBuf[256] = {};
    if (regBuf[0] == 0) std::strncpy(regBuf, c.registryUrl.empty() ? app.registryUrl.c_str() : c.registryUrl.c_str(), sizeof regBuf - 1);
    if (ImGui::InputText("registry url", regBuf, sizeof regBuf)) { c.registryUrl = regBuf; app.registryUrl = regBuf; app.reqCheckOnline = true; }
    ImGui::TextDisabled("http://localhost:5173 while the site is local; the public domain once it is up. checked every 30 s.");
    ImGui::TextDisabled("wallpaper settings (draw, size, fps, battery) live on the plasma wallpaper page: right click the desktop, configure.");
    if (primaryKey("save settings")) {
        c.vsync = s.vsync; c.fpsCap = s.fpsCap; c.poster = s.poster;
        app.status = saveSettings(c) ? "saved " + settingsPath().string() : "could not save settings";
    }
    h2("");
    if (key("quit")) app.reqQuit = true;
}

void Panel::tabAccount(App& app) {
    if (app.me) {
        lbl("signed in");
        ImGui::Text("%s", app.me->name.c_str()); ImGui::SameLine(); ImGui::TextDisabled("%s", app.me->email.c_str());
        ImGui::Text("claimable rolls in the bank: %d / %d", app.me->banked, app.me->max);
        if (app.me->nextInMs > 0) ImGui::TextDisabled("next one in %s", humanMs(app.me->nextInMs).c_str());
        if (primaryKey("sync vault")) app.reqSync = true;
        ImGui::TextDisabled("states for every seed you hold, fetches what you own but do not hold");
        if (key("sign out", -1)) app.reqLogout = true;
    } else {
        lbl("sign in");
        if (!app.online) msg("offline: the registry is not reachable.", true);
        else ImGui::TextWrapped("sign in on the registry, it shows a token, paste it here.");
        std::string url = app.registryUrl + "/login?app=1";
        if (primaryKey("open the sign-in page")) { std::string cmd = "xdg-open '" + url + "' >/dev/null 2>&1 &"; if (std::system(cmd.c_str()) != 0) app.status = "could not open a browser, go to " + url; }
        ImGui::TextDisabled("%s", url.c_str());
        static char tokBuf[2048] = {};
        ImGui::InputText("token", tokBuf, sizeof tokBuf, ImGuiInputTextFlags_Password);
        ImGui::BeginDisabled(!app.online || tokBuf[0] == 0);
        if (primaryKey("connect")) { app.reqConnectToken = tokBuf; std::memset(tokBuf, 0, sizeof tokBuf); }
        ImGui::EndDisabled();
        hint("the whole token, it is long. copy it with the key on the sign-in page, paste it here, connect. 30 days.");
    }
    h2("your .chaos files");
    {
        static char dirBuf[1024] = {};
        if (dirBuf[0] == 0) std::strncpy(dirBuf, vault::dir().c_str(), sizeof dirBuf - 1);
        ImGui::InputText("folder", dirBuf, sizeof dirBuf);
        if (key("choose a folder")) app.reqVaultDir = "?";
        ImGui::SameLine();
        if (key("use this path")) app.reqVaultDir = dirBuf;
        if (!app.reqVaultDir.empty()) dirBuf[0] = 0;   // re-read after the move
        ImGui::TextDisabled("every roll you keep, every claim and every sync lands here as <seed>.chaos + .png. the wallpaper reads the same folder.");
    }
    h2("registry");
    ImGui::Text("%s", app.registryUrl.c_str()); ImGui::SameLine();
    if (app.online) tag("online", g_accent, g_accent); else tag("offline", tk.RED, tk.RED);
    ImGui::SameLine(); if (key("check now")) app.reqCheckOnline = true;
    h2("this install");
    auto inst = vault::instanceId();
    std::string instHex; char b[4]; for (uint8_t x : inst) { std::snprintf(b, 4, "%02x", x); instHex += b; }
    std::string pkHex; for (int i = 0; i < 8; i++) { std::snprintf(b, 4, "%02x", CHAOS_SERVER_PUBKEY[i]); pkHex += b; }
    lbl("instance"); ImGui::TextColored(tk.SILVER2, "%s", instHex.c_str());
    lbl("registry key"); ImGui::TextColored(tk.SILVER2, "%s... (dev key)", pkHex.c_str());
    lbl("vault"); ImGui::TextWrapped("%s", vault::dir().c_str());
    (void)app;
}

} // namespace chaos
