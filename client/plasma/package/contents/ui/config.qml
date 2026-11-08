import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.chaos.wallpaper 1.0

// a plain column from the left edge. plasma's FormLayout centres everything, which reads as "way off to the side"
ColumnLayout {
    id: root
    property alias cfg_Seed: seedField.text
    property string cfg_Mode: ""
    property alias cfg_Fps: fpsSpin.value
    property alias cfg_Registry: registryField.text
    property alias cfg_PauseOnBattery: batteryCheck.checked
    property string cfg_Extras: ""
    property double cfg_RenderScale: 1.0
    property int cfg_Samples: 2
    property string cfg_SeedDefault: ""
    property string cfg_ModeDefault: ""
    property int cfg_FpsDefault: 30
    property string cfg_RegistryDefault: "https://chaos-eng.tetstack.dev"
    property bool cfg_PauseOnBatteryDefault: true
    property string cfg_ExtrasDefault: ""
    property double cfg_RenderScaleDefault: 0.75
    property int cfg_SamplesDefault: 0
    property alias cfg_Lite: liteCheck.checked
    property bool cfg_LiteDefault: false
    property alias cfg_PauseCovered: coveredCheck.checked
    property bool cfg_PauseCoveredDefault: false
    property alias cfg_ModelOn: modelCheck.checked
    property bool cfg_ModelOnDefault: false
    property alias cfg_Model: modelField.text
    property string cfg_ModelDefault: ""
    property string cfg_ModelSkin: "mono"
    property string cfg_ModelSkinDefault: "mono"
    property alias cfg_ModelColor: colorField.text
    property string cfg_ModelColorDefault: "#e7eee9"
    property string cfg_ModelSkinSeed: ""
    property string cfg_ModelSkinSeedDefault: ""

    property var vaultEntries: ChaosVault.entries()
    property string liveStatus: ChaosVault.status()
    Timer { interval: 1000; running: true; repeat: true; onTriggered: root.liveStatus = ChaosVault.status() }

    spacing: Kirigami.Units.largeSpacing
    Layout.fillWidth: true

    component Head : QQC2.Label { font.bold: true; Layout.topMargin: Kirigami.Units.largeSpacing }
    component Note : QQC2.Label { opacity: 0.7; wrapMode: Text.WordWrap; Layout.fillWidth: true; Layout.maximumWidth: 720 }

    Head { text: "status" }
    Note { opacity: 1; text: root.liveStatus === "" ? "nothing reported yet (the wallpaper writes here once it draws; log: " + ChaosVault.logPath() + ")" : root.liveStatus }

    Head { text: "which chaos" }
    QQC2.ComboBox {
        id: vaultBox
        Layout.fillWidth: true; Layout.maximumWidth: 720
        textRole: "label"
        model: [{ label: "random each start (needs the registry and a signed-in app)", seed: "" }].concat(root.vaultEntries)
        Component.onCompleted: {
            for (let i = 0; i < model.length; i++) if (model[i].seed === root.cfg_Seed) { currentIndex = i; return }
            currentIndex = root.cfg_Seed === "" ? 0 : -1
        }
        onActivated: seedField.text = model[currentIndex].seed
    }
    Note { text: root.vaultEntries.length === 0 ? "the vault is empty. roll and keep something in the chaos app; everything kept shows up here (" + ChaosVault.vaultDir() + ")" : root.vaultEntries.length + " in " + ChaosVault.vaultDir() + ". claimed first, then rolled, then previews. anything kept plays, online or not." }
    QQC2.TextField { id: seedField; Layout.fillWidth: true; Layout.maximumWidth: 720; placeholderText: "or paste a seed: hex, base58 or 24 words" }

    Head { text: "how it is drawn" }
    RowLayout {
        spacing: Kirigami.Units.largeSpacing
        QQC2.Label { text: "mode" }
        QQC2.ComboBox {
            id: modeBox; textRole: "text"; valueRole: "value"
            model: [{ text: "native (the seed's own)", value: "" }, { text: "zen", value: "zen" }, { text: "calm", value: "calm" }, { text: "chaos", value: "chaos" }, { text: "bleed", value: "bleed" }]
            Component.onCompleted: currentIndex = Math.max(0, indexOfValue(root.cfg_Mode))
            onActivated: root.cfg_Mode = currentValue
        }
        QQC2.Label { text: "draw" }
        QQC2.ComboBox {
            id: extrasBox; textRole: "text"; valueRole: "value"
            model: [{ text: "everything the seed rolled", value: "" }, { text: "no satellites", value: "nosats" }, { text: "the main shape alone", value: "main" }]
            Component.onCompleted: currentIndex = Math.max(0, indexOfValue(root.cfg_Extras))
            onActivated: root.cfg_Extras = currentValue
        }
    }
    Note { text: "draw is this screen's choice only; the chaos itself, its value and its certificate do not change." }

    Head { text: "cost" }
    RowLayout {
        spacing: Kirigami.Units.largeSpacing
        QQC2.Label { text: "fps" }
        QQC2.SpinBox { id: fpsSpin; from: 1; to: 120 }
        QQC2.Label { text: "render size" }
        QQC2.ComboBox {
            id: scaleBox; textRole: "text"; valueRole: "value"
            model: [{ text: "full", value: 1.0 }, { text: "3/4", value: 0.75 }, { text: "half", value: 0.5 }]
            Component.onCompleted: { let i = indexOfValue(root.cfg_RenderScale); currentIndex = i < 0 ? 0 : i }
            onActivated: root.cfg_RenderScale = currentValue
        }
        QQC2.Label { text: "anti-aliasing" }
        QQC2.ComboBox {
            id: aaBox; textRole: "text"; valueRole: "value"
            model: [{ text: "off", value: 0 }, { text: "2x", value: 2 }, { text: "4x", value: 4 }]
            Component.onCompleted: { let i = indexOfValue(root.cfg_Samples); currentIndex = i < 0 ? 0 : i }
            onActivated: root.cfg_Samples = currentValue
        }
    }
    Note { text: "50 fps and above locks to the display's refresh (smoothest), 30 is the light option. a smaller render size draws fewer pixels and the desktop scales it up: the easy win on a 4k screen or on battery." }
    QQC2.CheckBox { id: liteCheck; text: "lite detail (cheaper background noise and bloom; for weak gpus)" }
    QQC2.CheckBox { id: batteryCheck; text: "pause on battery" }
    QQC2.CheckBox { id: coveredCheck; text: "pause under a fullscreen window (games), slow down under a maximised one (off by default: turn it on if you want the gpu back while a window covers the desktop)" }

    Head { text: "your own model" }
    QQC2.CheckBox { id: modelCheck; text: "put a .cheng on the stage instead of the seed's shape (satellites go; the rest stays the seed's)" }
    QQC2.TextField { id: modelField; Layout.fillWidth: true; Layout.maximumWidth: 720; placeholderText: "path to a .cheng (the app's model tab converts obj / stl / gltf / glb / blend into one)" }
    RowLayout {
        spacing: Kirigami.Units.largeSpacing
        QQC2.Label { text: "skin" }
        QQC2.ComboBox {
            id: skinBox; textRole: "text"; valueRole: "value"
            model: [{ text: "own (its textures and colours)", value: "own" }, { text: "berry (the seed's skin)", value: "berry" }, { text: "mono (one colour)", value: "mono" }, { text: "wire", value: "wire" }, { text: "random each start", value: "random" }]
            Component.onCompleted: currentIndex = Math.max(0, indexOfValue(root.cfg_ModelSkin))
            onActivated: root.cfg_ModelSkin = currentValue
        }
        QQC2.Label { text: "colour" }
        QQC2.TextField { id: colorField; placeholderText: "#rrggbb"; Layout.preferredWidth: 110 }
    }
    RowLayout {
        spacing: Kirigami.Units.largeSpacing
        QQC2.Label { text: "look from" }
        QQC2.ComboBox {
            id: lookBox; textRole: "label"; Layout.preferredWidth: 420
            model: [{ label: "the current seed", seed: "" }].concat(root.vaultEntries)
            Component.onCompleted: { for (let i = 0; i < model.length; i++) if (model[i].seed === root.cfg_ModelSkinSeed) { currentIndex = i; return } currentIndex = 0 }
            onActivated: root.cfg_ModelSkinSeed = model[currentIndex].seed
        }
    }
    Note { text: "berry skin, palette, lights and grading from that seed on your model. this is a display choice on this screen: the seed, its value and its file do not change. nothing here is a seed or can be claimed." }

    Head { text: "registry" }
    QQC2.TextField { id: registryField; Layout.fillWidth: true; Layout.maximumWidth: 720 }
    Note { text: "generating needs the registry and a signed-in app. anything claimed plays offline. when nothing may play, the lobby (one fixed public seed) shows instead of a black desktop. log: " + ChaosVault.logPath() }
    Item { Layout.fillHeight: true }
}
