import QtQuick

Rectangle {
    id: display
    required property var app
    width: 52; height: 29; radius: 3
    color: "#0b1218"
    border.width: .6; border.color: "#48545b"
    readonly property bool hasTrack: app.deckPlayer.count > 0
    Text {
        x: 4; y: 3; width: parent.width-8; height: 12
        text: display.hasTrack ? app.time(app.trackVisualProgress * app.deckPlayer.duration) : "00:00"
        color: "#e0ebe7"; font.family: "monospace"; font.pixelSize: 9
        font.letterSpacing: .3; textFormat: Text.PlainText
    }
    Text {
        x: 4; y: 18; width: parent.width-12; height: 8
        text: display.hasTrack ? app.deckPlayer.title : "READY"
        textFormat: Text.PlainText; color: "#c5d4d1"; font.pixelSize: 6; elide: Text.ElideRight
    }
    Rectangle { x: parent.width-6; y: 20; width: 2; height: 2; radius: 1; color: app.deckPlayer.playing ? "#ea883e" : "#586e75" }
    Rectangle {
        x: 1; y: 1; width: parent.width-2; height: 9; radius: 2
        gradient: Gradient { GradientStop { position: 0; color: "#1ce0edf3" } GradientStop { position: 1; color: "transparent" } }
    }
}
