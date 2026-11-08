#pragma once
// ChaosView: a QQuickFramebufferObject that draws the engine. plasmashell hosts it as a wallpaper.
// the gl work happens on qt's render thread, inside qt's context; we load gl from that context once.
// the deal with plasmashell: never throw out of a qt callback, never block the gui thread, never touch a dead
// object from a worker, never leave the desktop black without saying why (the status, the log, and the lobby).
// plasmashell will not tell you what went wrong. it just paints its fallback and shuts up. so we tell ourselves.
#include <QQuickFramebufferObject>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QFutureWatcher>
#include <QVariantList>
#include <QtQml/qqml.h>
#include <memory>
#include <atomic>

class ChaosView : public QQuickFramebufferObject {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QString seed READ seed WRITE setSeed NOTIFY seedChanged)        // hex / base58 / words. empty = random (online) or an activated one (offline)
    Q_PROPERTY(QString mode READ mode WRITE setMode NOTIFY modeChanged)        // "", zen, calm, chaos, bleed
    Q_PROPERTY(int fps READ fps WRITE setFps NOTIFY fpsChanged)
    Q_PROPERTY(QString registry READ registry WRITE setRegistry NOTIFY registryChanged)
    Q_PROPERTY(bool pauseOnBattery READ pauseOnBattery WRITE setPauseOnBattery NOTIFY pauseOnBatteryChanged)
    Q_PROPERTY(bool preview READ preview WRITE setPreview NOTIFY previewChanged)   // the settings dialog's thumbnail: low fps, no registry probe
    Q_PROPERTY(QString extras READ extras WRITE setExtras NOTIFY extrasChanged)       // "" everything, "nosats" no satellites, "main" the main shape alone
    Q_PROPERTY(double renderScale READ renderScale WRITE setRenderScale NOTIFY renderScaleChanged)   // 0.5 .. 1: the engine draws smaller, the desktop scales it up
    Q_PROPERTY(int samples READ samples WRITE setSamples NOTIFY samplesChanged)                     // msaa: 0, 2, 4
    Q_PROPERTY(bool lite READ lite WRITE setLite NOTIFY liteChanged)                                 // cheaper background noise and bloom chain
    Q_PROPERTY(bool modelOn READ modelOn WRITE setModelOn NOTIFY modelChanged)                       // your own .cheng on the stage
    Q_PROPERTY(QString model READ model WRITE setModel NOTIFY modelChanged)
    Q_PROPERTY(QString modelSkin READ modelSkin WRITE setModelSkin NOTIFY modelChanged)
    Q_PROPERTY(QString modelColor READ modelColor WRITE setModelColor NOTIFY modelChanged)
    Q_PROPERTY(QString modelSkinSeed READ modelSkinSeed WRITE setModelSkinSeed NOTIFY modelChanged)   // hex: wear this seed's look
    Q_PROPERTY(int coverage READ coverage WRITE setCoverage NOTIFY coverageChanged)                  // 0 desktop visible, 1 a maximised window over it, 2 a fullscreen one (a game): paused
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(bool online READ online NOTIFY onlineChanged)
public:
    explicit ChaosView(QQuickItem* parent = nullptr);
    ~ChaosView() override;
    Renderer* createRenderer() const override;
    void componentComplete() override;         // properties are set by now: first probe happens here, not in the ctor

    QString seed() const { return m_seed; }        void setSeed(const QString& s);
    QString mode() const { return m_mode; }        void setMode(const QString& m);
    int fps() const { return m_fps; }              void setFps(int f);
    QString registry() const { return m_registry; } void setRegistry(const QString& r);
    bool pauseOnBattery() const { return m_pauseOnBattery; } void setPauseOnBattery(bool b);
    bool preview() const { return m_preview; }     void setPreview(bool p);
    QString extras() const { return m_extras; }     void setExtras(const QString& e);
    double renderScale() const { return m_scale; }  void setRenderScale(double s);
    int samples() const { return m_samples; }       void setSamples(int s);
    bool lite() const { return m_lite; }            void setLite(bool l);
    bool modelOn() const { return m_modelOn; }      void setModelOn(bool b);
    QString model() const { return m_model; }       void setModel(const QString& p);
    QString modelSkin() const { return m_modelSkin; } void setModelSkin(const QString& s);
    QString modelColor() const { return m_modelColor; } void setModelColor(const QString& c);
    QString modelSkinSeed() const { return m_modelSkinSeed; } void setModelSkinSeed(const QString& s);
    int coverage() const { return m_coverage; }     void setCoverage(int c);
    QString status() const { return m_status; }
    bool online() const { return m_online.load(); }

    struct Snapshot { QString seed, mode, extras; double scale; int samples; bool lite; bool online; bool preview; bool modelOn; QString model, modelSkin, modelColor, modelSkinSeed; };
    Snapshot snapshot() const { return { m_seed, m_mode, m_extras, m_scale, m_samples, m_lite, m_online.load(), m_preview, m_modelOn, m_model, m_modelSkin, m_modelColor, m_modelSkinSeed }; }
    void setStatus(const QString& s);          // gui thread only. the renderer hands its status over inside synchronize()

signals:
    void seedChanged(); void modeChanged(); void fpsChanged(); void registryChanged(); void pauseOnBatteryChanged(); void previewChanged(); void statusChanged(); void onlineChanged(); void extrasChanged(); void renderScaleChanged(); void samplesChanged(); void liteChanged(); void modelChanged(); void coverageChanged();

private:
    void probe();
    void scheduleReprobe();
    void repace();                              // timer vs vsync, after fps / preview / window changes
    QString m_seed, m_mode, m_registry, m_status, m_extras;
    double m_scale = 0.75;
    int m_samples = 0;
    bool m_lite = false;
    bool m_modelOn = false; QString m_model, m_modelSkin = "mono", m_modelColor = "#e7eee9", m_modelSkinSeed;
    int m_coverage = 0;
    int m_fps = 30;
    bool m_vsyncPaced = false;
    bool m_pauseOnBattery = true, m_preview = false, m_paused = false, m_complete = false;
    std::atomic<bool> m_online{ false };
    QTimer m_tick, m_power, m_reprobe;
    // the registry probe: a worker computes, the watcher (a member, so it dies with us) delivers on the gui thread.
    // nothing on the worker ever touches this object.
    QFutureWatcher<bool> m_probe;
};

// the vault and the status, for the settings page: what this install holds, and what the wallpaper last said.
// status() is process wide: the live wallpaper writes it, the settings dialog reads it (same plasmashell).
class ChaosVault : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON
public:
    Q_INVOKABLE QVariantList entries() const;      // [{seed, short, tier, family, mode, state, activated, label}]
    Q_INVOKABLE QString vaultDir() const;
    Q_INVOKABLE QString status() const;            // the last line the wallpaper reported
    Q_INVOKABLE QStringList log() const;           // the last few lines, newest last
    Q_INVOKABLE QString logPath() const;           // ~/.cache/chaos/plasma.log
};
