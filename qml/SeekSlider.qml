import QtQuick
import Spun 1.0

SpunSlider {
    id: control
    required property var app
    enabled: visible && !app.swapRunning && app.deckPlayer.duration > 0 && (!app.useCider || app.ciderService.canSeek)
    value: app.progress
    valueText: app.time(value * app.deckPlayer.duration)
    Accessible.name: "Playback position"
    onMoved: app.deckPlayer.seek(value * app.deckPlayer.duration)
    background: Item {
        x: 4; y: 4; width: control.availableWidth; height: control.availableHeight
        readonly property real head: parent.visualPosition * (width - 4) + 2
        opacity: parent.enabled ? 1 : SpunStyle.disabledOpacity
        ProgressRing {
            objectName: control.objectName + "Wave"
            width: Math.max(0, parent.head - 5); height: parent.height
            visible: width > 0
            linear: true; progress: 1
            phase: control.app.wavePhase
            amplitude: control.app.deckPlayer.playing ? 2.8 : 0
            accent: control.app.accent
            Behavior on amplitude { NumberAnimation { duration: SpunStyle.enter } }
        }
        Rectangle {
            anchors.verticalCenter: parent.verticalCenter
            x: Math.min(parent.width, parent.head + 5); width: parent.width - x
            height: 4; radius: 2; color: theme.colors.outline
        }
        Rectangle {
            anchors.verticalCenter: parent.verticalCenter; anchors.right: parent.right
            width: 3; height: 3; radius: 1.5; color: control.app.accent
            visible: control.app.progress < .95
        }
    }
}
