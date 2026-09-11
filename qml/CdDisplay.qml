import QtQuick

Rectangle {
    required property var app
    width: 240; height: 76
    color: "#090e10"
    readonly property color phosphor: Qt.tint("#ffb64f", Qt.alpha(app.accent,.35))
    Text { x: 10; y: 6; text: parent.app.useCider ? "CIDER" : "LOCAL"; color: parent.phosphor; font.pixelSize: 12; font.letterSpacing: 1.3 }
    Text { x: 10; y: 25; width: 218; elide: Text.ElideRight; text: parent.app.deckPlayer.count>0 ? parent.app.deckPlayer.title : "NO DISC"; color: parent.phosphor; font.pixelSize: 19 }
    Text { x: 10; y: 53; text: parent.app.time(parent.app.deckPlayer.position); color: parent.phosphor; font.pixelSize: 13; font.family: "monospace" }
    Text { x: 98; y: 53; text: parent.app.deckPlayer.playing ? "PLAY" : "PAUSE"; color: parent.phosphor; font.pixelSize: 11 }
    Text { anchors.right: parent.right; anchors.rightMargin: 10; y: 53; text: Math.round(parent.app.deckPlayer.volume*100); color: parent.phosphor; font.pixelSize: 11 }
}
