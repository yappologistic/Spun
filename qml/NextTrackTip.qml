import QtQuick
import QtQuick.Controls

SpunToolTip {
    id: peek
    required property var app
    objectName: "nextTrackTip"
    property bool requested: false
    readonly property bool remote: app.useCider
    readonly property var track: {
        if (!visible || !requested) return ({})
        if (remote && (!app.ciderService.queueReady || app.ciderService.queueBusy || app.ciderService.queueError.length)) return ({})
        if (!remote && app.deckPlayer.shuffle && app.deckPlayer.count > 1) return ({})
        const queue = app.deckPlayer.queue
        const index = app.deckPlayer.currentIndex + 1
        return queue[index] || (!remote && queue.length ? queue[0] : ({}))
    }
    readonly property string summary: track.title || (remote && app.ciderService.queueBusy ? "Checking next track…" : !remote && app.deckPlayer.shuffle && app.deckPlayer.count > 1 ? "Next track is shuffled" : remote && (!app.ciderService.queueReady || app.ciderService.queueError.length) ? "Next track unavailable" : "No upcoming track")
    onAboutToShow: { requested = true; if (remote) app.ciderService.refreshQueue() }
    onClosed: requested = false
    Connections { target: peek.app.ciderService; function onTrackChanged() { if (peek.visible && peek.remote) peek.app.ciderService.refreshQueue() } }
    implicitWidth: 264; implicitHeight: 80; padding: 12
    contentItem: Loader {
        active: peek.visible
        sourceComponent: Item {
            implicitWidth: 240; implicitHeight: 56
            Rectangle { width: 52; height: 52; anchors.verticalCenter: parent.verticalCenter; radius: 8; color: peek.app.inset
                Image { id: art; anchors.fill: parent; source: peek.track.artwork || ""; sourceSize: Qt.size(104,104); fillMode: Image.PreserveAspectCrop; asynchronous: true; cache: false }
                Glyph { anchors.centerIn: parent; visible: art.status !== Image.Ready; name: "disc"; ink: peek.app.mutedInk }
            }
            SpunText { x: 64; y: 1; width: parent.width - 64; text: "Up next"; color: peek.app.accent; font.pixelSize: SpunStyle.caption }
            SpunText { objectName: "nextTrackTitle"; x: 64; y: 19; width: parent.width - 64; text: peek.summary; color: peek.app.ink; font.pixelSize: SpunStyle.body; elide: Text.ElideRight }
            SpunText { x: 64; y: 40; width: parent.width - 64; text: peek.track.artist || ""; color: peek.app.mutedInk; font.pixelSize: SpunStyle.caption; elide: Text.ElideRight }
        }
    }
}
