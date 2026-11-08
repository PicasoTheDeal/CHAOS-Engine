#include "chaositem.h"
#include <QOpenGLFramebufferObject>
#include <QOpenGLContext>
#include <QQuickWindow>
#include <QQuickOpenGLUtils>
#include <QElapsedTimer>
#include <QVariantMap>
#include <QStandardPaths>
#include <QDateTime>
#include <QFile>
#include <QDir>
#include <QMutex>
#include <QMutexLocker>
#include <QtConcurrent/QtConcurrent>
#include <exception>
#include <algorithm>
#include <cstring>
#include "render/gl_boot.h"
#include "render/renderer.h"
#include "scene/dimension.h"
#include "seed/seed.h"
#include "seed/rarity.h"
#include "wallpaper_seed.h"
#include "sync/api.h"
#include "vault/vault.h"
#include "chaos_format.h"
#include "vault/activation.h"
#include "settings.h"
#include "model/cheng.h"
#include "platform/power.h"

using namespace chaos;

namespace {

// ---- status bus: one line of truth per process, mirrored to a small log file so a black desktop is never silent
struct StatusBus {
    QMutex mu;
    QString last;
    QStringList lines;
    QString path;
    void say(const QString& s) {
        QMutexLocker lk(&mu);
        if (s == last) return;
        last = s;
        const QString line = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss") + "  " + s;
        lines.push_back(line);
        while (lines.size() > 40) lines.pop_front();
        try {
            if (path.isEmpty()) {
                QString dir = QDir(QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation)).filePath("chaos");   // ~/.cache/chaos
                QDir().mkpath(dir);
                path = QDir(dir).absoluteFilePath("plasma.log");
            }
            QFile f(path);
            const auto mode = f.size() > 256 * 1024 ? (QIODevice::WriteOnly | QIODevice::Truncate) : (QIODevice::WriteOnly | QIODevice::Append);
            if (f.open(mode)) { f.write((line + "\n").toUtf8()); f.close(); }
        } catch (...) {}
    }
};
StatusBus& bus() { static StatusBus b; return b; }

// one gl bootstrap per process; mesa shares dispatch across contexts so loading once is enough
std::atomic<int> g_glState{ 0 };   // 0 untried, >0 version (major*10+minor), -1 failed

constexpr int kMaxFboSide = 8192;
constexpr int kVsyncFrom = 50;          // at this fps and above we ride the window's own swap instead of a timer

       // nothing sane is bigger; a bad size must not become a gpu allocation
constexpr int kMaxFailures = 3;         // consecutive render exceptions before we stop rebuilding and just clear
constexpr double kRetryAfterS = 3.0;    // pause between rebuild attempts after an exception

class FboRenderer : public QQuickFramebufferObject::Renderer {
public:
    FboRenderer() { m_clock.start(); }

    // synchronize runs on the render thread while the gui thread is blocked: the only place we may touch the item
    void synchronize(QQuickFramebufferObject* item) override {
        auto* v = static_cast<ChaosView*>(item);
        const double oldScale = m_snap.scale;
        m_snap = v->snapshot();
        if (m_snap.scale != oldScale && framebufferObject()) invalidateFramebufferObject();   // rebuilt at the new size before the next render
        if (!m_pendingStatus.isEmpty()) { v->setStatus(m_pendingStatus); m_pendingStatus.clear(); }
    }

    // the fbo may be smaller than the item: qt stretches the texture, the engine pays for fewer pixels
    QOpenGLFramebufferObject* createFramebufferObject(const QSize& size) override {
        QOpenGLFramebufferObjectFormat f;
        f.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);
        const double sc = std::clamp(m_snap.scale > 0 ? m_snap.scale : 1.0, 0.25, 1.0);
        QSize s(std::clamp(int(size.width() * sc), 1, kMaxFboSide), std::clamp(int(size.height() * sc), 1, kMaxFboSide));
        return new QOpenGLFramebufferObject(s, f);
    }

    void tell(const QString& s) { m_pendingStatus = s; }   // delivered at the next synchronize

    void render() override {
        try { renderGuarded(); }
        catch (const std::exception& e) { failed(QString("render error: ") + e.what()); }
        catch (...) { failed("render error"); }
        try { QQuickOpenGLUtils::resetOpenGLState(); } catch (...) {}
    }

private:
    void failed(const QString& why) {
        m_renderer.reset(); m_dim.reset();
        m_failures++;
        m_retryAt = m_clock.elapsed() / 1000.0 + kRetryAfterS;
        tell(why + (m_failures >= kMaxFailures ? "  (giving up until the settings change)" : "  (retrying)"));
    }

    void dark(unsigned fbo) { glboot::clearFramebuffer(fbo, 0.043f, 0.047f, 0.043f); }   // #0b0c0b, the page background

    bool bootGl() {
        if (g_glState.load() == 0) {
            auto* ctx = QOpenGLContext::currentContext();
            int v = 0;
            if (ctx && !ctx->isOpenGLES()) v = glboot::load([](const char* n) -> void* { return reinterpret_cast<void*>(QOpenGLContext::currentContext()->getProcAddress(n)); });
            if (ctx && ctx->isOpenGLES()) tell("plasmashell runs opengl es here; the engine needs desktop opengl 4.3 (QT_QUICK_BACKEND / QSG_RHI_BACKEND=opengl, desktop gl)");
            else if (v == 0) tell("could not load opengl in plasmashell's context");
            else if (v < glboot::kNeeded) tell(QString("plasmashell's context is opengl %1.%2; the engine needs 4.3").arg(v / 10).arg(v % 10));
            g_glState.store(v >= glboot::kNeeded ? v : -1);
        }
        return g_glState.load() > 0;
    }

    void renderGuarded() {
        auto* fbo = framebufferObject();
        if (!fbo) return;
        if (!bootGl()) { dark(fbo->handle()); return; }
        const QSize sz = fbo->size();
        const double now = m_clock.elapsed() / 1000.0;

        const QString key = m_snap.seed + "\n" + m_snap.mode + "\n" + (m_snap.modelOn ? m_snap.model + "|" + m_snap.modelSkin + "|" + m_snap.modelColor + "|" + m_snap.modelSkinSeed : QString());
        if (key != m_key) { m_failures = 0; m_retryAt = 0; }                    // the settings changed: a fresh start
        const bool rePick = !m_dim || key != m_key || (m_fallback && m_snap.online != m_onlineAtPick);
        if (rePick) {
            if (m_failures >= kMaxFailures || now < m_retryAt) { dark(fbo->handle()); return; }
            std::string note;
            auto picked = pickWallpaperSeed(m_snap.seed.toStdString(), m_snap.online, note);
            m_fallback = !picked;
            Seed32 seed = picked ? *picked : lobbySeed();                           // never black: the lobby plays instead
            m_renderer.reset(); m_dim.reset();
            m_dim = std::make_unique<Dimension>(rollDimension(seed));
            Mode m; if (!m_snap.mode.isEmpty() && parseMode(m_snap.mode.toStdString(), m)) m_dim->setDisplayMode(m);
            if (m_snap.modelOn && !m_snap.model.isEmpty()) {
                if (auto mm = model::readCheng(m_snap.model.toStdString())) {
                    float rgb[3] = { 0.9f, 0.93f, 0.91f }; parseHexColor(m_snap.modelColor.toStdString(), rgb);
                    applyModel(*m_dim, std::move(*mm), modelSkinIndex(m_snap.modelSkin.toStdString(), unsigned(now)), glm::vec3(rgb[0], rgb[1], rgb[2]));
                    if (!m_snap.modelSkinSeed.isEmpty()) { try { Dimension src = rollDimension(seedFromHex(m_snap.modelSkinSeed.toStdString())); copyLook(*m_dim, src); } catch (...) {} }
                } else if (!m_snap.preview) tell("model file not readable: " + m_snap.model + " (showing the seed's shape)");
            }
            m_renderer = std::make_unique<chaos::Renderer>(*m_dim, sz.width(), sz.height());
            m_key = key; m_onlineAtPick = m_snap.online;
            m_last = now;
            if (!m_snap.preview) {
                QString what = QString::fromStdString(toShortHex(seed)) + " / " + tierName(m_dim->tier);
                tell(m_fallback ? "lobby (" + QString::fromStdString(note) + ")" : "showing " + what + (note.empty() ? "" : " (" + QString::fromStdString(note) + ")"));
            }
        }
        if (!m_renderer) return;
        float dt = float(std::clamp(now - m_last, 0.0, 0.1)); m_last = now;
        m_renderer->setLayers(layersFromName(m_snap.extras.toStdString()));
        m_renderer->setQuality({ 1.f, m_snap.samples, m_snap.lite || m_snap.preview });   // the thumbnail is always lite
        m_renderer->resize(sz.width(), sz.height());
        m_renderer->frame(float(now), dt, fbo->handle());
        m_failures = 0;
    }

    QString m_pendingStatus;
    ChaosView::Snapshot m_snap;
    std::unique_ptr<Dimension> m_dim;
    std::unique_ptr<chaos::Renderer> m_renderer;
    QString m_key;
    bool m_fallback = false, m_onlineAtPick = false;
    int m_failures = 0;
    double m_retryAt = 0;
    QElapsedTimer m_clock;
    double m_last = 0;
};

} // namespace

static void applyVaultDir() { static bool done = false; if (done) return; done = true; try { auto s = loadSettings(); if (!s.vaultDir.empty()) vault::setDir(s.vaultDir); } catch (...) {} }

ChaosView::ChaosView(QQuickItem* parent) : QQuickFramebufferObject(parent) {
    applyVaultDir();
    setMirrorVertically(true);
    connect(&m_probe, &QFutureWatcher<bool>::finished, this, [this] {
        bool on = false;
        try { on = m_probe.result(); } catch (...) {}
        bool was = m_online.exchange(on);
        if (was != on) emit onlineChanged();
        setStatus(on ? "registry online" : "registry offline: only kept or activated seeds play, else the lobby");
        scheduleReprobe();
        update();
    });
    m_registry = QString::fromStdString(api::defaultRegistryUrl());
    m_tick.setTimerType(Qt::PreciseTimer);
    connect(&m_tick, &QTimer::timeout, this, [this] { if (!m_paused && m_coverage != 2 && isVisible()) update(); });
    connect(this, &QQuickItem::windowChanged, this, [this](QQuickWindow* w) {
        if (w) connect(w, &QQuickWindow::frameSwapped, this, [this] { if (m_vsyncPaced && !m_paused && m_coverage == 0 && isVisible()) update(); }, Qt::UniqueConnection);
        repace();
    });
    repace();
    connect(&m_power, &QTimer::timeout, this, [this] {
        bool p = m_pauseOnBattery && power::onBattery();
        if (p != m_paused) { m_paused = p; setStatus(p ? "paused (battery)" : "resumed"); if (!p) update(); }
    });
    m_power.start(2000);
    m_reprobe.setSingleShot(true);
    connect(&m_reprobe, &QTimer::timeout, this, [this] { probe(); });
}

// the watcher is a member and dies with us; the worker holds only a copy of the url, so nothing waits here.
// (waiting would stall plasmashell for the probe's timeout every time the wallpaper is swapped.)
ChaosView::~ChaosView() = default;

QQuickFramebufferObject::Renderer* ChaosView::createRenderer() const { return new FboRenderer(); }

void ChaosView::componentComplete() {
    QQuickFramebufferObject::componentComplete();
    m_complete = true;
    if (!m_preview) QTimer::singleShot(0, this, [this] { probe(); });     // after plasma has pushed the configuration in
}

void ChaosView::scheduleReprobe() {
    if (m_preview) return;
    m_reprobe.start(m_online.load() ? 10 * 60 * 1000 : 30 * 1000);       // offline: look again soon; online: now and then
}

void ChaosView::probe() {
    if (m_preview) { m_online.store(false); return; }   // the settings thumbnail never talks to the network
    if (m_probe.isRunning()) return;
    const std::string url = m_registry.toStdString();  // the worker gets a copy, nothing else
    if (url.empty()) { m_online.store(false); setStatus("no registry set: only kept or activated seeds play, else the lobby"); update(); return; }
    setStatus("asking the registry " + m_registry);
    m_probe.setFuture(QtConcurrent::run([url] { try { return api::health(url).has_value(); } catch (...) { return false; } }));
}

void ChaosView::setSeed(const QString& s) { if (s == m_seed) return; m_seed = s; emit seedChanged(); update(); }
void ChaosView::setMode(const QString& m) { if (m == m_mode) return; m_mode = m; emit modeChanged(); update(); }
// smooth means one frame per display refresh with no timer beating against it. so at 50 fps and above the item asks for
// a frame each time the window swaps (vsync paces it); below that a precise timer paces it and the swap is left alone.
void ChaosView::repace() {
    if (m_coverage == 2 && !m_preview) { m_vsyncPaced = false; m_tick.stop(); return; }      // covered by a fullscreen window: nothing
    int want = m_preview ? std::min(m_fps, 10) : std::max(1, m_fps);
    if (m_coverage == 1 && !m_preview) want = std::min(want, 4);
    m_vsyncPaced = !m_preview && want >= kVsyncFrom && window() != nullptr;
    if (m_vsyncPaced) { m_tick.stop(); update(); }
    else { m_tick.setInterval(1000 / want); if (!m_tick.isActive()) m_tick.start(); }
}
void ChaosView::setFps(int f) { f = std::clamp(f, 1, 120); if (f == m_fps) return; m_fps = f; emit fpsChanged(); repace(); }
void ChaosView::setExtras(const QString& e) { if (e == m_extras) return; m_extras = e; emit extrasChanged(); update(); }
void ChaosView::setModelOn(bool b) { if (b == m_modelOn) return; m_modelOn = b; emit modelChanged(); update(); }
void ChaosView::setModel(const QString& p) { if (p == m_model) return; m_model = p; emit modelChanged(); update(); }
void ChaosView::setModelSkin(const QString& s) { if (s == m_modelSkin) return; m_modelSkin = s; emit modelChanged(); update(); }
void ChaosView::setModelSkinSeed(const QString& s) { if (s == m_modelSkinSeed) return; m_modelSkinSeed = s; emit modelChanged(); update(); }
void ChaosView::setModelColor(const QString& c) { if (c == m_modelColor) return; m_modelColor = c; emit modelChanged(); update(); }
// coverage from the qml side (TasksModel): 2 = a fullscreen window on this screen (a game): no frames at all.
// 1 = a maximised window: a few frames a second, enough to not look frozen in a glimpse. 0 = normal pacing
void ChaosView::setCoverage(int c) {
    c = std::clamp(c, 0, 2);
    if (c == m_coverage) return;
    m_coverage = c; emit coverageChanged();
    setStatus(c == 2 ? "paused: a fullscreen window covers the desktop" : c == 1 ? "slow: a maximised window covers the desktop" : "resumed: desktop visible");
    repace();
}
void ChaosView::setLite(bool l) { if (l == m_lite) return; m_lite = l; emit liteChanged(); update(); }
void ChaosView::setSamples(int s) { s = s >= 4 ? 4 : s >= 2 ? 2 : 0; if (s == m_samples) return; m_samples = s; emit samplesChanged(); update(); }
void ChaosView::setRenderScale(double s) { s = std::clamp(s, 0.25, 1.0); if (s == m_scale) return; m_scale = s; emit renderScaleChanged(); update(); }
void ChaosView::setRegistry(const QString& r) { if (r == m_registry) return; m_registry = r; emit registryChanged(); if (m_complete) probe(); }
void ChaosView::setPauseOnBattery(bool b) { if (b == m_pauseOnBattery) return; m_pauseOnBattery = b; emit pauseOnBatteryChanged(); }
void ChaosView::setPreview(bool p) {
    if (p == m_preview) return;
    m_preview = p; emit previewChanged();
    repace();
    if (!m_preview && m_complete) probe();
}
void ChaosView::setStatus(const QString& s) {
    if (s == m_status) return;
    m_status = s; emit statusChanged();
    if (!m_preview) bus().say(s);
}

// ---- the vault and the status, for the settings page ----
QVariantList ChaosVault::entries() const {
    QVariantList out;
    applyVaultDir();
    try {
        auto all = vault::list();
        auto act = activation::load();
        // claimed first, then rolled, then previews: anything kept here may play (showing it again is not generation)
        std::stable_sort(all.begin(), all.end(), [](const vault::Entry& x, const vault::Entry& y) { auto rank = [](uint8_t st) { return st == CHAOS_STATE_CLAIMED ? 0 : st == CHAOS_STATE_ROLLED ? 1 : 2; }; return rank(x.file.state) < rank(y.file.state); });
        for (const auto& e : all) {
            Seed32 s; std::memcpy(s.data(), e.file.seed_full, 32);
            std::string hex = toHex(s);
            bool activated = act.count(hex) > 0;
            QVariantMap m;
            m["seed"] = QString::fromStdString(hex);
            m["short"] = QString::fromStdString(toShortHex(s));
            m["tier"] = tierName(Tier(e.file.tier));
            m["family"] = familyName(e.file.family);
            m["mode"] = modeName(Mode(e.file.mode));
            m["state"] = cf::stateName(e.file.state);
            m["activated"] = activated;
            m["label"] = QString("%1  %2 / %3 / %4%5").arg(QString::fromStdString(toShortHex(s)), tierName(Tier(e.file.tier)), familyName(e.file.family), cf::stateName(e.file.state), activated ? "  (plays offline)" : "");
            out.push_back(m);
        }
    } catch (...) {}
    return out;
}
QString ChaosVault::vaultDir() const { try { return QString::fromStdString(vault::dir().string()); } catch (...) { return {}; } }
QString ChaosVault::status() const { QMutexLocker lk(&bus().mu); return bus().last; }
QStringList ChaosVault::log() const { QMutexLocker lk(&bus().mu); return bus().lines; }
QString ChaosVault::logPath() const { QMutexLocker lk(&bus().mu); return bus().path.isEmpty() ? QString("~/.cache/chaos/plasma.log") : bus().path; }
