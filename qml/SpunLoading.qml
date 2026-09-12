import QtQuick

Item {
    id: control
    property string label: "Loading"
    readonly property bool animating: visible && SpunStyle.motion
    implicitHeight: 32
    Accessible.role: Accessible.Indicator
    Accessible.name: label
    Accessible.description: "In progress"
    Accessible.ignored: !visible
    Rectangle {
        id: track
        width: parent.width; height: 4; radius: 2
        anchors.verticalCenter: parent.verticalCenter
        color: theme.colors.outline
        clip: true
        Rectangle {
            id: indicator
            width: track.width * .35; height: 4; radius: 2
            color: theme.colors.accent
            x: (track.width - width) / 2
        }
    }
    NumberAnimation {
        id: travel
        target: indicator; property: "x"
        from: -indicator.width; to: track.width
        duration: 1400; loops: Animation.Infinite
        running: control.animating
        easing.type: Easing.InOutSine
    }
    onAnimatingChanged: if (!animating) indicator.x = (track.width - indicator.width) / 2
}
