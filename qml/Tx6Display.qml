import QtQuick

Item {
    id: screen

    property var controls

    width: 40
    height: 55

    Rectangle {
        anchors.fill: parent
        color: tx6.powered ? "#c4d9de" : "#344146"
        radius: 4
        border.color: "#717b80"
        border.width: 0.8
    }

    Item {
        anchors.fill: parent
        visible: tx6.powered

        Text {
            x: 3
            y: 3
            text: tx6.available ? "USB    L R" : "LOCAL"
            font.family: "monospace"
            font.pixelSize: 6
            color: "#182d36"
        }

        Text {
            x: 3
            y: 16
            text: screen.controls ? screen.controls.displayTitle : "MASTER"
            font.family: "monospace"
            font.pixelSize: 6
            color: "#182d36"
        }

        Text {
            x: 3
            y: 29
            text: screen.controls ? screen.controls.displayValue : "65"
            font.family: "monospace"
            font.pixelSize: 18
            color: "#172b34"
        }

        Repeater {
            model: 2
            Item {
                required property int index
                property int side: index
                x: 31+index*4
                Repeater {
                    model: 12
                    Rectangle {
                        required property int index
                        y: 47-index*2.8; width: 2.5; height: 1.7
                        color: "#243b43"
                        opacity: tx6.stereoMeters[parent.side]>Math.pow(10,(-48+index*4)/20)?1:.15
                    }
                }
            }
        }
    }
}
