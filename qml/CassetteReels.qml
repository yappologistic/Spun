import QtQuick

Item {
    id: tape
    required property var app
    readonly property real leftAngle: outgoing ? app.outgoingCassetteLeftAngle : app.cassetteLeftAngle
    readonly property real rightAngle: outgoing ? app.outgoingCassetteRightAngle : app.cassetteRightAngle
    property bool outgoing: false
    readonly property real progress: Math.max(0, Math.min(1, app.cassetteVisualProgress))
    Repeater {
        model: 2
        Item {
            required property int index
            x: (index === 0 ? 113.6 : 296.4) - 40; y: 150.2
            width: 80; height: 80
            Rectangle {
                anchors.centerIn: parent
                width: 78
                scale: Math.sqrt(1936 + 3993 * (index === 0 ? 1 - tape.progress : tape.progress)) / 78
                height: width; radius: width / 2
                id: woundTape
                color: "#30251e"; border.width: 1; border.color: "#171513"
                Repeater {
                    model: 28
                    Rectangle {
                        required property int index
                        anchors.centerIn: parent
                        width: 37 + (woundTape.width - 39) * (index + 1) / 29
                        height: width; radius: width / 2; color: "transparent"
                        border.width: .35; border.color: index % 3 ? "#69523b" : "#181512"
                        opacity: .48
                    }
                }
                Behavior on scale { enabled: tape.app.animate && !tape.outgoing; NumberAnimation { duration: 200 } }
            }
            // Clear flange and fixed lighting sit over the moving tape pack.
            Rectangle {
                anchors.centerIn: parent; width: 76; height: 76; radius: 38
                color: "#07d7e0de"; border.width: .6; border.color: "#30d9ddcc"
                Rectangle { anchors.centerIn: parent; width: 69; height: 69; radius: 34.5; color: "transparent"; border.width: .5; border.color: "#1ee2e6d8" }
            }
            Rectangle {
                anchors.centerIn: parent; anchors.verticalCenterOffset: 1.5
                width: 43; height: 43; radius: 21.5; color: "#90090808"
            }
            Rectangle {
                anchors.centerIn: parent; width: 40; height: 40; radius: 20
                gradient: Gradient {
                    GradientStop { position: 0; color: "#ece4d0" }
                    GradientStop { position: .45; color: Qt.tint("#ccc5af", Qt.alpha(tape.app.accent, .08)) }
                    GradientStop { position: 1; color: "#8d8878" }
                }
                border.width: .7; border.color: "#5a584d"
                Rectangle { anchors.centerIn: parent; width: 36; height: 36; radius: 18; color: "transparent"; border.width: .6; border.color: "#80fff4d4" }
                Item {
                    objectName: "cassetteReel" + parent.parent.index
                    anchors.fill: parent
                    rotation: parent.parent.index === 0 ? tape.leftAngle : tape.rightAngle
                    Repeater {
                        model: 3
                        Rectangle {
                            required property int index
                            x: 16; y: 4; width: 8; height: 7; radius: 2.5
                            color: "#37372f"; border.width: .6; border.color: "#aca592"
                            transform: Rotation { origin.x: 4; origin.y: 16; angle: index * 120 }
                        }
                    }
                    Rectangle { anchors.centerIn: parent; width: 18; height: 18; radius: 9; color: "#111414"; border.width: .6; border.color: "#8c8978" }
                    Repeater {
                        model: 6
                        Rectangle {
                            required property int index
                            x: 18.3; y: 10.2; width: 3.4; height: 4.2; radius: .5
                            color: "#d6ceba"
                            transform: Rotation { origin.x: 1.7; origin.y: 9.8; angle: index * 60 }
                        }
                    }
                }
            }
            Rectangle {
                anchors.centerIn: parent; width: 76; height: 76; radius: 38
                gradient: Gradient {
                    GradientStop { position: 0; color: "#12ffffff" }
                    GradientStop { position: .3; color: "#00ffffff" }
                    GradientStop { position: 1; color: "#18000000" }
                }
            }
        }
    }
    Rectangle {
        x: 24; y: 88; width: 362; height: 43; radius: 2
        color: "transparent"
        border.width: 0
        Rectangle { x: 7; y: 7; width: 28; height: 28; radius: 1; color: "#34362f"
            SpunText { anchors.centerIn: parent; text: "A"; font.pixelSize: 21; color: "#e5dfcb" }
        }
        Rectangle { x: 45; y: 32; width: parent.width - 55; height: .7; color: "#807c6c" }
        SpunText {
            x: 45; y: 9; width: parent.width - 55; height: 22
            text: tape.app.deckPlayer.album || tape.app.deckPlayer.title || "Cassette"
            color: "#30332c"; font.pixelSize: SpunStyle.body; font.italic: true; elide: Text.ElideRight
        }
    }
}
