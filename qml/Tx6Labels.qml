import QtQuick

Item {
    width: 244
    height: 352

    Text {
        x: 14
        y: 14
        text: "TX–6"
        font.pixelSize: 17
        font.weight: Font.Light
        color: "#3e4349"
    }

    Text {
        x: 205
        y: 185
        text: "FX"
        font.pixelSize: 10
        font.weight: Font.Light
        color: "#45484d"
    }

    Text {
        x: 198
        y: 299
        text: "shift"
        font.pixelSize: 10
        font.weight: Font.Light
        color: "#45484d"
    }

    Repeater {
        model: 6

        Item {
            required property int index

            x: 22 + index * 29.3

            Repeater {
                model: 5

                Rectangle {
                    required property int index

                    x: -0.9
                    y: 166 + index * 6
                    width: 1.8
                    height: 2.3
                    radius: 1
                    color: "#939695"
                }

            }

            Rectangle {
                x: -0.9
                y: 298
                width: 1.8
                height: 1.8
                radius: 1
                color: "#939695"
            }

        }

    }

}
