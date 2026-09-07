import QtQuick
import QtQuick.Controls
Switch {
    id: control
    required property var app
    property string glyphName: ""
    implicitHeight: 48
    padding: 0; spacing: 0
    font.family: SpunStyle.family; font.pixelSize: SpunStyle.body
    Accessible.name: text
    background: Rectangle {
        radius: SpunStyle.rowRadius
        color: "transparent"
        SpunStateLayer { anchors.fill: parent; radius: parent.radius; color: control.app.ink; enabled: control.enabled; pressed: control.down; focused: control.visualFocus; hovered: control.hovered }
        border.width: control.visualFocus ? 2 : 0; border.color: control.app.accent
    }
    indicator: Rectangle {
        x: control.width - width - 12; anchors.verticalCenter: parent.verticalCenter
        width: 40; height: 24; radius: 12
        color: control.checked ? control.app.accent : control.app.inset
        border.width: control.checked ? 0 : 2
        border.color: control.app.mutedInk
        opacity: control.enabled ? 1 : .4
        Behavior on color { ColorAnimation { duration: SpunStyle.feedback } }
        Rectangle {
            SpunSpring { id: thumbPosition; targetValue: control.checked ? 19 : 5; epsilon: .1 }
            SpunSpring { id: thumbSize; targetValue: control.checked ? 18 : 14; epsilon: .1 }
            x: thumbPosition.value; anchors.verticalCenter: parent.verticalCenter
            width: thumbSize.value; height: width; radius: width / 2
            color: control.checked ? theme.colors.onAccent : control.app.mutedInk
        }
    }
    contentItem: Item {
        opacity: control.enabled ? 1 : .4
        Glyph { x: 12; anchors.verticalCenter: parent.verticalCenter; name: control.glyphName; ink: control.app.mutedInk }
        SpunText { x: 44; width: control.width - x - 64; anchors.verticalCenter: parent.verticalCenter; text: control.text; font: control.font; color: control.app.ink; elide: Text.ElideRight }
    }
}
