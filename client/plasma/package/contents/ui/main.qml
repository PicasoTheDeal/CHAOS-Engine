import QtQuick
import org.kde.plasma.plasmoid
import org.chaos.wallpaper 1.0

// plasma 6: the root of a wallpaper package must be a WallpaperItem, and the settings arrive as its `configuration`
// (the old `wallpaper` context id is gone; using it leaves a black desktop and a ReferenceError in the journal)
WallpaperItem {
    id: root
    // windows over the desktop: a fullscreen one pauses the engine, a maximised one slows it to a few frames a second
    Loader { id: cover; active: root.width >= 700; source: "Coverage.qml"; onLoaded: item.screenGeometry = Qt.binding(() => root.Window.window ? Qt.rect(root.Window.window.x, root.Window.window.y, root.Window.window.width, root.Window.window.height) : Qt.rect(0, 0, root.width, root.height)) }
    Rectangle {
        anchors.fill: parent
        color: "#0b0c0b"
        ChaosView {
            anchors.fill: parent
            seed: root.configuration.Seed
            mode: root.configuration.Mode
            fps: root.configuration.Fps
            registry: root.configuration.Registry
            pauseOnBattery: root.configuration.PauseOnBattery
            extras: root.configuration.Extras
            renderScale: root.configuration.RenderScale
            samples: root.configuration.Samples
            lite: root.configuration.Lite
            modelOn: root.configuration.ModelOn
            model: root.configuration.Model
            modelSkin: root.configuration.ModelSkin
            modelColor: root.configuration.ModelColor
            modelSkinSeed: root.configuration.ModelSkinSeed
            coverage: root.configuration.PauseCovered && cover.item ? cover.item.coverage : 0
            // the settings dialog shows this same qml as a small thumbnail; keep that one cheap and quiet
            preview: root.width < 700
        }
    }
}
